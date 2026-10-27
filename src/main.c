#include <stdio.h>
#include <stdlib.h>
#include "tablero.h"
#include "configuracion.h"

int main(int argc, char *argv[])
{
    Configuracion config = configuracion_por_defecto();

    if (!configuracion_leer_argumentos(argc, argv, &config)) {
        return EXIT_FAILURE;
    }

    printf("Torneo de Komikan\n");
    printf("Jugadores: %d\n", config.total_jugadores);
    printf("Mesas simultaneas: %d\n", config.max_mesas);
    printf("Retardos: %d a %d ms\n",
           config.retardo_min_ms,
           config.retardo_max_ms);
    printf("Semilla: %u\n", config.semilla);

Tablero tablero;
tablero_inicializar(&tablero);


printf("Posiciones de la cuadricula: %d\n",
       tablero.cantidad_posiciones);

printf("Conexion 0-1: %d\n",
       tablero_son_adyacentes(&tablero, 0, 1));

printf("Conexion 0-5: %d\n",
       tablero_son_adyacentes(&tablero, 0, 5));

printf("Conexion 4-5: %d\n",
       tablero_son_adyacentes(&tablero, 4, 5));

printf("Conexion 0-6: %d\n",
       tablero_son_adyacentes(&tablero, 0, 6));

printf("Conexion 25-27: %d\n",
       tablero_son_adyacentes(&tablero, 25, 27));

printf("Conexion 27-2: %d\n",
       tablero_son_adyacentes(&tablero, 27, 2));

printf("Conexion 25-2: %d\n",
       tablero_son_adyacentes(&tablero, 25, 2));

printf("Diagonal 0-6: %d\n",
       tablero_son_adyacentes(&tablero, 0, 6));

printf("Diagonal 6-12: %d\n",
       tablero_son_adyacentes(&tablero, 6, 12));

printf("Diagonal 1-5: %d\n",
       tablero_son_adyacentes(&tablero, 1, 5));

tablero_imprimir(&tablero);

    return EXIT_SUCCESS;
}