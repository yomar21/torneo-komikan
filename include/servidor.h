#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "configuracion.h"
#include "tablero.h"

typedef struct {
    int mostrar_eventos;
    int mostrar_tablero;
} OpcionesServidor;

typedef struct {
    int jugadores_creados;
    int jugadores_unidos;    /* threads recogidos con join */
    int llegados;
    int atendidos;           /* jugadores que jugaron una partida */
    int sin_pareja;
    int partidas;
    int victorias_puma;
    int victorias_perros;
    int errores;             /* partidas que terminaron con error */
} ResumenServidor;

/* Hace llegar a los jugadores, los empareja de a 2 y juega cada partida
 * (por ahora una tras otra). Con inicial = NULL usa la disposicion por defecto.
 * Devuelve 1 si todo salio bien y 0 si hubo algun error. */
int servidor_ejecutar(const Configuracion *config, const Tablero *inicial,
                      const OpcionesServidor *opciones, ResumenServidor *resumen);

/* Devuelve 1 si el primer jugador de la pareja es el Puma y 0 si juega con
 * los Perros. Depende solo de la semilla y del numero de partida. */
int servidor_puma_es_primero(unsigned int semilla, int numero_partida);

#endif
