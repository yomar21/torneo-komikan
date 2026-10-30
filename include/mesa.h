#ifndef MESA_H
#define MESA_H

#include "configuracion.h"
#include "tablero.h"

typedef enum {
    MESA_POR_VICTORIA,
    MESA_LIMITE_PRUEBA,
    MESA_ERROR
} MotivoCierre;

typedef struct {
    unsigned int limite_propuestas;
    int mostrar_eventos;
    int mostrar_tablero;
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

int mesa_ejecutar(const Configuracion *config, const Tablero *inicial,
                 const OpcionesMesa *opciones, ResumenMesa *resumen);
const char *mesa_describir_cierre(const ResumenMesa *resumen);

#endif
