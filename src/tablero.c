#include "tablero.h"
#include <stdio.h>
static void conectar(Tablero *tablero, int a, int b){
    tablero->conexiones[a][b]=1;
    tablero->conexiones[b][a]=1;
}

void tablero_inicializar(Tablero *tablero){
    *tablero=(Tablero){0};
    
    tablero->cantidad_posiciones=29;

    for (int fila=0; fila<LADO_CUADRICULA; fila++){
        for(int columna=0; columna<LADO_CUADRICULA; columna++){
            int posicion= fila*LADO_CUADRICULA + columna;
            
            if(columna<LADO_CUADRICULA-1){
                conectar(tablero,posicion,posicion+1);
            }
            if(fila<LADO_CUADRICULA-1){
                conectar(tablero,posicion,posicion+LADO_CUADRICULA);
            }
            if ((fila+ columna)%2==0 && fila<LADO_CUADRICULA-1){
               if(columna>0){
                conectar(tablero,posicion,posicion+LADO_CUADRICULA-1);
               }
                if(columna<LADO_CUADRICULA-1){
                 conectar(tablero,posicion,posicion+LADO_CUADRICULA+1);
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

int tablero_son_adyacentes( const Tablero *tablero,  int origen,  int destino){
    if(origen<0 || origen>= tablero->cantidad_posiciones ||
       destino<0 || destino>= tablero->cantidad_posiciones){
        return 0;
       }
       return tablero->conexiones[origen][destino];
}

static char simbolo_ocupante(Ocupante ocupante){
    switch(ocupante){
        case PUMA:
        return 'P';
        case PERRO:
        return 'D';
        case VACIA:
        return '.';
    }
}

void tablero_imprimir(const Tablero *tablero)
{
    enum {
        ANCHO_NODO = 5,
        SEPARACION = 8,
        ALTO_ENTRE_FILAS = 4,
        ANCHO_LINEA = (LADO_CUADRICULA - 1) * SEPARACION + ANCHO_NODO
    };

    printf("\nCuadricula Principal\n");
    printf("P=puma, D=perro, .=vacio\n\n");

    for (int fila = 0; fila < LADO_CUADRICULA; fila++) {
        for (int columna = 0; columna < LADO_CUADRICULA; columna++) {
            int posicion = fila * LADO_CUADRICULA + columna;

            printf("%02d[%c]", posicion,
                   simbolo_ocupante(tablero->ocupantes[posicion]));

            if (columna < LADO_CUADRICULA - 1) {
                printf("%s", tablero_son_adyacentes(
                           tablero, posicion, posicion + 1) ? "---" : "   ");
            }
        }
        printf("\n");

        if (fila == LADO_CUADRICULA - 1) {
            break;
        }

        /* Cada conexion ocupa tres lineas entre los nodos. */
        for (int paso = 1; paso < ALTO_ENTRE_FILAS; paso++) {
            char linea[ANCHO_LINEA + 1];

            for (int i = 0; i < ANCHO_LINEA; i++) {
                linea[i] = ' ';
            }
            linea[ANCHO_LINEA] = '\0';

            for (int columna = 0; columna < LADO_CUADRICULA; columna++) {
                int posicion = fila * LADO_CUADRICULA + columna;
                int abajo = posicion + LADO_CUADRICULA;
                int centro = columna * SEPARACION + ANCHO_NODO / 2;
                int avance = SEPARACION * paso / ALTO_ENTRE_FILAS;

                if (tablero_son_adyacentes(tablero, posicion, abajo)) {
                    linea[centro] = '|';
                }

                if (columna < LADO_CUADRICULA - 1) {
                    int derecha = tablero_son_adyacentes(
                        tablero, posicion, abajo + 1);
                    int izquierda = tablero_son_adyacentes(
                        tablero, posicion + 1, abajo);
                    int x_derecha = centro + avance;
                    int x_izquierda = centro + SEPARACION - avance;

                    if (derecha) {
                        linea[x_derecha] = '\\';
                    }
                    if (izquierda) {
                        linea[x_izquierda] =
                            derecha && x_derecha == x_izquierda ? 'X' : '/';
                    }
                }
            }
            printf("%s\n", linea);
        }
    }
    printf("\n");
}

void tablero_colocar_fichas_iniciales(Tablero *tablero){
    for(int posicion=0; posicion<tablero->cantidad_posiciones;posicion++){
        tablero->ocupantes[posicion]=VACIA;

    }

tablero->ocupantes[25]=PUMA;

for(int posicion=15;posicion<=24;posicion++){
    tablero->ocupantes[posicion]=PERRO;
}

tablero->ocupantes[10]=PERRO;
tablero->ocupantes[14]=PERRO;
}

int tablero_movimiento_valido(const Tablero *tablero, int origen, int destino){
    if(origen<0 || origen >= tablero->cantidad_posiciones ||destino<0 || destino>=tablero->cantidad_posiciones){
        return 0;
    }

    if(tablero->ocupantes[origen]==VACIA){
        return 0;
    }

    if(tablero->ocupantes[destino] !=VACIA){
        return 0;
    }
    return tablero_son_adyacentes(tablero,origen,destino);
}

int tablero_mover(Tablero *tablero,int origen, int destino){

if(!tablero_movimiento_valido(tablero,origen,destino)){
    return 0;
}

tablero->ocupantes[destino]=tablero->ocupantes[destino]=tablero->ocupantes[origen];
tablero->ocupantes[origen]=VACIA;
return 1;
}