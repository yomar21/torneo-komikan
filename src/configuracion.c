#include "configuracion.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POSICION_MAXIMA 28
#define RETARDO_MAXIMO_MS 60000
#define MAX_CARACTERES_LINEA 255

Configuracion configuracion_por_defecto(void)
{
    Configuracion config = {
        .total_jugadores = 0,
        .max_mesas = 0,
        .retardo_min_ms = 50,
        .retardo_max_ms = 300,
        .semilla = 0,
        .usar_disposicion = 0,
        .pos_puma = -1
    };

    /* desde el inicio no hay posiciones para los perros, por lo que se usa -1 para decirlo */
    for (int i = 0; i < CONFIG_TOTAL_PERROS; i++) {
        config.pos_perros[i] = -1;
    }
    return config;
}

static int leer_entero_positivo(const char *texto, int *resultado)
{
    char *fin;
    errno = 0;
    long valor = strtol(texto, &fin, 10);

    /* No aceptamos un numero incompleto, como "6abc", ni uno que no quepa. */
    if (errno != 0 || fin == texto || *fin != '\0' ||
        valor <= 0 || valor > INT_MAX) {
        return 0;
    }
    *resultado = (int)valor;
    return 1;
}

/* Quitamos los espacios del principio y del final del texto.
 * Devolvemos un puntero al inicio del texto sin esos espacios. */
static char *recortar(char *texto)
{
    while (*texto != '\0' && isspace((unsigned char)*texto)) {
        texto++;
    }

    size_t largo = strlen(texto);
    while (largo > 0 && isspace((unsigned char)texto[largo - 1])) {
        texto[largo - 1] = '\0';
        largo--;
    }
    return texto;
}

/* Convertimos el texto a numero y comprobamos que esté
 * dentro del rango permitido. Si es valido, guardamos el resultado. */
static int leer_entero_en_rango(const char *texto, long minimo, long maximo,
                                long *resultado)
{
    if (texto == NULL || *texto == '\0') {
        return 0;/* El texto está vacío. */
    }

    char *fin;
    errno = 0;/* Reiniciamos errno antes de convertir el texto. */
    long valor = strtol(texto, &fin, 10);

    if (errno == ERANGE) return 0;/* El número es demasiado grande o pequeño. */
    if (fin == texto) return 0;/* No se pudo leer ningún número. */
    if (*fin != '\0') return 0;/* Hay caracteres adicionales después del número. */
    if (valor < minimo || valor > maximo) return 0;

    *resultado = valor;/* Guardamos el número solamente si pasó todas las revisiones. */
    return 1;
}

/* Convertimos el texto a un numero sin signo.
 * Permitimos que sea 0, pero no aceptamos numeros negativos. */
static int leer_semilla(const char *texto, unsigned int *resultado)
{
    if (texto == NULL || *texto == '\0') {
        return 0;
    }
    if (texto[0] == '-') {
        return 0;/* Evitamos aceptar valores negativos, ya que strtoul puede convertirlos. */
    }

    char *fin;
    errno = 0;
    unsigned long valor = strtoul(texto, &fin, 10);

    if (errno == ERANGE) return 0;
    if (fin == texto || *fin != '\0') return 0;
    if (valor > UINT_MAX) return 0;/* El número supera el máximo de unsigned int. */

    *resultado = (unsigned int)valor;
    return 1;
}

/* Separamos la lista usando las comas y guardamos cada posicion
 * en el arreglo. Todas deben estar dentro del rango permitido
 * y debemos recibir exactamente la cantidad esperada. */
