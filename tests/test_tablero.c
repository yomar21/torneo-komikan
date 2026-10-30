#include "tablero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

static void poner(Tablero *tablero, int posicion, Ocupante ocupante, int id)
{
    tablero->ocupantes[posicion] = ocupante;
    tablero->identificadores[posicion] = id;
}

int main(void)
{
    int comprobaciones = 0;
    Tablero tablero;
    tablero_inicializar(&tablero);
    tablero_colocar_fichas_iniciales(&tablero);
    COMPROBAR(tablero_estado_consistente(&tablero));
    COMPROBAR(tablero_evaluar_resultado(&tablero) == PARTIDA_EN_CURSO);
    COMPROBAR(tablero_buscar_ficha(&tablero, ID_PUMA) == 25);
    for (int id = 1; id <= TOTAL_FICHAS; id++) {
        COMPROBAR(tablero_buscar_ficha(&tablero, id) >= 0);
    }
    for (int a = 0; a < TOTAL_POSICIONES; a++) {
        COMPROBAR(!tablero_son_adyacentes(&tablero, a, a));
        for (int b = 0; b < TOTAL_POSICIONES; b++) {
            COMPROBAR(tablero_son_adyacentes(&tablero, a, b) ==
                      tablero_son_adyacentes(&tablero, b, a));
        }
    }
    COMPROBAR(tablero_son_adyacentes(&tablero, 0, 6));
    COMPROBAR(!tablero_son_adyacentes(&tablero, 1, 5));
    COMPROBAR(!tablero_son_adyacentes(&tablero, 4, 5));
    COMPROBAR(tablero_son_adyacentes(&tablero, 0, 26));
    COMPROBAR(!tablero_son_adyacentes(&tablero, -1, 0));
    COMPROBAR(!tablero_son_adyacentes(&tablero, 0, 29));

    int id = tablero.identificadores[10];
    COMPROBAR(tablero_mover(&tablero, 10, 11));
    COMPROBAR(tablero_buscar_ficha(&tablero, id) == 11);
    COMPROBAR(tablero.identificadores[10] == 0);
    Tablero anterior = tablero;
    COMPROBAR(!tablero_mover(&tablero, 15, 16));
    COMPROBAR(!tablero_mover(&tablero, 0, 1));
    COMPROBAR(!tablero_mover(&tablero, -1, 1));
    COMPROBAR(memcmp(&anterior, &tablero, sizeof tablero) == 0);
    COMPROBAR(tablero_estado_consistente(&tablero));

    tablero_inicializar(&tablero);
    poner(&tablero, 0, PUMA, 1);
    poner(&tablero, 6, PERRO, 2);
    COMPROBAR(!tablero_captura_valida(&tablero, 0, 6, 7));
    poner(&tablero, 12, PERRO, 3);
    COMPROBAR(!tablero_captura_valida(&tablero, 0, 6, 12));
    poner(&tablero, 12, VACIA, 0);
    COMPROBAR(tablero_capturar(&tablero, 0, 6, 12));
    COMPROBAR(tablero_buscar_ficha(&tablero, 1) == 12);
    COMPROBAR(tablero_buscar_ficha(&tablero, 2) == -1);
    COMPROBAR(tablero.ocupantes[0] == VACIA && tablero.ocupantes[6] == VACIA);
    COMPROBAR(tablero.perros_capturados == 1);
    COMPROBAR(!tablero_capturar(&tablero, 0, 6, 12));
    COMPROBAR(tablero.perros_capturados == 1);

    tablero_inicializar(&tablero);
    poner(&tablero, 25, PUMA, 1);
    poner(&tablero, 26, PERRO, 2);
    COMPROBAR(tablero_capturar(&tablero, 25, 26, 0));
    COMPROBAR(tablero_buscar_ficha(&tablero, 1) == 0);

    tablero_inicializar(&tablero);
    tablero_colocar_fichas_iniciales(&tablero);
    for (int posicion = 15; posicion <= 20; posicion++) {
        poner(&tablero, posicion, VACIA, 0);
    }
    tablero.perros_capturados = 6;
    COMPROBAR(tablero_evaluar_resultado(&tablero) == VICTORIA_PUMA);

    tablero_inicializar(&tablero);
    poner(&tablero, 0, PUMA, 1);
    const int posiciones[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 25, 26};
    for (int i = 0; i < TOTAL_PERROS; i++) {
        poner(&tablero, posiciones[i], PERRO, i + 2);
    }
    COMPROBAR(tablero_estado_consistente(&tablero));
    COMPROBAR(tablero_evaluar_resultado(&tablero) == VICTORIA_PERROS);
    id = tablero.identificadores[2];
    poner(&tablero, 2, VACIA, 0);
    poner(&tablero, 13, PERRO, id);
    COMPROBAR(tablero_evaluar_resultado(&tablero) == PARTIDA_EN_CURSO);
    poner(&tablero, 0, VACIA, 0);
    COMPROBAR(tablero_evaluar_resultado(&tablero) == ESTADO_INVALIDO);

    tablero_inicializar(&tablero);
    tablero_colocar_fichas_iniciales(&tablero);
    tablero.identificadores[15] = tablero.identificadores[16];
    COMPROBAR(!tablero_estado_consistente(&tablero));
    tablero_colocar_fichas_iniciales(&tablero);
    poner(&tablero, 15, VACIA, 0);
    COMPROBAR(!tablero_estado_consistente(&tablero));
    printf("Tablero: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
