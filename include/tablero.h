#ifndef TABLERO_H
#define TABLERO_H

#define LADO_CUADRICULA 5
#define MAX_POCISIONES 32

typedef enum{
VACIA,
PUMA,
PERRO,
}Ocupante;

typedef struct{
    int cantidad_posiciones;
    Ocupante ocupantes[MAX_POCISIONES];
    int conexiones[MAX_POCISIONES][MAX_POCISIONES];
} Tablero;

void tablero_inicializar(Tablero *tablero);

int tablero_son_adyacentes(
    const Tablero *tablero,
    int origen,
    int destino
);

void tablero_imprimir(const Tablero *tablero);

#endif