#!/usr/bin/env bash
# Gate de F5: sesiones firmadas, Basic, JWT HS256, rate limit, multipart,
# CSRF y CSP, todo por HTTP real contra un paquete instalado con `psc add`.
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
base="http://127.0.0.1:$port"

cat >"$tmp/pengu.toml" <<'TOML'
[project]
name = "portus_security"
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

const SECRET as string is "clave-de-prueba"

weave login with context as ref to portus.Context into result of void to portus.HttpError:
    let name as maybe string is calling context.query_param with "user"
    let who as string is name or else "anon"
    var session as portus.Session is calling portus.sessions_start with context
    calling portus.sessions_set with sigil of session, "user", who
    calling portus.sessions_save with context, sigil of session
    calling context.send_text with "dentro"
    return calling portus.ok_void

weave me with context as ref to portus.Context into result of void to portus.HttpError:
    let found as maybe portus.Session is calling portus.sessions_load with context
    if not found.is_present:
        calling context.set_status with 401
        calling context.send_text with "fuera"
        return calling portus.ok_void
    var session as portus.Session is found.value
    let user as maybe string is calling portus.sessions_get with sigil of session, "user"
    let who as string is user or else "?"
    calling context.send_text with "hola {who}"
    return calling portus.ok_void

weave logout with context as ref to portus.Context into result of void to portus.HttpError:
    let found as maybe portus.Session is calling portus.sessions_load with context
    if found.is_present:
        var session as portus.Session is found.value
        calling portus.sessions_destroy with context, sigil of session
    calling context.send_text with "fuera"
    return calling portus.ok_void

weave basic with context as ref to portus.Context into result of void to portus.HttpError:
    let user as maybe string is calling portus.basic_auth with context, "ada", "secreto"
    if not user.is_present:
        calling context.set_status with 401
        calling context.send_text with "no autorizado"
        return calling portus.ok_void
    calling context.send_text with "hola {user.value}"
    return calling portus.ok_void

weave jwt with context as ref to portus.Context into result of void to portus.HttpError:
    let claims as maybe string is calling portus.jwt_bearer with context, SECRET
    if not claims.is_present:
        calling context.set_status with 401
        calling context.send_text with "token malo"
        return calling portus.ok_void
    calling context.send_text with claims.value
    return calling portus.ok_void

weave plain with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "ok"
    return calling portus.ok_void

weave upload with context as ref to portus.Context into result of void to portus.HttpError:
    let parts as maybe list of portus.multipart.Part is calling context.body_multipart
    if not parts.is_present:
        calling context.send_status with 400, "sin partes"
        return calling portus.ok_void
    var all as list of portus.multipart.Part is parts.value
    let found as maybe portus.multipart.Part is calling portus.multipart.part_named with all, "titulo"
    if found.is_present:
        var part as portus.multipart.Part is found.value
        calling context.send_text with "titulo={part.data}"
        calling portus.multipart.free_part with part
    else:
        calling context.send_text with "sin titulo"
    calling portus.multipart.free_parts with all
    return calling portus.ok_void

weave csrf_post with context as ref to portus.Context into result of void to portus.HttpError:
    let ok as bool is calling portus.csrf_ok with context
    if not ok:
        calling context.send_status with 403, "csrf"
        return calling portus.ok_void
    calling context.send_text with "csrf ok"
    return calling portus.ok_void

weave mint with context as ref to portus.Context into result of void to portus.HttpError:
    var token as string is calling portus.jwt_sign with SECRET, "{\"sub\":\"ada\"}"
    defer banish token
    calling context.send_text with token
    return calling portus.ok_void

weave csrf_token with context as ref to portus.Context into result of void to portus.HttpError:
    var token as string is calling portus.issue_csrf_token with context
    defer banish token
    calling context.send_text with token
    return calling portus.ok_void

