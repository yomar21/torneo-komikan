#include "cola.h"

#include <stdio.h>
#include <stdlib.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    int comprobaciones = 0;
    int valor = -1;

    /* Capacidades invalidas. */
    COMPROBAR(cola_crear(0) == NULL);
    COMPROBAR(cola_crear(-3) == NULL);

    /* Cola vacia y llena. */
    Cola *cola = cola_crear(3);
    COMPROBAR(cola != NULL);
    COMPROBAR(cola_tamano(cola) == 0);
    COMPROBAR(!cola_desencolar(cola, &valor));
    COMPROBAR(valor == -1);                  /* no toca la salida si fallo */
    COMPROBAR(cola_encolar(cola, 10));
    COMPROBAR(cola_encolar(cola, 20));
    COMPROBAR(cola_encolar(cola, 30));
    COMPROBAR(cola_tamano(cola) == 3);
    COMPROBAR(!cola_encolar(cola, 40));      /* llena: rechaza */
    COMPROBAR(cola_tamano(cola) == 3);

    /* Orden FIFO: sale primero lo que entro primero. */
    COMPROBAR(cola_desencolar(cola, &valor) && valor == 10);
    COMPROBAR(cola_desencolar(cola, &valor) && valor == 20);
    COMPROBAR(cola_tamano(cola) == 1);

    /* Vuelta circular: el indice pasa del final al comienzo del arreglo. */
    COMPROBAR(cola_encolar(cola, 40));
    COMPROBAR(cola_encolar(cola, 50));
    COMPROBAR(cola_tamano(cola) == 3);
    COMPROBAR(cola_desencolar(cola, &valor) && valor == 30);
    COMPROBAR(cola_desencolar(cola, &valor) && valor == 40);
    COMPROBAR(cola_desencolar(cola, &valor) && valor == 50);
    COMPROBAR(cola_tamano(cola) == 0);
    cola_destruir(cola);

    /* Muchas vueltas con capacidad 1 y 4: siempre FIFO. */
    int capacidades[] = {1, 4};
    for (int c = 0; c < 2; c++) {
        Cola *otra = cola_crear(capacidades[c]);
        COMPROBAR(otra != NULL);
        for (int i = 0; i < 100; i++) {
            COMPROBAR(cola_encolar(otra, i));
            COMPROBAR(cola_desencolar(otra, &valor) && valor == i);
        }
        cola_destruir(otra);
    }

    cola_destruir(NULL);                     /* destruir NULL no hace nada */
    printf("Cola: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
