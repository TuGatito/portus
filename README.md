# portus

Framework HTTP para [PenguScript](https://github.com/TuGatito/PenguScript), al
estilo de Express.js y Hono.js: rutas con parámetros, middlewares, contexto de
petición, respuestas tipadas con `result`, estáticos con ETag, SSE, WebSocket y
sesiones firmadas.

- **Sin dependencias en C**: `llhttp` va vendorizado en el paquete y el resto es
  un bucle `poll(2)` propio de ~2 000 líneas. `psc add` y listo.
- **Un hilo, sin `async`**: los handlers son `weave`s con nombre y devuelven
  `result of void to HttpError`. El servidor no bloquea con `next()`.
- **Memoria explícita**: `Application` y los cuerpos son tuyos; el contexto
  presta sus cadenas. Todo está documentado en la API (`psc doc`).

Requiere **PenguScript 0.28.9 o superior** (portus arregló varios bugs del
compilador que necesitaba; ver [`scratch/portus-compiler-fixes.md`](scratch/portus-compiler-fixes.md)).

## Instalación

```sh
cd mi-proyecto
psc add https://github.com/TuGatito/portus --trust
```

`--trust` es necesario porque `psc add` ejecuta el script de construcción de la
dependencia. En un proyecto local, `psc add ../portus --trust`.

## Hola mundo

```pengu
## src/main.pengu
import portus

weave hello with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "hola desde portus"
    return calling portus.ok_void

weave main into int:
    var app as portus.Application is calling portus.new_application
    let added as result of void to portus.PatternError is calling portus.get with sigil of app, "/", hello
    let served as result of int to string is calling portus.serve with sigil of app, 3000
    return served or else 1
```

```sh
psc run src/main.pengu     # http://127.0.0.1:3000
```

## Qué trae

| Área | API |
|---|---|
| Rutas | `get`, `post`, `put`, `patch`, `delete`, `head`, `options`, `any`; `:param`, `*wildcard`; `new_group(app, "/api")`; `HEAD`/`OPTIONS`/405 automáticos |
| Contexto | `param`, `query_param`, `header`, `cookie`, `body_text`, `body_json`, `body_form`, `body_multipart`, `set_header`, `set_status`, `send_text`, `send_html`, `send_json`, `send_body_with_type`, `send_status`, `redirect`, `no_content`, `not_modified`, `mark_sent` |
| Middlewares | `use`, `use_at`, `after`; `use_request_id`, `use_security_headers`, `use_cors`, `use_logger`, `use_compress` (gzip), `use_csp`, `use_rate_limit`, `use_rate_limit_at` |
| Errores | `HttpError` (`bad_request`, `not_found`, `conflict`, …), `on_error`, `on_not_found`, `status_of` |
| Estáticos | `mount_static(app, "/static", "public")` con `Content-Type`, `ETag`, `Last-Modified`, `If-None-Match`, `Range` |
| Streaming | `stream_begin`/`stream_write`/`stream_end`, `sse_open`/`sse_send`/`sse_close` |
| WebSocket | `ws_handler`, `ws_text`, `ws_upgrade`; `Conn` con `send_text`, `send_bytes`, `close` |
| Sesiones | `sessions_configure`, `sessions_start`, `sessions_load`, `sessions_get`/`set`/`save`/`destroy` (cookie firmada HMAC-SHA-256, almacén en memoria) |
| Auth | Basic, Bearer, JWT HS256 (`jwt_sign`, `jwt_verify`, `jwt_claims`, `jwt_bearer`) |
| Seguridad | CSRF double-submit (`csrf_protect`), CSP, cabeceras de seguridad, rate limit |
| HTTP | HTTP/1.1 con keep-alive, `chunked`, pipelining, límites de cabecera/cuerpo, timeouts, `400/413/431/500/503` |

Ejemplos completos en [`examples/`](examples/) (los mismos de Express, adaptados:
`hello-world`, `params`, `static-files`, `downloads`, `vhost`, `session`,
`auth-jwt`, `websocket-chat`, …). Se compilan todos con:

```sh
./examples/run.sh
```

## Cómo se usa un `Context`

```pengu
weave create with context as ref to portus.Context into result of void to portus.HttpError:
    let form as portus.Queries is calling context.body_form
    let found as maybe string is calling portus.query_get with form, "name"
    if name as string is found:
        calling context.set_status with 201
        calling context.send_json with "{\"ok\":true}"
        return calling portus.ok_void
    return calling portus.err_void with (calling portus.bad_request with "falta name")
```

Los errores se devuelven; no hay `panic` para entrada del usuario.

## Estructura

```
src/portus.pengu        fachada pública (alias + serve)
src/portus/*.pengu      módulos (router, contexto, middlewares, sesiones, ws…)
include/portus_native.h servidor poll(2) + llhttp, header-only
include/llhttp/         llhttp 9.4.3 vendorizado (MIT, ver THIRD_PARTY.md)
examples/               23 ejemplos ejecutables
tests/                  e2e de HTTP, WebSocket, seguridad y Autobahn
docs/api/               API generada con `psc doc`
scratch/                auditoría, diseño, plan y bugs del compilador
```

## Pruebas

```sh
PSC=/ruta/a/psc ./examples/run.sh     # 23 ejemplos
PSC=/ruta/a/psc bash tests/e2e_hello.sh
PSC=/ruta/a/psc bash tests/e2e_ws.sh
PSC=/ruta/a/psc bash tests/e2e_security.sh
PSC=/ruta/a/psc WSTEST=/ruta/a/wstest ./tests/autobahn.sh   # conformidad WS
```

## Estado

F0–F6 cerrados y etiquetados (`v0.1.0`); F7 (TLS nativo y escalado) en curso.
El diseño, las decisiones aprobadas y las limitaciones conocidas están en
[`scratch/portus.md`](scratch/portus.md). Lo que se aplaza (hilos, TLS nativo,
HTTP/2, wasm…) en [`scratch/portus-deferred.md`](scratch/portus-deferred.md).

Licencia MIT.
