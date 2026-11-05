#include "servidor.h"

#include "cola.h"
#include "log.h"
#include "mesa.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    JUGADOR_EN_ESPERA,
    JUGADOR_JUGANDO,
    JUGADOR_FINALIZADO,
    JUGADOR_SIN_PAREJA,
    JUGADOR_CANCELADO,       /* el servidor se detuvo antes de que jugara */
    JUGADOR_FALLIDO          /* termino inesperadamente antes de jugar */
} EstadoJugador;

typedef struct {
    int id;                  /* de 1 a N */
    EstadoJugador estado;    /* se usa solo con el mutex tomado */
    pthread_cond_t cond;     /* aqui espera el jugador */
    struct SlotMesa *slot;   /* su partida; se fija al emparejarlo */
    Bando bando;
    int llego;               /* ya fue contado en servidor->llegados */
} Jugador;

/* Una mesa fisica. Hay a lo mas M y se reutilizan. */
typedef struct SlotMesa {
    Servidor *servidor;
    int indice;              /* el id de la mesa es indice + 1 */
    int ocupado;             /* se usa solo con el mutex tomado */
    int tiene_hilo;          /* hay un thread que falta recoger con join */
    pthread_t hilo;
    Mesa *mesa;              /* mesa en juego; solo con el mutex del servidor tomado */
    int abandono;            /* 0 = nadie; 1 = sale Puma; 2 = salen Perros */
    int fallo_pendiente;     /* 1: la salida fue inesperada; decidir solo en el juez */
    int numero;              /* numero de la partida */
    int primero;
    int segundo;
} SlotMesa;

struct Servidor {
    Configuracion config;
    OpcionesServidor opciones;
    Tablero inicial;
    int usar_inicial;
    ResumenServidor resumen;         /* se actualiza con el mutex tomado */
    pthread_mutex_t mutex;           /* protege la cola, los estados, las mesas y el resumen */
    pthread_cond_t cond_matchmaker;  /* avisa que llego un jugador, se libero una mesa o se detuvo */
    Cola *cola;                      /* jugadores en orden de llegada */
    Jugador *jugadores;
    SlotMesa *slots;
    int max_mesas;                   /* tamano de slots */
    int mesas_activas;
    int esperados;                   /* cuantos jugadores van a llegar */
    int llegados;
    int detenido;                    /* ya no se aceptan mas jugadores */
    int corrido;                     /* servidor_correr ya se llamo */
    int registro_completo;           /* ya se fijo el numero de hilos creados */
    int fallos_antes_de_llegar;      /* hilos terminados antes de contar su llegada */
};

typedef struct {
    Servidor *servidor;
    int indice;
} DatosJugador;

/* Al cancelar un pthread_cond_wait, POSIX vuelve a tomar el mutex ANTES de
 * ejecutar los handlers de limpieza. El indicador evita tomarlo dos veces.
 * Solo se habilita la cancelacion diferida mientras el hilo espera. */
typedef struct {
    Servidor *servidor;
    Jugador *jugador;
    int mutex_tomado;
} LimpiezaJugador;

static int esperar_jugador(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    int error = pthread_cond_wait(cond, mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    return error;
}

static int esperar_jugador_hasta(pthread_cond_t *cond, pthread_mutex_t *mutex,
                                 const struct timespec *plazo)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    int error = pthread_cond_timedwait(cond, mutex, plazo);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    return error;
}

/* Remueve un jugador de la cola sin alterar el orden de los demas. Se llama
 * exclusivamente con el mutex del servidor adquirido. */
static void retirar_de_cola(Cola *cola, int id)
{
    int cantidad = cola_tamano(cola);
    for (int i = 0; i < cantidad; i++) {
        int actual;
        cola_desencolar(cola, &actual);
        if (actual != id) {
            cola_encolar(cola, actual);
        }
    }
}

