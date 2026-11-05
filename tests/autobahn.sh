#!/usr/bin/env bash
# Gate de interoperabilidad WebSocket: ejecuta la suite de Autobahn
# (`wstest -m fuzzingclient`) contra el servidor de `tests/autobahn/server`.
#
# Uso:
#   ./tests/autobahn.sh                 # casos funcionales (1–7 y 10)
#   CASES="9.*" ./tests/autobahn.sh     # solo rendimiento (lento, MBs)
#   WSTEST=/ruta/a/wstest ./tests/autobahn.sh
#
# Variables:
#   PSC      compilador (por defecto `psc`)
#   WSTEST   binario de wstest (por defecto `wstest` o /tmp/ab/env/bin/wstest)
#   CASES    lista separada por espacios de patrones de caso
#   OUT      directorio de informes (por defecto, temporal; se borra)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVER_DIR="$ROOT/tests/autobahn/server"
PSC="${PSC:-$(command -v psc || true)}"
WSTEST="${WSTEST:-$(command -v wstest || true)}"
if [ -z "$WSTEST" ] && [ -x /tmp/ab/env/bin/wstest ]; then
    WSTEST=/tmp/ab/env/bin/wstest
fi
CASES="${CASES:-1.* 2.* 3.* 4.* 5.* 6.* 7.* 10.*}"

if [ -z "$PSC" ]; then
    echo "FAIL: no se encontró psc (define PSC=/ruta/a/psc)"
    exit 1
fi
if [ -z "$WSTEST" ]; then
    echo "FAIL: no se encontró wstest (instala autobahntestsuite o define WSTEST=)"
    exit 1
fi
export PENGU_HOME="${PENGU_HOME:-$(cd "$(dirname "$PSC")/.." && pwd)}"

tmp="${OUT:-$(mktemp -d)}"
mkdir -p "$tmp"
server_pid=""
cleanup() {
    [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null
    [ -n "${OUT:-}" ] || rm -rf "$tmp"
}
trap cleanup EXIT

echo "==> compilando el servidor de eco"
# La dependencia es una copia: si el header C del repo es más nuevo, hay que
# reinstalarla o se compilaría contra el shim viejo.
if [ -d "$SERVER_DIR/lib/portus" ] && [ "$ROOT/include/portus_native.h" -nt "$SERVER_DIR/lib/portus/include/portus_native.h" ]; then
    rm -rf "$SERVER_DIR/lib" "$SERVER_DIR/pengu.lock"
fi
if [ ! -d "$SERVER_DIR/lib/portus" ]; then
    (cd "$SERVER_DIR" && "$PSC" add "$ROOT" --trust >/dev/null) || {
        echo "FAIL: psc add de portus"; exit 1; }
fi
(cd "$SERVER_DIR" && "$PSC" build src/main.pengu >/dev/null) || {
    echo "FAIL: no compila tests/autobahn/server"; exit 1; }

port="$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')"
# El servidor escucha en el 39091 fijo del proyecto: se parchea el puerto en
# el manifiesto para poder usar uno libre sin tocar el código.
sed -i "s/^const PORT as int is .*/const PORT as int is $port/" "$SERVER_DIR/src/main.pengu"
(cd "$SERVER_DIR" && "$PSC" build src/main.pengu >/dev/null) || {
    echo "FAIL: no compila el servidor con el puerto libre"; exit 1; }

"$SERVER_DIR/build/main" >"$tmp/server.log" 2>&1 &
server_pid=$!
for _ in $(seq 1 50); do
    if python3 - "$port" <<'PY'
import socket, sys
s = socket.socket()
s.settimeout(0.2)
sys.exit(0 if s.connect_ex(("127.0.0.1", int(sys.argv[1]))) == 0 else 1)
PY
    then break; fi
    sleep 0.1
done

python3 - "$tmp/config.json" "$port" "$CASES" <<'PY'
import json, sys
out, port, cases = sys.argv[1], int(sys.argv[2]), sys.argv[3].split()
cfg = {
    "outdir": "./reports/clients",
    "servers": [{"url": "ws://127.0.0.1:%d/echo" % port, "agent": "portus"}],
    "cases": cases,
    "exclude-cases": [],
    "exclude-agent-cases": {},
}
json.dump(cfg, open(out, "w"), indent=2)
PY

echo "==> Autobahn: $CASES"
(cd "$tmp" && "$WSTEST" -m fuzzingclient -s config.json >wstest.log 2>&1)
wstest_rc=$?

python3 - "$tmp/reports/clients/index.json" <<'PY'
import json, sys
path = sys.argv[1]
try:
    index = json.load(open(path))
except Exception as exc:
    print("FAIL: sin informe de Autobahn (%s)" % exc)
    sys.exit(1)
doc = index.get("portus", index)
cases = doc.get("cases", doc)
if not cases:
    print("FAIL: el informe no trae casos")
    sys.exit(1)


def key(name):
    return [int(p) if p.isdigit() else p for p in name.split(".")]


failed = []
groups = {}
for name in cases:
    info = cases[name]
    behavior = info.get("behavior")
    close = info.get("behaviorClose")
    bad = behavior in ("FAILED", "WRONG CODE") or close in ("FAILED", "WRONG CODE")
    lax = (not bad) and (behavior == "NON-STRICT" or close == "NON-STRICT")
    group = name.split(".")[0]
    total, fails = groups.get(group, (0, 0))
    groups[group] = (total + 1, fails + (1 if bad else 0))
    if bad:
        failed.append((name, behavior, close, info.get("remoteCloseCode")))

lax_count = sum(1 for name in cases if (
    cases[name].get("behavior") == "NON-STRICT" or cases[name].get("behaviorClose") == "NON-STRICT"))
print("grupo  casos  fallos")
for group in sorted(groups):
    total, fails = groups[group]
    print("%-5s  %5d  %5d" % (group, total, fails))
print("total: %d casos, %d fallos, %d non-strict" % (len(cases), len(failed), lax_count))
for name, behavior, close, code in sorted(failed, key=lambda f: key(f[0])):
    print("  FAIL %-8s behavior=%s close=%s code=%s" % (name, behavior, close, code))
sys.exit(1 if failed else 0)
PY
report_rc=$?

if [ "$wstest_rc" -ne 0 ]; then
    echo "FAIL: wstest terminó con $wstest_rc; log:"
    tail -20 "$tmp/wstest.log"
    exit 1
fi
[ "$report_rc" -eq 0 ] || { echo "==> informe en $tmp/reports/clients/index.html"; exit 1; }
echo "OK autobahn ($CASES)"
