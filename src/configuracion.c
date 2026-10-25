#include "configuracion.h"

Configuracion configuracion_por_defecto(void){

    Configuracion config;
    config.retardo_min_ms=50;
    config.retardo_max_ms=300;
    return config;
}