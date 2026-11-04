# Portus — Plan por fases

> Estado: **propuesta, pendiente de aprobación**. No se escribe código de
> portus hasta que el usuario dé el visto bueno (ver
> [`portus-decisions.md`](portus-decisions.md)).
>
> Estimación: 1 persona + agente, ~14–18 semanas. Cada fase termina en un
> **gate** verificable; si el gate no pasa, no se avanza.

---

## 0. Resumen

| Fase | Tema | Semanas | Gate |
|---|---|---|---|
| F0 | Compilador, esqueleto, transporte C | 2–3 | `psc add … --trust` + hello world real |
| F1 | Núcleo: app, router, contexto, dispatch | 3 | API REST con params y middleware |
| F2 | Cuerpos, cookies, estáticos, CORS, compresión | 3 | 10 ejemplos de Express portados |
| F3 | Streaming, SSE, ficheros | 1–2 | descarga grande sin cargar en RAM |
| F4 | WebSocket | 2–3 | chat + Autobahn (subconjunto) |
| F5 | Sesiones, auth, CSRF, rate limit, multipart | 3 | login + JWT + subida |
| F6 | Ejemplos completos, docs, release 0.1.0 | 2 | `psc doc` limpio, 25 ejemplos |
| F7 | Post-1.0 (TLS, cluster, métricas…) | — | fuera de v1 |

Total: **16–19 semanas** de calendario si todo va bien; 20+ con imprevistos de
compilador. La fase F0 es la de mayor riesgo (depende de bugs ajenos).

---

## F0 — Compilador y esqueleto (2–3 semanas)

**Objetivo:** demostrar que un servidor HTTP real se puede construir y
distribuir con la cadena actual, y eliminar los bloqueos del compilador.

### F0.1 Arreglos de PenguScript (bloqueantes)

| # | Bug | Prioridad | Arreglo |
|---|---|---|---|
| 1 | `psc add` sin `--name` corrompe el manifiesto | **P0** | `src/psc/cmd/deps.pengu` `derive_name`: dejar de banear `s` (o copiar antes) |
| 2 | Llamar un `alias` de tipo función da E0020 | **P1** | `src/psc/sema/infer/core.pengu` ~713: resolver alias/ref hasta `TkFn` |
| 3 | Genérico que reenvía `arena_alloc_one of T` genera `void**` | **P1** | codegen de llamadas genéricas: usar el tipo instanciado del retorno |
| 4 | Llamar un campo-función da E0004 | P2 | ruta de llamada: probar campo antes que método |
| 5 | Variable con nombre de keyword C genera C inválido | P2 | `safe_c_name` en el sitio de llamada |

Estrategia: **rama `fix/portus-blockers` en el checkout de PenguScript**, PRs
separados por bug, tests de regresión cada uno (`psc test`). Mientras el PR no
entre, portus fija esa rama en `pengu.toml`? No: `psc add` de portus no puede
depender de un compilador parcheado. Por eso:

- Bugs 1 y 2 → **parche local + PR upstream**; el código de portus evita el bug
  2 (los handlers se declaran con el tipo inline, no con alias) hasta que entre.
- Bug 3 → se evita por diseño (configs con tipo concreto).
- Bugs 4 y 5 → se evitan (locales temporales, nombres sin keyword).

Con esto, portus compila **con el 0.28.0 sin parchear** salvo el bug 1, que
solo afecta al comando de instalación y tiene workaround (`--name`). El README
documenta el workaround hasta que salga 0.28.1.

### F0.2 Distribución

- `pengu.toml` con `output = "static"`, `src_dir = "src"`,
  `include_dir = "include"`.
- Layout `src/portus.pengu` + `src/portus/*` + `include/*.h` (validado en
  `/tmp/demo`).
- Vendorizar llhttp en `include/llhttp/` con `LICENSE`, `SOURCE`, versión.
- Amalgamar los `.c` de llhttp en un solo `llhttp_amalgam.c` incluido desde
  `portus_native.h` (una sola TU → `psc add` funciona sin `-l`).
- `include/portus_native.h` header-only.
- Prueba de instalación: proyecto nuevo + `psc add <ruta> --name portus
  --trust` + hello world.

