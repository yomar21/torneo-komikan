#include "configuracion.h"
#include "tablero.h"            /* solo para comparar TOTAL_PERROS */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             /* close */

static int comprobaciones = 0;

/* Cuenta la comprobacion y, si falla, dice DONDE y termina.
 * No usamos assert: con -DNDEBUG desaparece, y aqui hay llamadas
 * con efectos dentro de las condiciones. */
#define COMPROBAR(cond)                                              \
    do {                                                             \
        comprobaciones++;                                            \
        if (!(cond)) {                                               \
            fprintf(stderr, "FALLA %s:%d: %s\n",                     \
                    __FILE__, __LINE__, #cond);                      \
            exit(1);                                                 \
        }                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/* Utilidades para los archivos temporales                             */
/* ------------------------------------------------------------------ */

/* Escribe 'contenido' en un archivo temporal nuevo y deja su ruta en
 * 'ruta' (al menos 64 bytes). Devuelve 1 si pudo. */
static int crear_temporal(char *ruta, const char *contenido)
{
    strcpy(ruta, "/tmp/torneo_cfg_XXXXXX");
    int fd = mkstemp(ruta);             /* reemplaza las X por un nombre unico */
    if (fd == -1) {
        return 0;
    }

    FILE *f = fdopen(fd, "w");
    if (f == NULL) {
        close(fd);
        remove(ruta);
        return 0;
    }
    fputs(contenido, f);
    fclose(f);                          /* fclose tambien cierra el descriptor */
    return 1;
}

/* Escribe el contenido en un temporal, lo lee con el parser y lo borra. */
static int leer_texto(const char *contenido, Configuracion *config)
{
    char ruta[64];
    COMPROBAR(crear_temporal(ruta, contenido));
    int resultado = configuracion_leer_archivo(ruta, config);
    remove(ruta);                       /* no dejar basura en el disco */
    return resultado;
}

/* El contenido es invalido: el parser debe devolver 0 Y no tocar la config. */
static void debe_fallar(const char *contenido)
{
    Configuracion c = configuracion_por_defecto();
    c.total_jugadores = 2;
    c.max_mesas = 1;
    Configuracion antes = c;            /* copia para comparar despues */

    COMPROBAR(leer_texto(contenido, &c) == 0);
    COMPROBAR(memcmp(&c, &antes, sizeof c) == 0);   /* config intacta */
}

/* ------------------------------------------------------------------ */
/* Argumentos (los 10 casos que ya existian)                           */
/* ------------------------------------------------------------------ */

static void probar_argumentos(void)
{
    Configuracion config = configuracion_por_defecto();
    COMPROBAR(config.retardo_min_ms == 50 && config.retardo_max_ms == 300);
    char *valido[] = {"torneo", "6", "2"};
    COMPROBAR(configuracion_leer_argumentos(3, valido, &config));
    COMPROBAR(config.total_jugadores == 6 && config.max_mesas == 2);

    char *cero[] = {"torneo", "0", "2"};
    char *negativo[] = {"torneo", "2", "-1"};
    char *texto[] = {"torneo", "6abc", "2"};
    char *vacio[] = {"torneo", "", "2"};
    char *desborde[] = {"torneo", "99999999999999999999999", "2"};
    COMPROBAR(!configuracion_leer_argumentos(3, cero, &config));
    COMPROBAR(!configuracion_leer_argumentos(3, negativo, &config));
    COMPROBAR(!configuracion_leer_argumentos(3, texto, &config));
    COMPROBAR(!configuracion_leer_argumentos(3, vacio, &config));
    COMPROBAR(!configuracion_leer_argumentos(3, desborde, &config));
    COMPROBAR(!configuracion_leer_argumentos(1, valido, &config));

    /* Antes exigia que 'N M config.txt' fallara siempre. Ahora falla solo
     * si el archivo no existe o es invalido; usamos una ruta absoluta que
     * no existe para que el resultado no dependa del directorio actual. */
    char *inexistente[] = {"torneo", "2", "1", "/tmp/torneo_no_existe.txt"};
    COMPROBAR(!configuracion_leer_argumentos(4, inexistente, &config));
}

/* N M archivo: el archivo se lee y sus valores llegan a la configuracion. */
static void probar_argumentos_con_archivo(void)
{
    char ruta[64];
    COMPROBAR(crear_temporal(ruta, "semilla=7\nretardo_min_ms=1\nretardo_max_ms=3\n"));

    char *argv_ok[] = {"torneo", "2", "1", ruta};
    Configuracion c = configuracion_por_defecto();
    COMPROBAR(configuracion_leer_argumentos(4, argv_ok, &c) == 1);
    COMPROBAR(c.total_jugadores == 2 && c.max_mesas == 1);
    COMPROBAR(c.semilla == 7 && c.retardo_min_ms == 1 && c.retardo_max_ms == 3);
    remove(ruta);

    /* Archivo invalido: configuracion_leer_argumentos debe devolver 0 */
    COMPROBAR(crear_temporal(ruta, "semilla=-1\n"));
    char *argv_malo[] = {"torneo", "2", "1", ruta};
    c = configuracion_por_defecto();
    COMPROBAR(configuracion_leer_argumentos(4, argv_malo, &c) == 0);
    remove(ruta);
}

/* ------------------------------------------------------------------ */
/* Archivos validos                                                    */
/* ------------------------------------------------------------------ */

static void probar_validos(void)
{
    Configuracion c;

    /* Archivo completo */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("# ejemplo\nretardo_min_ms=10\nretardo_max_ms=20\n"
                         "semilla=42\npuma=25\n"
                         "perros=10,14,15,16,17,18,19,20,21,22,23,24\n", &c) == 1);
    COMPROBAR(c.retardo_min_ms == 10 && c.retardo_max_ms == 20);
    COMPROBAR(c.semilla == 42);
    COMPROBAR(c.usar_disposicion == 1 && c.pos_puma == 25);
    COMPROBAR(c.pos_perros[0] == 10 && c.pos_perros[11] == 24);

    /* Solo comentarios y lineas vacias: todo queda por defecto */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("# nada\n\n   \n# mas nada\n", &c) == 1);
    COMPROBAR(c.retardo_min_ms == 50 && c.retardo_max_ms == 300);
    COMPROBAR(c.semilla == 0 && c.usar_disposicion == 0);

    /* Archivo completamente vacio */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("", &c) == 1 && c.usar_disposicion == 0);

    /* Solo retardos */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("retardo_min_ms=1\nretardo_max_ms=3\n", &c) == 1);
    COMPROBAR(c.retardo_min_ms == 1 && c.retardo_max_ms == 3);
    COMPROBAR(c.semilla == 0 && c.usar_disposicion == 0);

    /* Solo semilla */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("semilla=123456\n", &c) == 1 && c.semilla == 123456);
    COMPROBAR(c.retardo_min_ms == 50 && c.retardo_max_ms == 300);

    /* Semilla 0 es valida */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("semilla=0\n", &c) == 1 && c.semilla == 0);

    /* La semilla maxima (UINT_MAX) es valida */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("semilla=4294967295\n", &c) == 1 && c.semilla == 4294967295u);

    /* Retardos en los extremos del rango permitido */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("retardo_min_ms=1\nretardo_max_ms=60000\n", &c) == 1);
    COMPROBAR(c.retardo_min_ms == 1 && c.retardo_max_ms == 60000);

    /* Ultima linea sin salto de linea */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("semilla=7", &c) == 1 && c.semilla == 7);

    /* Fines de linea CRLF y espacios alrededor de '=' y de comas */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto(" retardo_min_ms = 5 \r\nretardo_max_ms=9\r\n"
                         "puma = 0\r\n"
                         "perros = 1 , 5,6,2,10,11,12,3,7,8,13,4\r\n", &c) == 1);
    COMPROBAR(c.retardo_min_ms == 5 && c.pos_puma == 0);
    COMPROBAR(c.pos_perros[0] == 1 && c.pos_perros[1] == 5);

    /* Las claves pueden venir en cualquier orden */
    c = configuracion_por_defecto();
    COMPROBAR(leer_texto("perros=1,2,3,4,5,6,7,8,9,10,11,12\npuma=0\n", &c) == 1);
    COMPROBAR(c.usar_disposicion == 1 && c.pos_puma == 0);

    /* El archivo NO pisa total_jugadores ni max_mesas */
    c = configuracion_por_defecto();
    c.total_jugadores = 2;
    c.max_mesas = 1;
    COMPROBAR(leer_texto("semilla=7\n", &c) == 1);
    COMPROBAR(c.total_jugadores == 2 && c.max_mesas == 1);
}

