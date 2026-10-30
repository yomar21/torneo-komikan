# Torneo de Komikan concurrente

Tarea 2 de Sistemas Operativos, desarrollada por **Thiare y Yomar** en C17.

## Estado del avance

Ya funciona una **mesa concurrente**: un thread por ficha (1 Puma y 12 Perros),
un thread juez, retardos aleatorios y un tablero compartido protegido con un
mutex POSIX. Las fichas proponen acciones y el juez las valida en orden de llegada.
No hay turnos alternados entre bandos.

Este avance todavia no es el torneo completo. Por ahora solo admite `N=2` y `M=1`.
Los dos jugadores se representan como los bandos de esa pareja; los threads
que simulan su llegada y el emparejamiento se agregaran en el siguiente bloque.

## Compilar y ejecutar

Desde la terminal de VS Code, dentro de esta carpeta:

```sh
make
./torneo 2 1
```

Se muestra el tablero completo, incluido el trono, los eventos y el resultado.
Las conexiones verticales mantienen tres lineas entre posiciones.
Al terminar debe informar que recogio los **13 threads de fichas y el juez**.
Una ficha capturada deja de actuar y su thread termina.

La demostracion tiene un limite de **500 propuestas**, definido en `src/main.c`.
Si se alcanza antes de una victoria, la mesa termina **sin ganador**. Este limite
es una proteccion provisional para las pruebas, no una regla del Komikan.
Con movimientos aleatorios no se puede prometer que toda partida tenga una
victoria en una cantidad determinada de acciones.

## Probar las reglas y la concurrencia

```sh
make test
```

Se ejecutan pruebas de argumentos, movimientos, identidades, capturas,
victorias e integridad del tablero, y veinte simulaciones concurrentes rapidas.
Los mensajes de entradas invalidas durante la prueba de configuracion son esperados.
Los binarios de prueba se guardan en `build/`, que Git ignora.

Para revisar accesos invalidos y comportamiento indefinido, con un compilador
que incluya los sanitizadores:

```sh
make test-sanitize
```

Para revisar carreras de datos entre threads:

```sh
make test-tsan
```

Las herramientas no demuestran la ausencia de todos los errores; detectan
problemas en las ejecuciones que se prueban.

El comando `gcc` de macOS utiliza Apple Clang. Se debe verificar tambien la
compilacion con GCC en Linux antes de entregar.

## Modulos

| Archivo | Responsabilidad |
| --- | --- |
| `src/main.c` | Inicia la demostracion y presenta el resumen. |
| `src/configuracion.c` | Valores iniciales y validacion de N y M. |
| `src/tablero.c` | Posiciones, fichas, conexiones y reglas. No administra threads. |
| `src/mesa.c` | Threads de fichas, juez, cola de propuestas y sincronizacion. |
| `include/` | Tipos y funciones publicas de cada modulo. |
| `tests/` | Escenarios separados del programa principal. |

Para estudiar el flujo y las decisiones, ver [el avance explicado](docs/avance.md).

## Reglas adoptadas

- Cuadricula 5x5 con conexiones horizontales, verticales y diagonales alternadas.
- Trono con cuatro posiciones adicionales: 25, 26, 27 y 28; total de 29.
- El cruce central del trono se considera posicion jugable.
- Puma inicial en 25; Perros en 10, 14 y de 15 a 24.
- Un salto captura un solo Perro; la posicion de aterrizaje debe estar vacia.
- Las tres posiciones del salto deben estar alineadas y a igual distancia.
- Cada salto es una accion independiente con un nuevo retardo.
- El Puma prefiere capturar cuando puede; elige entre sus opciones disponibles.
- Gana el Puma con seis capturas. Ganan los Perros si no puede caminar ni capturar.
- Identificador 1 para el Puma, 2 a 13 para los Perros, 0 para posiciones vacias.

## Pendiente para la entrega

- Simular N llegadas de jugadores mediante threads y formar parejas.
- Gestionar hasta M mesas simultaneas con espera pasiva y reutilizacion.
- Resolver y documentar el jugador sobrante si N es impar.
- Leer retardos, semilla y posiciones desde un archivo de configuracion.
- Detectar abandonos o fallos internos de jugadores y adjudicar el resultado.
- Atender SIGINT y coordinar el cierre de todo el servidor.
- Evaluar concurrencia, escalabilidad y recursos en Linux.
- Redactar el informe PDF de hasta dos paginas.

La semilla inicial es 0. Cada ficha tiene un generador privado; la misma semilla
no garantiza el mismo orden de movimientos entre threads.