/* Se ejecuta si el hilo sale mediante pthread_exit() o pthread_cancel().
 * No se ejecuta en el retorno normal: pthread_cleanup_pop(0).
 * Una senal fatal como SIGSEGV sin aislar procesos NO se puede recuperar aqui. */
static void limpiar_jugador_fallido(void *argumento)
{
    LimpiezaJugador *limpieza = argumento;
    Servidor *servidor = limpieza->servidor;
    Jugador *jugador = limpieza->jugador;
    if (!limpieza->mutex_tomado) {
        pthread_mutex_lock(&servidor->mutex);
    }
    int fallo_en_partida = jugador->estado == JUGADOR_JUGANDO;
    int numero_partida = fallo_en_partida && jugador->slot != NULL ?
                         jugador->slot->numero : 0;

    if (jugador->estado == JUGADOR_JUGANDO) {
        SlotMesa *slot = jugador->slot;
        servidor->resumen.fallos_jugadores++;
        /* La salida se convierte en EVENTO. El handler no adjudica victorias:
         * unicamente despierta al juez (o guarda el aviso si no hay mesa aun).
         * Lock order: servidor -> mesa, nunca en sentido contrario. */
        if (slot != NULL && slot->abandono == 0) {
            slot->abandono = jugador->bando == BANDO_PUMA ? 1 : 2;
            slot->fallo_pendiente = 1;
            if (slot->mesa != NULL) {
                mesa_notificar_fallo(slot->mesa, jugador->bando);
            }
        }
    } else if (jugador->estado == JUGADOR_EN_ESPERA) {
        /* Un fallo antes de jugar no debe dejar una entrada fantasma en la
         * cola ni bloquear al matchmaker esperando una llegada imposible. */
        jugador->estado = JUGADOR_FALLIDO;
        servidor->resumen.fallos_sin_partida++;
        if (jugador->llego) {
            retirar_de_cola(servidor->cola, jugador->id);
        } else if (servidor->registro_completo) {
            servidor->esperados--;
        } else {
            servidor->fallos_antes_de_llegar++;
        }
        pthread_cond_signal(&servidor->cond_matchmaker);
    }
    limpieza->mutex_tomado = 0;
    pthread_mutex_unlock(&servidor->mutex);
    if (fallo_en_partida && servidor->opciones.mostrar_eventos) {
        log_evento(0, "Fallo inesperado del hilo del jugador %d durante la partida %d",
                   jugador->id, numero_partida);
    }
}

int servidor_puma_es_primero(unsigned int semilla, int numero_partida)
{
    /* rand_r guarda su estado en la variable: sirve con threads. */
    unsigned int estado = semilla ^ (2246822519u * (unsigned int)numero_partida);
    return rand_r(&estado) % 2 == 0;
}

