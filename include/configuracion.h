#ifndef CONFIGURACION_H
#define CONFIGURACION_H
#define CONFIG_TOTAL_PERROS 12


typedef struct {
    int total_jugadores;
    int max_mesas;
    int retardo_min_ms;
    int retardo_max_ms;
    unsigned int semilla;
    int prob_abandono;      /* 0 a 100: % de jugadores que se retiran a mitad de partida */
    int abandono_max_ms;    /* el retiro ocurre entre 0 y este tiempo despues de empezar */

    int usar_disposicion;
    int pos_puma;
    int pos_perros[CONFIG_TOTAL_PERROS];
} Configuracion;

Configuracion configuracion_por_defecto(void);
int configuracion_leer_argumentos(int argc, char *argv[], Configuracion *config);
int configuracion_leer_archivo(const char *ruta, Configuracion *config);

#endif
