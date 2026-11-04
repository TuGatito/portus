#!/usr/bin/env bash
# Gate de F0: instala portus como lo haría un usuario (`psc add <repo> --trust`
# sin `--name`), compila un hello world, lo arranca y comprueba la respuesta por
# HTTP real. No usa nada del árbol de portus salvo el propio repositorio como
# fuente del paquete.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PSC="${PSC:-$(command -v psc || true)}"
if [ -z "$PSC" ]; then
    echo "FAIL: no se encontró psc (define PSC=/ruta/a/psc)"
    exit 1
fi
export PENGU_HOME="${PENGU_HOME:-$(cd "$(dirname "$PSC")/.." && pwd)}"

tmp="$(mktemp -d)"
server_pid=""
cleanup() {
    [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null
    rm -rf "$tmp"
}
trap cleanup EXIT

# Puerto libre elegido por el sistema (el servidor aún no anuncia el puerto
# cuando se pide 0: `serve` bloquea y no devuelve hasta que se detiene).
port="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"

cat >"$tmp/pengu.toml" <<'TOML'
[project]
name = "portus_e2e"
version = "0.1.0"
entry = "src/main.pengu"
output = "exe"

[build]
src_dir = "src"
include_dir = "include"
cc = "gcc"
cflags = ["-Wall"]
TOML
mkdir -p "$tmp/src"
cat >"$tmp/src/main.pengu" <<PENGU
import std.spark
import portus

weave hello with request as ref to portus.Request, response as ref to portus.Response into void:
    calling response.set_header with "Content-Type", "text/plain; charset=utf-8"
    calling response.write_text with "hola desde portus"
    calling response.mark_sent

weave main into int:
    var options as portus.ServeOptions is calling portus.default_options
    set options.port is $port
    let served as result of int to string is calling portus.serve with options, hello
    let bound as int is served or:
        calling spark.println with "no se pudo escuchar"
        return 1
    return bound
PENGU

( cd "$tmp" && "$PSC" add "$ROOT" --trust ) >"$tmp/add.log" 2>&1 || {
    echo "FAIL: psc add falló"; tail -20 "$tmp/add.log"; exit 1; }
grep -qF "path = \"$ROOT\"" "$tmp/pengu.toml" || {
    echo "FAIL: el manifiesto no apunta al repo"; grep -n portus "$tmp/pengu.toml"; exit 1; }

( cd "$tmp" && "$PSC" build src/main.pengu ) >"$tmp/build.log" 2>&1 || {
    echo "FAIL: no compila el consumidor"; tail -30 "$tmp/build.log"; exit 1; }

( cd "$tmp" && exec ./build/main >"$tmp/server.log" 2>&1 ) &
server_pid=$!

for _ in $(seq 1 50); do
    curl -sf --max-time 1 -o /dev/null "http://127.0.0.1:$port/" && break
    sleep 0.1
done

body="$(curl -sf --max-time 3 "http://127.0.0.1:$port/")" || {
    echo "FAIL: curl falló en 127.0.0.1:$port"; cat "$tmp/server.log"; exit 1; }
[ "$body" = "hola desde portus" ] || { echo "FAIL: cuerpo '$body'"; exit 1; }

code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 "http://127.0.0.1:$port/x")"
[ "$code" = "200" ] || { echo "FAIL: código $code (sin router, todo va al hello)"; exit 1; }

# Keep-alive: dos peticiones por la misma conexión.
resp="$(
    python3 - "$port" <<'PY'
import socket, sys
port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=2)
s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
s.recv(65536)
s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
print(b"hola desde portus" in s.recv(65536))
s.close()
PY
)"
[ "$resp" = "True" ] || { echo "FAIL: la segunda petición keep-alive no respondió"; exit 1; }

echo "OK e2e hello (puerto $port)"
