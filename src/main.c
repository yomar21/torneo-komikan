#include "configuracion.h"
#include "log.h"
#include "servidor.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Servidor *servidor;
    sigset_t senales;
} DatosSenales;

/* Thread dedicado a las senales. Un manejador clasico no puede tomar mutex ni
 * usar el log; aqui, con sigwait, si. SIGUSR1 es el aviso interno de cierre. */
static void *atender_senales(void *argumento)
{
    DatosSenales *datos = argumento;
    for (;;) {
        int senal = 0;
        if (sigwait(&datos->senales, &senal) != 0) {
            continue;
        }
        if (senal == SIGUSR1) {
            break;
        }
        log_evento(0, "Ctrl+C recibido: no se aceptan mas jugadores");
        servidor_detener(datos->servidor);   /* con otro Ctrl+C no pasa nada mas */
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    Configuracion config = configuracion_por_defecto();
    if (!configuracion_leer_argumentos(argc, argv, &config)) {
        return EXIT_FAILURE;
    }
    Tablero inicial;
    const Tablero *disposicion = NULL;
    if (config.usar_disposicion) {
        tablero_inicializar(&inicial);
        if (!tablero_colocar_fichas(&inicial, config.pos_puma, config.pos_perros)) {
            fprintf(stderr, "Error: la disposicion inicial no es valida para el tablero.\n");
            return EXIT_FAILURE;
        }
        disposicion = &inicial;
    }
    printf("Komikan - torneo concurrente\n");
    printf("Jugadores: %d. Mesas maximas simultaneas: %d.\n",
           config.total_jugadores, config.max_mesas);
    printf("13 threads de fichas y 1 juez por partida; sin turnos.\n");
    printf("Retardos: %d a %d ms. Semilla: %u\n", config.retardo_min_ms,
           config.retardo_max_ms, config.semilla);
    printf("Disposicion inicial: %s.\n", disposicion != NULL ? "personalizada" : "por defecto");

    /* Las senales se bloquean antes de crear cualquier thread: todos los threads
     * heredan la mascara y solo las recibe el thread de senales. */
    DatosSenales senales;
    sigemptyset(&senales.senales);
    sigaddset(&senales.senales, SIGINT);
    sigaddset(&senales.senales, SIGUSR1);
    sigset_t anterior;
    int error = pthread_sigmask(SIG_BLOCK, &senales.senales, &anterior);
    if (error != 0) {
        fprintf(stderr, "No se pudieron bloquear las senales: %s\n", strerror(error));
        return EXIT_FAILURE;
    }

    OpcionesServidor opciones = {.mostrar_eventos = 1, .mostrar_tablero = 1};
    Servidor *servidor = servidor_crear(&config, disposicion, &opciones);
    if (servidor == NULL) {
        return EXIT_FAILURE;
    }
    senales.servidor = servidor;
    log_iniciar(NULL);

    pthread_t hilo_senales;
    int con_senales = 1;
    error = pthread_create(&hilo_senales, NULL, atender_senales, &senales);
    if (error != 0) {
        /* Sin thread de senales, Ctrl+C volveria a cortar el programa de golpe. */
        fprintf(stderr, "No se pudo crear el thread de senales: %s\n", strerror(error));
        pthread_sigmask(SIG_SETMASK, &anterior, NULL);
        con_senales = 0;
    }

    ResumenServidor resumen;
    int correcto = servidor_correr(servidor, &resumen);

    if (con_senales) {
        /* Avisa al thread de senales que el torneo termino y lo recoge. */
        pthread_kill(hilo_senales, SIGUSR1);
        pthread_join(hilo_senales, NULL);
    }
    servidor_imprimir_resumen(&resumen, config.max_mesas);
    log_cerrar();
    servidor_destruir(servidor);
    return correcto ? EXIT_SUCCESS : EXIT_FAILURE;
}
