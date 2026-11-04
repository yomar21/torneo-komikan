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

Al terminar, el servidor escribe un resumen con los jugadores que llegaron, jugaron
y quedaron sin pareja, las partidas (victorias de cada bando, por abandono,
canceladas y con error) y el maximo de mesas activas a la vez. Las cuentas se
comprueban entre si: cada jugador y cada partida se cuenta una sola vez.

Con **Ctrl+C** el servidor deja de aceptar jugadores de inmediato: los que aun no
llegaron y los que esperan pareja en la cola quedan cancelados, y no se forman
mas parejas. Las partidas que ya estan en juego se cancelan de forma ordenada (sin
ganador) y el programa sale solo, mostrando el resumen. Las senales las atiende un thread
dedicado (con `sigwait`), porque un manejador clasico no puede usar mutex ni el log.

Al terminar, tanto en la salida normal como con Ctrl+C, el programa recoge todos sus
threads (matchmaker, jugadores, mesas y senales), destruye sus mutex y condiciones,
libera la memoria y cierra el log. No usa memoria compartida, semaforos con nombre
ni descriptores propios, asi que no deja recursos huerfanos en el sistema. El
mensaje final es `Torneo finalizado` o `Servidor detenido por SIGINT`.
`make test-sigint` lo comprueba: lanza un torneo grande, lo interrumpe a los 0,2, 1 y
2 segundos y verifica codigo de salida 0, que no queda ningun proceso vivo, que solo
hay descriptores 0, 1 y 2, que `/dev/shm` e IPC no cambian y, con AddressSanitizer,
que no hay fugas de memoria.

El programa termina con codigo 0 si el torneo acabo normalmente y con 1 si hubo
un error (argumentos o configuracion invalidos, o un fallo al crear threads).

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
| `prob_abandono` | entero de 0 a 100: porcentaje de jugadores que se retiran | 0 |
| `abandono_max_ms` | entero de 1 a 60000: el retiro ocurre entre 0 y este tiempo tras empezar la partida | 1000 |
| `puma` | posicion de 0 a TOTAL_POSICIONES-1 | 25 |
| `perros` | exactamente 12 posiciones validas del tablero, separadas por coma | 10,14,15 a 24 |

**Abandono de jugadores.** Con `prob_abandono` mayor que 0, cada jugador decide con su
propio generador si se retirara de su partida y en que instante. Si la partida
sigue en curso cuando llega ese instante, el Juez la cierra de inmediato y el
oponente gana **por abandono**. El jugador que se retira no puede dejar nada
tomado: ninguno mantiene un lock fuera de las funciones de la mesa, y la mesa
siempre libera sus mutex y condiciones al cerrarse. Ejemplo:
`prob_abandono=100` y `abandono_max_ms=200` hacen que casi toda partida termine por abandono.

**Fallo inesperado de un hilo de jugador.** Ademas del abandono programado,
el servidor registra un handler `pthread_cleanup_push` por jugador. Si el hilo
termina mediante `pthread_cancel` o `pthread_exit` mientras juega, el handler
notifica la salida inesperada, marca al bando responsable y llama a
`mesa_notificar_fallo` (o guarda el aviso si la mesa aun se esta creando).
La notificacion NO adjudica la victoria: el hilo juez se despierta con la
condicion de mesa, inspecciona el aviso bajo su mutex y resuelve la partida
con `MESA_FALLO_JUGADOR`, declarando al contrario ganador por abandono. Los
hilos de la mesa terminan y se recogen normalmente. La
limpieza respeta el mutex del servidor aun si la cancelacion sucede durante
`pthread_cond_wait`; si ocurre antes del emparejamiento, el jugador se quita de
la cola y el matchmaker deja de esperarlo. Los contadores distinguen fallos
de hilos durante la partida y antes de jugar. `make test` incluye pruebas que
cancelan hilos reales de ambos bandos, comprueban que fue el juez quien
resolvio el fallo y verifican la liberacion de mutex y condiciones despues
de hacer `join` a todos los hilos. La cancelacion POSIX es diferida y solo
se habilita durante las esperas `pthread_cond_wait/timedwait`; POSIX recupera
el mutex antes de llamar al handler, que lo libera. Ningun jugador toma el
mutex interno de la mesa. No se destruyen mutexes con otros hilos vivos.

**Limite de la recuperacion:** esto protege ante terminaciones recuperables de
threads. Una senal fatal no manejada como `SIGSEGV` normalmente finaliza TODO el
proceso POSIX. Para tolerar caidas arbitrarias seria necesario aislar a los
jugadores en procesos y supervisarlos con IPC; no se afirma esa garantia aqui.

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
| `src/main.c` | Lee argumentos, atiende Ctrl+C en un thread de senales, llama al servidor y presenta el resumen. |
| `src/servidor.c` | Threads de jugador, matchmaker (parejas de a 2) y hasta M mesas simultaneas, cada una en su thread. |
| `src/log.c` | Salida unica con mutex: prefijo `[Mesa k]` o `[Servidor]` y tableros que no se mezclan entre mesas. |
| `src/cola.c` | Cola FIFO circular de enteros; no es thread-safe, la protege el servidor. |
| `src/configuracion.c` | Valores iniciales y validacion de N y M y lectura de config.txt |
| `src/tablero.c` | Posiciones, fichas, conexiones y reglas. No administra threads. |
| `src/mesa.c` | Threads de fichas, juez, cola de propuestas y sincronizacion. La mesa se crea con `mesa_crear`, se juega con `mesa_jugar`, otro thread puede pedirle terminar con `mesa_cancelar` y se libera con `mesa_destruir`. |
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
- Detectar abandonos y salidas inesperadas recuperables de hilos de jugador (Listo; fallos fatales del proceso quedan fuera del modelo de threads).
- Atender SIGINT: dejar de aceptar jugadores, cancelar las partidas en curso y cerrar sin fugas (Listo, `make test-sigint`).
- Evaluar concurrencia, escalabilidad y recursos en Linux.
- Redactar el informe PDF de hasta dos paginas.

La semilla inicial es 0. Cada ficha tiene un generador privado; la misma semilla
no garantiza el mismo orden de movimientos entre threads.
