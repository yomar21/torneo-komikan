#ifndef CONFIGURACION_H
#define CONFIGURACION_H
typedef struct {
int total_jugadores;
int max_mesas;
int retardo_min_ms;
int retardo_max_ms;
unsigned int semilla;
} Configuracion;

Configuracion configuracion_por_defecto(void);

int configuracion_leer_argumentos(
    int argc,
    char *argv[],
    Configuracion *config
);
#endif