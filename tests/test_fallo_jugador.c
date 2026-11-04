/* Pruebas internas: se incluye servidor.c para cancelar un hilo de jugador
 * real sin exponer una API de inyeccion de fallos en el programa entregado. */
#include "../src/servidor.c"

#include <stdio.h>
#include <stdlib.h>

#define COMPROBAR(expresion) do { \
    comprobaciones++; \
    if (!(expresion)) { \
        fprintf(stderr, "Fallo en %s:%d: %s\n", __FILE__, __LINE__, #expresion); \
        return 0; \
    } \
} while (0)

static int comprobaciones;

static Configuracion configuracion_prueba(void)
{
    Configuracion cfg = configuracion_por_defecto();
    cfg.total_jugadores = 2;
    cfg.max_mesas = 1;
    cfg.retardo_min_ms = 10;
    cfg.retardo_max_ms = 20;
    cfg.prob_abandono = 0;
    return cfg;
}

/* La cancellation real de pthread ejecuta el handler con el mutex del
 * servidor tomado, tal como lo exige POSIX al salir de cond_wait. */
static int probar_fallo_durante_partida(Bando bando)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesServidor opciones = {0};
    OpcionesMesa opciones_mesa = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    Mesa *mesa = mesa_crear(&cfg, NULL, &opciones_mesa);
    COMPROBAR(mesa != NULL);

    SlotMesa *slot = &servidor->slots[0];
    slot->ocupado = 1;
    slot->numero = 1;
    slot->primero = 1;
    slot->segundo = 2;
    slot->mesa = mesa;
    servidor->jugadores[0].estado = JUGADOR_JUGANDO;
    servidor->jugadores[0].slot = slot;
    servidor->jugadores[0].bando = bando;
    DatosJugador datos = {.servidor = servidor, .indice = 0};

    pthread_t hilo;
    COMPROBAR(pthread_create(&hilo, NULL, ejecutar_jugador, &datos) == 0);
    COMPROBAR(pthread_cancel(hilo) == 0);
    void *salida = NULL;
    COMPROBAR(pthread_join(hilo, &salida) == 0);
    COMPROBAR(salida == PTHREAD_CANCELED);
    /* El handler debe devolver el mutex incluso al cancelar cond_wait. */
    COMPROBAR(pthread_mutex_trylock(&servidor->mutex) == 0);
    COMPROBAR(pthread_mutex_unlock(&servidor->mutex) == 0);
    COMPROBAR(slot->abandono == (bando == BANDO_PUMA ? 1 : 2));
    COMPROBAR(servidor->resumen.fallos_jugadores == 1);

    ResumenMesa resumen;
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_FALLO_JUGADOR);
    COMPROBAR(resumen.fallo_detectado_por_juez == 1);
    COMPROBAR(strstr(mesa_describir_cierre(&resumen), "fallo inesperado") != NULL);
    COMPROBAR(resumen.resultado == (bando == BANDO_PUMA ? VICTORIA_PERROS : VICTORIA_PUMA));
    COMPROBAR(resumen.fichas_creadas == resumen.fichas_unidas);
    COMPROBAR(resumen.juez_unido == 1);
    slot->mesa = NULL;
    mesa_destruir(mesa);
    servidor_destruir(servidor);
    return 1;
}

/* El jugador falla cuando ya esta emparejado, pero antes de que la mesa se
 * registre: slot->abandono se conserva y se aplica al crear la mesa. */
static int probar_fallo_antes_de_crear_mesa(void)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesServidor opciones = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    SlotMesa *slot = &servidor->slots[0];
    slot->ocupado = 1;
    slot->numero = 1;
    slot->primero = 1;
    slot->segundo = 2;
    servidor->mesas_activas = 1;
    servidor->resumen.partidas = 1;
    servidor->jugadores[0].estado = JUGADOR_JUGANDO;
    servidor->jugadores[1].estado = JUGADOR_JUGANDO;
    servidor->jugadores[1].slot = slot;
    Bando bando = servidor_puma_es_primero(cfg.semilla, 1) ? BANDO_PERROS : BANDO_PUMA;
    servidor->jugadores[1].bando = bando;
    DatosJugador datos = {.servidor = servidor, .indice = 1};
    pthread_t hilo;
    COMPROBAR(pthread_create(&hilo, NULL, ejecutar_jugador, &datos) == 0);
    COMPROBAR(pthread_cancel(hilo) == 0);
    COMPROBAR(pthread_join(hilo, NULL) == 0);
    COMPROBAR(slot->mesa == NULL);
    COMPROBAR(slot->abandono == (bando == BANDO_PUMA ? 1 : 2));
    COMPROBAR(servidor->resumen.fallos_jugadores == 1);
    COMPROBAR(ejecutar_mesa(slot) == NULL);
    COMPROBAR(slot->mesa == NULL && slot->ocupado == 0);
    COMPROBAR(servidor->resumen.por_abandono == 1);
    COMPROBAR(servidor->resumen.atendidos == 2);
    COMPROBAR(servidor->mesas_activas == 0);
    COMPROBAR(servidor_resumen_coherente(&servidor->resumen, 2));
    servidor_destruir(servidor);
    return 1;
}

