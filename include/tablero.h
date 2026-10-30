#ifndef TABLERO_H
#define TABLERO_H

#define LADO_CUADRICULA 5
#define MAX_POSICIONES 32
/* Conservamos el nombre anterior para no romper codigo ya escrito. */
#define MAX_POCISIONES MAX_POSICIONES
#define TOTAL_POSICIONES 29
#define TOTAL_PERROS 12
#define TOTAL_FICHAS 13
#define ID_PUMA 1

typedef enum { VACIA, PUMA, PERRO } Ocupante;

typedef struct {
    int cantidad_posiciones;
    int perros_capturados;
    Ocupante ocupantes[MAX_POSICIONES];
    int identificadores[MAX_POSICIONES];
    int conexiones[MAX_POSICIONES][MAX_POSICIONES];
} Tablero;

typedef enum {
    PARTIDA_EN_CURSO,
    VICTORIA_PUMA,
    VICTORIA_PERROS,
    ESTADO_INVALIDO
} ResultadoPartida;

void tablero_inicializar(Tablero *tablero);
void tablero_colocar_fichas_iniciales(Tablero *tablero);
void tablero_imprimir(const Tablero *tablero);
int tablero_buscar_ficha(const Tablero *tablero, int identificador);
int tablero_estado_consistente(const Tablero *tablero);
int tablero_son_adyacentes(const Tablero *tablero, int origen, int destino);
int tablero_movimiento_valido(const Tablero *tablero, int origen, int destino);
int tablero_mover(Tablero *tablero, int origen, int destino);
int tablero_captura_valida(const Tablero *tablero, int origen,
                          int intermedia, int destino);
int tablero_capturar(Tablero *tablero, int origen, int intermedia, int destino);
ResultadoPartida tablero_evaluar_resultado(const Tablero *tablero);

#endif
