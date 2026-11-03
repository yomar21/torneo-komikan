#include "log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

enum { LINEAS = 500, TABLEROS = 200, LINEAS_POR_TABLERO = 27 };

static void *escribir_lineas(void *argumento)
{
    int mesa = *(int *)argumento;
    for (int i = 0; i < LINEAS; i++) {
        log_evento(mesa, "linea %d de la mesa %d", i, mesa);
    }
    return NULL;
}

/* Cada mesa dibuja tableros con un numero de capturas propio. */
static void *escribir_tableros(void *argumento)
{
    int mesa = *(int *)argumento;
    Tablero tablero;
    tablero_inicializar(&tablero);
    tablero_colocar_fichas_iniciales(&tablero);
    for (int i = 0; i < TABLEROS; i++) {
        tablero.perros_capturados = (i % (mesa + 5));
        log_tablero(mesa, &tablero);
    }
    return NULL;
}

/* Lee todo el archivo y lo parte en lineas. Devuelve la cantidad (o -1). */
static int leer_lineas(const char *ruta, char **texto, char ***lineas)
{
    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        return -1;
    }
    fseek(archivo, 0, SEEK_END);
    long tamano = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);
    *texto = malloc((size_t)tamano + 1);
    if (*texto == NULL || fread(*texto, 1, (size_t)tamano, archivo) != (size_t)tamano) {
        fclose(archivo);
        return -1;
    }
    fclose(archivo);
    (*texto)[tamano] = '\0';
    int cantidad = 0;
    for (long i = 0; i < tamano; i++) {
        cantidad += (*texto)[i] == '\n';
    }
    *lineas = malloc((size_t)(cantidad + 1) * sizeof(**lineas));
    if (*lineas == NULL) {
        return -1;
    }
    char *actual = *texto;
    for (int i = 0; i < cantidad; i++) {
        (*lineas)[i] = actual;
        actual = strchr(actual, '\n');
        *actual++ = '\0';
    }
    return cantidad;
}

int main(void)
{
    int comprobaciones = 0;
    char ruta[] = "/tmp/torneo_log_XXXXXX";
    int descriptor = mkstemp(ruta);
    COMPROBAR(descriptor >= 0);
    close(descriptor);

    log_consola(0);   /* el test no llena la pantalla */
    COMPROBAR(!log_iniciar("/ruta/que/no/existe/log.txt"));
    COMPROBAR(log_iniciar(NULL));

    /* 1. Dos threads escriben lineas: ninguna se pierde ni se mezcla. */
    COMPROBAR(log_iniciar(ruta));
    int mesas[2] = {1, 2};
    pthread_t hilos[2];
    for (int i = 0; i < 2; i++) {
        COMPROBAR(pthread_create(&hilos[i], NULL, escribir_lineas, &mesas[i]) == 0);
    }
    for (int i = 0; i < 2; i++) {
        COMPROBAR(pthread_join(hilos[i], NULL) == 0);
    }
    log_evento(0, "fin de las lineas");
    log_cerrar();

    char *texto = NULL;
    char **lineas = NULL;
    int cantidad = leer_lineas(ruta, &texto, &lineas);
    COMPROBAR(cantidad == 2 * LINEAS + 1);
    int siguiente[3] = {0, 0, 0};   /* la proxima linea que se espera de cada mesa */
    for (int i = 0; i < 2 * LINEAS; i++) {
        int mesa, numero, mesa_texto;
        char sobra;
        COMPROBAR(sscanf(lineas[i], "[Mesa %d] linea %d de la mesa %d%c",
                         &mesa, &numero, &mesa_texto, &sobra) == 3);
        COMPROBAR((mesa == 1 || mesa == 2) && mesa == mesa_texto);
        COMPROBAR(numero == siguiente[mesa]);   /* el orden de cada mesa se conserva */
        siguiente[mesa]++;
    }
    COMPROBAR(siguiente[1] == LINEAS && siguiente[2] == LINEAS);
    COMPROBAR(strcmp(lineas[2 * LINEAS], "[Servidor] fin de las lineas") == 0);
    free(lineas);
    free(texto);

    /* 2. Dos threads escriben tableros: cada bloque queda completo y junto. */
    COMPROBAR(log_iniciar(ruta));
    for (int i = 0; i < 2; i++) {
        COMPROBAR(pthread_create(&hilos[i], NULL, escribir_tableros, &mesas[i]) == 0);
    }
    for (int i = 0; i < 2; i++) {
        COMPROBAR(pthread_join(hilos[i], NULL) == 0);
    }
    log_cerrar();

    cantidad = leer_lineas(ruta, &texto, &lineas);
    COMPROBAR(cantidad == 2 * TABLEROS * LINEAS_POR_TABLERO);
    int bloques[3] = {0, 0, 0};
    for (int inicio = 0; inicio < cantidad; inicio += LINEAS_POR_TABLERO) {
        int mesa = 0;
        COMPROBAR(sscanf(lineas[inicio], "[Mesa %d] Tablero completo", &mesa) == 1);
        COMPROBAR(mesa == 1 || mesa == 2);
        char prefijo[32];
        snprintf(prefijo, sizeof prefijo, "[Mesa %d] ", mesa);
        for (int i = 0; i < LINEAS_POR_TABLERO; i++) {
            /* Todas las lineas del bloque llevan el prefijo de la misma mesa. */
            COMPROBAR(strncmp(lineas[inicio + i], prefijo, strlen(prefijo)) == 0);
        }
        int capturados = -1;
        COMPROBAR(sscanf(lineas[inicio + LINEAS_POR_TABLERO - 1],
                         "[Mesa %*d] Capturados: %d", &capturados) == 1);
        COMPROBAR(capturados == bloques[mesa] % (mesa + 5));   /* sin mezclar y en orden */
        bloques[mesa]++;
    }
    COMPROBAR(bloques[1] == TABLEROS && bloques[2] == TABLEROS);
    free(lineas);
    free(texto);

    /* 3. Una linea muy larga se corta sin romper nada. */
    COMPROBAR(log_iniciar(ruta));
    char larga[2000];
    memset(larga, 'a', sizeof larga - 1);
    larga[sizeof larga - 1] = '\0';
    log_evento(3, "%s", larga);
    log_evento(3, "despues");
    log_cerrar();
    cantidad = leer_lineas(ruta, &texto, &lineas);
    COMPROBAR(cantidad == 2);
    COMPROBAR(strncmp(lineas[0], "[Mesa 3] aaaa", 13) == 0 && strlen(lineas[0]) < 600);
    COMPROBAR(strcmp(lineas[1], "[Mesa 3] despues") == 0);
    free(lineas);
    free(texto);

    log_consola(1);
    unlink(ruta);
    printf("Log: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