static void *ejecutar_jugador(void *argumento)
{
    DatosJugador *datos = argumento;
    Servidor *servidor = datos->servidor;
    Jugador *jugador = &servidor->jugadores[datos->indice];
    LimpiezaJugador limpieza = {servidor, jugador, 0};
    pthread_cleanup_push(limpiar_jugador_fallido, &limpieza);
    /* Fuera de las esperas, evita interrumpirse dentro de logs o locks. */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    /* Cada jugador tiene su propio generador de numeros aleatorios. */
    unsigned int estado = servidor->config.semilla ^
                          (2654435761u * (unsigned int)jugador->id);
    int ms = rand_r(&estado) % (servidor->config.retardo_max_ms + 1);
    /* Con su propio generador decide si se retirara de su partida y cuando. */
    int abandonara = rand_r(&estado) % 100 < servidor->config.prob_abandono;
    int ms_abandono = rand_r(&estado) % (servidor->config.abandono_max_ms + 1);
    struct timespec hasta;
    timespec_get(&hasta, TIME_UTC);
    hasta.tv_sec += ms / 1000;
    hasta.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (hasta.tv_nsec >= 1000000000L) {
        hasta.tv_sec++;
        hasta.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&servidor->mutex);
    limpieza.mutex_tomado = 1;
    /* Espera su hora de llegada. Solo servidor_detener lo despierta antes. */
    while (jugador->estado == JUGADOR_EN_ESPERA) {
        int error = esperar_jugador_hasta(&jugador->cond, &servidor->mutex, &hasta);
        if (error == ETIMEDOUT) {
            break;
        }
        if (error != 0) {
            pthread_exit(PTHREAD_CANCELED); /* activa la limpieza */
        }
    }

    /* Si lo cancelaron mientras esperaba, no llega a la cola. */
    if (jugador->estado == JUGADOR_EN_ESPERA) {
        servidor->llegados++;
        jugador->llego = 1;
        if (cola_encolar(servidor->cola, jugador->id)) {
            if (servidor->opciones.mostrar_eventos) {
                log_evento(0, "Llega el jugador %d", jugador->id);
            }
        } else {
            jugador->estado = JUGADOR_SIN_PAREJA;   /* no deberia pasar */
            servidor->resumen.sin_pareja++;
        }
        pthread_cond_signal(&servidor->cond_matchmaker);
    }

    /* Espera pareja. */
    while (jugador->estado == JUGADOR_EN_ESPERA) {
        if (esperar_jugador(&jugador->cond, &servidor->mutex) != 0) {
            pthread_exit(PTHREAD_CANCELED);
        }
    }

#ifdef KOMIKAN_TEST_FALLO_INTEGRADO
    /* Unicamente en el ejecutable de pruebas: provoca la terminacion real
     * del jugador 1 tras el emparejamiento. La rutina POSIX registrada arriba
     * notifica al juez; el ejecutable normal no contiene esta ruta. */
    if (jugador->estado == JUGADOR_JUGANDO && jugador->id == 1) {
        pthread_exit(PTHREAD_CANCELED);
    }
#endif

    /* Ya juega. Si le toca retirarse, espera su instante o el fin de la partida. */
    if (jugador->estado == JUGADOR_JUGANDO && abandonara) {
        struct timespec plazo;
        timespec_get(&plazo, TIME_UTC);
        plazo.tv_sec += ms_abandono / 1000;
        plazo.tv_nsec += (long)(ms_abandono % 1000) * 1000000L;
        if (plazo.tv_nsec >= 1000000000L) {
            plazo.tv_sec++;
            plazo.tv_nsec -= 1000000000L;
        }
        while (jugador->estado == JUGADOR_JUGANDO) {
            int error = esperar_jugador_hasta(&jugador->cond, &servidor->mutex, &plazo);
            if (error == ETIMEDOUT) {
                break;
            }
            if (error != 0) {
                pthread_exit(PTHREAD_CANCELED);
            }
        }
        if (jugador->estado == JUGADOR_JUGANDO) {
            /* Vencio el plazo y su partida sigue. Se avisa con el mutex del servidor
             * tomado: asi el hilo de la mesa no puede destruirla en medio. Si la mesa
             * aun no existe, queda anotado y la mesa lo respeta al crearse. */
            SlotMesa *slot = jugador->slot;
            if (slot->abandono == 0) {
                slot->abandono = jugador->bando == BANDO_PUMA ? 1 : 2;
                /* Solo el primer aviso se entrega. El juez, NO el hilo del
                 * jugador, decidira el resultado cuando procese el evento. */
                if (slot->mesa != NULL) {
                    mesa_abandono(slot->mesa, jugador->bando);
                }
            }
        }
    }

    /* Espera hasta que su partida termine o lo cancelen. */
    while (jugador->estado == JUGADOR_JUGANDO) {
        if (esperar_jugador(&jugador->cond, &servidor->mutex) != 0) {
            pthread_exit(PTHREAD_CANCELED);
        }
    }
    pthread_mutex_unlock(&servidor->mutex);
    limpieza.mutex_tomado = 0;
    pthread_cleanup_pop(0);         /* terminacion normal: no hay abandono */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    return NULL;
}

