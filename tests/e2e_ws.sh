#!/usr/bin/env bash
# Gate de F3: instala portus con `psc add <repo> --trust`, compila un servidor
# con una ruta WebSocket y habla con él por un socket real (handshake, eco de
# texto, ping/pong y cierre). Sin dependencias de Python más allá de la
# biblioteca estándar.
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

port="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"

cat >"$tmp/pengu.toml" <<'TOML'
[project]
name = "portus_ws_e2e"
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

weave on_message with data as ref to void, conn as ref to portus.WebSocketConn, opcode as int, payload as ref to frozen byte, length as int into void:
    if opcode == 1:
        var body as string is calling portus.ws_text with payload, length
        calling conn.send_text with body
        banish body
    return

weave chat with context as ref to portus.Context into result of void to portus.HttpError:
    let handler as portus.WebSocketHandler is calling portus.ws_handler with on_message, (transmute 0 to ref to void)
    let opened as bool is calling portus.ws_upgrade with context, handler
    if not opened:
        return calling portus.err_void with (calling portus.bad_request with "no es websocket")
    return calling portus.ok_void

weave main into int:
    var app as portus.Application is calling portus.new_application
    let added as result of void to portus.PatternError is calling portus.get with sigil of app, "/ws", chat
    let served as result of int to string is calling portus.serve with sigil of app, $port
    let bound as int is served or:
        calling spark.println with "no se pudo escuchar"
        return 1
    return bound
PENGU

( cd "$tmp" && "$PSC" add "$ROOT" --trust ) >"$tmp/add.log" 2>&1 || {
    echo "FAIL: psc add falló"; tail -20 "$tmp/add.log"; exit 1; }
grep -qF "path = \"$ROOT\"" "$tmp/pengu.toml" || {
    echo "FAIL: el manifiesto no apunta al repo"; exit 1; }

( cd "$tmp" && "$PSC" build src/main.pengu ) >"$tmp/build.log" 2>&1 || {
    echo "FAIL: no compila el consumidor"; tail -30 "$tmp/build.log"; exit 1; }

( cd "$tmp" && exec ./build/main >"$tmp/server.log" 2>&1 ) &
server_pid=$!

for _ in $(seq 1 50); do
    python3 - "$port" <<'PY' >/dev/null 2>&1 && break
import socket, sys
socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=0.2).close()
PY
    sleep 0.1
done

out="$(
    python3 - "$port" <<'PY'
import base64, hashlib, os, socket, struct, sys
port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=3)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall(
    f"GET /ws HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\n"
    f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n".encode()
)
head = b""
while b"\r\n\r\n" not in head:
    head += s.recv(1024)
if b"101 Switching Protocols" not in head:
    print("FAIL handshake")
    raise SystemExit
expected = base64.b64encode(
    hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
).decode()
if ("Sec-WebSocket-Accept: " + expected) not in head.decode():
    print("FAIL accept")
    raise SystemExit

def send(op, payload):
    mask = os.urandom(4)
    n = len(payload)
    if n < 126:
        header = bytes([0x80 | op, 0x80 | n])
    elif n < 65536:
        header = bytes([0x80 | op, 0x80 | 126]) + struct.pack(">H", n)
    else:
        header = bytes([0x80 | op, 0x80 | 127]) + struct.pack(">Q", n)
    s.sendall(header + mask + bytes(c ^ mask[i % 4] for i, c in enumerate(payload)))

def recv():
    h = s.recv(2)
    if len(h) < 2:
        return None
    op, n = h[0] & 0x0F, h[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", s.recv(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", s.recv(8))[0]
    data = b""
    while len(data) < n:
        data += s.recv(n - len(data))
    return op, data

send(1, b"hola")
if recv() != (1, b"hola"):
    print("FAIL eco")
    raise SystemExit
send(1, b"x" * 300)
got = recv()
if got != (1, b"x" * 300):
    print("FAIL eco largo")
    raise SystemExit
send(9, b"ping")
if recv() != (10, b"ping"):
    print("FAIL pong")
    raise SystemExit
send(8, struct.pack(">H", 1000))
got = recv()
if got is None or got[0] != 8:
    print("FAIL cierre")
    raise SystemExit
print("ok")
PY
)"

[ "$out" = "ok" ] || { echo "FAIL: websocket: $out"; cat "$tmp/server.log"; exit 1; }
echo "OK e2e websocket (puerto $port)"
