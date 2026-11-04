# Portus — Estrategia de pruebas

> Portus se prueba en cuatro niveles. Ninguno sustituye a otro. Todo corre en
> CI en Linux (gcc y clang) y macOS (clang).

---

## 1. Pirámide

```
        ┌──────────────────────┐
        │ e2e socket (curl)    │  pocos, lentos, reales
        ├──────────────────────┤
        │ conformidad (Autobahn, │  solo WS y HTTP raro
        │  h2spec si algún día) │
        ├──────────────────────┤
        │ integración           │  TestClient en proceso
        │ (TestClient)          │  muchos
        ├──────────────────────┤
        │ unitarios             │  `test "…":` en cada módulo
        └──────────────────────┘
```

Meta orientativa: **250 unitarios, 80 de integración, 25 e2e** al cerrar v1.

---

## 2. Unitarios (`psc test`)

Bloques `test "nombre":` en el propio módulo. Los que más importan:

| Módulo | Casos |
|---|---|
| `route` | literal, `:param`, `:param` vacío, `*` vacío, `*` con barras, `*ruta`, multi-param, prefijo, trailing slash, case, `..`/`//`, score |
| `header` | get case-insensitive, `add` para `Set-Cookie`, `remove`, orden, valores con comas |
| `cookie` | parseo, comillas, valores raros, firma/verificación, firma manipulada, expiración |
| `mime` | 60 extensiones, fallback `application/octet-stream`, charset |
| `body` | JSON válido/inválido, form con `%20`/`+`, límite de tamaño, content-type ausente |
| `error` | cada variante → status, `public_message` en dev/prod |
| `status` | `reason_phrase` para los 40 códigos usados |
| `context` | `send_*` dos veces → `Internal`; `Content-Length` correcto; `HEAD` |
| `application` | `mount` consume el router; `_sealed`; hooks en orden |
| `sse` | formato de evento, `id`, `retry`, comentario |
| `websocket` | aceptación RFC 6455 (vector conocido), frame corto/largo, máscara, close |

Regla: **sin red, sin reloj, sin ficheros** en unitarios. Si hace falta tiempo,
se inyecta (`context.started_ms` se puede fijar en el test).

---

## 3. Integración (`TestClient`)

`TestClient` no abre sockets: construye un `Request`, llama al mismo
`dispatch_request` que usa el servidor y captura `Response`. Esto obliga a que
el dispatcher sea **agnóstico del transporte**:

```pengu
weave dispatch_request with app as ref to Application, request as Request, response as ref to Response into void
```

Casos:

- 404 y 405 con `Allow` correcto (orden alfabético).
- `OPTIONS` automático.
- `HEAD` sin cuerpo pero con `Content-Length` de la GET.
- Cadena: `before` que no envía → sigue; `before` que envía → corta.
- Error en `before` → `on_error` → finishers.
- `on_error` que no envía → 500 por defecto.
- Finishers ven el body final (compresión, ETag).
- Cookies: ida y vuelta con `TestClient`.
- Sesión: login → cookie → siguiente petición autenticada.
- Rate limit: N+1 → 429.
- CORS: preflight `OPTIONS` con `Access-Control-Request-Method`.
- Estáticos: `../` fuera de raíz → 404; `Range` → 206; `If-None-Match` → 304.
- Cuerpos: 1 MiB + 1 → 413.

---

## 4. e2e (socket real)

Scripts `tests/e2e/*.sh`. Patrón:

```sh
# arranca el ejemplo en puerto efímero, espera, prueba, mata
PORT=0 ./build/ejemplo &
PID=$!
sleep 0.2
curl -fsS "http://127.0.0.1:$PORT/" | grep -q "Hola"
kill $PID
```

Puerto efímero: `Config.port = 0`; `Server.port` devuelve el real; el ejemplo lo
imprime en stdout como `port=<n>` para que el script lo lea.

Qué se cubre aquí y no en integración:

- keep-alive (100 peticiones, misma conexión).
- `chunked` real y streaming con `curl -N`.
- pipelining básico (dos peticiones seguidas sin esperar).
- timeouts (conexión que no envía nada → cierre a los `idle_timeout_ms`).
- señales: SIGINT → cierre limpio, exit 0.
- ficheros grandes con `sendfile`.

---

## 5. Conformidad

- **WebSocket:** Autobahn `fuzzingclient` (Docker) contra `/ws`. Se aceptan los
  casos 1.x, 2.x, 5.x, 6.x, 7.x; `permessage-deflate` queda fuera y se documenta
  como `unimplemented` en el reporte.
- **HTTP:** no hay suite oficial ligera; se usan los e2e + una tabla de casos
  raros (cabecera duplicada, `Content-Length` mentiroso, `Transfer-Encoding:
  chunked` + `Content-Length` a la vez → 400, `Expect: 100-continue`).
- **llhttp:** ya trae sus propios tests y fuzzing; no los duplicamos, pero
  corremos su corpus contra nuestro shim (ver §6).

---

## 6. Fuzzing y sanitizers

- **libFuzzer** sobre el shim: entrada = bytes crudos → `llhttp` → callback
  vacío. Busca crashes y desbordamientos. `tools/fuzz/parse_fuzz.c`.
- **ASan + UBSan** en CI para todos los tests de integración:
  `cflags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]` y
  `ldflags` equivalentes, en un perfil `asan`.
- **valgrind** en los e2e lentos: 0 "definitely lost" al cerrar el servidor.
- **Fuga de app**: crear/banear 1000 apps y 10 000 contextos en un test;
  RSS estable.

---

## 7. Benchmarks (informativos, no gates)

- `hello-world` (texto) y `json` con `wrk -t4 -c100 -d10s`.
- Comparación documentada con Express y Hono en la misma máquina, **sin**
  prometer cifras. Se guardan en `bench/RESULTS.md` con fecha y CPU.
- Perfilado con `perf record` si el número sorprende.

---

## 8. CI

```yaml
# .github/workflows/ci.yml (esquema)
jobs:
  test:
    matrix: [ubuntu-gcc, ubuntu-clang, macos-clang]
    steps:
      - build psc 0.28.0
      - psc check
      - psc test --json
      - psc fmt --check
      - psc doc
      - tests/e2e/*.sh
  asan:
    steps: [ build con perfil asan, tests de integración ]
  fuzz:
    steps: [ 60s de libFuzzer, artefactos si falla ]
```

- Pin de versión del compilador en `README.md` y en CI.
- `psc doctor` en el primer paso.
- Un job semanal con la `main` de PenguScript para detectar regresiones.

---

## 9. Datos de prueba

`tests/fixtures/`: JSON válidos/inválidos, formularios, un PNG de 1 KiB, un
fichero de 5 MiB generado en el script (no commiteado), certificados de prueba
si algún día hay TLS.

Nada de fixtures binarios grandes en git.