/* Juega una partida. Se llama sin el mutex tomado.
 * Devuelve 0 si hubo un error, 1 si gano el Puma, 2 si ganaron los Perros y
 * 3 si la partida se cancelo y 4 si termino por abandono. */
static int jugar_partida(Servidor *servidor, SlotMesa *slot, int numero, int id_mesa,
                         int primero, int segundo)
{
    int puma_primero = servidor_puma_es_primero(servidor->config.semilla, numero);
    int id_puma = puma_primero ? primero : segundo;
    int id_perros = puma_primero ? segundo : primero;
    if (servidor->opciones.mostrar_eventos) {
        log_evento(0, "Mesa %d: jugador %d (Puma) vs jugador %d (Perros)",
                   id_mesa, id_puma, id_perros);
    }

    Configuracion config = servidor->config;
    config.semilla = servidor->config.semilla ^ (unsigned int)numero;
    OpcionesMesa opciones = {
        .limite_propuestas = 0,
        .mostrar_eventos = servidor->opciones.mostrar_eventos,
        .mostrar_tablero = servidor->opciones.mostrar_tablero,
        .id_mesa = id_mesa
    };
    Mesa *mesa = mesa_crear(&config, servidor->usar_inicial ? &servidor->inicial : NULL,
                            &opciones);
    if (mesa == NULL) {
        return 0;
    }

    /* Desde aqui servidor_detener puede cancelar esta mesa. Si ya se detuvo, la
     * cancelamos nosotros: asi no se escapa una mesa creada justo despues. */
    pthread_mutex_lock(&servidor->mutex);
    slot->mesa = mesa;
    int ya_detenido = servidor->detenido;
    int abandono = slot->abandono;
    int fallo_pendiente = slot->fallo_pendiente;
    pthread_mutex_unlock(&servidor->mutex);
    if (ya_detenido) {
        mesa_cancelar(mesa);
    }
    if (abandono != 0) {
        if (fallo_pendiente) {
            mesa_notificar_fallo(mesa, abandono == 1 ? BANDO_PUMA : BANDO_PERROS);
        } else {
            mesa_abandono(mesa, abandono == 1 ? BANDO_PUMA : BANDO_PERROS);
        }
    }

    ResumenMesa resumen_mesa;
    int correcto = mesa_jugar(mesa, &resumen_mesa);

    /* El puntero se borra ANTES de destruir la mesa, con el mutex del servidor
     * tomado: asi servidor_detener nunca toca una mesa ya liberada. */
    pthread_mutex_lock(&servidor->mutex);
    slot->mesa = NULL;
    pthread_mutex_unlock(&servidor->mutex);
    mesa_destruir(mesa);

    if (!correcto) {
        return 0;
    }
    if (servidor->opciones.mostrar_eventos) {
        log_evento(0, "Fin de la partida %d (mesa %d): %s", numero, id_mesa,
                   mesa_describir_cierre(&resumen_mesa));
    }
    if (resumen_mesa.motivo == MESA_CANCELADA) {
        return 3;
    }
    if (resumen_mesa.motivo == MESA_ABANDONO ||
        resumen_mesa.motivo == MESA_FALLO_JUGADOR) {
        return 4;
    }
    return resumen_mesa.resultado == VICTORIA_PUMA ? 1 : 2;
}