### F0.3 Transporte mínimo

- `portus_server_new/run/stop/free/port`.
- `poll(2)` + accept + keep-alive + timeouts.
- llhttp: parseo incremental, `Content-Length` y `chunked`.
- `portus_request_respond` con serialización de cabeceras y cuerpo.
- Errores: 400 sintaxis, 413 cuerpo, 431 cabeceras, 505 versión.
- Callback a Pengu con `userdata = ref to void`.

### Gate F0

1. `psc add … --trust` en un proyecto limpio, sin pasos manuales.
2. `curl -v localhost:3000/` responde con cabeceras correctas y cierra limpio.
3. 100 peticiones keep-alive seguidas sin fugas (ASan).
4. `psc test` del binding pasa.

**Riesgo:** llhttp en un solo TU. Mitigación: si el amalgamado da problemas,
tres `.c` en `c/` (el manifiesto compila `c_dir`; verificar que un dep pueda
traer `c/` — si no, header-only con `#include` de los `.c` desde un `.h`).

---

## F1 — Núcleo (3 semanas)

**Objetivo:** la API que el usuario toca.

- `config`, `status`, `error`.
- `header` (case-insensitive), `request`, `response`.
- `handler` (thin/thick), `layer`, `route` (compile/match/score).
- `context` con accesores y `send_*`.
- `application` con registro, hooks, estado, `listen`.
- `router` + `mount`.
- `dispatcher` con la cadena de §6 del diseño, 404/405/OPTIONS/HEAD.
- `server` (run/stop).
- `test_client` (en proceso).
- Middleware mínimo: `logger`, `request_id`.

### Gate F1

- `examples/hello-world`, `examples/params`, `examples/multi-router`,
  `examples/route-middleware`, `examples/error` funcionando.
- Tests: 60+ unitarios (matching, score, headers, cookies, dispatch).
- Un handler que devuelve `err_of …` produce el status correcto y `on_error`
  se ejecuta antes de los finishers.

---

## F2 — Cuerpos y HTTP útil (3 semanas)

- `body`: JSON (con `std.json`), `application/x-www-form-urlencoded`.
- `cookie`: parseo, `Set-Cookie`, firma HMAC.
- `mime` (tabla de ~60 extensiones).
- `static_file`: raíz segura (sin `..`), index, ETag, `Last-Modified`,
  `Range`, `If-None-Match`, `If-Modified-Since`, 304.
- `compress`: gzip/deflate con `std.compressus`, `Vary: Accept-Encoding`,
  umbral, no comprimir ya comprimido ni `HEAD`/`204`/`304`.
- Middlewares: `cors`, `security_headers`, `request_id` (ya), `logger`
  completo, `timeout`.
- `send_file`, `redirect`, `no_content`, `not_modified`.

### Gate F2

- 10 ejemplos de Express portados (`static-files`, `cookies`,
  `content-negotiation`, `error-pages`, `route-map`, `route-separation`,
  `resource`, `web-service`, `downloads`, `vhost`).
- `curl --compressed` devuelve gzip válido; `curl -r 0-99` devuelve 206.
- Fuga cero en 1000 peticiones (ASan).

---

## F3 — Streaming y SSE (1–2 semanas)

- `stream_open` → chunked, escritura por trozos, backpressure básica (cola de
  salida del shim, límite de 1 MiB; si se supera, se bloquea el handler y se
  vuelca).
- `sse_open`, `send`, `comment`, `close`; heartbeats.
- `send_file` con `sendfile(2)` si está disponible, si no, lectura por bloques.
- Timeout de petición aplicado por el shim.

### Gate F3

- Ejemplo `/stream` que emite 1 MB en trozos sin pico de memoria (RSS estable).
- `curl -N /eventos` recibe eventos cada segundo durante 30 s.

---

## F4 — WebSocket (2–3 semanas)

- Handshake RFC 6455 (`Sec-WebSocket-Accept` con SHA-1 + base64; SHA-1 en C).
- Framing: texto, binario, ping/pong, close, fragmentación, masking obligatorio
  del cliente, límite de tamaño.
