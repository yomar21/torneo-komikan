# Torneo de Komikan concurrente

Tarea 2 de Sistemas Operativos, desarrollada por **Thiare y Yomar** en C17.

## Estado del avance

Ya funciona una **mesa concurrente**: un thread por ficha (1 Puma y 12 Perros),
un thread juez, retardos aleatorios y un tablero compartido protegido con un
mutex POSIX. Las fichas proponen acciones y el juez las valida en orden de llegada.
No hay turnos alternados entre bandos.

Este avance todavia no es el torneo completo. Ya funciona `./torneo N M` con
cualquier N: los N jugadores llegan de forma concurrente (un thread por jugador),
se emparejan de a 2 y cada pareja juega su partida en una mesa. Nunca hay mas de
M mesas activas a la vez: si estan todas ocupadas, los jugadores restantes
esperan sin gastar CPU hasta que se libera una. Si N es impar, el ultimo
jugador queda sin pareja y el programa termina sin colgarse.

## Compilar y ejecutar

Desde la terminal de VS Code, dentro de esta carpeta:

```sh
make
./torneo 6 1
./torneo 5 1 config_ejemplo.txt
```

Se muestra el tablero completo, incluido el trono, los eventos y el resultado.
Las conexiones verticales mantienen tres lineas entre posiciones.
Cada partida recoge sus **13 threads de fichas y el juez**, y al final se informa
que se recogieron todos los threads de jugador.
Una ficha capturada deja de actuar y su thread termina.

La demostracion tiene un limite de **500 propuestas**, definido en `src/main.c`.
Si se alcanza antes de una victoria, la mesa termina **sin ganador**. Este limite
es una proteccion provisional para las pruebas, no una regla del Komikan.
Con movimientos aleatorios no se puede prometer que toda partida tenga una
victoria en una cantidad determinada de acciones.

## Archivo de configuracion

El uso completo es `./torneo N M [config.txt]`. El tercer argumento es opcional;
sin el, el programa se comporta con los valores por defecto. En
`config_ejemplo.txt` hay un ejemplo comentado.

Formato: una clave por linea, `clave=valor`. Las lineas vacias y las que
empiezan con `#` se ignoran; los comentarios no se admiten al final de un valor.
Todas las claves son opcionales y ninguna puede repetirse.

| Clave | Valor | Por defecto |
| --- | --- | --- |
| `retardo_min_ms` | entero de 1 a 60000 | 50 |
| `retardo_max_ms` | entero de 1 a 60000, no menor que el minimo | 300 |
| `semilla` | entero sin signo (el 0 es valido) | 0 |
| `puma` | posicion de 0 a TOTAL_POSICIONES-1 | 25 |
| `perros` | exactamente 12 posiciones validas del tablero, separadas por coma | 10,14,15 a 24 |

Reglas de la disposicion: `puma` y `perros` se definen juntas o ninguna; no puede
haber posiciones repetidas ni un perro sobre el Puma. El orden de la lista
`perros` asigna los identificadores 2 a 13.

Ante cualquier error el programa informa la ruta y el numero de linea, termina
con codigo 1 y no aplica ningun valor del archivo (todo o nada).

## Probar las reglas y la concurrencia

```sh
make test
```

Se ejecutan pruebas de argumentos, movimientos, identidades, capturas,
victorias e integridad del tablero, veinte simulaciones concurrentes rapidas y del archivo de configuración.
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
| `src/main.c` | Lee argumentos, llama al servidor y presenta el resumen. |
| `src/servidor.c` | Threads de jugador, matchmaker (parejas de a 2) y hasta M mesas simultaneas, cada una en su thread. |
| `src/log.c` | Salida unica con mutex: prefijo `[Mesa k]` o `[Servidor]` y tableros que no se mezclan entre mesas. |
| `src/cola.c` | Cola FIFO circular de enteros; no es thread-safe, la protege el servidor. |
| `src/configuracion.c` | Valores iniciales y validacion de N y M y lectura de config.txt |
| `src/tablero.c` | Posiciones, fichas, conexiones y reglas. No administra threads. |
| `src/mesa.c` | Threads de fichas, juez, cola de propuestas y sincronizacion. |
| `include/` | Tipos y funciones publicas de cada modulo. |
| `tests/` | Escenarios separados del programa principal. |

Para estudiar el flujo y las decisiones, ver [el avance explicado](docs/avance.md).

## Reglas adoptadas

- Cuadricula 5x5 con conexiones horizontales, verticales y diagonales alternadas.
- Trono con cuatro posiciones adicionales: 25, 26, 27 y 28; total de 29.
- El cruce central del trono se considera posicion jugable.
- Puma inicial en 25; Perros en 10, 14 y de 15 a 24 (disposición por defecto; se puede cambiar desde el archivo).
- Un salto captura un solo Perro; la posicion de aterrizaje debe estar vacia.
- Las tres posiciones del salto deben estar alineadas y a igual distancia.
- Cada salto es una accion independiente con un nuevo retardo.
- El Puma prefiere capturar cuando puede; elige entre sus opciones disponibles.
- Gana el Puma con seis capturas. Ganan los Perros si no puede caminar ni capturar.
- Identificador 1 para el Puma, 2 a 13 para los Perros, 0 para posiciones vacias.

## Pendiente para la entrega

- Simular N llegadas de jugadores mediante threads y formar parejas (Listo).
- Gestionar hasta M mesas simultaneas con espera pasiva y reutilizacion (Listo).
- Resolver y documentar el jugador sobrante si N es impar (Listo: queda sin pareja).
- Leer retardos, semilla y posiciones desde un archivo de configuracion(Listo).
- Detectar abandonos o fallos internos de jugadores y adjudicar el resultado.
- Atender SIGINT y coordinar el cierre de todo el servidor.
- Evaluar concurrencia, escalabilidad y recursos en Linux.
- Redactar el informe PDF de hasta dos paginas.

La semilla inicial es 0. Cada ficha tiene un generador privado; la misma semilla
no garantiza el mismo orden de movimientos entre threads.
