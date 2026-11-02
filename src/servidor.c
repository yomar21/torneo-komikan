#include "servidor.h"

#include "cola.h"
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
    JUGADOR_SIN_PAREJA
} EstadoJugador;

typedef struct {
    int id;                  /* de 1 a N */
    EstadoJugador estado;    /* se usa solo con el mutex tomado */
    pthread_cond_t cond;     /* aqui espera el jugador */
} Jugador;

typedef struct {
    Configuracion config;
    OpcionesServidor opciones;
    pthread_mutex_t mutex;           /* protege la cola, los estados y llegados */
    pthread_cond_t cond_matchmaker;  /* avisa que llego un jugador */
    Cola *cola;                      /* jugadores en orden de llegada */
    Jugador *jugadores;
    int esperados;                   /* cuantos jugadores van a llegar */
    int llegados;
} Servidor;

typedef struct {
    Servidor *servidor;
    int indice;
} DatosJugador;

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

    /* Cada jugador tiene su propio generador de numeros aleatorios. */
    unsigned int estado = servidor->config.semilla ^
                          (2654435761u * (unsigned int)jugador->id);
    int ms = rand_r(&estado) % (servidor->config.retardo_max_ms + 1);
    struct timespec hasta;
    timespec_get(&hasta, TIME_UTC);
    hasta.tv_sec += ms / 1000;
    hasta.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (hasta.tv_nsec >= 1000000000L) {
        hasta.tv_sec++;
        hasta.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&servidor->mutex);
    /* Espera su hora de llegada. Si despierta antes, vuelve a esperar. */
    int error;
    do {
        error = pthread_cond_timedwait(&jugador->cond, &servidor->mutex, &hasta);
    } while (error == 0);

    servidor->llegados++;
    if (cola_encolar(servidor->cola, jugador->id)) {
        if (servidor->opciones.mostrar_eventos) {
            printf("[Servidor] Llega el jugador %d\n", jugador->id);
        }
    } else {
        jugador->estado = JUGADOR_SIN_PAREJA;   /* no deberia pasar */
    }
    pthread_cond_signal(&servidor->cond_matchmaker);

    /* Espera hasta que su partida termine o se sepa que no hay pareja. */
    while (jugador->estado == JUGADOR_EN_ESPERA || jugador->estado == JUGADOR_JUGANDO) {
        pthread_cond_wait(&jugador->cond, &servidor->mutex);
    }
    pthread_mutex_unlock(&servidor->mutex);
    return NULL;
}

/* Juega una partida. Se llama sin el mutex tomado. */
static void jugar_partida(Servidor *servidor, const Tablero *inicial, int numero,
                          int primero, int segundo, ResumenServidor *resumen)
{
    int puma_primero = servidor_puma_es_primero(servidor->config.semilla, numero);
    int id_puma = puma_primero ? primero : segundo;
    int id_perros = puma_primero ? segundo : primero;
    if (servidor->opciones.mostrar_eventos) {
        printf("[Servidor] Emparejados: jugador %d (Puma) vs jugador %d (Perros)\n",
               id_puma, id_perros);
    }

    Configuracion config = servidor->config;
    config.semilla = servidor->config.semilla ^ (unsigned int)numero;
    OpcionesMesa opciones = {
        .limite_propuestas = 0,
        .mostrar_eventos = servidor->opciones.mostrar_eventos,
        .mostrar_tablero = servidor->opciones.mostrar_tablero,
        .id_mesa = 1
    };
    ResumenMesa resumen_mesa;
    if (!mesa_ejecutar(&config, inicial, &opciones, &resumen_mesa)) {
        resumen->errores++;
        return;
    }
    if (resumen_mesa.resultado == VICTORIA_PUMA) {
        resumen->victorias_puma++;
    } else {
        resumen->victorias_perros++;
    }
    if (servidor->opciones.mostrar_eventos) {
        printf("[Servidor] Fin de la partida %d: %s\n", numero,
               mesa_describir_cierre(&resumen_mesa));
    }
}

