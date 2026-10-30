#include "mesa.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* La cola admite una propuesta pendiente por ficha. */
typedef struct {
    int id;
    int origen;
    int intermedia;
    int destino;
    int pendiente;
} Propuesta;

typedef struct {
    Tablero tablero;
    Configuracion config;
    OpcionesMesa opciones;
    ResumenMesa resumen;
    pthread_mutex_t mutex;
    pthread_cond_t cambio;
    int iniciada;
    int terminada;
    int error;
    Propuesta propuestas[TOTAL_FICHAS];
    int cola[TOTAL_FICHAS];
    int cabeza;
    int cantidad;
} Mesa;

typedef struct {
    Mesa *mesa;
    int indice;
    uint32_t aleatorio;
} DatosFicha;

static uint32_t siguiente_aleatorio(uint32_t *estado)
{
    /* Cada ficha tiene su propio estado: no comparte rand() con las otras. */
    *estado = *estado * UINT32_C(1664525) + UINT32_C(1013904223);
    return *estado;
}

static void terminar(Mesa *mesa, MotivoCierre motivo, ResultadoPartida resultado)
{
    mesa->terminada = 1;
    mesa->resumen.motivo = motivo;
    mesa->resumen.resultado = resultado;
    pthread_cond_broadcast(&mesa->cambio);
}

static int esperar_cambio(Mesa *mesa)
{
    int error = pthread_cond_wait(&mesa->cambio, &mesa->mutex);
    if (error != 0) {
        mesa->error = error;
        terminar(mesa, MESA_ERROR, ESTADO_INVALIDO);
    }
    return error == 0;
}

static void esperar_retardo(DatosFicha *datos)
{
    Mesa *mesa = datos->mesa;
    unsigned int rango = (unsigned int)(mesa->config.retardo_max_ms -
                                      mesa->config.retardo_min_ms + 1);
    int ms = mesa->config.retardo_min_ms +
             (int)(siguiente_aleatorio(&datos->aleatorio) % rango);
    struct timespec hasta;
    timespec_get(&hasta, TIME_UTC);
    hasta.tv_sec += ms / 1000;
    hasta.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (hasta.tv_nsec >= 1000000000L) {
        hasta.tv_sec++;
        hasta.tv_nsec -= 1000000000L;
    }

    /* Esta espera libera el mutex; al terminar la partida se despierta antes. */
    while (!mesa->terminada) {
        int error = pthread_cond_timedwait(&mesa->cambio, &mesa->mutex, &hasta);
        if (error == ETIMEDOUT) {
            break;
        }
        if (error != 0) {
            mesa->error = error;
            terminar(mesa, MESA_ERROR, ESTADO_INVALIDO);
            break;
        }
    }
}

static int elegir_propuesta(DatosFicha *datos, Propuesta *propuesta)
{
    Mesa *mesa = datos->mesa;
    int origen = tablero_buscar_ficha(&mesa->tablero, propuesta->id);
    if (origen < 0) {
        return 0;
    }
    propuesta->origen = origen;
    propuesta->intermedia = -1;
    int opciones = 0;

    /* El Puma prefiere capturar si tiene algun salto disponible. */
    if (propuesta->id == ID_PUMA) {
        for (int medio = 0; medio < mesa->tablero.cantidad_posiciones; medio++) {
            for (int destino = 0; destino < mesa->tablero.cantidad_posiciones; destino++) {
                if (tablero_captura_valida(&mesa->tablero, origen, medio, destino)) {
                    opciones++;
                    if (siguiente_aleatorio(&datos->aleatorio) % (unsigned int)opciones == 0) {
                        propuesta->intermedia = medio;
                        propuesta->destino = destino;
                    }
                }
            }
        }
        if (opciones > 0) {
            return 1;
        }
    }
    for (int destino = 0; destino < mesa->tablero.cantidad_posiciones; destino++) {
        if (tablero_movimiento_valido(&mesa->tablero, origen, destino)) {
            opciones++;
            if (siguiente_aleatorio(&datos->aleatorio) % (unsigned int)opciones == 0) {
                propuesta->destino = destino;
            }
        }
    }
    return opciones > 0;
}

