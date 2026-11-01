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

static void colocar_iniciales_referencia(Tablero *tablero)
{
    tablero->perros_capturados = 0;
    for (int posicion = 0; posicion < tablero->cantidad_posiciones; posicion++) {
        tablero->ocupantes[posicion] = VACIA;
        tablero->identificadores[posicion] = 0;
    }
    tablero->ocupantes[25] = PUMA;
    for (int posicion = 15; posicion <= 24; posicion++) {
        tablero->ocupantes[posicion] = PERRO;
    }
    tablero->ocupantes[10] = PERRO;
    tablero->ocupantes[14] = PERRO;

    int siguiente_perro = 2;
    for (int posicion = 0; posicion < tablero->cantidad_posiciones; posicion++) {
        if (tablero->ocupantes[posicion] == PUMA) {
            tablero->identificadores[posicion] = ID_PUMA;
        } else if (tablero->ocupantes[posicion] == PERRO) {
            tablero->identificadores[posicion] = siguiente_perro++;
        }
    }
}

/* Devuelve 1 si la disposicion fue rechazada Y el tablero quedo intacto. */
static int rechazada_sin_modificar(int pos_puma, const int perros[TOTAL_PERROS])
{
    Tablero t;
    tablero_inicializar(&t);
    tablero_colocar_fichas_iniciales(&t);
    t.perros_capturados = 3;            /* distinto de 0: si algo lo reinicia, se nota */

    Tablero antes;
    memcpy(&antes, &t, sizeof t);       /* memcpy (no '='): copia tambien el relleno */

    int resultado = tablero_colocar_fichas(&t, pos_puma, perros);
    return resultado == 0 && memcmp(&t, &antes, sizeof t) == 0;
}

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
    /* ---- tablero_colocar_fichas ---- */
    {
        static const int por_defecto[TOTAL_PERROS] = {10, 14, 15, 16, 17, 18,
                                                      19, 20, 21, 22, 23, 24};

        /* 1. La disposicion por defecto es identica a la de antes */
        Tablero nuevo, viejo;
        tablero_inicializar(&nuevo);
        tablero_inicializar(&viejo);
        tablero_colocar_fichas_iniciales(&nuevo);
        colocar_iniciales_referencia(&viejo);
        for (int p = 0; p < TOTAL_POSICIONES; p++) {
            COMPROBAR(nuevo.ocupantes[p] == viejo.ocupantes[p]);
            COMPROBAR(nuevo.identificadores[p] == viejo.identificadores[p]);
        }
        COMPROBAR(nuevo.perros_capturados == 0);
        COMPROBAR(tablero_estado_consistente(&nuevo));
        COMPROBAR(nuevo.identificadores[25] == ID_PUMA);
        COMPROBAR(nuevo.identificadores[10] == 2 && nuevo.identificadores[14] == 3);
        COMPROBAR(nuevo.identificadores[15] == 4 && nuevo.identificadores[24] == 13);

        /* 2. Disposicion personalizada valida; el orden de la lista fija los ids */
        static const int perros_a[TOTAL_PERROS] = {1, 5, 6, 2, 10, 11,
                                                   12, 3, 7, 8, 13, 4};
        Tablero t;
        tablero_inicializar(&t);
        COMPROBAR(tablero_colocar_fichas(&t, 0, perros_a) == 1);
        COMPROBAR(tablero_estado_consistente(&t));
        COMPROBAR(t.ocupantes[0] == PUMA && t.identificadores[0] == ID_PUMA);
        COMPROBAR(t.identificadores[1] == 2);       /* primer perro de la lista */
        COMPROBAR(t.identificadores[5] == 3);       /* segundo perro de la lista */
        COMPROBAR(t.identificadores[4] == 13);      /* ultimo perro de la lista */
        COMPROBAR(tablero_buscar_ficha(&t, 13) == 4);
        COMPROBAR(t.ocupantes[9] == VACIA);         /* una casilla no usada */

        /* 3. Colocar de nuevo sobre un tablero usado: limpia y reinicia capturas */
        t.perros_capturados = 5;
        COMPROBAR(tablero_colocar_fichas(&t, 25, por_defecto) == 1);
        COMPROBAR(t.perros_capturados == 0);
        COMPROBAR(tablero_estado_consistente(&t));
        COMPROBAR(t.ocupantes[0] == VACIA && t.identificadores[0] == 0);

        /* 4. Extremos validos: puma en 28 y perro en 0 */
        static const int perros_b[TOTAL_PERROS] = {0, 1, 2, 3, 4, 5,
                                                   6, 7, 8, 9, 10, 11};
        tablero_inicializar(&t);
        COMPROBAR(tablero_colocar_fichas(&t, 28, perros_b) == 1);
        COMPROBAR(tablero_estado_consistente(&t));

        /* 5. Invalidos: devuelven 0 y no modifican el tablero */
        COMPROBAR(rechazada_sin_modificar(29, por_defecto));    /* puma fuera */
        COMPROBAR(rechazada_sin_modificar(-1, por_defecto));    /* puma negativo */
        COMPROBAR(rechazada_sin_modificar(24, por_defecto));    /* puma sobre un perro */

        int perros_c[TOTAL_PERROS];
        memcpy(perros_c, por_defecto, sizeof perros_c);
        perros_c[11] = 29;
        COMPROBAR(rechazada_sin_modificar(25, perros_c));       /* perro fuera */
        memcpy(perros_c, por_defecto, sizeof perros_c);
        perros_c[0] = -1;
        COMPROBAR(rechazada_sin_modificar(25, perros_c));       /* perro negativo */
        memcpy(perros_c, por_defecto, sizeof perros_c);
        perros_c[11] = perros_c[0];
        COMPROBAR(rechazada_sin_modificar(25, perros_c));       /* perro repetido */

        /* 6. Tablero sin inicializar y punteros nulos */
        Tablero sin_iniciar;
        memset(&sin_iniciar, 0, sizeof sin_iniciar);
        COMPROBAR(tablero_colocar_fichas(&sin_iniciar, 25, por_defecto) == 0);
        COMPROBAR(tablero_colocar_fichas(NULL, 25, por_defecto) == 0);
        COMPROBAR(tablero_colocar_fichas(&t, 25, NULL) == 0);
    }
    printf("Tablero: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