/* ------------------------------------------------------------------ */
/* Archivos invalidos                                                  */
/* ------------------------------------------------------------------ */

static void probar_invalidos(void)
{
    static const char *casos[] = {
        "clave_desconocida=1\n",
        "Semilla=1\n",                                   /* mayusculas: otra clave */
        "semilla=1\nsemilla=2\n",                        /* clave repetida */
        "semilla\n",                                     /* sin '=' */
        "semilla=\n",                                    /* valor vacio */
        "retardo_min_ms=50abc\n",                        /* basura */
        "retardo_min_ms=50 # algo\n",                    /* comentario al final */
        "retardo_min_ms=0\n",                            /* minimo - 1 */
        "retardo_min_ms=10\nretardo_max_ms=5\n",         /* max < min */
        "retardo_min_ms=400\n",                          /* min > max por defecto (300) */
        "retardo_max_ms=60001\n",                        /* maximo + 1 */
        "semilla=-1\n",
        "semilla=4294967296\n",                          /* UINT_MAX + 1 */
        "puma=3\n",                                      /* puma sin perros */
        "perros=1,2,3,4,5,6,7,8,9,10,11,12\n",           /* perros sin puma */
        "puma=0\nperros=1,2,3,4,5,6,7,8,9,10,11\n",      /* 11 perros */
        "puma=0\nperros=1,2,3,4,5,6,7,8,9,10,11,12,13\n",/* 13 perros */
        "puma=0\nperros=1,1,3,4,5,6,7,8,9,10,11,12\n",   /* perro repetido */
        "puma=1\nperros=1,2,3,4,5,6,7,8,9,10,11,12\n",   /* puma sobre un perro */
        "puma=29\n",                                     /* posicion 29 */
        "puma=-1\n",                                     /* posicion negativa */
        "puma=0\nperros=1,2,3,4,5,6,7,8,9,10,11,29\n",   /* un perro en 29 */
        "puma=0\nperros=1,2,3,4,5,6,7,8,9,10,11,12,\n",  /* coma final */
        "puma=0\nperros=1,,3,4,5,6,7,8,9,10,11,12\n",    /* '1,,3' */
        "puma=0\nperros=,1,2,3,4,5,6,7,8,9,10,11\n",     /* coma inicial */
        /* error en una linea tardia: lo anterior no debe quedar aplicado */
        "semilla=5\nretardo_min_ms=10\nretardo_max_ms=20\nmalo\n",
    };
    for (size_t i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        debe_fallar(casos[i]);
    }

    /* Linea de mas de 255 caracteres */
    char larga[400];
    memset(larga, 'a', 300);
    larga[300] = '\n';
    larga[301] = '\0';
    debe_fallar(larga);

    /* Archivo inexistente (ruta absoluta que no existe) */
    Configuracion c = configuracion_por_defecto();
    COMPROBAR(configuracion_leer_archivo("/tmp/torneo_no_existe.txt", &c) == 0);
}

int main(void)
{
    /* Las dos constantes del 12 no deben desincronizarse */
    COMPROBAR(CONFIG_TOTAL_PERROS == TOTAL_PERROS);

    probar_argumentos();
    probar_argumentos_con_archivo();
    probar_validos();
    probar_invalidos();

    printf("Configuracion: %d comprobaciones correctas "
           "(los errores anteriores eran esperados).\n", comprobaciones);
    return 0;
}