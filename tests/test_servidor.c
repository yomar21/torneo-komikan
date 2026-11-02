#include "servidor.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

/* Cuenta los threads del proceso (solo Linux). Devuelve -1 si falla. */
static int contar_threads(void)
{
    DIR *directorio = opendir("/proc/self/task");
    if (directorio == NULL) {
        return -1;
    }
    int cantidad = 0;
    struct dirent *entrada;
    while ((entrada = readdir(directorio)) != NULL) {
        if (entrada->d_name[0] != '.') {
            cantidad++;
        }
    }
    closedir(directorio);
    return cantidad;
}

int main(void)
{
    int comprobaciones = 0;
    int totales[] = {1, 2, 3, 4, 5, 10};
    Configuracion config = configuracion_por_defecto();
    config.retardo_min_ms = 1;
    config.retardo_max_ms = 3;
    config.max_mesas = 1;
    OpcionesServidor opciones = {.mostrar_eventos = 0, .mostrar_tablero = 0};

    /* Calentamiento: TSAN crea un thread propio con el primer pthread_create. */
    ResumenServidor calentamiento;
    config.total_jugadores = 1;
    COMPROBAR(servidor_ejecutar(&config, NULL, &opciones, &calentamiento));
    int threads_antes = contar_threads();

    for (int c = 0; c < 6; c++) {
        int n = totales[c];
        config.total_jugadores = n;
        config.semilla = (unsigned int)n;
        ResumenServidor r;
        COMPROBAR(servidor_ejecutar(&config, NULL, &opciones, &r));
        COMPROBAR(r.jugadores_creados == n && r.jugadores_unidos == n);
        COMPROBAR(r.llegados == n);
        COMPROBAR(r.atendidos + r.sin_pareja == n);
        COMPROBAR(r.partidas == n / 2);
        COMPROBAR(r.atendidos == 2 * (n / 2));
        COMPROBAR(r.sin_pareja == n % 2);   /* con N impar uno queda sin pareja */
        COMPROBAR(r.victorias_puma + r.victorias_perros == r.partidas);
        COMPROBAR(r.errores == 0);
        COMPROBAR(r.max_simultaneas <= 1);   /* con M = 1 las partidas no se solapan */
    }

    /* Deben quedar los mismos threads de antes (el sistema tarda en retirarlos). */
    int threads = contar_threads();
    for (int intento = 0; threads > threads_antes && intento < 100; intento++) {
        struct timespec pausa = {.tv_sec = 0, .tv_nsec = 5000000L};
        nanosleep(&pausa, NULL);
        threads = contar_threads();
    }
    COMPROBAR(threads == -1 || threads == threads_antes);

    /* Varias mesas a la vez: nunca mas de M y todas las partidas terminan. */
    int mesas[] = {1, 2, 3, 8};
    for (int m = 0; m < 4; m++) {
        config.total_jugadores = 8;
        config.max_mesas = mesas[m];
        config.semilla = 100u + (unsigned int)m;
        ResumenServidor r8;
        COMPROBAR(servidor_ejecutar(&config, NULL, &opciones, &r8));
        COMPROBAR(r8.partidas == 4 && r8.atendidos == 8 && r8.errores == 0);
        COMPROBAR(r8.victorias_puma + r8.victorias_perros == 4);
        COMPROBAR(r8.max_simultaneas >= 1 && r8.max_simultaneas <= mesas[m]);
        COMPROBAR(r8.jugadores_unidos == 8);
        if (mesas[m] == 1) {
            COMPROBAR(r8.max_simultaneas == 1);
        } else {
            COMPROBAR(r8.max_simultaneas >= 2);   /* se juega en paralelo */
        }
    }
    /* Mas mesas que parejas: solo se usan las necesarias. */
    config.total_jugadores = 4;
    config.max_mesas = 100;
    ResumenServidor rm;
    COMPROBAR(servidor_ejecutar(&config, NULL, &opciones, &rm));
    COMPROBAR(rm.partidas == 2 && rm.max_simultaneas <= 2);
    config.max_mesas = 1;

    /* Con una disposicion inicial dada. */
    Tablero inicial;
    tablero_inicializar(&inicial);
    tablero_colocar_fichas_iniciales(&inicial);
    config.total_jugadores = 4;
    ResumenServidor r;
    COMPROBAR(servidor_ejecutar(&config, &inicial, &opciones, &r));
    COMPROBAR(r.partidas == 2 && r.sin_pareja == 0);

    /* Quien es el Puma depende solo de la semilla y la partida. */
    int puma_primero = 0;
    for (int partida = 1; partida <= 100; partida++) {
        int a = servidor_puma_es_primero(42, partida);
        COMPROBAR(a == servidor_puma_es_primero(42, partida));
        puma_primero += a;
    }
    COMPROBAR(puma_primero > 20 && puma_primero < 80);   /* sale de ambas formas */

    /* Configuraciones invalidas. */
    config.total_jugadores = 0;
    COMPROBAR(!servidor_ejecutar(&config, NULL, &opciones, &r));
    config.total_jugadores = 2;
    config.max_mesas = 0;
    COMPROBAR(!servidor_ejecutar(&config, NULL, &opciones, &r));
    config.max_mesas = 1;
    config.retardo_min_ms = 0;
    COMPROBAR(!servidor_ejecutar(&config, NULL, &opciones, &r));

    printf("Servidor: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