static void *ejecutar_ficha(void *argumento)
{
    DatosFicha *datos = argumento;
    Mesa *mesa = datos->mesa;
    Propuesta *propuesta = &mesa->propuestas[datos->indice];
    pthread_mutex_lock(&mesa->mutex);
    while (!mesa->iniciada && !mesa->terminada) {
        esperar_cambio(mesa);
    }

    while (!mesa->terminada) {
        esperar_retardo(datos);
        if (mesa->terminada || tablero_buscar_ficha(&mesa->tablero, propuesta->id) < 0) {
            break;
        }
        if (!elegir_propuesta(datos, propuesta)) {
            continue;
        }
        int final = (mesa->cabeza + mesa->cantidad) % TOTAL_FICHAS;
        mesa->cola[final] = datos->indice;
        mesa->cantidad++;
        propuesta->pendiente = 1;
        pthread_cond_broadcast(&mesa->cambio);

        /* No consultamos en un ciclo ocupado: dormimos hasta la respuesta. */
        while (propuesta->pendiente && !mesa->terminada) {
            esperar_cambio(mesa);
        }
    }
    pthread_mutex_unlock(&mesa->mutex);
    return NULL;
}

static void *ejecutar_juez(void *argumento)
{
    Mesa *mesa = argumento;
    pthread_mutex_lock(&mesa->mutex);
    while (!mesa->iniciada && !mesa->terminada) {
        esperar_cambio(mesa);
    }
    if (!mesa->terminada) {
        ResultadoPartida resultado = tablero_evaluar_resultado(&mesa->tablero);
        if (resultado != PARTIDA_EN_CURSO) {
            terminar(mesa, resultado == ESTADO_INVALIDO ? MESA_ERROR : MESA_POR_VICTORIA,
                     resultado);
        }
    }

    while (!mesa->terminada) {
        while (mesa->cantidad == 0 && !mesa->terminada) {
            esperar_cambio(mesa);
        }
        if (mesa->terminada) {
            break;
        }
        int indice = mesa->cola[mesa->cabeza];
        mesa->cabeza = (mesa->cabeza + 1) % TOTAL_FICHAS;
        mesa->cantidad--;
        Propuesta propuesta = mesa->propuestas[indice];
        int aceptada = 0;

        /* Una propuesta pudo quedar vieja: el juez la valida otra vez. */
        if (mesa->tablero.identificadores[propuesta.origen] == propuesta.id) {
            if (propuesta.intermedia >= 0) {
                aceptada = tablero_capturar(&mesa->tablero, propuesta.origen,
                                            propuesta.intermedia, propuesta.destino);
            } else {
                aceptada = tablero_mover(&mesa->tablero, propuesta.origen,
                                        propuesta.destino);
            }
        }
        mesa->resumen.propuestas++;
        if (aceptada) {
            mesa->resumen.movimientos++;
            mesa->resumen.capturas += propuesta.intermedia >= 0;
        } else {
            mesa->resumen.rechazadas++;
        }
        mesa->propuestas[indice].pendiente = 0;
        ResultadoPartida resultado = tablero_evaluar_resultado(&mesa->tablero);
        if (resultado != PARTIDA_EN_CURSO) {
            terminar(mesa, resultado == ESTADO_INVALIDO ? MESA_ERROR : MESA_POR_VICTORIA,
                     resultado);
        } else if (mesa->resumen.propuestas >= mesa->opciones.limite_propuestas) {
            terminar(mesa, MESA_LIMITE_PRUEBA, PARTIDA_EN_CURSO);
        }
        Tablero copia = mesa->tablero;
        unsigned int numero = mesa->resumen.propuestas;
        pthread_cond_broadcast(&mesa->cambio);
        pthread_mutex_unlock(&mesa->mutex);

        /* Imprimimos una copia para no retener el tablero durante la salida. */
        if (mesa->opciones.mostrar_eventos) {
            printf("[Mesa 1, propuesta %u] Ficha %d: %d -> %d%s: %s\n",
                   numero, propuesta.id, propuesta.origen, propuesta.destino,
                   propuesta.intermedia >= 0 ? " (captura)" : "",
                   aceptada ? "aceptada" : "rechazada; el tablero cambio");
        }
        if (aceptada && mesa->opciones.mostrar_tablero) {
            tablero_imprimir(&copia);
        }
        pthread_mutex_lock(&mesa->mutex);
    }
    pthread_mutex_unlock(&mesa->mutex);
    return NULL;
}