/* Fallar antes de jugar tampoco puede dejar una entrada fantasma en la cola
 * o hacer que el matchmaker espere una llegada imposible. */
static int probar_fallo_antes_de_partida(int en_cola)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesServidor opciones = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    servidor->registro_completo = 1;
    servidor->esperados = 1;
    if (en_cola) {
        servidor->jugadores[0].llego = 1;
        servidor->llegados = 1;
        COMPROBAR(cola_encolar(servidor->cola, 1));
    }
    DatosJugador datos = {.servidor = servidor, .indice = 0};
    pthread_t hilo;
    COMPROBAR(pthread_create(&hilo, NULL, ejecutar_jugador, &datos) == 0);
    COMPROBAR(pthread_cancel(hilo) == 0);
    COMPROBAR(pthread_join(hilo, NULL) == 0);
    COMPROBAR(servidor->jugadores[0].estado == JUGADOR_FALLIDO);
    COMPROBAR(servidor->resumen.fallos_sin_partida == 1);
    COMPROBAR(servidor->esperados == (en_cola ? 1 : 0));
    COMPROBAR(cola_tamano(servidor->cola) == 0);
    servidor_destruir(servidor);
    return 1;
}

/* Una salida normal NO es una falla y no debe adjudicar victoria alguna. */
static int probar_salida_normal(void)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesServidor opciones = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    servidor->jugadores[0].estado = JUGADOR_FINALIZADO;
    DatosJugador datos = {.servidor = servidor, .indice = 0};
    pthread_t hilo;
    COMPROBAR(pthread_create(&hilo, NULL, ejecutar_jugador, &datos) == 0);
    void *salida = (void *)1;
    COMPROBAR(pthread_join(hilo, &salida) == 0);
    COMPROBAR(salida == NULL);
    COMPROBAR(servidor->resumen.fallos_jugadores == 0);
    COMPROBAR(servidor->resumen.fallos_sin_partida == 0);
    servidor_destruir(servidor);
    return 1;
}

/* pthread_exit tambien ejecuta handlers de limpieza registrados. */
static void *simular_pthread_exit(void *argumento)
{
    LimpiezaJugador limpieza = *(LimpiezaJugador *)argumento;
    pthread_cleanup_push(limpiar_jugador_fallido, &limpieza);
    pthread_exit((void *)1);
    pthread_cleanup_pop(0);
    return NULL;
}

static int probar_fallo_con_pthread_exit(void)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesServidor opciones = {0};
    OpcionesMesa opciones_mesa = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    Mesa *mesa = mesa_crear(&cfg, NULL, &opciones_mesa);
    COMPROBAR(mesa != NULL);
    SlotMesa *slot = &servidor->slots[0];
    slot->numero = 1;
    slot->mesa = mesa;
    servidor->jugadores[0].estado = JUGADOR_JUGANDO;
    servidor->jugadores[0].slot = slot;
    servidor->jugadores[0].bando = BANDO_PUMA;
    LimpiezaJugador contexto = {servidor, &servidor->jugadores[0], 0};
    pthread_t hilo;
    COMPROBAR(pthread_create(&hilo, NULL, simular_pthread_exit, &contexto) == 0);
    COMPROBAR(pthread_join(hilo, NULL) == 0);
    COMPROBAR(pthread_mutex_trylock(&servidor->mutex) == 0);
    COMPROBAR(pthread_mutex_unlock(&servidor->mutex) == 0);
    COMPROBAR(slot->abandono == 1);
    COMPROBAR(servidor->resumen.fallos_jugadores == 1);
    ResumenMesa resumen;
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.motivo == MESA_FALLO_JUGADOR);
    COMPROBAR(resumen.fallo_detectado_por_juez == 1);
    COMPROBAR(strstr(mesa_describir_cierre(&resumen), "fallo inesperado") != NULL);
    COMPROBAR(resumen.resultado == VICTORIA_PERROS);
    slot->mesa = NULL;
    mesa_destruir(mesa);
    servidor_destruir(servidor);
    return 1;
}


/* En esta prueba el juez YA esta ejecutando la mesa cuando el jugador falla.
 * No se solicita abandono manual ni se llama a ninguna funcion del juez:
 * el evento nace automaticamente en la limpieza de pthread_cancel. */
typedef struct {
    Mesa *mesa;
    ResumenMesa resumen;
    int correcto;
} MesaEnCurso;

static void *jugar_en_thread(void *argumento)
{
    MesaEnCurso *datos = argumento;
    datos->correcto = mesa_jugar(datos->mesa, &datos->resumen);
    return NULL;
}

