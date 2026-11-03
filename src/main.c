#include "configuracion.h"
#include "log.h"
#include "servidor.h"

#include <stdio.h>
#include <stdlib.h>

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

    OpcionesServidor opciones = {.mostrar_eventos = 1, .mostrar_tablero = 1};
    ResumenServidor resumen;
    log_iniciar(NULL);
    int correcto = servidor_ejecutar(&config, disposicion, &opciones, &resumen);

    servidor_imprimir_resumen(&resumen, config.max_mesas);
    log_cerrar();
    return correcto ? EXIT_SUCCESS : EXIT_FAILURE;
}
