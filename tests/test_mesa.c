#include "mesa.h"

#include <stdio.h>
#include <stdlib.h>

#define COMPROBAR(condicion) do { \
    comprobaciones++; \
    if (!(condicion)) { \
        fprintf(stderr, "Fallo en linea %d: %s\n", __LINE__, #condicion); \
        return EXIT_FAILURE; \
    } \
} while (0)

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
    printf("Mesa: %d comprobaciones correctas, 20 simulaciones, "
           "%u capturas y %u propuestas obsoletas rechazadas.\n",
           comprobaciones, capturas, rechazadas);
    return EXIT_SUCCESS;
}
