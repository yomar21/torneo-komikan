#include "configuracion.h"
#include "mesa.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    Configuracion config = configuracion_por_defecto();
    if (!configuracion_leer_argumentos(argc, argv, &config)) {
        return EXIT_FAILURE;
    }
    /* Aun no es el torneo completo: esta etapa permite probar una sola pareja. */
    if (config.total_jugadores != 2 || config.max_mesas != 1) {
        fprintf(stderr, "Etapa actual: una mesa con dos jugadores.\n"
                        "Ejecuta: %s 2 1\n"
                        "El emparejamiento de N jugadores y M mesas sigue pendiente.\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    printf("Komikan - demostracion de una mesa concurrente\n");
    printf("13 threads de fichas y 1 juez; sin turnos.\n");
    printf("Retardos: %d a %d ms. Semilla: %u\n",
           config.retardo_min_ms, config.retardo_max_ms, config.semilla);

    OpcionesMesa opciones = {
        .limite_propuestas = 500,
        .mostrar_eventos = 1,
        .mostrar_tablero = 1
    };
    printf("Limite de demostracion: %u propuestas; no asigna ganador al alcanzarse.\n",
           opciones.limite_propuestas);
    ResumenMesa resumen;
    if (!mesa_ejecutar(&config, NULL, &opciones, &resumen)) {
        return EXIT_FAILURE;
    }
    printf("\nResultado: %s.\n", mesa_describir_cierre(&resumen));
    printf("Movimientos aceptados: %u; capturas: %u; propuestas rechazadas: %u.\n",
           resumen.movimientos, resumen.capturas, resumen.rechazadas);
    printf("Threads recogidos: %d/%d fichas y %d juez.\n",
           resumen.fichas_unidas, resumen.fichas_creadas, resumen.juez_unido);
    return EXIT_SUCCESS;
}