static int leer_lista_posiciones(const char *texto, int *destino,
                                 int cantidad_esperada)
{
    int cantidad = 0;
    const char *inicio = texto;

    for (;;) {
        /* Buscamos la siguiente coma para saber dónde termina cada posición. */
        const char *coma = strchr(inicio, ',');
        size_t largo = (coma != NULL) ? (size_t)(coma - inicio) : strlen(inicio);

        /* Copiamos la posición a una cadena temporal para poder quitarle los espacios. */
        char elemento[32];
        if (largo >= sizeof elemento) return 0;
        memcpy(elemento, inicio, largo);
        elemento[largo] = '\0';

        long valor;
        if (!leer_entero_en_rango(recortar(elemento), 0, POSICION_MAXIMA, &valor)) {
            return 0;/* No se pudo convertir la posición o está fuera del rango. */
        }

        if (cantidad >= cantidad_esperada) return 0;/* Ya recibimos más posiciones de las permitidas. */
        destino[cantidad++] = (int)valor;

        if (coma == NULL) break;/* No quedan más posiciones por leer. */
        inicio = coma + 1;
    }

    return cantidad == cantidad_esperada;/* Comprobamos que recibimos todas las posiciones. */
}

/* Funciones para mostrar errores del archivo de configuración.
 * Una indica la línea donde ocurrió el problema y la otra
 * se usa cuando el error afecta a varias opciones. */
static void error_en_linea(const char *ruta, int numero, const char *motivo)
{
    fprintf(stderr, "Error en %s, linea %d: %s.\n", ruta, numero, motivo);
}

static void error_global(const char *ruta, const char *motivo)
{
    fprintf(stderr, "Error en %s: %s.\n", ruta, motivo);
}

/* Leemos el archivo de configuración y revisamos que sus datos sean válidos.
 * Trabajamos primero con una copia para evitar modificar la configuración
 * original si encontramos algún error. */