static int probar_fallo_con_juez_activo(Bando bando)
{
    Configuracion cfg = configuracion_prueba();
    cfg.retardo_min_ms = 150;
    cfg.retardo_max_ms = 180;
    cfg.semilla = 311u;
    OpcionesServidor opciones = {0};
    OpcionesMesa opciones_mesa = {0};
    Servidor *servidor = servidor_crear(&cfg, NULL, &opciones);
    COMPROBAR(servidor != NULL);
    Mesa *mesa = mesa_crear(&cfg, NULL, &opciones_mesa);
    COMPROBAR(mesa != NULL);
    SlotMesa *slot = &servidor->slots[0];
    slot->ocupado = 1;
    slot->numero = 1;
    slot->primero = 1;
    slot->segundo = 2;
    slot->mesa = mesa;
    servidor->jugadores[0].estado = JUGADOR_JUGANDO;
    servidor->jugadores[0].slot = slot;
    servidor->jugadores[0].bando = bando;
    DatosJugador datos = {.servidor = servidor, .indice = 0};

    pthread_t hilo, hilo_mesa;
    MesaEnCurso curso = {.mesa = mesa};
    COMPROBAR(pthread_create(&hilo, NULL, ejecutar_jugador, &datos) == 0);
    COMPROBAR(pthread_create(&hilo_mesa, NULL, jugar_en_thread, &curso) == 0);
    struct timespec pausa = {.tv_sec = 0, .tv_nsec = 25000000L};
    nanosleep(&pausa, NULL);
    COMPROBAR(pthread_cancel(hilo) == 0);
    void *salida = NULL;
    COMPROBAR(pthread_join(hilo, &salida) == 0);
    COMPROBAR(salida == PTHREAD_CANCELED);
    COMPROBAR(pthread_mutex_trylock(&servidor->mutex) == 0);
    COMPROBAR(pthread_mutex_unlock(&servidor->mutex) == 0);
    COMPROBAR(pthread_join(hilo_mesa, NULL) == 0);
    COMPROBAR(curso.correcto);
    COMPROBAR(curso.resumen.motivo == MESA_FALLO_JUGADOR);
    COMPROBAR(curso.resumen.fallo_detectado_por_juez == 1);
    COMPROBAR(curso.resumen.resultado ==
             (bando == BANDO_PUMA ? VICTORIA_PERROS : VICTORIA_PUMA));
    COMPROBAR(curso.resumen.fichas_unidas == TOTAL_FICHAS);
    COMPROBAR(curso.resumen.juez_unido == 1);
    COMPROBAR(servidor->resumen.fallos_jugadores == 1);

    slot->mesa = NULL;
    mesa_destruir(mesa);
    servidor_destruir(servidor);
    return 1;
}

/* La mesa solo registra el evento; la resolucion debe salir del juez.
 * Si la notificacion ocurre ANTES del arranque, el aviso no se pierde. */
static int probar_fallo_notificado_antes_del_juez(void)
{
    Configuracion cfg = configuracion_prueba();
    OpcionesMesa opciones = {0};
    Mesa *mesa = mesa_crear(&cfg, NULL, &opciones);
    COMPROBAR(mesa != NULL);
    mesa_notificar_fallo(mesa, BANDO_PERROS);
    mesa_notificar_fallo(mesa, BANDO_PUMA); /* segundo aviso no prevalece */
    ResumenMesa resumen;
    COMPROBAR(mesa_jugar(mesa, &resumen));
    COMPROBAR(resumen.fallo_detectado_por_juez == 1);
    COMPROBAR(resumen.motivo == MESA_FALLO_JUGADOR);
    COMPROBAR(resumen.resultado == VICTORIA_PUMA);
    COMPROBAR(resumen.propuestas == 0);
    COMPROBAR(resumen.fichas_creadas == TOTAL_FICHAS);
    COMPROBAR(resumen.fichas_unidas == TOTAL_FICHAS);
    COMPROBAR(resumen.juez_unido == 1);
    mesa_destruir(mesa);
    return 1;
}

int main(void)
{
    if (!probar_fallo_con_juez_activo(BANDO_PUMA) ||
        !probar_fallo_con_juez_activo(BANDO_PERROS) ||
        !probar_fallo_notificado_antes_del_juez() ||
        !probar_fallo_durante_partida(BANDO_PUMA) ||
        !probar_fallo_durante_partida(BANDO_PERROS) ||
        !probar_fallo_antes_de_crear_mesa() ||
        !probar_fallo_con_pthread_exit() ||
        !probar_fallo_antes_de_partida(0) ||
        !probar_fallo_antes_de_partida(1) ||
        !probar_salida_normal()) {
        return EXIT_FAILURE;
    }
    printf("Fallos de jugadores: %d comprobaciones correctas.\n", comprobaciones);
    return EXIT_SUCCESS;
}
