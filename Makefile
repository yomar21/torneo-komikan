CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -Wall -Wextra -std=c17 -pthread
LDLIBS = -pthread

SOURCES = src/main.c src/configuracion.c src/tablero.c src/mesa.c src/log.c src/cola.c src/servidor.c
HEADERS = $(wildcard include/*.h)
TEST_BINS = build/test_tablero build/test_mesa build/test_configuracion build/test_cola build/test_servidor build/test_log build/test_fallo_jugador

.PHONY: all test test-sanitize test-tsan test-sigint test-sigint-tsan test-integracion-fallo clean

all: torneo

torneo: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

build:
	mkdir -p build

build/torneo_san: $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $@ $(LDLIBS)

# Ejecutable real con inyeccion habilitada solo durante la prueba integrada.
build/torneo_fallo_inyectado: $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DKOMIKAN_TEST_FALLO_INTEGRADO $(SOURCES) -o $@ $(LDLIBS)

test-integracion-fallo: torneo build/torneo_fallo_inyectado
	sh tests/test_fallo_integracion.sh ./build/torneo_fallo_inyectado

build/torneo_tsan: $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread $(SOURCES) -o $@ $(LDLIBS)

# Ctrl+C: con el programa normal y con ASan (que ademas busca fugas de memoria).
test-sigint: torneo build/torneo_san
	sh tests/test_sigint.sh ./torneo
	sh tests/test_sigint.sh ./build/torneo_san

test-sigint-tsan: build/torneo_tsan
	sh tests/test_sigint.sh ./build/torneo_tsan

build/test_tablero: tests/test_tablero.c src/tablero.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_tablero.c src/tablero.c -o $@ $(LDLIBS)

build/test_mesa: tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_configuracion: tests/test_configuracion.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_configuracion.c src/configuracion.c -o $@ $(LDLIBS)

build/test_cola: tests/test_cola.c src/cola.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_cola.c src/cola.c -o $@ $(LDLIBS)

build/test_log: tests/test_log.c src/log.c src/tablero.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_log.c src/log.c src/tablero.c -o $@ $(LDLIBS)

build/test_servidor: tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

# La prueba incluye servidor.c para inducir pthread_cancel sin agregar APIs
# artificiales de fallos a la aplicacion publica.
build/test_fallo_jugador: tests/test_fallo_jugador.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_fallo_jugador.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

test: torneo $(TEST_BINS) build/torneo_fallo_inyectado
	./build/test_tablero
	./build/test_configuracion
	./build/test_mesa
	./build/test_cola
	./build/test_servidor
	./build/test_fallo_jugador
	./build/test_log
	sh tests/test_fallo_integracion.sh ./build/torneo_fallo_inyectado

build/test_tablero_san: tests/test_tablero.c src/tablero.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_tablero.c src/tablero.c -o $@ $(LDLIBS)

build/test_mesa_san: tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_cola_san: tests/test_cola.c src/cola.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_cola.c src/cola.c -o $@ $(LDLIBS)

build/test_log_san: tests/test_log.c src/log.c src/tablero.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_log.c src/log.c src/tablero.c -o $@ $(LDLIBS)

build/test_log_tsan: tests/test_log.c src/log.c src/tablero.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread tests/test_log.c src/log.c src/tablero.c -o $@ $(LDLIBS)

build/test_servidor_san: tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_fallo_jugador_san: tests/test_fallo_jugador.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_fallo_jugador.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_servidor_tsan: tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_mesa_tsan: tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

test-sanitize: build/test_tablero_san build/test_mesa_san build/test_cola_san build/test_servidor_san build/test_fallo_jugador_san build/test_log_san
	./build/test_tablero_san
	./build/test_mesa_san
	./build/test_cola_san
	./build/test_servidor_san
	./build/test_fallo_jugador_san
	./build/test_log_san

# Las pruebas que fuerzan pthread_cancel se ejecutan sin TSAN: aqui el
# sanitizador puede quedar detenido durante esa operacion. El resto si usa TSAN.
test-tsan: build/test_mesa_tsan build/test_servidor_tsan build/test_log_tsan
	./build/test_mesa_tsan
	./build/test_servidor_tsan
	./build/test_log_tsan

clean:
	rm -f torneo $(TEST_BINS) build/torneo_fallo_inyectado build/test_log_san build/test_log_tsan build/torneo_san build/torneo_tsan build/test_tablero_san build/test_mesa_san build/test_mesa_tsan build/test_cola_san build/test_servidor_san build/test_servidor_tsan build/test_fallo_jugador_san
