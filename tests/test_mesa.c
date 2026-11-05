#include "mesa.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

typedef struct {
    Mesa *mesa;
    int espera_ms;
    int veces;
} DatosCancelador;

/* Thread auxiliar: espera un poco y cancela la mesa una o varias veces. */
static void *cancelar_despues(void *argumento)
{
    DatosCancelador *datos = argumento;
    struct timespec pausa = {.tv_sec = datos->espera_ms / 1000,
                             .tv_nsec = (long)(datos->espera_ms % 1000) * 1000000L};
    nanosleep(&pausa, NULL);
    for (int i = 0; i < datos->veces; i++) {
        mesa_cancelar(datos->mesa);
    }
    return NULL;
}

typedef struct {
    Mesa *mesa;
    int espera_ms;
    Bando bando;
} DatosAbandono;

/* Thread auxiliar: espera un poco y hace que un jugador se retire. */
static void *abandonar_despues(void *argumento)
{
    DatosAbandono *datos = argumento;
    struct timespec pausa = {.tv_sec = datos->espera_ms / 1000,
                             .tv_nsec = (long)(datos->espera_ms % 1000) * 1000000L};
    nanosleep(&pausa, NULL);
    mesa_abandono(datos->mesa, datos->bando);
    return NULL;
}

int main(void)
{
    int comprobaciones = 0;
    unsigned int rechazadas = 0;
    unsigned int capturas = 0;
    Configuracion config = configuracion_por_defecto();
    config.retardo_min_ms = 1;
    config.retardo_max_ms = 3;
    OpcionesMesa opciones = {
        .limite_propuestas = 120, .mostrar_eventos = 0, .mostrar_tablero = 0
    };

    for (unsigned int semilla = 0; semilla < 20; semilla++) {
        config.semilla = semilla;
        ResumenMesa resumen;
        COMPROBAR(mesa_ejecutar(&config, NULL, &opciones, &resumen));
        COMPROBAR(tablero_estado_consistente(&resumen.tablero_final));
        COMPROBAR(resumen.fichas_creadas == TOTAL_FICHAS);
        COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);
        COMPROBAR(resumen.propuestas <= opciones.limite_propuestas);
        COMPROBAR(resumen.propuestas == resumen.movimientos + resumen.rechazadas);
        COMPROBAR(resumen.capturas == (unsigned int)resumen.tablero_final.perros_capturados);
        COMPROBAR(resumen.motivo == MESA_LIMITE_PRUEBA ||
                  (resumen.motivo == MESA_POR_VICTORIA &&
                   resumen.resultado == tablero_evaluar_resultado(&resumen.tablero_final)));
        rechazadas += resumen.rechazadas;
        capturas += resumen.capturas;
    }

    /* Una partida ya terminada tambien debe recoger todos sus threads. */
    Tablero bloqueado;
    tablero_inicializar(&bloqueado);
    bloqueado.ocupantes[0] = PUMA;
    bloqueado.identificadores[0] = ID_PUMA;
    const int posiciones[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 25, 26};
    for (int i = 0; i < TOTAL_PERROS; i++) {
        bloqueado.ocupantes[posiciones[i]] = PERRO;
        bloqueado.identificadores[posiciones[i]] = i + 2;
    }
    ResumenMesa resumen;
    COMPROBAR(mesa_ejecutar(&config, &bloqueado, &opciones, &resumen));
    COMPROBAR(resumen.resultado == VICTORIA_PERROS);
    COMPROBAR(resumen.propuestas == 0);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);

    opciones.limite_propuestas = 1;
    COMPROBAR(mesa_ejecutar(&config, NULL, &opciones, &resumen));
    COMPROBAR(resumen.motivo == MESA_LIMITE_PRUEBA);
    COMPROBAR(resumen.resultado == PARTIDA_EN_CURSO);
    COMPROBAR(resumen.propuestas == 1);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);

    /* Limite 0 = sin limite: la partida solo termina porque alguien gana. */
    opciones.limite_propuestas = 0;
    config.semilla = 7;
    COMPROBAR(mesa_ejecutar(&config, NULL, &opciones, &resumen));
    COMPROBAR(resumen.motivo == MESA_POR_VICTORIA);
    COMPROBAR(resumen.resultado != PARTIDA_EN_CURSO);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);

    /* Cancelacion: partidas lentas para que la cancelacion llegue en pleno juego. */
    Configuracion lenta = configuracion_por_defecto();
    lenta.retardo_min_ms = 20;
    lenta.retardo_max_ms = 40;
    OpcionesMesa sin_limite = {.limite_propuestas = 0};

    /* 1. Cancelar a los 50 ms, desde otro thread, con la partida en juego. */
    Mesa *mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    DatosCancelador cancelador = {.mesa = mesa, .espera_ms = 50, .veces = 1};
    pthread_t auxiliar;
    COMPROBAR(pthread_create(&auxiliar, NULL, cancelar_despues, &cancelador) == 0);
    struct timespec inicio, fin;
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    clock_gettime(CLOCK_MONOTONIC, &fin);
    COMPROBAR(pthread_join(auxiliar, NULL) == 0);
    long ms = (fin.tv_sec - inicio.tv_sec) * 1000 + (fin.tv_nsec - inicio.tv_nsec) / 1000000;
    COMPROBAR(ms < 1000);   /* termino por la cancelacion, no por jugar toda la partida */
    COMPROBAR(resumen.motivo == MESA_CANCELADA);
    COMPROBAR(resumen.resultado == PARTIDA_EN_CURSO);
    COMPROBAR(resumen.fichas_creadas == TOTAL_FICHAS && resumen.fichas_unidas == TOTAL_FICHAS);
    COMPROBAR(resumen.juez_unido);
    COMPROBAR(tablero_estado_consistente(&resumen.tablero_final));
    COMPROBAR(resumen.propuestas == resumen.movimientos + resumen.rechazadas);
    COMPROBAR(mesa_describir_cierre(&resumen)[0] == 'p');   /* "partida cancelada..." */
    mesa_destruir(mesa);

    /* 2. Cancelar antes de jugar: la partida no avanza y se recoge todo. */
    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_cancelar(mesa);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_CANCELADA);
    COMPROBAR(resumen.propuestas == 0 && resumen.movimientos == 0);
    COMPROBAR(resumen.fichas_creadas == TOTAL_FICHAS && resumen.fichas_unidas == TOTAL_FICHAS);
    COMPROBAR(resumen.juez_unido);
    mesa_destruir(mesa);

    /* 3. Cancelar despues de terminar y cancelar varias veces: no pasa nada. */
    OpcionesMesa con_limite = {.limite_propuestas = 5};
    mesa = mesa_crear(&config, NULL, &con_limite);
    COMPROBAR(mesa != NULL);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_LIMITE_PRUEBA && resumen.propuestas == 5);
    mesa_cancelar(mesa);
    mesa_cancelar(mesa);
    COMPROBAR(resumen.motivo == MESA_LIMITE_PRUEBA);
    mesa_destruir(mesa);

    /* 3b. La cancelacion compite con el fin de la partida: gana quien llega primero
     * y el motivo no cambia despues. */
    for (int espera = 0; espera <= 100; espera += 20) {
        OpcionesMesa cinco = {.limite_propuestas = 5};
        mesa = mesa_crear(&lenta, NULL, &cinco);
        COMPROBAR(mesa != NULL);
        cancelador = (DatosCancelador){.mesa = mesa, .espera_ms = espera, .veces = 3};
        COMPROBAR(pthread_create(&auxiliar, NULL, cancelar_despues, &cancelador) == 0);
        COMPROBAR(mesa_jugar(mesa, &resumen));
        COMPROBAR(pthread_join(auxiliar, NULL) == 0);
        COMPROBAR((resumen.motivo == MESA_LIMITE_PRUEBA && resumen.propuestas == 5) ||
                  (resumen.motivo == MESA_CANCELADA && resumen.propuestas < 5));
        COMPROBAR(resumen.resultado == PARTIDA_EN_CURSO);
        COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);
        mesa_destruir(mesa);
    }

    /* 4. Muchas cancelaciones juntas desde otro thread: el resultado es uno solo. */
    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    cancelador = (DatosCancelador){.mesa = mesa, .espera_ms = 30, .veces = 1000};
    COMPROBAR(pthread_create(&auxiliar, NULL, cancelar_despues, &cancelador) == 0);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(pthread_join(auxiliar, NULL) == 0);
    COMPROBAR(resumen.motivo == MESA_CANCELADA);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);
    mesa_destruir(mesa);

    /* 5. Crear y destruir sin jugar, y crear con datos invalidos. */
    mesa = mesa_crear(&config, NULL, &opciones);
    COMPROBAR(mesa != NULL);
    mesa_destruir(mesa);
    mesa_destruir(NULL);
    Configuracion mala = config;
    mala.retardo_min_ms = 0;
    COMPROBAR(mesa_crear(&mala, NULL, &opciones) == NULL);
    Tablero roto = bloqueado;
    roto.ocupantes[0] = VACIA;
    COMPROBAR(mesa_crear(&config, &roto, &opciones) == NULL);

    /* 6. Una mesa solo se juega una vez. */
    mesa = mesa_crear(&config, NULL, &con_limite);
    COMPROBAR(mesa != NULL);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    ResumenMesa otro;
    COMPROBAR(!mesa_jugar(mesa, &otro));
    mesa_destruir(mesa);

    /* Abandono: el oponente gana, en todos los casos se recogen los 14 hilos. */
    for (int bando = 0; bando < 2; bando++) {
        mesa = mesa_crear(&lenta, NULL, &sin_limite);
        COMPROBAR(mesa != NULL);
        /* Abandona en pleno juego, desde otro thread, a los 50 ms. */
        DatosAbandono ab = {.mesa = mesa, .espera_ms = 50, .bando = bando == 0 ? BANDO_PUMA : BANDO_PERROS};
        COMPROBAR(pthread_create(&auxiliar, NULL, abandonar_despues, &ab) == 0);
        COMPROBAR(mesa_jugar(mesa, &resumen));
        COMPROBAR(pthread_join(auxiliar, NULL) == 0);
        COMPROBAR(resumen.motivo == MESA_ABANDONO);
        COMPROBAR(resumen.abandono_detectado_por_juez == 1);
        COMPROBAR(resumen.fallo_detectado_por_juez == 0);
        COMPROBAR(resumen.resultado == (bando == 0 ? VICTORIA_PERROS : VICTORIA_PUMA));
        COMPROBAR(resumen.fichas_creadas == TOTAL_FICHAS && resumen.fichas_unidas == TOTAL_FICHAS);
        COMPROBAR(resumen.juez_unido);
        COMPROBAR(tablero_estado_consistente(&resumen.tablero_final));
        COMPROBAR(strstr(mesa_describir_cierre(&resumen), "abandono") != NULL);
        mesa_destruir(mesa);
    }

    /* Abandono antes de empezar a jugar. */
    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_abandono(mesa, BANDO_PUMA);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_ABANDONO && resumen.resultado == VICTORIA_PERROS);
    COMPROBAR(resumen.abandono_detectado_por_juez == 1);
    COMPROBAR(resumen.propuestas == 0);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);
    mesa_destruir(mesa);

    /* El primero que llega manda: abandono y cancelacion juntos, o dos abandonos. */
    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_cancelar(mesa);
    mesa_abandono(mesa, BANDO_PERROS);    /* llega tarde: no cambia el cierre */
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_CANCELADA && resumen.resultado == PARTIDA_EN_CURSO);
    mesa_destruir(mesa);

    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_abandono(mesa, BANDO_PERROS);
    mesa_abandono(mesa, BANDO_PUMA);      /* el segundo abandono se ignora */
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_ABANDONO && resumen.resultado == VICTORIA_PUMA);
    COMPROBAR(resumen.abandono_detectado_por_juez == 1);
    mesa_destruir(mesa);

    /* Abandono vs fallo antes del juez: el primer aviso prevalece. */
    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_abandono(mesa, BANDO_PUMA);
    mesa_notificar_fallo(mesa, BANDO_PERROS);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_ABANDONO && resumen.resultado == VICTORIA_PERROS);
    COMPROBAR(resumen.abandono_detectado_por_juez == 1 && resumen.fallo_detectado_por_juez == 0);
    mesa_destruir(mesa);

    mesa = mesa_crear(&lenta, NULL, &sin_limite);
    COMPROBAR(mesa != NULL);
    mesa_notificar_fallo(mesa, BANDO_PERROS);
    mesa_abandono(mesa, BANDO_PUMA);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_FALLO_JUGADOR && resumen.resultado == VICTORIA_PUMA);
    COMPROBAR(resumen.fallo_detectado_por_juez == 1 && resumen.abandono_detectado_por_juez == 0);
    mesa_destruir(mesa);

    /* Abandono despues de terminar, y abandono compitiendo con el fin natural. */
    mesa = mesa_crear(&config, NULL, &con_limite);
    COMPROBAR(mesa != NULL);
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_LIMITE_PRUEBA);
    mesa_abandono(mesa, BANDO_PUMA);      /* ya habia terminado: no pasa nada */
    mesa_destruir(mesa);
    for (int espera = 0; espera <= 100; espera += 20) {
        OpcionesMesa cinco = {.limite_propuestas = 5};
        mesa = mesa_crear(&lenta, NULL, &cinco);
        COMPROBAR(mesa != NULL);
        DatosAbandono ab = {.mesa = mesa, .espera_ms = espera, .bando = BANDO_PERROS};
        COMPROBAR(pthread_create(&auxiliar, NULL, abandonar_despues, &ab) == 0);
        COMPROBAR(mesa_jugar(mesa, &resumen));
        COMPROBAR(pthread_join(auxiliar, NULL) == 0);
        COMPROBAR((resumen.motivo == MESA_LIMITE_PRUEBA && resumen.propuestas == 5) ||
                  (resumen.motivo == MESA_ABANDONO && resumen.resultado == VICTORIA_PUMA));
        COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS && resumen.juez_unido);
        mesa_destruir(mesa);
    }

    printf("Mesa: %d comprobaciones correctas, 20 simulaciones, "
           "%u capturas y %u propuestas obsoletas rechazadas.\n",
           comprobaciones, capturas, rechazadas);
    return EXIT_SUCCESS;
}