/* Thread de una mesa: juega la partida y despues libera la mesa. */
static void *ejecutar_mesa(void *argumento)
{
    SlotMesa *slot = argumento;
    Servidor *servidor = slot->servidor;
    int resultado = jugar_partida(servidor, slot, slot->numero, slot->indice + 1,
                                  slot->primero, slot->segundo);

    pthread_mutex_lock(&servidor->mutex);
    ResumenServidor *resumen = &servidor->resumen;
    EstadoJugador estado_final = JUGADOR_FINALIZADO;
    if (resultado == 0) {
        resumen->errores++;
        resumen->atendidos += 2;
    } else if (resultado == 1) {
        resumen->victorias_puma++;
        resumen->atendidos += 2;
    } else if (resultado == 2) {
        resumen->victorias_perros++;
        resumen->atendidos += 2;
    } else if (resultado == 4) {
        resumen->por_abandono++;
        resumen->atendidos += 2;
    } else {
        /* Partida cancelada por Ctrl+C: sus dos jugadores quedan cancelados. */
        resumen->cancelados_en_partida++;
        resumen->cancelados += 2;
        estado_final = JUGADOR_CANCELADO;
    }
    servidor->jugadores[slot->primero - 1].estado = estado_final;
    servidor->jugadores[slot->segundo - 1].estado = estado_final;
    slot->ocupado = 0;
    servidor->mesas_activas--;
    pthread_cond_signal(&servidor->jugadores[slot->primero - 1].cond);
    pthread_cond_signal(&servidor->jugadores[slot->segundo - 1].cond);
    pthread_cond_signal(&servidor->cond_matchmaker);   /* hay una mesa libre */
    pthread_mutex_unlock(&servidor->mutex);
    return NULL;
}

