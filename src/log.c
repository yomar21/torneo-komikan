#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LINEA = 512 };

/* El log es el ultimo lock que se toma: dentro de el no se toma ningun otro. */
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;
static FILE *archivo = NULL;
static int consola = 1;

int log_iniciar(const char *ruta_o_NULL)
{
    log_cerrar();
    if (ruta_o_NULL == NULL) {
        return 1;
    }
    FILE *nuevo = fopen(ruta_o_NULL, "w");
    if (nuevo == NULL) {
        return 0;
    }
    pthread_mutex_lock(&candado);
    archivo = nuevo;
    pthread_mutex_unlock(&candado);
    return 1;
}

void log_consola(int activa)
{
    pthread_mutex_lock(&candado);
    consola = activa;
    pthread_mutex_unlock(&candado);
}

static void armar_prefijo(char *prefijo, size_t tamano, int mesa)
{
    if (mesa == 0) {
        snprintf(prefijo, tamano, "[Servidor] ");
    } else {
        snprintf(prefijo, tamano, "[Mesa %d] ", mesa);
    }
}

/* Escribe una linea completa. Se llama con el candado tomado. */
static void escribir_linea(const char *prefijo, const char *texto)
{
    if (consola) {
        fputs(prefijo, stdout);
        fputs(texto, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
    if (archivo != NULL) {
        fputs(prefijo, archivo);
        fputs(texto, archivo);
        fputc('\n', archivo);
        fflush(archivo);
    }
}

void log_evento(int mesa, const char *formato, ...)
{
    char prefijo[32];
    char texto[MAX_LINEA];
    armar_prefijo(prefijo, sizeof prefijo, mesa);
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(texto, sizeof texto, formato, argumentos);
    va_end(argumentos);

    pthread_mutex_lock(&candado);
    escribir_linea(prefijo, texto);
    pthread_mutex_unlock(&candado);
}

void log_tablero(int mesa, const Tablero *tablero)
{
    char prefijo[32];
    armar_prefijo(prefijo, sizeof prefijo, mesa);

    /* El dibujo se arma fuera del candado: asi se sostiene poco tiempo. */
    char *dibujo = NULL;
    size_t largo = 0;
    FILE *memoria = open_memstream(&dibujo, &largo);
    if (memoria == NULL) {
        log_evento(mesa, "No se pudo dibujar el tablero");
        return;
    }
    tablero_imprimir_en(memoria, tablero);
    fclose(memoria);

    /* Todas las lineas del tablero se escriben juntas, cada una con su prefijo. */
    pthread_mutex_lock(&candado);
    char *inicio = dibujo;
    while (*inicio != '\0') {
        char *fin = strchr(inicio, '\n');
        size_t n = fin != NULL ? (size_t)(fin - inicio) : strlen(inicio);
        if (n > 0) {
            char linea[MAX_LINEA];
            snprintf(linea, sizeof linea, "%.*s", (int)n, inicio);
            escribir_linea(prefijo, linea);
        }
        inicio += n + (fin != NULL ? 1 : 0);
    }
    pthread_mutex_unlock(&candado);
    free(dibujo);
}

void log_cerrar(void)
{
    pthread_mutex_lock(&candado);
    if (archivo != NULL) {
        fclose(archivo);
        archivo = NULL;
    }
    pthread_mutex_unlock(&candado);
}
