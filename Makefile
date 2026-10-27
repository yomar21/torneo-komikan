CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -Wall -Wextra -std=c17 -pthread

SOURCES = src/main.c src/configuracion.c src/tablero.c
HEADERS = include/configuracion.h include/tablero.h

.PHONY: all clean

all: torneo

torneo: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o torneo

clean:
	rm -f torneo