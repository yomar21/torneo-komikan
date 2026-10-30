#include "configuracion.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    Configuracion config = configuracion_por_defecto();
    assert(config.retardo_min_ms == 50 && config.retardo_max_ms == 300);
    char *valido[] = {"torneo", "6", "2"};
    assert(configuracion_leer_argumentos(3, valido, &config));
    assert(config.total_jugadores == 6 && config.max_mesas == 2);

    char *cero[] = {"torneo", "0", "2"};
    char *negativo[] = {"torneo", "2", "-1"};
    char *texto[] = {"torneo", "6abc", "2"};
    char *vacio[] = {"torneo", "", "2"};
    char *desborde[] = {"torneo", "99999999999999999999999", "2"};
    char *archivo[] = {"torneo", "2", "1", "config.txt"};
    assert(!configuracion_leer_argumentos(3, cero, &config));
    assert(!configuracion_leer_argumentos(3, negativo, &config));
    assert(!configuracion_leer_argumentos(3, texto, &config));
    assert(!configuracion_leer_argumentos(3, vacio, &config));
    assert(!configuracion_leer_argumentos(3, desborde, &config));
    assert(!configuracion_leer_argumentos(1, valido, &config));
    assert(!configuracion_leer_argumentos(4, archivo, &config));
    puts("Configuracion: 10 comprobaciones correctas (los errores anteriores eran esperados).");
    return 0;
}