static void ejecutar_matchmaker(Servidor *servidor)
{
    ResumenServidor *resumen = &servidor->resumen;
    pthread_mutex_lock(&servidor->mutex);
    for (;;) {
        /* Espera una pareja y una mesa libre, que ya no llegue nadie mas o que
         * se detenga el servidor. */
        while (!servidor->detenido &&
               !(cola_tamano(servidor->cola) >= 2 &&
                 servidor->mesas_activas < servidor->max_mesas) &&
               !(servidor->llegados >= servidor->esperados &&
                 cola_tamano(servidor->cola) < 2)) {
            pthread_cond_wait(&servidor->cond_matchmaker, &servidor->mutex);
        }
        if (servidor->detenido) {
            break;   /* servidor_detener ya cancelo las mesas en juego */
        }
        int primero = 0;
        int segundo = 0;
        if (cola_tamano(servidor->cola) >= 2) {
            cola_desencolar(servidor->cola, &primero);
            cola_desencolar(servidor->cola, &segundo);
            servidor->jugadores[primero - 1].estado = JUGADOR_JUGANDO;
            servidor->jugadores[segundo - 1].estado = JUGADOR_JUGANDO;

            /* Hay una mesa libre porque mesas_activas < max_mesas. */
            int indice = 0;
            while (servidor->slots[indice].ocupado) {
                indice++;
            }
            SlotMesa *slot = &servidor->slots[indice];
            int recoger = slot->tiene_hilo;
            pthread_t anterior = slot->hilo;
            slot->tiene_hilo = 0;
            slot->ocupado = 1;
            slot->numero = ++resumen->partidas;
            slot->primero = primero;
            slot->segundo = segundo;
            slot->abandono = 0;
            slot->fallo_pendiente = 0;
            int puma_primero = servidor_puma_es_primero(servidor->config.semilla, slot->numero);
            servidor->jugadores[primero - 1].slot = slot;
            servidor->jugadores[primero - 1].bando = puma_primero ? BANDO_PUMA : BANDO_PERROS;
            servidor->jugadores[segundo - 1].slot = slot;
            servidor->jugadores[segundo - 1].bando = puma_primero ? BANDO_PERROS : BANDO_PUMA;
            /* Les avisa que ya juegan: si les toca retirarse, empiezan a contar. */
            pthread_cond_signal(&servidor->jugadores[primero - 1].cond);
            pthread_cond_signal(&servidor->jugadores[segundo - 1].cond);
            servidor->mesas_activas++;
            if (servidor->mesas_activas > resumen->max_simultaneas) {
                resumen->max_simultaneas = servidor->mesas_activas;
            }

            /* El join y la creacion se hacen sin el mutex. La mesa ya esta
             * marcada como ocupada, asi que nadie mas la toma. */
            pthread_mutex_unlock(&servidor->mutex);
            if (recoger) {
                pthread_join(anterior, NULL);   /* ya habia terminado */
            }
            int error = pthread_create(&slot->hilo, NULL, ejecutar_mesa, slot);
            pthread_mutex_lock(&servidor->mutex);
            if (error == 0) {
                slot->tiene_hilo = 1;
            } else {
                fprintf(stderr, "Error al crear la mesa %d: %s\n", indice + 1,
                        strerror(error));
                resumen->errores++;
                resumen->atendidos += 2;
                slot->ocupado = 0;
                servidor->mesas_activas--;
                servidor->jugadores[primero - 1].estado = JUGADOR_FINALIZADO;
                servidor->jugadores[segundo - 1].estado = JUGADOR_FINALIZADO;
                pthread_cond_signal(&servidor->jugadores[primero - 1].cond);
                pthread_cond_signal(&servidor->jugadores[segundo - 1].cond);
            }
        } else {
            /* Ya llegaron todos. Queda 0 o 1 jugador. */
            if (cola_tamano(servidor->cola) == 1) {
                cola_desencolar(servidor->cola, &primero);
                servidor->jugadores[primero - 1].estado = JUGADOR_SIN_PAREJA;
                resumen->sin_pareja++;
                pthread_cond_signal(&servidor->jugadores[primero - 1].cond);
                if (servidor->opciones.mostrar_eventos) {
                    log_evento(0, "Jugador %d sin pareja: no hay oponente", primero);
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&servidor->mutex);
}

/* Libera lo que servidor_crear logro reservar. */
static void liberar_servidor(Servidor *servidor, int condiciones, int con_mutex, int con_cond)
{
    for (int i = 0; i < condiciones; i++) {
        pthread_cond_destroy(&servidor->jugadores[i].cond);
    }
    if (con_cond) {
        pthread_cond_destroy(&servidor->cond_matchmaker);
    }
    if (con_mutex) {
        pthread_mutex_destroy(&servidor->mutex);
    }
    cola_destruir(servidor->cola);
    free(servidor->jugadores);
    free(servidor->slots);
    free(servidor);
}

Servidor *servidor_crear(const Configuracion *config, const Tablero *inicial,
                         const OpcionesServidor *opciones)
{
    if (config->total_jugadores < 1 || config->max_mesas < 1 ||
        config->retardo_min_ms < 1 ||
        config->retardo_max_ms < config->retardo_min_ms ||
        config->retardo_max_ms > 60000 ||
        config->prob_abandono < 0 || config->prob_abandono > 100 ||
        config->abandono_max_ms < 1 || config->abandono_max_ms > 60000) {
        fprintf(stderr, "Configuracion del servidor invalida.\n");
        return NULL;
    }
    Servidor *servidor = calloc(1, sizeof(*servidor));
    if (servidor == NULL) {
        fprintf(stderr, "No hay memoria para el servidor.\n");
        return NULL;
    }
    int total = config->total_jugadores;
    /* No hacen falta mas mesas que parejas. */
    int mesas = total / 2 < 1 ? 1 : total / 2;
    if (config->max_mesas < mesas) {
        mesas = config->max_mesas;
    }
    servidor->config = *config;
    servidor->opciones = *opciones;
    servidor->max_mesas = mesas;
    if (inicial != NULL) {
        servidor->inicial = *inicial;
        servidor->usar_inicial = 1;
    }
    servidor->cola = cola_crear(total);
    servidor->jugadores = calloc((size_t)total, sizeof(*servidor->jugadores));
    servidor->slots = calloc((size_t)mesas, sizeof(*servidor->slots));
    if (servidor->cola == NULL || servidor->jugadores == NULL || servidor->slots == NULL) {
        fprintf(stderr, "No hay memoria para el servidor.\n");
        liberar_servidor(servidor, 0, 0, 0);
        return NULL;
    }
    for (int i = 0; i < mesas; i++) {
        servidor->slots[i].servidor = servidor;
        servidor->slots[i].indice = i;
    }
    int error = pthread_mutex_init(&servidor->mutex, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear el mutex: %s\n", strerror(error));
        liberar_servidor(servidor, 0, 0, 0);
        return NULL;
    }
    error = pthread_cond_init(&servidor->cond_matchmaker, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear la condicion: %s\n", strerror(error));
        liberar_servidor(servidor, 0, 1, 0);
        return NULL;
    }
    int condiciones = 0;   /* jugadores con su condicion creada */
    for (int i = 0; i < total; i++) {
        servidor->jugadores[i].id = i + 1;
        servidor->jugadores[i].estado = JUGADOR_EN_ESPERA;
        error = pthread_cond_init(&servidor->jugadores[i].cond, NULL);
        if (error != 0) {
            fprintf(stderr, "No se pudo crear la condicion: %s\n", strerror(error));
            liberar_servidor(servidor, condiciones, 1, 1);
            return NULL;
        }
        condiciones++;
    }
    return servidor;
}

void servidor_destruir(Servidor *servidor)
{
    if (servidor == NULL) {
        return;
    }
    liberar_servidor(servidor, servidor->config.total_jugadores, 1, 1);
}

void servidor_detener(Servidor *servidor)
{
    pthread_mutex_lock(&servidor->mutex);
    if (servidor->detenido) {
        pthread_mutex_unlock(&servidor->mutex);
        return;
    }
    servidor->detenido = 1;

    /* Cancela a los que no han llegado y a los que esperan en la cola. Los que
     * ya estan jugando siguen: su partida termina por su cuenta. */
    int cancelados = 0;
    for (int i = 0; i < servidor->config.total_jugadores; i++) {
        Jugador *jugador = &servidor->jugadores[i];
        if (jugador->estado == JUGADOR_EN_ESPERA) {
            jugador->estado = JUGADOR_CANCELADO;
            cancelados++;
            pthread_cond_signal(&jugador->cond);
        }
    }
    servidor->resumen.cancelados += cancelados;
    int vaciado;
    while (cola_desencolar(servidor->cola, &vaciado)) {
    }
    pthread_cond_broadcast(&servidor->cond_matchmaker);

    /* Pide a cada mesa en juego que termine. Orden de locks: servidor -> mesa.
     * Ninguna mesa toma el mutex del servidor mientras tiene el suyo. El puntero
     * es NULL cuando la mesa ya termino, asi que nunca se cancela una liberada. */
    int mesas_canceladas = 0;
    for (int i = 0; i < servidor->max_mesas; i++) {
        if (servidor->slots[i].mesa != NULL) {
            mesa_cancelar(servidor->slots[i].mesa);
            mesas_canceladas++;
        }
    }
    pthread_mutex_unlock(&servidor->mutex);

    /* El log se escribe fuera del mutex del servidor. */
    if (servidor->opciones.mostrar_eventos) {
        log_evento(0, "Servidor detenido: no entra nadie mas (%d jugadores cancelados, "
                   "%d mesas canceladas)", cancelados, mesas_canceladas);
    }
}

int servidor_correr(Servidor *servidor, ResumenServidor *resumen)
{
    int total = servidor->config.total_jugadores;
    pthread_mutex_lock(&servidor->mutex);
    int repetido = servidor->corrido;
    servidor->corrido = 1;
    pthread_mutex_unlock(&servidor->mutex);
    if (repetido) {
        fprintf(stderr, "El servidor ya se ejecuto.\n");
        *resumen = (ResumenServidor){0};
        return 0;
    }
    pthread_t *threads = malloc((size_t)total * sizeof(*threads));
    DatosJugador *datos = malloc((size_t)total * sizeof(*datos));
    if (threads == NULL || datos == NULL) {
        fprintf(stderr, "No hay memoria para el servidor.\n");
        free(threads);
        free(datos);
        *resumen = (ResumenServidor){0};
        return 0;
    }

    /* Un thread por jugador. Si alguno falla, se espera solo a los creados. */
    int creados = 0;
    for (int i = 0; i < total; i++) {
        datos[i] = (DatosJugador){.servidor = servidor, .indice = i};
        int error = pthread_create(&threads[i], NULL, ejecutar_jugador, &datos[i]);
        if (error != 0) {
            fprintf(stderr, "Error al crear el jugador %d: %s\n", i + 1, strerror(error));
            break;
        }
        creados++;
    }
    pthread_mutex_lock(&servidor->mutex);
    servidor->resumen.jugadores_creados = creados;
    servidor->esperados = creados - servidor->fallos_antes_de_llegar;
    servidor->registro_completo = 1;
    pthread_cond_signal(&servidor->cond_matchmaker);
    pthread_mutex_unlock(&servidor->mutex);

    ejecutar_matchmaker(servidor);

    /* Recoge los threads de las mesas y despues los de los jugadores. */
    for (int i = 0; i < servidor->max_mesas; i++) {
        if (servidor->slots[i].tiene_hilo) {
            pthread_join(servidor->slots[i].hilo, NULL);
            servidor->slots[i].tiene_hilo = 0;
        }
    }
    int unidos = 0;
    for (int i = 0; i < creados; i++) {
        if (pthread_join(threads[i], NULL) == 0) {
            unidos++;
        }
    }
    pthread_mutex_lock(&servidor->mutex);
    servidor->resumen.jugadores_unidos = unidos;
    servidor->resumen.llegados = servidor->llegados;
    *resumen = servidor->resumen;
    pthread_mutex_unlock(&servidor->mutex);
    free(threads);
    free(datos);
    return resumen->jugadores_creados == total && resumen->jugadores_unidos == total &&
           resumen->errores == 0 && resumen->fallos_sin_partida == 0 &&
           servidor_resumen_coherente(resumen, total);
}

int servidor_ejecutar(const Configuracion *config, const Tablero *inicial,
                      const OpcionesServidor *opciones, ResumenServidor *resumen)
{
    *resumen = (ResumenServidor){0};
    Servidor *servidor = servidor_crear(config, inicial, opciones);
    if (servidor == NULL) {
        return 0;
    }
    int correcto = servidor_correr(servidor, resumen);
    servidor_destruir(servidor);
    return correcto;
}

void servidor_imprimir_resumen(const ResumenServidor *resumen, int max_mesas)
{
    log_evento(0, "---- Resumen del torneo ----");
    log_evento(0, "Jugadores: %d llegaron, %d jugaron, %d quedaron sin pareja y %d cancelados.",
               resumen->llegados, resumen->atendidos, resumen->sin_pareja, resumen->cancelados);
    log_evento(0, "Fallos inesperados de hilos: %d durante partidas, %d antes de jugar.",
               resumen->fallos_jugadores, resumen->fallos_sin_partida);
    log_evento(0, "Partidas: %d (gana el Puma: %d; ganan los Perros: %d; por abandono: %d; "
               "canceladas: %d; con error: %d).",
               resumen->partidas, resumen->victorias_puma, resumen->victorias_perros,
               resumen->por_abandono, resumen->cancelados_en_partida, resumen->errores);
    log_evento(0, "Maximo de mesas activas a la vez: %d (limite %d).",
               resumen->max_simultaneas, max_mesas);
    log_evento(0, "Threads de jugador recogidos: %d/%d.",
               resumen->jugadores_unidos, resumen->jugadores_creados);
}

int servidor_resumen_coherente(const ResumenServidor *resumen, int total)
{
    int jugadores = resumen->atendidos + resumen->sin_pareja + resumen->cancelados +
                   resumen->fallos_sin_partida;
    int partidas = resumen->victorias_puma + resumen->victorias_perros +
                   resumen->por_abandono + resumen->cancelados_en_partida + resumen->errores;
    return jugadores == total && resumen->partidas == partidas;
}
