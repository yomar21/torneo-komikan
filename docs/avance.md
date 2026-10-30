# Como funciona este avance

Para Yomar y Thiare: conviene leer primero las reglas de `tablero.c` y despues
seguir `ejecutar_ficha` y `ejecutar_juez` en `mesa.c`.

## El recorrido de una accion

1. La ficha espera un retardo aleatorio sin retener el mutex.
2. Con el mutex adquirido, busca su identificador y elige una accion posible.
3. Coloca la propuesta en una cola y espera la respuesta del juez.
4. El juez toma la propuesta mas antigua y comprueba otra vez su validez.
5. Si sigue siendo valida, cambia el tablero y evalua si termino la partida.
6. Despierta a las fichas y muestra una copia consistente del tablero.

La segunda validacion es necesaria: desde que una ficha propuso moverse,
otra pudo ocupar el destino, mover al Perro intermedio o capturar a la ficha.
No se confia en una decision tomada sobre un estado anterior.

## Que protege el mutex

El mismo mutex protege el tablero, las propuestas, la cola y el estado de la mesa.
Validar una accion y aplicarla sucede dentro de esa misma proteccion.
Solo el juez modifica el tablero durante la partida. Las fichas lo consultan
bajo el mutex. La inicializacion ocurre antes de crear los threads.

Elegimos un mutex por mesa porque facilita entender y verificar las operaciones
completas. Serializa el acceso al tablero, pero permite que las trece fichas
esperen y soliciten acciones independientemente. No impone turnos entre bandos.
El costo de esta sencillez es que no hay movimientos del tablero en paralelo.

## Para que sirve la condicion

`pthread_cond_wait` y `pthread_cond_timedwait` liberan el mutex mientras el thread
espera y lo recuperan antes de continuar. La condicion avisa que algo cambio;
el `while` vuelve a comprobar si ya se puede avanzar.
La condicion compartida puede despertar a varios threads; cada uno revisa su
propia espera. No hay un bucle que consulte constantemente sin dormir.

## Inicio y final

Los threads esperan una senal de inicio hasta que terminaron de crearse todos.
Si falla `pthread_create`, se avisa a los que ya existen y se los recoge.
Al terminar la partida se despierta a todos, se hace `pthread_join` de cada uno
y solo entonces se destruyen la condicion y el mutex. No se usa `pthread_cancel`.

Ese cierre cubre esta mesa en ejecucion normal y errores de creacion. No cubre
aun SIGINT, abandono de jugadores ni recuperacion de un thread que muera de
forma arbitraria con datos a medio modificar.

## Riesgos que podemos explicar

- **Carreras:** la lectura y escritura compartidas estan protegidas por el mutex.
- **Deadlock:** usamos un unico mutex y las esperas de condicion lo liberan.
  El programa principal hace los joins sin retener ese mutex.
- **Starvation:** la cola atiende propuestas en orden de llegada. Eso no garantiza
  que el planificador del sistema reparta CPU equitativamente entre threads.
- **Livelock o partidas largas:** puede haber propuestas desactualizadas o ciclos
  del juego. La demostracion corta tras 500 propuestas sin inventar un ganador.

## Que comprueban las pruebas

Las antiguas impresiones de depuracion se reemplazaron por comprobaciones que
hacen fallar `make test` si no obtienen lo esperado. Se conserva un escenario de
Puma encerrado que tambien bloquea la salida al trono.

Durante cada propuesta el juez verifica que haya un solo Puma, identidades
validas sin duplicados y doce Perros entre vivos y capturados. Estos controles
son una ayuda para detectar errores; no sustituyen todas las pruebas.

## Siguiente bloque

Construir el matchmaker y los threads de llegada de jugadores. La funcion
`mesa_ejecutar` ya concentra la vida de una partida, pero la salida dice Mesa 1;
al permitir varias mesas se agregara un identificador y se coordinara el log.
Despues se completaran configuracion de entrada, abandono y cierre con SIGINT.

Este documento son notas de trabajo, no el informe final exigido por la guia.
