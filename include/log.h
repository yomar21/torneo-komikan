#ifndef LOG_H
#define LOG_H

#include "tablero.h"

/* Salida unica del programa: todo lo que se imprime pasa por aqui y un mutex
 * evita que se mezclen las lineas de distintas mesas.
 * log_iniciar y log_cerrar se llaman con un solo thread, antes y despues del torneo. */

/* Con una ruta, escribe tambien en ese archivo. Devuelve 0 si no pudo abrirlo. */
int log_iniciar(const char *ruta_o_NULL);

/* Activa o apaga la salida por consola (por defecto esta activa). */
void log_consola(int activa);

/* Escribe una linea con el prefijo [Mesa k]; la mesa 0 usa [Servidor]. */
void log_evento(int mesa, const char *formato, ...) __attribute__((format(printf, 2, 3)));

/* Escribe el tablero completo, sin que se intercale con otras salidas. */
void log_tablero(int mesa, const Tablero *tablero);

void log_cerrar(void);

#endif