int configuracion_leer_archivo(const char *ruta, Configuracion *config)
{
    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        fprintf(stderr, "Error al abrir %s: %s.\n", ruta, strerror(errno));
        return 0;
    }

    Configuracion copia = *config;
    int ok = 1;
    int numero_linea = 0;

    /* Una bandera por clave, para detectar claves repetidas */
    int vista_min = 0, vista_max = 0, vista_semilla = 0;
    int vista_puma = 0, vista_perros = 0;

    /* +2: el '\n' y el '\0' */
    char linea[MAX_CARACTERES_LINEA + 2];

    while (ok && fgets(linea, sizeof linea, archivo) != NULL) {
        numero_linea++;
        size_t largo = strlen(linea);
        if (largo > 0 && linea[largo - 1] != '\n' && !feof(archivo)) {
            error_en_linea(ruta, numero_linea, "linea demasiado larga");
            ok = 0;
            break;
        }

        char *texto = recortar(linea);
        if (*texto == '\0' || *texto == '#') {
            continue;                   /* linea vacia o comentario */
        }

        char *igual = strchr(texto, '=');
        if (igual == NULL) {
            error_en_linea(ruta, numero_linea, "falta el signo '='");
            ok = 0;
            break;
        }

        *igual = '\0';
        char *clave = recortar(texto);
        char *valor = recortar(igual + 1);

        if (*valor == '\0') {
            error_en_linea(ruta, numero_linea, "valor vacio");
            ok = 0;
            break;
        }

        long numero;

        if (strcmp(clave, "retardo_min_ms") == 0) {
            if (vista_min) {
                error_en_linea(ruta, numero_linea, "clave repetida");
                ok = 0;
                break;
            }
            vista_min = 1;
            if (!leer_entero_en_rango(valor, 1, RETARDO_MAXIMO_MS, &numero)) {
                error_en_linea(ruta, numero_linea,
                               "retardo_min_ms debe ser un entero entre 1 y 60000");
                ok = 0;
                break;
            }
            copia.retardo_min_ms = (int)numero;

        } else if (strcmp(clave, "retardo_max_ms") == 0) {
            if (vista_max) {
                error_en_linea(ruta, numero_linea, "clave repetida");
                ok = 0;
                break;
            }
            vista_max = 1;
            if (!leer_entero_en_rango(valor, 1, RETARDO_MAXIMO_MS, &numero)) {
                error_en_linea(ruta, numero_linea,
                               "retardo_max_ms debe ser un entero entre 1 y 60000");
                ok = 0;
                break;
            }
            copia.retardo_max_ms = (int)numero;

        } else if (strcmp(clave, "semilla") == 0) {
            if (vista_semilla) {
                error_en_linea(ruta, numero_linea, "clave repetida");
                ok = 0;
                break;
            }
            vista_semilla = 1;
            if (!leer_semilla(valor, &copia.semilla)) {
                error_en_linea(ruta, numero_linea,
                               "semilla debe ser un entero sin signo");
                ok = 0;
                break;
            }

        } else if (strcmp(clave, "puma") == 0) {
            if (vista_puma) {
                error_en_linea(ruta, numero_linea, "clave repetida");
                ok = 0;
                break;
            }
            vista_puma = 1;
            if (!leer_entero_en_rango(valor, 0, POSICION_MAXIMA, &numero)) {
                error_en_linea(ruta, numero_linea,
                               "puma debe ser una posicion entre 0 y 28");
                ok = 0;
                break;
            }
            copia.pos_puma = (int)numero;

        } else if (strcmp(clave, "perros") == 0) {
            if (vista_perros) {
                error_en_linea(ruta, numero_linea, "clave repetida");
                ok = 0;
                break;
            }
            vista_perros = 1;
            if (!leer_lista_posiciones(valor, copia.pos_perros,
                                       CONFIG_TOTAL_PERROS)) {
                error_en_linea(ruta, numero_linea,
                               "perros debe ser una lista de exactamente 12 "
                               "posiciones entre 0 y 28 separadas por coma");
                ok = 0;
                break;
            }

        } else {
            error_en_linea(ruta, numero_linea, "clave desconocida");
            ok = 0;
            break;
        }
    }
    if (ok && copia.retardo_min_ms > copia.retardo_max_ms) {
        error_global(ruta, "retardo_min_ms no puede ser mayor que retardo_max_ms");
        ok = 0;
    }

    if (ok && vista_puma != vista_perros) {
        error_global(ruta, "puma y perros deben definirse juntos");
        ok = 0;
    }

    if (ok && vista_puma && vista_perros) {
        for (int i = 0; i < CONFIG_TOTAL_PERROS && ok; i++) {
            if (copia.pos_perros[i] == copia.pos_puma) {
                error_global(ruta, "el puma y un perro estan en la misma posicion");
                ok = 0;
            }
            for (int j = i + 1; j < CONFIG_TOTAL_PERROS && ok; j++) {
                if (copia.pos_perros[i] == copia.pos_perros[j]) {
                    error_global(ruta, "hay posiciones repetidas entre los perros");
                    ok = 0;
                }
            }
        }
    }
    if (ok) {
        config->retardo_min_ms = copia.retardo_min_ms;
        config->retardo_max_ms = copia.retardo_max_ms;
        config->semilla = copia.semilla;
        if (vista_puma && vista_perros) {
            config->usar_disposicion = 1;
            config->pos_puma = copia.pos_puma;
            for (int i = 0; i < CONFIG_TOTAL_PERROS; i++) {
                config->pos_perros[i] = copia.pos_perros[i];
            }
        }
    }

    fclose(archivo);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Argumentos de la linea de comandos                                  */
/* ------------------------------------------------------------------ */

int configuracion_leer_argumentos(int argc, char *argv[], Configuracion *config)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 0;
    }
    if (!leer_entero_positivo(argv[1], &config->total_jugadores)) {
        fprintf(stderr, "Error: N debe ser un entero positivo.\n");
        return 0;
    }
    if (!leer_entero_positivo(argv[2], &config->max_mesas)) {
        fprintf(stderr, "Error: M debe ser un entero positivo.\n");
        return 0;
    }
    if (argc == 4 && !configuracion_leer_archivo(argv[3], config)) {
        return 0;
    }
    return 1;
}