int mesa_ejecutar(const Configuracion *config, const Tablero *inicial,
                 const OpcionesMesa *opciones, ResumenMesa *resumen)
{
    *resumen = (ResumenMesa){.resultado = ESTADO_INVALIDO, .motivo = MESA_ERROR};
    if (config->retardo_min_ms < 1 || config->retardo_max_ms < config->retardo_min_ms ||
        config->retardo_max_ms > 60000 || opciones->limite_propuestas == 0) {
        fprintf(stderr, "Configuracion de mesa invalida.\n");
        return 0;
    }
    Mesa mesa = {0};
    mesa.config = *config;
    mesa.opciones = *opciones;
    if (inicial != NULL) {
        mesa.tablero = *inicial;
    } else {
        tablero_inicializar(&mesa.tablero);
        tablero_colocar_fichas_iniciales(&mesa.tablero);
    }
    if (!tablero_estado_consistente(&mesa.tablero)) {
        fprintf(stderr, "No se puede iniciar una mesa con un tablero inconsistente.\n");
        return 0;
    }
    int error = pthread_mutex_init(&mesa.mutex, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear el mutex: %s\n", strerror(error));
        return 0;
    }
    error = pthread_cond_init(&mesa.cambio, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear la condicion: %s\n", strerror(error));
        pthread_mutex_destroy(&mesa.mutex);
        return 0;
    }
    if (opciones->mostrar_tablero) {
        tablero_imprimir(&mesa.tablero);
    }
    pthread_t juez;
    pthread_t fichas[TOTAL_FICHAS];
    DatosFicha datos[TOTAL_FICHAS];
    int juez_creado = 0;
    error = pthread_create(&juez, NULL, ejecutar_juez, &mesa);
    if (error == 0) {
        juez_creado = 1;
        for (int i = 0; i < TOTAL_FICHAS; i++) {
            mesa.propuestas[i].id = i + 1;
            datos[i] = (DatosFicha){
                .mesa = &mesa, .indice = i,
                .aleatorio = (uint32_t)config->semilla ^
                             (UINT32_C(2654435761) * (uint32_t)(i + 1))
            };
            error = pthread_create(&fichas[i], NULL, ejecutar_ficha, &datos[i]);
            if (error != 0) {
                break;
            }
            mesa.resumen.fichas_creadas++;
        }
    }
    pthread_mutex_lock(&mesa.mutex);
    if (error != 0) {
        mesa.error = error;
        terminar(&mesa, MESA_ERROR, ESTADO_INVALIDO);
    } else {
        /* Abrimos la salida recien cuando todos los threads existen. */
        mesa.iniciada = 1;
        pthread_cond_broadcast(&mesa.cambio);
    }
    pthread_mutex_unlock(&mesa.mutex);

    /* Primero esperamos a los threads; despues destruimos sus recursos. */
    for (int i = 0; i < mesa.resumen.fichas_creadas; i++) {
        int estado = pthread_join(fichas[i], NULL);
        if (estado == 0) {
            mesa.resumen.fichas_unidas++;
        }
    }
    if (juez_creado) {
        mesa.resumen.juez_unido = pthread_join(juez, NULL) == 0;
    }
    mesa.resumen.tablero_final = mesa.tablero;
    *resumen = mesa.resumen;
    pthread_cond_destroy(&mesa.cambio);
    pthread_mutex_destroy(&mesa.mutex);
    if (mesa.error != 0) {
        fprintf(stderr, "Error de threads: %s\n", strerror(mesa.error));
    }
    return resumen->motivo != MESA_ERROR &&
           resumen->fichas_creadas == resumen->fichas_unidas && resumen->juez_unido;
}

const char *mesa_describir_cierre(const ResumenMesa *resumen)
{
    if (resumen->motivo == MESA_LIMITE_PRUEBA) {
        return "limite de demostracion alcanzado; sin ganador";
    }
    if (resumen->motivo == MESA_ERROR) {
        return "partida abortada por error";
    }
    if (resumen->resultado == VICTORIA_PUMA) {
        return "gana el Puma";
    }
    return "ganan los Perros";
}
