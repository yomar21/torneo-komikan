#!/bin/sh
# Prueba de punta a punta: usa main + servidor + juez, sin mockear el flujo.
set -eu

binario=${1:-./build/torneo_fallo_inyectado}
salida=$(mktemp)
trap 'rm -f "$salida"' EXIT HUP INT TERM

if command -v timeout >/dev/null 2>&1; then
    if ! timeout 20s "$binario" 2 1 tests/config_rapida.txt > "$salida" 2>&1; then
        cat "$salida" >&2
        echo 'FALLA: el torneo inyectado no termino correctamente' >&2
        exit 1
    fi
else
    if ! "$binario" 2 1 tests/config_rapida.txt > "$salida" 2>&1; then
        cat "$salida" >&2
        echo 'FALLA: el torneo inyectado no termino correctamente' >&2
        exit 1
    fi
fi

comprobar() {
    if ! grep -Fq "$1" "$salida"; then
        cat "$salida" >&2
        echo "FALLA: falta la evidencia: $1" >&2
        exit 1
    fi
}

comprobar 'Fallo inesperado del hilo del jugador 1 durante la partida 1'
comprobar 'Fin de la partida 1 (mesa 1): fallo inesperado de jugador:'
comprobar 'Fallos inesperados de hilos: 1 durante partidas, 0 antes de jugar.'
comprobar 'por abandono: 1'
comprobar 'Threads de jugador recogidos: 2/2.'
comprobar 'Torneo finalizado'

# Abandono voluntario del ejecutable normal: se distingue de una falla de hilo.
config_abandono=$(mktemp)
trap 'rm -f "$salida" "$config_abandono"' EXIT HUP INT TERM
cat > "$config_abandono" <<'EOF'
retardo_min_ms=50
retardo_max_ms=100
semilla=42
prob_abandono=100
abandono_max_ms=1
EOF
if command -v timeout >/dev/null 2>&1; then
    if ! timeout 20s ./torneo 2 1 "$config_abandono" > "$salida" 2>&1; then
        cat "$salida" >&2
        echo 'FALLA: abandono del torneo real no termino correctamente' >&2
        exit 1
    fi
else
    if ! ./torneo 2 1 "$config_abandono" > "$salida" 2>&1; then
        cat "$salida" >&2
        echo 'FALLA: abandono del torneo real no termino correctamente' >&2
        exit 1
    fi
fi
comprobar 'abandona la partida:'
comprobar 'por abandono: 1'
comprobar 'Fallos inesperados de hilos: 0 durante partidas, 0 antes de jugar.'
comprobar 'Threads de jugador recogidos: 2/2.'
comprobar 'Torneo finalizado'
echo 'Integracion de fallos y abandono: juez adjudico resultados, resumen correcto y hilos recogidos.'
