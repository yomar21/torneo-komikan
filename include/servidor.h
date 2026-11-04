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
    int cancelados;          /* jugadores que no jugaron porque se detuvo el servidor */
    int partidas;
    int victorias_puma;
    int victorias_perros;
    int por_abandono;        /* partidas que terminaron porque un jugador se retiro */
    int cancelados_en_partida;   /* partidas canceladas mientras se jugaban */
    int errores;             /* partidas que terminaron con error */
    int max_simultaneas;     /* mayor numero de mesas activas a la vez */
} ResumenServidor;

/* El servidor es opaco: se crea con servidor_crear y se libera con servidor_destruir. */
typedef struct Servidor Servidor;

/* Crea un servidor listo para correr. Con inicial = NULL usa la disposicion por
 * defecto. Devuelve NULL (y escribe el motivo por stderr) si algo falla. */
Servidor *servidor_crear(const Configuracion *config, const Tablero *inicial,
                         const OpcionesServidor *opciones);

/* Hace llegar a los jugadores, los empareja de a 2 y juega cada pareja en una
 * mesa. Nunca hay mas de config->max_mesas mesas activas; si estan todas
 * ocupadas, el resto espera. Vuelve cuando terminaron todos los threads.
 * Se llama una sola vez. Devuelve 1 si todo salio bien y 0 si hubo algun error. */
int servidor_correr(Servidor *servidor, ResumenServidor *resumen);

/* Deja de aceptar jugadores: los que no han llegado y los que esperan pareja
 * quedan cancelados, y las partidas en curso se cancelan (sin ganador). Se puede
 * llamar desde otro thread, antes, durante o despues de servidor_correr, y
 * varias veces. */
void servidor_detener(Servidor *servidor);

/* Libera el servidor. No se debe llamar mientras otro thread esta dentro de
 * servidor_correr o servidor_detener con este mismo servidor. */
void servidor_destruir(Servidor *servidor);

/* Crea el servidor, lo corre y lo destruye. */
int servidor_ejecutar(const Configuracion *config, const Tablero *inicial,
                      const OpcionesServidor *opciones, ResumenServidor *resumen);

/* Escribe el resumen del torneo en el log. max_mesas es el limite M. */
void servidor_imprimir_resumen(const ResumenServidor *resumen, int max_mesas);

/* Comprueba que las cuentas cuadran: cada jugador tiene un solo destino y cada
 * partida un solo final. total es el N con el que se ejecuto el torneo. */
int servidor_resumen_coherente(const ResumenServidor *resumen, int total);

/* Devuelve 1 si el primer jugador de la pareja es el Puma y 0 si juega con
 * los Perros. Depende solo de la semilla y del numero de partida. */
int servidor_puma_es_primero(unsigned int semilla, int numero_partida);

#endif
