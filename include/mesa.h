#ifndef MESA_H
#define MESA_H

#include "configuracion.h"
#include "tablero.h"

typedef enum {
    MESA_POR_VICTORIA,
    MESA_LIMITE_PRUEBA,
    MESA_ERROR,
    MESA_CANCELADA             /* otro thread pidio terminar: no hay ganador */
} MotivoCierre;

typedef struct {
    unsigned int limite_propuestas;
    int mostrar_eventos;
    int mostrar_tablero;
    int id_mesa;               /* numero que se muestra en los mensajes */
} OpcionesMesa;

typedef struct {
    ResultadoPartida resultado;
    MotivoCierre motivo;
    unsigned int propuestas;
    unsigned int movimientos;
    unsigned int capturas;
    unsigned int rechazadas;
    int fichas_creadas;
    int fichas_unidas;
    int juez_unido;
    Tablero tablero_final;
} ResumenMesa;

/* La mesa es opaca: se crea con mesa_crear y se libera con mesa_destruir. */
typedef struct Mesa Mesa;

/* Crea una mesa lista para jugar. Con inicial = NULL usa la disposicion por
 * defecto. Devuelve NULL (y escribe el motivo por stderr) si algo falla. */
Mesa *mesa_crear(const Configuracion *config, const Tablero *inicial,
                 const OpcionesMesa *opciones);

/* Juega la partida y vuelve cuando terminaron todos los threads de la mesa.
 * Se llama una sola vez por mesa. Devuelve 1 si todo salio bien. */
int mesa_jugar(Mesa *mesa, ResumenMesa *resumen);

/* Pide que la partida termine con motivo MESA_CANCELADA. Se puede llamar desde
 * otro thread, antes, durante o despues de mesa_jugar, y varias veces: si la
 * partida ya habia terminado no cambia nada. */
void mesa_cancelar(Mesa *mesa);

/* Libera la mesa. No se debe llamar mientras otro thread esta dentro de
 * mesa_jugar o mesa_cancelar con esta misma mesa. */
void mesa_destruir(Mesa *mesa);

/* Crea la mesa, la juega y la destruye. */
int mesa_ejecutar(const Configuracion *config, const Tablero *inicial,
                 const OpcionesMesa *opciones, ResumenMesa *resumen);
const char *mesa_describir_cierre(const ResumenMesa *resumen);

#endif
