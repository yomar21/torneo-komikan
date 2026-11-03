CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -Wall -Wextra -std=c17 -pthread
LDLIBS = -pthread

SOURCES = src/main.c src/configuracion.c src/tablero.c src/mesa.c src/log.c src/cola.c src/servidor.c
HEADERS = $(wildcard include/*.h)
TEST_BINS = build/test_tablero build/test_mesa build/test_configuracion build/test_cola build/test_servidor build/test_log

.PHONY: all test test-sanitize test-tsan clean

all: torneo

torneo: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDLIBS)

build:
	mkdir -p build

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

test: $(TEST_BINS)
	./build/test_tablero
	./build/test_configuracion
	./build/test_mesa
	./build/test_cola
	./build/test_servidor
	./build/test_log

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

build/test_servidor_tsan: tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread tests/test_servidor.c src/servidor.c src/cola.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

build/test_mesa_tsan: tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -O1 -fsanitize=thread tests/test_mesa.c src/mesa.c src/log.c src/tablero.c src/configuracion.c -o $@ $(LDLIBS)

test-sanitize: build/test_tablero_san build/test_mesa_san build/test_cola_san build/test_servidor_san build/test_log_san
	./build/test_tablero_san
	./build/test_mesa_san
	./build/test_cola_san
	./build/test_servidor_san
	./build/test_log_san

test-tsan: build/test_mesa_tsan build/test_servidor_tsan build/test_log_tsan
	./build/test_mesa_tsan
	./build/test_servidor_tsan
	./build/test_log_tsan

clean:
	rm -f torneo $(TEST_BINS) build/test_log_san build/test_log_tsan build/test_tablero_san build/test_mesa_san build/test_mesa_tsan build/test_cola_san build/test_servidor_san build/test_servidor_tsan
