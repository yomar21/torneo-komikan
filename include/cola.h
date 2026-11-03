#ifndef COLA_H
#define COLA_H

/* Cola FIFO circular de enteros. No es thread-safe: la protege quien la usa. */
typedef struct Cola Cola;

/* Devuelve NULL si la capacidad no es positiva o falta memoria. */
Cola *cola_crear(int capacidad);

/* Devuelven 1 si la operacion funciono y 0 si la cola esta llena / vacia. */
int cola_encolar(Cola *cola, int valor);
int cola_desencolar(Cola *cola, int *valor);

int cola_tamano(const Cola *cola);
void cola_destruir(Cola *cola);

#endif