- `websocket.get`, `Connection`, rooms, broadcast, `connection_count`.
- `concept Handler` + `bind`.
- Cierre limpio, códigos, timeout de ping.

### Gate F4

- Ejemplo de chat con 10 clientes simultáneos.
- Autobahn `fuzzingclient` sobre el subconjunto: 1.x, 2.x, 5.x, 6.x, 7.x
  (sin permessage-deflate). Meta: 0 "failed".

---

## F5 — Seguridad y sesiones (3 semanas)

- `session`: cookie firmada + store en memoria con expiración; API `Session`.
- `auth`: Basic, Bearer, JWT HS256 (validación de firma, `exp`, `iat`, `nbf`;
  sin RS256 en v1).
- `csrf`: double-submit cookie, token por sesión, comprobación en métodos
  mutantes.
- `rate_limit`: token bucket por clave (IP o sesión), cabeceras
  `RateLimit-*`, 429.
- `multipart`: parser `multipart/form-data`, límites, `Part`.
- `security_headers`: CSP configurable.

### Gate F5

- Ejemplo de login con sesión + ejemplo `web-service` con JWT.
- Test de subida de fichero de 5 MB.
- Test de rate limit: 101 peticiones → 429.

---

## F6 — Ejemplos, documentación y release (2 semanas)

- Portar **todos** los ejemplos posibles de Express (ver
  [`portus-examples.md`](portus-examples.md)): 24 ejemplos + 3 propios (chat WS,
  SSE, REST completa).
- `examples/README.md` con el mapeo Express → portus.
- `psc doc` limpio; `docs/api/` generado y commiteado.
- README del repo: instalación, quickstart, límites, benchmarks.
- `CHANGELOG.md`; versión `0.1.0`.
- Benchmarks `wrk`/`bombardier` vs Express y Hono (documentados, sin promesas).
- CI: `psc check`, `psc test`, `psc fmt --check`, `psc doc`, ASan, Linux +
  macOS.

### Gate F6

- Un usuario externo sigue el README y tiene una app corriendo en <5 minutos.
- Todos los ejemplos pasan en CI.
- Sin warnings de `psc check` ni de `psc fmt --check`.

---

## F7 — Post-1.0 (no comprometido)

- TLS con mbedTLS (D8).
- Cluster multi-proceso (`std.filum` + fork; requiere C).
- Métricas Prometheus, `/health`, `/ready`.
- `permessage-deflate`, `wss` server (con TLS).
- HTTP/2 (solo si aparece una razón fuerte).
- Validación de esquemas (tipo Zod) — módulo satélite, no en el núcleo.

---

## Criterios transversales (aplican a toda fase)

1. **Cada módulo público con `##`**; `psc doc` corre en CI.
2. **Cada fase añade tests**: unitarios + integración con `TestClient` +
   e2e con `curl` en script.
3. **ASan/UBSan en CI** sobre el C generado; valgrind en los tests de
   integración lentos.
4. **Cero dependencias nuevas** sin pasar por la tabla de licencias.
5. **Sin `panic` para entrada del usuario.**
6. **Sin cambios de API sin entrada en `portus-decisions.md`.**

---

## Riesgos

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| Bugs del compilador bloquean una firma | Alta | Medio | Spikes en F0; rediseño localizado; PR upstream |
| llhttp no amalgama bien | Media | Medio | Incluir los `.c` desde el `.h` (una TU) |
| `psc add` no trae C del dep | Media | Alto | Header-only validado en `/tmp/demo`; si falla, `c/` + verificación |
| Rendimiento pobre por copias | Media | Bajo | Copiar en v1; medir en F2; zero-copy en 0.2 |
| Autobahn falla en fragmentación | Media | Bajo | Subconjunto documentado; no bloquear release |
| El usuario cambia de idea en mitad | Baja | Alto | Docs vivos; decisiones fechadas |

---

## Qué NO se hace en v1 (y por qué)

- Templates: no hay motor; el usuario construye strings.
- HTTP/2: el proxy lo termina.
- TLS: ídem; el binding de mbedTLS es un proyecto aparte.
- Async: el lenguaje no lo tiene; el modelo es síncrono.
- Multi-worker: F7.
- `wasm`: sin sockets.
