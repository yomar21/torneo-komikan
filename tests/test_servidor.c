#include "servidor.h"

#include "log.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

typedef struct {
    Servidor *servidor;
    int espera_ms;
    int veces;
} DatosParada;

/* Thread auxiliar: espera un poco y detiene el servidor una o varias veces. */
static void *detener_despues(void *argumento)
{
    DatosParada *datos = argumento;
    struct timespec pausa = {.tv_sec = datos->espera_ms / 1000,
                             .tv_nsec = (long)(datos->espera_ms % 1000) * 1000000L};
    nanosleep(&pausa, NULL);
    for (int i = 0; i < datos->veces; i++) {
        servidor_detener(datos->servidor);
    }
    return NULL;
}

/* Corre un torneo y lo detiene a los espera_ms. Devuelve 0 si algo fallo. */
static int torneo_detenido(const Configuracion *config, int espera_ms, int veces,
                           ResumenServidor *resumen)
{
    OpcionesServidor silencio = {0};
    Servidor *servidor = servidor_crear(config, NULL, &silencio);
    if (servidor == NULL) {
        return 0;
    }
    DatosParada datos = {.servidor = servidor, .espera_ms = espera_ms, .veces = veces};
    pthread_t auxiliar;
    if (pthread_create(&auxiliar, NULL, detener_despues, &datos) != 0) {
        servidor_destruir(servidor);
        return 0;
    }
    int correcto = servidor_correr(servidor, resumen);
    pthread_join(auxiliar, NULL);
    servidor_destruir(servidor);
    return correcto;
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
        COMPROBAR(servidor_resumen_coherente(&r, n));   /* ningun jugador ni partida se pierde */
        COMPROBAR(r.cancelados == 0 && r.por_abandono == 0 && r.cancelados_en_partida == 0);
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
        COMPROBAR(servidor_resumen_coherente(&r8, 8));
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

    /* El comprobador de invariantes detecta cuentas que no cuadran. */
    ResumenServidor falso = {.atendidos = 6, .sin_pareja = 1, .partidas = 3,
                             .victorias_puma = 2, .victorias_perros = 1};
    COMPROBAR(servidor_resumen_coherente(&falso, 7));
    COMPROBAR(!servidor_resumen_coherente(&falso, 8));   /* falta un jugador */
    falso.partidas = 4;
    COMPROBAR(!servidor_resumen_coherente(&falso, 7));   /* sobra una partida */
    falso.partidas = 3;
    falso.cancelados = 1;
    COMPROBAR(!servidor_resumen_coherente(&falso, 7));   /* un jugador contado dos veces */

    /* El resumen se escribe en el log con los numeros del torneo. */
    char ruta[] = "/tmp/torneo_resumen_XXXXXX";
    int descriptor = mkstemp(ruta);
    COMPROBAR(descriptor >= 0);
    close(descriptor);
    log_consola(0);
    COMPROBAR(log_iniciar(ruta));
    ResumenServidor para_imprimir = {.llegados = 7, .atendidos = 6, .sin_pareja = 1,
                                     .partidas = 3, .victorias_puma = 2,
                                     .victorias_perros = 1, .max_simultaneas = 2,
                                     .jugadores_creados = 7, .jugadores_unidos = 7};
    servidor_imprimir_resumen(&para_imprimir, 3);
    log_cerrar();
    log_consola(1);
    char texto[2048] = "";
    FILE *archivo = fopen(ruta, "r");
    COMPROBAR(archivo != NULL);
    size_t leidos = fread(texto, 1, sizeof texto - 1, archivo);
    texto[leidos] = '\0';
    fclose(archivo);
    unlink(ruta);
    COMPROBAR(strstr(texto, "[Servidor] ---- Resumen del torneo ----") != NULL);
    COMPROBAR(strstr(texto, "7 llegaron, 6 jugaron, 1 quedaron sin pareja y 0 cancelados") != NULL);
    COMPROBAR(strstr(texto, "Partidas: 3 (gana el Puma: 2; ganan los Perros: 1") != NULL);
    COMPROBAR(strstr(texto, "Maximo de mesas activas a la vez: 2 (limite 3)") != NULL);
    COMPROBAR(strstr(texto, "recogidos: 7/7") != NULL);

    /* Con una disposicion inicial dada. */
    Tablero inicial;
    tablero_inicializar(&inicial);
    tablero_colocar_fichas_iniciales(&inicial);
    config.total_jugadores = 4;
    ResumenServidor r;
    COMPROBAR(servidor_ejecutar(&config, &inicial, &opciones, &r));
    COMPROBAR(r.partidas == 2 && r.sin_pareja == 0);
    COMPROBAR(servidor_resumen_coherente(&r, 4));

    /* Detener el servidor: deja de entrar gente y el programa termina solo. */
    config.total_jugadores = 200;
    config.max_mesas = 3;
    ResumenServidor rp;
    time_t antes = time(NULL);
    COMPROBAR(torneo_detenido(&config, 300, 1, &rp));
    COMPROBAR(time(NULL) - antes < 30);   /* no se quedo colgado */
    COMPROBAR(servidor_resumen_coherente(&rp, 200));
    COMPROBAR(rp.cancelados > 0 && rp.cancelados < 200);
    COMPROBAR(rp.partidas >= 1 && rp.atendidos == 2 * rp.partidas);
    COMPROBAR(rp.errores == 0 && rp.max_simultaneas <= 3);
    COMPROBAR(rp.jugadores_creados == 200 && rp.jugadores_unidos == 200);

    /* Detener antes de empezar: nadie juega y todos quedan cancelados. */
    config.total_jugadores = 10;
    config.max_mesas = 2;
    OpcionesServidor silencio = {0};
    Servidor *servidor = servidor_crear(&config, NULL, &silencio);
    COMPROBAR(servidor != NULL);
    servidor_detener(servidor);
    COMPROBAR(servidor_correr(servidor, &rp));
    COMPROBAR(rp.partidas == 0 && rp.atendidos == 0 && rp.cancelados == 10);
    COMPROBAR(rp.llegados == 0 && rp.jugadores_unidos == 10);
    COMPROBAR(servidor_resumen_coherente(&rp, 10));
    ResumenServidor otra;
    COMPROBAR(!servidor_correr(servidor, &otra));   /* un servidor solo corre una vez */
    servidor_destruir(servidor);

    /* Detener cuando ya termino todo no cambia nada, y se puede repetir. */
    servidor = servidor_crear(&config, NULL, &silencio);
    COMPROBAR(servidor != NULL);
    COMPROBAR(servidor_correr(servidor, &rp));
    COMPROBAR(rp.cancelados == 0 && rp.partidas == 5);
    servidor_detener(servidor);
    servidor_detener(servidor);
    servidor_destruir(servidor);
    servidor_destruir(NULL);

    /* Muchas paradas juntas desde otro thread: la cancelacion cuenta una sola vez. */
    config.total_jugadores = 100;
    COMPROBAR(torneo_detenido(&config, 5, 1000, &rp));
    COMPROBAR(servidor_resumen_coherente(&rp, 100));

    /* Parada a distintos tiempos, con llegadas repartidas en el tiempo: algunos
     * jugadores se cancelan antes de llegar. */
    Configuracion repartida = config;
    repartida.total_jugadores = 60;
    repartida.retardo_max_ms = 40;
    ResumenServidor rr;
    COMPROBAR(torneo_detenido(&repartida, 20, 1, &rr));
    COMPROBAR(servidor_resumen_coherente(&rr, 60));
    COMPROBAR(rr.llegados < 60 && rr.cancelados > 0);   /* no llegaron todos */
    COMPROBAR(rr.atendidos == 2 * rr.partidas && rr.errores == 0);

    /* Repetir la parada muchas veces, a tiempos distintos, para buscar carreras. */
    config.total_jugadores = 40;
    config.max_mesas = 2;
    for (int i = 0; i < 30; i++) {
        ResumenServidor rs;
        COMPROBAR(torneo_detenido(&config, i * 2, 1, &rs));
        COMPROBAR(servidor_resumen_coherente(&rs, 40));
        COMPROBAR(rs.jugadores_creados == 40 && rs.jugadores_unidos == 40);
        COMPROBAR(rs.atendidos == 2 * rs.partidas && rs.errores == 0);
        COMPROBAR(rs.max_simultaneas <= 2);
    }
    config.max_mesas = 1;

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
