#ifndef CONFIGURACION_H
#define CONFIGURACION_H
typedef struct {
int retardo_min_ms;
int retardo_max_ms;
} Configuracion;

Configuracion configuracion_por_defecto(void);

#endif