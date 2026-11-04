#!/bin/sh
# Comprueba que Ctrl+C (SIGINT) cierra el programa de forma ordenada:
#   uso: tests/test_sigint.sh ./torneo
# Para cada tiempo de espera lanza un torneo grande, lo interrumpe y verifica
# el codigo de salida, el mensaje final, que no queda ningun proceso vivo, que
# solo hay descriptores 0, 1 y 2 abiertos y que no aparecen recursos huerfanos
# en /dev/shm ni en IPC de System V. Si el binario esta compilado con
# sanitizers, una fuga o un error hace que el programa salga con codigo distinto de 0.

BIN=${1:-./torneo}
SALIDA=$(mktemp /tmp/torneo_sigint_XXXXXX)
fallos=0

fallar() {
    echo "  FALLA: $1"
    fallos=$((fallos + 1))
}

recursos() {
    { ls -A /dev/shm 2>/dev/null; ipcs -m -s -q 2>/dev/null | grep -E '^0x|^[0-9]'; } | sort | md5sum
}

esperar_fin() {   # espera hasta 10 s a que termine el proceso $1
    i=0
    while kill -0 "$1" 2>/dev/null && [ $i -lt 100 ]; do
        sleep 0.1
        i=$((i + 1))
    done
    ! kill -0 "$1" 2>/dev/null
}

antes=$(recursos)

for espera in 0.2 1 2; do
    echo "Ctrl+C a los $espera s (N=200, M=3, retardos por defecto)"
    "$BIN" 200 3 tests/config_lenta.txt > "$SALIDA" 2>&1 &
    pid=$!
    sleep "$espera"

    fds=$(ls /proc/$pid/fd 2>/dev/null | wc -l)
    if [ "$fds" -gt 3 ]; then
        fallar "hay $fds descriptores abiertos y solo deberian ser 0, 1 y 2"
    fi

    kill -INT "$pid"
    if ! esperar_fin "$pid"; then
        fallar "el programa no termino en 10 s despues de Ctrl+C"
        kill -9 "$pid" 2>/dev/null
        continue
    fi
    wait "$pid"
    codigo=$?
    [ "$codigo" -eq 0 ] || fallar "codigo de salida $codigo (se esperaba 0)"
    grep -q "Servidor detenido por SIGINT" "$SALIDA" || fallar "falta el mensaje 'Servidor detenido por SIGINT'"
    grep -q "Resumen del torneo" "$SALIDA" || fallar "falta el resumen final"
    if grep -qE "ERROR: (Address|Leak|Thread)Sanitizer|WARNING: ThreadSanitizer|runtime error" "$SALIDA"; then
        fallar "un sanitizer detecto un problema"
        grep -E "ERROR|WARNING|runtime error" "$SALIDA" | head -5
    fi
done

echo "Dos Ctrl+C seguidos"
"$BIN" 200 3 tests/config_lenta.txt > "$SALIDA" 2>&1 &
pid=$!
sleep 0.5
kill -INT "$pid"
kill -INT "$pid" 2>/dev/null
esperar_fin "$pid" || { fallar "se colgo con dos Ctrl+C"; kill -9 "$pid" 2>/dev/null; }
wait "$pid"
[ $? -eq 0 ] || fallar "codigo de salida distinto de 0 con dos Ctrl+C"

echo "Ejecucion normal, sin Ctrl+C"
"$BIN" 20 3 tests/config_rapida.txt > "$SALIDA" 2>&1 &
pid=$!
esperar_fin "$pid" || { fallar "la ejecucion normal se colgo"; kill -9 "$pid" 2>/dev/null; }
wait "$pid"
[ $? -eq 0 ] || fallar "codigo de salida distinto de 0 en la ejecucion normal"
grep -q "Torneo finalizado" "$SALIDA" || fallar "falta el mensaje 'Torneo finalizado'"

despues=$(recursos)
[ "$antes" = "$despues" ] || fallar "quedaron recursos huerfanos en /dev/shm o en IPC"
if pgrep -x "$(basename "$BIN")" >/dev/null 2>&1; then
    fallar "queda un proceso $(basename "$BIN") vivo"
fi

rm -f "$SALIDA"
if [ "$fallos" -eq 0 ]; then
    echo "SIGINT: todo correcto para $BIN."
    exit 0
fi
echo "SIGINT: $fallos comprobaciones fallaron para $BIN."
exit 1
