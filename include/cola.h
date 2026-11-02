#ifndef COLA_H
#define COLA_H

/*
 * Cola FIFO circular de enteros con capacidad fija.
 * Se usa para guardar los identificadores de jugador en orden de llegada.
 *
 * IMPORTANTE: esta cola NO es thread-safe. Quien la use desde varios threads
 * debe proteger cada llamada con su propio mutex (el servidor usa el suyo).
 */
typedef struct Cola Cola;

/* Devuelve NULL si la capacidad no es positiva o falta memoria. */
Cola *cola_crear(int capacidad);

/* Devuelven 1 si la operacion funciono y 0 si la cola esta llena / vacia. */
int cola_encolar(Cola *cola, int valor);
int cola_desencolar(Cola *cola, int *valor);

int cola_tamano(const Cola *cola);
void cola_destruir(Cola *cola);

#endif