weave main into int:
    var app as portus.Application is calling portus.new_application
    calling portus.sessions_configure with "secreto-de-sesion", 600000
    calling portus.use_rate_limit_at with sigil of app, "/limited", 2, 60000
    calling portus.use_csp with sigil of app, "default-src 'self'"
    let r_login as result of void to portus.PatternError is calling portus.get with sigil of app, "/login", login
    let r_me as result of void to portus.PatternError is calling portus.get with sigil of app, "/me", me
    let r_logout as result of void to portus.PatternError is calling portus.get with sigil of app, "/logout", logout
    let r_basic as result of void to portus.PatternError is calling portus.get with sigil of app, "/basic", basic
    let r_jwt as result of void to portus.PatternError is calling portus.get with sigil of app, "/jwt", jwt
    let r_mint as result of void to portus.PatternError is calling portus.get with sigil of app, "/mint", mint
    let r_csrf_token as result of void to portus.PatternError is calling portus.get with sigil of app, "/csrf-token", csrf_token
    let r_plain as result of void to portus.PatternError is calling portus.get with sigil of app, "/plain", plain
    let r_limited as result of void to portus.PatternError is calling portus.get with sigil of app, "/limited", plain
    let r_upload as result of void to portus.PatternError is calling portus.post with sigil of app, "/upload", upload
    let r_csrf as result of void to portus.PatternError is calling portus.post with sigil of app, "/csrf", csrf_post
    let served as result of int to string is calling portus.serve with sigil of app, ${port}
    return served or else 1
PENGU

cd "$tmp"
"$PSC" add "$ROOT" --trust >/dev/null 2>&1 || { echo "FAIL: psc add"; exit 1; }
grep -q "path = \"$ROOT\"" pengu.toml || { echo "FAIL: manifiesto"; exit 1; }
"$PSC" build src/main.pengu >/dev/null 2>&1 || { echo "FAIL: build del consumidor"; exit 1; }
server_log="$tmp/server.log"
./build/main >"$server_log" 2>&1 &
server_pid=$!
sleep 1.2

fail() { echo "FAIL: $1"; echo "--- log del servidor ---"; tail -20 "$server_log" 2>/dev/null; exit 1; }
code() { curl -s -o /dev/null -w "%{http_code}" "$@"; }

jar="$tmp/jar.txt"
body="$(curl -s -c "$jar" "$base/login?user=ada")"
[ "$body" = "dentro" ] || fail "login ($body)"
body="$(curl -s -b "$jar" "$base/me")"
[ "$body" = "hola ada" ] || fail "me ($body)"
c="$(code "$base/me")"; [ "$c" = "401" ] || fail "me sin cookie ($c)"
body="$(curl -s -c "$jar" -b "$jar" "$base/logout")"
[ "$body" = "fuera" ] || fail "logout"
c="$(code "$base/me")"; [ "$c" = "401" ] || fail "me tras logout ($c)"

c="$(code "$base/basic")"; [ "$c" = "401" ] || fail "basic sin credenciales ($c)"
body="$(curl -s -u ada:secreto "$base/basic")"
[ "$body" = "hola ada" ] || fail "basic ($body)"
c="$(code "$base/basic" -u ada:mal)"; [ "$c" = "401" ] || fail "basic mal ($c)"

token="$(curl -s "$base/mint")"
[ -n "$token" ] || fail "mint"
body="$(curl -s -H "Authorization: Bearer $token" "$base/jwt")"
echo "$body" | grep -q '"sub"' || fail "jwt ($body)"
c="$(code -H "Authorization: Bearer ${token}x" "$base/jwt")"; [ "$c" = "401" ] || fail "jwt malo ($c)"

c1="$(curl -s -o /dev/null -w "%{http_code}" -H "X-Forwarded-For: 9.9.9.9" "$base/limited")"
c2="$(curl -s -o /dev/null -w "%{http_code}" -H "X-Forwarded-For: 9.9.9.9" "$base/limited")"
c3="$(curl -s -o /dev/null -w "%{http_code}" -H "X-Forwarded-For: 9.9.9.9" "$base/limited")"
[ "$c1$c2" = "200200" ] || fail "rate limit primeros ($c1 $c2)"
[ "$c3" = "429" ] || fail "rate limit tercero ($c3)"

body="$(curl -s -F "titulo=hola" "$base/upload")"
[ "$body" = "titulo=hola" ] || fail "multipart ($body)"

c="$(code -X POST "$base/csrf")"; [ "$c" = "403" ] || fail "csrf sin token ($c)"
csrf="$(curl -s -c "$jar" -b "$jar" "$base/csrf-token")"
body="$(curl -s -b "$jar" -X POST -H "X-CSRF-Token: $csrf" "$base/csrf")"
[ "$body" = "csrf ok" ] || fail "csrf con token ($body)"

curl -s -D - -o /dev/null "$base/plain" | grep -qi "content-security-policy: default-src 'self'" || fail "csp"

echo "OK e2e security (puerto $port)"
