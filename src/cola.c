#include "cola.h"

#include <stdlib.h>

struct Cola {
    int *datos;
    int capacidad;
    int inicio;     /* indice del elemento mas antiguo */
    int cantidad;   /* cuantos elementos hay ahora */
};

Cola *cola_crear(int capacidad)
{
    if (capacidad <= 0) {
        return NULL;
    }
    Cola *cola = malloc(sizeof(*cola));
    if (cola == NULL) {
        return NULL;
    }
    cola->datos = malloc((size_t)capacidad * sizeof(*cola->datos));
    if (cola->datos == NULL) {
        free(cola);
        return NULL;
    }
    cola->capacidad = capacidad;
    cola->inicio = 0;
    cola->cantidad = 0;
    return cola;
}

int cola_encolar(Cola *cola, int valor)
{
    if (cola->cantidad == cola->capacidad) {
        return 0;
    }
    /* El % da la vuelta al final del arreglo. */
    int posicion = (cola->inicio + cola->cantidad) % cola->capacidad;
    cola->datos[posicion] = valor;
    cola->cantidad++;
    return 1;
}

int cola_desencolar(Cola *cola, int *valor)
{
    if (cola->cantidad == 0) {
        return 0;
    }
    *valor = cola->datos[cola->inicio];
    cola->inicio = (cola->inicio + 1) % cola->capacidad;
    cola->cantidad--;
    return 1;
}

int cola_tamano(const Cola *cola)
{
    return cola->cantidad;
}

void cola_destruir(Cola *cola)
{
    if (cola == NULL) {
        return;
    }
    free(cola->datos);
    free(cola);
}
