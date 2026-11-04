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

weave hello with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "hola desde portus"
    return calling portus.ok_void

weave user with context as ref to portus.Context into result of void to portus.HttpError:
    let name as maybe string is calling context.param with "name"
    let who as string is name or else "anon"
    calling context.send_text with "user {who}"
    return calling portus.ok_void

weave boom with context as ref to portus.Context into result of void to portus.HttpError:
    return calling portus.err_void with (calling portus.bad_request with "boom")

weave only_post with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "posted"
    return calling portus.ok_void

weave tag with data as ref to void, context as ref to portus.Context into result of void to portus.HttpError:
    calling context.set_header with "X-Portus", "1"
    return calling portus.ok_void

weave missing with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_status with 404, "sin ruta"
    return calling portus.ok_void

weave main into int:
    var app as portus.Application is calling portus.new_application
    let added as result of void to portus.PatternError is calling portus.get with sigil of app, "/", hello
    let named as result of void to portus.PatternError is calling portus.get with sigil of app, "/users/:name", user
    let broken as result of void to portus.PatternError is calling portus.get with sigil of app, "/boom", boom
    let posted as result of void to portus.PatternError is calling portus.post with sigil of app, "/only-post", only_post
    var api as portus.Group is calling portus.new_group with sigil of app, "/api"
    let grouped as result of void to portus.PatternError is calling api.get with "/ping", hello
    var mw as portus.Middleware is calling portus.middleware with tag
    calling portus.use with sigil of app, mw
    calling portus.on_not_found with sigil of app, missing
    let served as result of int to string is calling portus.serve with sigil of app, $port
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

user_body="$(curl -sf --max-time 3 "http://127.0.0.1:$port/users/ada")" || {
    echo "FAIL: curl falló para /users/ada"; cat "$tmp/server.log"; exit 1; }
[ "$user_body" = "user ada" ] || { echo "FAIL: parámetro de ruta '$user_body'"; exit 1; }

code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 "http://127.0.0.1:$port/nope")"
[ "$code" = "404" ] || { echo "FAIL: se esperaba 404, llegó $code"; exit 1; }
nf="$(curl -s --max-time 3 "http://127.0.0.1:$port/nope")"
[ "$nf" = "sin ruta" ] || { echo "FAIL: on_not_found devolvió '$nf'"; exit 1; }

hdr="$(curl -s -D - -o /dev/null --max-time 3 "http://127.0.0.1:$port/")"
echo "$hdr" | grep -qi "^X-Portus: 1" || { echo "FAIL: falta la cabecera del middleware"; exit 1; }

grouped_body="$(curl -s --max-time 3 "http://127.0.0.1:$port/api/ping")"
[ "$grouped_body" = "hola desde portus" ] || { echo "FAIL: grupo /api -> '$grouped_body'"; exit 1; }
boom_code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 "http://127.0.0.1:$port/boom")"
[ "$boom_code" = "400" ] || { echo "FAIL: err_void debía dar 400, dio $boom_code"; exit 1; }

head_headers="$(curl -s -I --max-time 3 "http://127.0.0.1:$port/")"
echo "$head_headers" | grep -qi "^content-length: " || { echo "FAIL: HEAD sin Content-Length"; exit 1; }
opt_headers="$(curl -s -D - -o /dev/null --max-time 3 -X OPTIONS "http://127.0.0.1:$port/")"
echo "$opt_headers" | head -1 | grep -q "204" || { echo "FAIL: OPTIONS debía dar 204"; exit 1; }
echo "$opt_headers" | grep -qi "^allow: " || { echo "FAIL: OPTIONS sin Allow"; exit 1; }
allow="$(curl -s -D - -o /dev/null --max-time 3 "http://127.0.0.1:$port/only-post")"
echo "$allow" | grep -qi "^Allow: POST" || { echo "FAIL: 405 sin Allow"; exit 1; }
[ "$(echo "$allow" | head -1 | grep -c ' 405 ')" = "1" ] || { echo "FAIL: se esperaba 405"; exit 1; }

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
