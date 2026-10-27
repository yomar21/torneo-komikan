#include "configuracion.h"
#include<limits.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
Configuracion configuracion_por_defecto(void){

    Configuracion config={
        .total_jugadores=0,
        .max_mesas=0,
        .retardo_min_ms=50,
        .retardo_max_ms=300,
        .semilla=0
    };
    return config;
}

static int leer_entero_positivo(const char *texto, int *resultado){
    char *fin;
    long valor;

    errno=0;
    valor=strtol(texto,&fin,10);
    if(errno!=0 || fin==texto || *fin!='\0' ||
    valor<=0 || valor>INT_MAX){
        return 0;
    }
    *resultado= (int)valor;
    return 1;
}

int configuracion_leer_argumentos(
    int argc,
    char *argv[],
    Configuracion *config
)
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

    if (argc == 4) {
        fprintf(stderr,
                "La lectura del archivo de configuracion "
                "todavia no esta implementada.\n");
        return 0;
    }

    return 1;
}