static void ejecutar_matchmaker(Servidor *servidor, const Tablero *inicial,
                                ResumenServidor *resumen)
{
    pthread_mutex_lock(&servidor->mutex);
    for (;;) {
        /* Espera una pareja o que ya no llegue nadie mas. */
        while (cola_tamano(servidor->cola) < 2 &&
               servidor->llegados < servidor->esperados) {
            pthread_cond_wait(&servidor->cond_matchmaker, &servidor->mutex);
        }
        int primero = 0;
        int segundo = 0;
        if (cola_tamano(servidor->cola) >= 2) {
            cola_desencolar(servidor->cola, &primero);
            cola_desencolar(servidor->cola, &segundo);
            servidor->jugadores[primero - 1].estado = JUGADOR_JUGANDO;
            servidor->jugadores[segundo - 1].estado = JUGADOR_JUGANDO;
            int numero = ++resumen->partidas;

            /* La partida es larga: se juega sin el mutex. */
            pthread_mutex_unlock(&servidor->mutex);
            jugar_partida(servidor, inicial, numero, primero, segundo, resumen);
            pthread_mutex_lock(&servidor->mutex);

            servidor->jugadores[primero - 1].estado = JUGADOR_FINALIZADO;
            servidor->jugadores[segundo - 1].estado = JUGADOR_FINALIZADO;
            resumen->atendidos += 2;
            pthread_cond_signal(&servidor->jugadores[primero - 1].cond);
            pthread_cond_signal(&servidor->jugadores[segundo - 1].cond);
        } else {
            /* Ya llegaron todos. Queda 0 o 1 jugador. */
            if (cola_tamano(servidor->cola) == 1) {
                cola_desencolar(servidor->cola, &primero);
                servidor->jugadores[primero - 1].estado = JUGADOR_SIN_PAREJA;
                resumen->sin_pareja++;
                pthread_cond_signal(&servidor->jugadores[primero - 1].cond);
                if (servidor->opciones.mostrar_eventos) {
                    printf("[Servidor] Jugador %d sin pareja: no hay oponente\n", primero);
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&servidor->mutex);
}

int servidor_ejecutar(const Configuracion *config, const Tablero *inicial,
                      const OpcionesServidor *opciones, ResumenServidor *resumen)
{
    *resumen = (ResumenServidor){0};
    if (config->total_jugadores < 1 || config->retardo_min_ms < 1 ||
        config->retardo_max_ms < config->retardo_min_ms ||
        config->retardo_max_ms > 60000) {
        fprintf(stderr, "Configuracion del servidor invalida.\n");
        return 0;
    }
    int total = config->total_jugadores;
    Servidor servidor = {.config = *config, .opciones = *opciones};
    servidor.cola = cola_crear(total);
    servidor.jugadores = calloc((size_t)total, sizeof(*servidor.jugadores));
    pthread_t *threads = malloc((size_t)total * sizeof(*threads));
    DatosJugador *datos = malloc((size_t)total * sizeof(*datos));
    if (servidor.cola == NULL || servidor.jugadores == NULL ||
        threads == NULL || datos == NULL) {
        fprintf(stderr, "No hay memoria para el servidor.\n");
        goto liberar;
    }
    int error = pthread_mutex_init(&servidor.mutex, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear el mutex: %s\n", strerror(error));
        goto liberar;
    }
    error = pthread_cond_init(&servidor.cond_matchmaker, NULL);
    if (error != 0) {
        fprintf(stderr, "No se pudo crear la condicion: %s\n", strerror(error));
        pthread_mutex_destroy(&servidor.mutex);
        goto liberar;
    }
    int condiciones = 0;   /* jugadores con su condicion creada */
    for (int i = 0; i < total; i++) {
        servidor.jugadores[i].id = i + 1;
        servidor.jugadores[i].estado = JUGADOR_EN_ESPERA;
        if (pthread_cond_init(&servidor.jugadores[i].cond, NULL) != 0) {
            break;
        }
        condiciones++;
    }

    /* Un thread por jugador. Si alguno falla, se espera solo a los creados. */
    for (int i = 0; i < condiciones; i++) {
        datos[i] = (DatosJugador){.servidor = &servidor, .indice = i};
        error = pthread_create(&threads[i], NULL, ejecutar_jugador, &datos[i]);
        if (error != 0) {
            fprintf(stderr, "Error al crear el jugador %d: %s\n", i + 1, strerror(error));
            break;
        }
        resumen->jugadores_creados++;
    }
    pthread_mutex_lock(&servidor.mutex);
    servidor.esperados = resumen->jugadores_creados;
    pthread_cond_signal(&servidor.cond_matchmaker);
    pthread_mutex_unlock(&servidor.mutex);

    ejecutar_matchmaker(&servidor, inicial, resumen);

    for (int i = 0; i < resumen->jugadores_creados; i++) {
        if (pthread_join(threads[i], NULL) == 0) {
            resumen->jugadores_unidos++;
        }
    }
    resumen->llegados = servidor.llegados;
    for (int i = 0; i < condiciones; i++) {
        pthread_cond_destroy(&servidor.jugadores[i].cond);
    }
    pthread_cond_destroy(&servidor.cond_matchmaker);
    pthread_mutex_destroy(&servidor.mutex);

liberar:
    cola_destruir(servidor.cola);
    free(servidor.jugadores);
    free(threads);
    free(datos);
    return resumen->jugadores_creados == total && resumen->jugadores_unidos == total &&
           resumen->errores == 0 && resumen->atendidos + resumen->sin_pareja == total;
}
