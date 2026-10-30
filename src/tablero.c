#include "tablero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int posicion_valida(const Tablero *tablero, int posicion)
{
    return tablero != NULL && posicion >= 0 &&
           posicion < tablero->cantidad_posiciones &&
           tablero->cantidad_posiciones <= MAX_POSICIONES;
}

static void conectar(Tablero *tablero, int a, int b)
{
    tablero->conexiones[a][b] = 1;
    tablero->conexiones[b][a] = 1;
}

void tablero_inicializar(Tablero *tablero)
{
    *tablero = (Tablero){0};
    tablero->cantidad_posiciones = TOTAL_POSICIONES;

    for (int fila = 0; fila < LADO_CUADRICULA; fila++) {
        for (int columna = 0; columna < LADO_CUADRICULA; columna++) {
            int posicion = fila * LADO_CUADRICULA + columna;
            if (columna < LADO_CUADRICULA - 1) {
                conectar(tablero, posicion, posicion + 1);
            }
            if (fila < LADO_CUADRICULA - 1) {
                conectar(tablero, posicion, posicion + LADO_CUADRICULA);
                if ((fila + columna) % 2 == 0) {
                    if (columna > 0) {
                        conectar(tablero, posicion, posicion + LADO_CUADRICULA - 1);
                    }
                    if (columna < LADO_CUADRICULA - 1) {
                        conectar(tablero, posicion, posicion + LADO_CUADRICULA + 1);
                    }
                }
            }
        }
    }

    conectar(tablero, 0, 26);
    conectar(tablero, 26, 25);
    conectar(tablero, 25, 28);
    conectar(tablero, 28, 4);
    conectar(tablero, 26, 27);
    conectar(tablero, 27, 28);
    conectar(tablero, 25, 27);
    conectar(tablero, 27, 2);
    conectar(tablero, 26, 1);
    conectar(tablero, 28, 3);
}

void tablero_colocar_fichas_iniciales(Tablero *tablero)
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

int tablero_buscar_ficha(const Tablero *tablero, int identificador)
{
    if (identificador < ID_PUMA || identificador > TOTAL_FICHAS) {
        return -1;
    }
    for (int posicion = 0; posicion < tablero->cantidad_posiciones; posicion++) {
        if (tablero->identificadores[posicion] == identificador) {
            return posicion;
        }
    }
    return -1;
}

int tablero_estado_consistente(const Tablero *tablero)
{
    if (tablero == NULL || tablero->cantidad_posiciones != TOTAL_POSICIONES ||
        tablero->perros_capturados < 0 || tablero->perros_capturados > TOTAL_PERROS) {
        return 0;
    }
    int vistos[TOTAL_FICHAS + 1] = {0};
    int pumas = 0;
    int perros = 0;
    for (int posicion = 0; posicion < tablero->cantidad_posiciones; posicion++) {
        Ocupante ocupante = tablero->ocupantes[posicion];
        int id = tablero->identificadores[posicion];
        if (ocupante == VACIA) {
            if (id != 0) {
                return 0;
            }
        } else {
            if (id < 1 || id > TOTAL_FICHAS || vistos[id]) {
                return 0;
            }
            vistos[id] = 1;
            if (ocupante == PUMA && id == ID_PUMA) {
                pumas++;
            } else if (ocupante == PERRO && id != ID_PUMA) {
                perros++;
            } else {
                return 0;
            }
        }
    }
    /* Los perros vivos y los capturados siempre deben sumar doce. */
    return pumas == 1 && perros + tablero->perros_capturados == TOTAL_PERROS;
}

int tablero_son_adyacentes(const Tablero *tablero, int origen, int destino)
{
    if (!posicion_valida(tablero, origen) || !posicion_valida(tablero, destino)) {
        return 0;
    }
    return tablero->conexiones[origen][destino];
}

int tablero_movimiento_valido(const Tablero *tablero, int origen, int destino)
{
    if (!tablero_son_adyacentes(tablero, origen, destino)) {
        return 0;
    }
    return (tablero->ocupantes[origen] == PUMA ||
            tablero->ocupantes[origen] == PERRO) &&
           tablero->ocupantes[destino] == VACIA;
}

int tablero_mover(Tablero *tablero, int origen, int destino)
{
    if (!tablero_movimiento_valido(tablero, origen, destino)) {
        return 0;
    }
    tablero->ocupantes[destino] = tablero->ocupantes[origen];
    tablero->identificadores[destino] = tablero->identificadores[origen];
    tablero->ocupantes[origen] = VACIA;
    tablero->identificadores[origen] = 0;
    return 1;
}

static int obtener_coordenadas(int posicion, int *x, int *y)
{
    if (posicion >= 0 && posicion < 25) {
        *x = posicion % LADO_CUADRICULA;
        *y = posicion / LADO_CUADRICULA;
        return 1;
    }
    switch (posicion) {
        case 25: *x = 2; *y = -2; return 1;
        case 26: *x = 1; *y = -1; return 1;
        case 27: *x = 2; *y = -1; return 1;
        case 28: *x = 3; *y = -1; return 1;
        default: return 0;
    }
}

