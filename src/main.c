#include <stdio.h>
#include "configuracion.h"
int main(void){
Configuracion config=configuracion_por_defecto();
printf("Torneo de komikan\n");
printf("Retardo minimo %d ms\n",config.retardo_min_ms);
printf("Retardo maximo %d ms\n",config.retardo_max_ms);
}