int tablero_captura_valida(const Tablero *tablero, int origen,
                          int intermedia, int destino)
{
    if (!tablero_son_adyacentes(tablero, origen, intermedia) ||
        !tablero_son_adyacentes(tablero, intermedia, destino)) {
        return 0;
    }
    if (tablero->ocupantes[origen] != PUMA ||
        tablero->ocupantes[intermedia] != PERRO ||
        tablero->ocupantes[destino] != VACIA) {
        return 0;
    }
    int xo, yo, xi, yi, xd, yd;
    if (!obtener_coordenadas(origen, &xo, &yo) ||
        !obtener_coordenadas(intermedia, &xi, &yi) ||
        !obtener_coordenadas(destino, &xd, &yd)) {
        return 0;
    }
    /* El perro debe estar a mitad del salto: no se dobla una esquina. */
    return xo + xd == 2 * xi && yo + yd == 2 * yi;
}

int tablero_capturar(Tablero *tablero, int origen, int intermedia, int destino)
{
    if (!tablero_captura_valida(tablero, origen, intermedia, destino)) {
        return 0;
    }
    tablero->ocupantes[destino] = PUMA;
    tablero->identificadores[destino] = tablero->identificadores[origen];
    tablero->ocupantes[origen] = VACIA;
    tablero->identificadores[origen] = 0;
    tablero->ocupantes[intermedia] = VACIA;
    tablero->identificadores[intermedia] = 0;
    tablero->perros_capturados++;
    return 1;
}

ResultadoPartida tablero_evaluar_resultado(const Tablero *tablero)
{
    if (!tablero_estado_consistente(tablero)) {
        return ESTADO_INVALIDO;
    }
    if (tablero->perros_capturados >= 6) {
        return VICTORIA_PUMA;
    }
    int puma = tablero_buscar_ficha(tablero, ID_PUMA);
    for (int destino = 0; destino < tablero->cantidad_posiciones; destino++) {
        if (tablero_movimiento_valido(tablero, puma, destino)) {
            return PARTIDA_EN_CURSO;
        }
        for (int intermedia = 0; intermedia < tablero->cantidad_posiciones; intermedia++) {
            if (tablero_captura_valida(tablero, puma, intermedia, destino)) {
                return PARTIDA_EN_CURSO;
            }
        }
    }
    return VICTORIA_PERROS;
}

static char simbolo_ocupante(Ocupante ocupante)
{
    switch (ocupante) {
        case PUMA: return 'P';
        case PERRO: return 'D';
        case VACIA: return '.';
        default: return '?';
    }
}

void tablero_imprimir(const Tablero *tablero)
{
    enum { ANCHO = 37, ALTO = 25 };
    char dibujo[ALTO][ANCHO + 1];
    for (int fila = 0; fila < ALTO; fila++) {
        memset(dibujo[fila], ' ', ANCHO);
        dibujo[fila][ANCHO] = '\0';
    }

    /* Usamos las mismas coordenadas para dibujar y validar los saltos. */
    for (int a = 0; a < tablero->cantidad_posiciones; a++) {
        int xa, ya;
        if (!obtener_coordenadas(a, &xa, &ya)) {
            continue;
        }
        int cx = xa * 8 + 2;
        int cy = (ya + 2) * 4;
        for (int b = a + 1; b < tablero->cantidad_posiciones; b++) {
            int xb, yb;
            if (!tablero_son_adyacentes(tablero, a, b) ||
                !obtener_coordenadas(b, &xb, &yb)) {
                continue;
            }
            int dx = (xb - xa) * 8;
            int dy = (yb - ya) * 4;
            int pasos = dy == 0 ? abs(dx) : abs(dy);
            char simbolo = dy == 0 ? '-' : dx == 0 ? '|' :
                           dx * dy > 0 ? '\\' : '/';
            for (int paso = 1; paso < pasos; paso++) {
                int x = cx + dx * paso / pasos;
                int y = cy + dy * paso / pasos;
                if (dibujo[y][x] != ' ' && dibujo[y][x] != simbolo) {
                    dibujo[y][x] = 'X';
                } else {
                    dibujo[y][x] = simbolo;
                }
            }
        }
    }
    for (int posicion = 0; posicion < tablero->cantidad_posiciones; posicion++) {
        int x, y;
        if (obtener_coordenadas(posicion, &x, &y)) {
            char nodo[6];
            snprintf(nodo, sizeof nodo, "%02d[%c]", posicion,
                     simbolo_ocupante(tablero->ocupantes[posicion]));
            memcpy(&dibujo[(y + 2) * 4][x * 8], nodo, 5);
        }
    }
    printf("\nTablero completo (P=Puma, D=Perro, .=vacia)\n");
    for (int fila = 0; fila < ALTO; fila++) {
        printf("%s\n", dibujo[fila]);
    }
    printf("Capturados: %d\n\n", tablero->perros_capturados);
}
