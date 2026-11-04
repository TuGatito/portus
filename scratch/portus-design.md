# Portus — Diseño y patrones (v1)

> Este documento fija **cómo se construye y cómo se usa** portus. Es la
> referencia para implementar y para revisar PRs. Si algo aquí no se cumple, el
> PR está mal.
>
> Decisiones abiertas: [`portus-decisions.md`](portus-decisions.md). Firmas
> concretas: [`portus-api.md`](portus-api.md). Reglas de código:
> [`portus-rules.md`](portus-rules.md).

---

## 1. Objetivo y no-objetivos

**Objetivo.** Un framework HTTP para PenguScript, instalable con `psc add`, que
cubra el 90 % de lo que se hace con Express/Hono: rutas con parámetros, verbos,
middleware, contexto de petición/respuesta, cuerpos JSON/form/multipart,
cookies, estáticos, compresión, sesiones, auth, CORS, rate limit, SSE y
WebSocket. API idiomática Pengu: métodos, `result`, ownership explícito, `##`
en todo.

**No-objetivos v1.**

- Motores de plantillas (`render`, EJS, Pug). Se envía HTML ya construido.
- HTTP/2, HTTP/3. Los termina un proxy.
- TLS nativo. Lo termina un proxy (decisión D8).
- Async/await: no existe en el lenguaje. Ver §3.
- Multi-worker en proceso (F6, opcional).
- Compatibilidad wasm (sin sockets).

---

## 2. Capas

```
┌──────────────────────────────────────────────────────────────┐
│  Tu app (main.pengu)                                         │
│    var app as portus.Application is ...                      │
│    calling app.get with "/hola", hola                        │
│    calling app.listen ... / server.run                       │
└───────────────────────────┬──────────────────────────────────┘
                            │ API pública (métodos, result)
┌───────────────────────────▼──────────────────────────────────┐
│  Núcleo Pengu                                                │
│    application · router · dispatcher · context · request     │
│    response · handler · layer · error · config               │
│    middleware/* (opcional, cada uno su import)               │
│    websocket · sse · static_file · test_client               │
└───────────────────────────┬──────────────────────────────────┘
                            │ portus.transport (interno)
┌───────────────────────────▼──────────────────────────────────┐
│  Binding C (portus.native)                                   │
│    include/portus_native.h  (header-only, 1 TU)              │
│    include/llhttp/*         (vendorizado, MIT)               │
└──────────────────────────────────────────────────────────────┘
```

Reglas de dependencia:

- El núcleo **no** importa middlewares.
- Los middlewares importan el núcleo y, si acaso, `portus.native` (p. ej.
  compresión usa `std.compressus`, no C).
- Solo `portus.transport` y `portus.websocket` importan `portus.native`.
- `portus.test_client` no toca C: usa el dispatcher directamente.

---

## 3. Modelo de ejecución

**Un hilo, un event loop, handlers síncronos.** El loop está en C (`poll(2)`);
cuando una petición está completa, llama a **un** callback de Pengu que ejecuta
la cadena de capas de principio a fin y escribe la respuesta.

Consecuencias (explícitas, no accidentales):

- Un handler que bloquea (fichero grande, llamada de red, cálculo pesado)
  bloquea **todo** el servidor. Es el mismo contrato que `readFileSync` en Node:
  permitido, pero documentado como error de diseño en producción.
- La solución de escala es **varios procesos** (F6) o un proxy delante; no hay
  pool de hilos en v1. Ver D9.
- No hay `next()` asíncrono. La cadena avanza si el handler **no envió** y
  **no devolvió error**.

Esto no es un detalle: condiciona la API. Por eso los handlers devuelven
`result of void to HttpError` y no reciben callbacks.

---

## 4. Tipos del núcleo

### 4.1 `Handler` y `Middleware`

Handler y middleware son **dos tipos distintos**, no una runa con unión: las
firmas difieren y una runa con las dos firmas no se puede emitir sin una unión
que el lenguaje no tiene. (Desviación aprobada respecto al borrador original.)

```pengu
# src/portus/handler.pengu
alias Handler as weave with context as ref to Context into result of void to HttpError
alias MiddlewareFn as weave with data as ref to void, context as ref to Context into result of void to HttpError
alias ErrorHandler as weave with context as ref to Context, failure as HttpError into result of void to HttpError
alias AppHook as weave with app as ref to Application into void

rune Middleware:
    invoke as MiddlewareFn
    data as ref to void
```

- **Thin** (`Handler`): el caso normal, sin configuración.
- **Thick** (`Middleware`): configuración en `data`. `weave middleware` envuelve
  un `MiddlewareFn`; el middleware recupera su config con
  `transmute data to ref to Config`.

No hay closures y no hay despacho dinámico: todo son punteros a función con
nombre. `Middleware` **no** se banea (no posee `data`); la configuración vive en
la arena de la `Application` (§5).

### 4.2 Entradas de la cadena

```pengu
rune RouteEntry:
    method as string      # "GET", "POST", …, "*" = cualquiera
    pattern as Pattern    # patrón compilado (segmentos) + score
    handler as Handler
    score as int          # especificidad; ordena las rutas

rune MiddlewareEntry:
    prefix as string      # "" = todas las rutas
    middleware as Middleware
```

`_befores` y `_finishers` son `list of MiddlewareEntry` en orden de registro;
`_routes` es `list of RouteEntry` ordenada por `score` al insertar.

### 4.3 `Application`

```pengu
rune Application:
    _routes as list of RouteEntry
    _befores as list of MiddlewareEntry
    _finishers as list of MiddlewareEntry
    _arena as arena.Arena            # configs de middleware
    _on_error as ErrorHandler        # no-op por defecto
    _has_on_error as bool
    _on_not_found as Handler         # no-op por defecto
    _has_on_not_found as bool
    _on_start as AppHook             # no-op por defecto
    _has_on_start as bool
    _on_shutdown as AppHook          # no-op por defecto
    _has_on_shutdown as bool
    _sealed as bool
```

`maybe weave …` no se usa para los hooks: `some (fn)` con un puntero a función
emite C inválido en 0.28.7 (bug 12), así que se guarda el no-op + un flag
`_has_*`. `_state` y `_config` llegarán en F2/F3 cuando exista el tipo `Config`.

**Ownership:** se crea una vez (`new_application`), se registra, se presta
(`ref to`), se banea una vez con `free_application`. Copiarla después de
registrar es un bug; `_sealed` lo detectará en modo debug (F5).

### 4.4 `Context`, `Request`, `Response`

`Context` es la vista por petición. Vive en el stack del dispatcher y se pasa
**siempre** por `ref to`; nunca se devuelve ni se guarda.

```pengu
rune Context:
    request as Request
    response as Response
    params as map of string to string
    state as map of string to string
    app as ref to Application
    request_id as string
    started_ms as long
    user_data as ref to void
    error as maybe HttpError
    _native as ref to void
    _streaming as bool
    _sent as bool
```

`Request` (todo owned por el contexto):

```pengu
rune Request:
    method as string        # "GET"
    method_code as int
    target as string        # ruta cruda, sin query
    path as string          # decodificada
    query as map of string to list of string
    version as string       # "1.1"
    headers as Headers
    body as string          # binario seguro (len)
    peer_ip as string
    is_secure as bool
    wants_close as bool
```

`Response`:

```pengu
rune Response:
    status as int
    headers as Headers
    body as string
    sent as bool
```

### 4.5 `Config`

Valores por defecto seguros:

| Campo | Defecto | Nota |
|---|---|---|
| `host` | `"127.0.0.1"` | explícito, no `0.0.0.0` |
| `port` | `3000` | |
| `max_body_bytes` | 1 MiB | 413 si se supera |
| `max_header_bytes` | 16 KiB | 431 |
| `max_connections` | 1024 | |
| `request_timeout_ms` | 30 000 | del primer byte al fin |
| `idle_timeout_ms` | 60 000 | keep-alive |
| `keep_alive` | `true` | |
| `dev_mode` | `false` | 404/500 con detalle |
| `trust_proxy` | `false` | X-Forwarded-For |
| `strict_trailing_slash` | `false` | `/x/` ≡ `/x` |
| `case_sensitive` | `true` | |
| `server_name` | `"portus"` | cabecera `Server` |

---

## 5. Ownership (reglas duras)

| Valor | Quién lo posee | Se libera con | Se pasa como |
|---|---|---|---|
| `Application` | el llamador | `banish app` | `ref to` |
| `Server` | el llamador | `banish server` | `ref to` |
| `Context` | el dispatcher (stack) | al salir de `dispatch` | `ref to` |
| `Request`/`Response` | el `Context` | con el contexto | `ref to` |
| `Handler` | valor | — | por valor |
| `data` de un `Handler` | la `Application` (arena) | `banish app` | `ref to void` |
| `string` que devuelve la librería | el llamador | `banish` | por valor |
| cuerpo de `send_*` | el `Response` | al terminar la petición | por valor (se **mueve**) |

Consecuencias que hay que respetar en el código:

1. **`send_text`, `send_json`, `send_bytes`, `send_html` toman posesión del
   cuerpo.** Si construyes un `string` dinámico y lo envías, no lo banes
   después. Los literales son prestados (`is_owned == 0`) y banearlos es no-op,
   pero la regla es la misma: tras enviar, no uses el string.
2. **Las cadenas de `context.request.*` son del contexto.** No las guardes en
   `app.state` ni en estructuras que sobrevivan a la petición.
3. **La config de un middleware se mueve a la app.** `options` se copia a la
   arena; no la banes después de registrar.
4. **`Application` no se copia después de registrar.** `_sealed` lo comprueba.
5. **El `Server` presta la `Application`.** La app debe vivir más que el
   servidor (en la práctica, toda la vida de `main`).

---

## 6. Pipeline de una petición

```
                       ┌─────────────┐
  socket ──llhttp─────▶│ Request     │
                       └──────┬──────┘
                              ▼
                    ┌───────────────────┐
                    │ 1. befores        │  use / use_at (orden registro)
                    │   ¿envió? ────────┼──▶ sí: saltar a 4
                    │   ¿error? ────────┼──▶ sí: 3
                    └─────────┬─────────┘
                              ▼
                    ┌───────────────────┐
                    │ 2. ruta           │  mejor match por score
                    │   handler         │  si no envía, siguiente match
                    └─────────┬─────────┘
                              ▼
                    ┌───────────────────┐
                    │ 3. error / 404    │  on_error / on_not_found / defecto
                    └─────────┬─────────┘
                              ▼
                    ┌───────────────────┐
                    │ 4. finishers      │  solo si sent
                    │   compress, etag, │
                    │   security, log   │
                    └─────────┬─────────┘
                              ▼
                    ┌───────────────────┐
                    │ 5. write          │  status + headers + body
                    └───────────────────┘
```

Pseudocódigo del dispatcher (implementación real en
`src/portus/dispatcher.pengu`):

```pengu
weave dispatch with app as ref to Application, native as ref to void into void:
    var context as Context is calling new_context with app, native
    let before_result as result of void to HttpError is calling run_befores with app, sigil of context
    if before_result.is_ok is false:
        set context.error is some before_result.error
        calling run_error with app, sigil of context
    if context.response.sent is false:
        calling run_route with app, sigil of context
    if context.response.sent is false:
        calling run_not_found with app, sigil of context
    if context.response.sent:
        calling run_finishers with app, sigil of context
        calling write_response with sigil of context
```

Propiedades que esto garantiza:

- **Middleware que no envía, no corta.** Es el equivalente a "no llamar a
  `next()`" en Express, pero al revés: continúa salvo que envíes.
- **Un error corta la cadena** y va al manejador de errores; los finishers
  siguen corriendo (un 500 también se loguea y se comprime).
- **Los finishers ven la respuesta final.** Por eso `compress`, `etag` y
  `logger` son finishers y no `before`.

### 6.1 Cómo se escribe un middleware propio

Caso sin configuración (thin): una función normal.

```pengu
weave marca with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.set_header with "X-Portus", "1"
    return portus.ok_void

calling app.use with marca
```

Caso con configuración (thick): una runa de config + una `invoke` + un
registrador que la aloja en la app.

```pengu
rune MarcaConfig:
    valor as string

weave marca_invoke with data as ref to void, context as ref to portus.Context into result of void to portus.HttpError:
    let config as ref to MarcaConfig is transmute data to ref to MarcaConfig
    calling context.set_header with "X-Portus", config->valor
    return portus.ok_void

weave use_marca with app as ref to portus.Application, valor as string into void:
    let allocator as ref to arena.Arena is calling app.arena
    let config as ref to MarcaConfig is calling arena.arena_alloc_one of MarcaConfig with allocator
    set essence of config is with valor is valor
    calling app.use_handler with (calling portus.handler_with with marca_invoke, (transmute config to ref to void))
```

`app.arena` expone la arena interna para autores de middleware. La llamada a
`arena_alloc_one` se hace **siempre con un tipo concreto**: envolverla en un
`weave ... shard T` dispara el bug #6 del compilador (ver
[`portus-compiler-fixes.md`](portus-compiler-fixes.md)). No se usa en el 99 %
de las apps.

---

## 7. Router

### 7.1 Patrones

| Patrón | Ejemplo | Matchea | `params` |
|---|---|---|---|
| literal | `/salud` | `/salud` | `{}` |
| param | `/usuarios/:id` | `/usuarios/42` | `id = "42"` |
| param múltiple | `/a/:x/b/:y` | `/a/1/b/2` | `x=1, y=2` |
| wildcard nombrado | `/ficheros/*ruta` | `/ficheros/a/b.txt` | `ruta = "a/b.txt"` |
| wildcard suelto | `/ficheros/*` | `/ficheros/a/b` | — |
| prefijo (`use`) | `/api` | `/api`, `/api/x` | — |

Reglas:

- `:param` exige un segmento no vacío y **se decodifica** (percent-encoding).
- `*` captura **el resto** de la ruta, incluidas barras; puede ir vacío.
- `:param` y `*` solo al inicio de un segmento; el resto es literal.
- Doble barra y `.`/`..` se normalizan antes de matchear.
- Trailing slash: tolerado por defecto; `strict_trailing_slash = true` lo
  distingue.
- Case-sensitive por defecto.

### 7.2 Orden

- **Befores**: orden de registro. Siempre antes que las rutas.
- **Rutas**: ordenadas por **especificidad** (más literal gana), y a igualdad,
  orden de registro. `score = 1000*literales + 10*params - 100*wildcards`.
- Si un handler de ruta no envía, se prueba la siguiente ruta que matchee
  (fallthrough), hasta agotar; entonces 404.

Esto evita la trampa de Express de registrar `/usuarios/me` antes que
`/usuarios/:id` y es determinista.

### 7.3 Automatismos

- **HEAD**: si no hay ruta HEAD, se usa la GET; el transporte suprime el cuerpo.
- **OPTIONS**: si no hay ruta OPTIONS y el path existe, `204` + `Allow`.
- **405**: si el path existe pero ningún método coincide, `405` + `Allow`.
- **404**: `on_not_found` o el manejador por defecto.

### 7.4 Routers y montaje

```pengu
var admin as portus.Router is calling portus.new_router
calling admin.get with "/panel", panel
calling admin.use with auth

calling app.mount with "/admin", admin     # consume `admin`
```

`mount` **mueve** las capas del router a la app con el prefijo aplicado; el
router queda vacío. Sin `ref to` colgantes, sin dobles liberaciones.

---

## 8. Errores

```pengu
omen HttpError:
    BadRequest with message as string
    Unauthorized
    Forbidden
    NotFound
    MethodNotAllowed with allow as string
    Conflict with message as string
    PayloadTooLarge
    UnsupportedMediaType
    TooManyRequests
    Internal with message as string
```

- Un handler devuelve `err_of with HttpError.NotFound` y el dispatcher lo
  convierte en respuesta (404). El `message` de las variantes con payload se
  usa solo en `dev_mode`.
- `on_error` recibe el contexto con `context.error` puesto; si envía, se
  respeta; si no, el manejador por defecto responde.
- **Nunca** `panic` para entrada del usuario. `panic` es para bugs del
  framework (invariantes).
- 500 por defecto: `{"error":"internal"}` (o texto en dev), sin filtrar stack.

---

## 9. Frontera C (contrato)

`include/portus_native.h`, todo `static`, un solo TU. Sin estado global mutable.

### 9.1 Modelo

- `portus_server`: listener + lista de conexiones + parser llhttp + buffers.
- `portus_conn`: socket, buffer de lectura (16 KiB), buffer de escritura,
  parser, deadline, modo (http | ws).
- Callback **uno por servidor**: `void (*on_request)(void* userdata,
  portus_request* req)`.
- Tras `on_request`, el shim comprueba que se respondió; si no, escribe un 500
  y cierra (bug del handler).

### 9.2 Accesores (prestados, válidos solo dentro del callback)

| Función | Devuelve |
|---|---|
| `portus_request_method(req)` | `const char*` |
| `portus_request_target(req, &len)` | `const char*` |
| `portus_request_version(req)` | `const char*` |
| `portus_request_header_count(req)` | `int` |
| `portus_request_header_name(req, i, &len)` | `const char*` |
| `portus_request_header_value(req, i, &len)` | `const char*` |
| `portus_request_body(req, &len)` | `const char*` |
| `portus_request_peer_ip(req)` | `const char*` |
| `portus_request_wants_close(req)` | `int` |
| `portus_request_is_upgrade(req)` | `int` |

### 9.3 Respuesta

| Función | Uso |
|---|---|
| `portus_request_respond(req, status, headers, hlen, body, blen)` | respuesta completa |
| `portus_request_begin_stream(req, status, headers, hlen)` | chunked |
| `portus_conn_write_chunk(conn, data, len)` | streaming |
| `portus_conn_end_stream(conn, trailers, tlen)` | fin |
| `portus_request_accept_upgrade(req, headers, hlen)` | 101 + modo WS |
| `portus_conn_send(conn, data, len)` | WS: frame ya construido |
| `portus_conn_close(conn, code)` | cierre |

### 9.4 Utilidades

`portus_sha1`, `portus_random_bytes`, `portus_constant_time_eq`,
`portus_now_ms` (monotónico), `portus_stat` (mtime/size), `portus_install_signals`,
`portus_server_port`.

### 9.5 Invariantes que el shim debe cumplir (y se testean)

- Nunca escribe más allá de los buffers; `write` parcial → cola.
- `EAGAIN`/`EWOULDBLOCK` no es error.
- Cierra conexión ante cabecera/cuerpo demasiado grandes con el código correcto.
- No hace `malloc` por petición si puede reutilizar el buffer de la conexión.
- Libera todo al cerrar el servidor (ASan limpio).
- `poll` con timeouts calculados; no busy-loop.
- `SIGPIPE` ignorado.

---

## 10. Módulos

```
src/portus.pengu                 fachada: alias públicos + helpers
src/portus/native.d.pengu        binding C (include + declare)
src/portus/transport.pengu       wrapper seguro del shim (interno)
src/portus/config.pengu          Config + defaults
src/portus/handler.pengu         Handler, HandlerShape, handler_with
src/portus/layer.pengu           Layer
src/portus/route.pengu           Pattern, Segment, compile, match, score
src/portus/request.pengu         Request + acceso a headers/query/body
src/portus/response.pengu        Response + emisión (send_*, redirect, stream)
src/portus/context.pengu         Context + accesores
src/portus/error.pengu           HttpError, mapeo a status
src/portus/application.pengu     Application + registro + hooks + listen
src/portus/router.pengu          Router + mount
src/portus/dispatcher.pengu      pipeline (interno)
src/portus/server.pengu          Server (run/stop/port)
src/portus/status.pengu          constantes de códigos
src/portus/header.pengu          Headers (case-insensitive)
src/portus/cookie.pengu          Cookie, jar, firma HMAC
src/portus/mime.pengu            extensión → content-type
src/portus/body.pengu            form-urlencoded, JSON
src/portus/multipart.pengu       parser multipart
src/portus/static_file.pengu     estáticos + ETag + Range + Last-Modified
src/portus/sse.pengu             Server-Sent Events
src/portus/websocket.pengu       handshake + framing + rooms
src/portus/test_client.pengu     TestClient en proceso
src/portus/middleware/cors.pengu
src/portus/middleware/logger.pengu
src/portus/middleware/request_id.pengu
src/portus/middleware/security_headers.pengu
src/portus/middleware/compress.pengu
src/portus/middleware/session.pengu
src/portus/middleware/auth.pengu
src/portus/middleware/csrf.pengu
src/portus/middleware/rate_limit.pengu
src/portus/middleware/timeout.pengu
```

Ids de módulo: `portus`, `portus.application`, `portus.middleware.cors`, etc.
La fachada reexporta tipos con `alias` (verificado que funciona entre módulos:
`alias Widget as demo.core.Widget`).

---

## 11. Nombres (resumen aplicado)

- Tipos: `Application`, `Router`, `Context`, `Request`, `Response`, `Handler`,
  `Layer`, `Config`, `Server`, `HttpError`, `Cookie`, `Headers`, `Route`,
  `Pattern`, `TestClient`.
- Variables y parámetros: `application`/`app` (solo dentro de la librería),
  `context`, `request`, `response`, `handler`, `pattern`, `config`.
- Métodos: verbo en forma base. `get`, `post`, `put`, `patch`, `delete`,
  `head`, `options`, `use`, `use_at`, `mount`, `listen`, `run`, `stop`,
  `send_text`, `send_json`, `send_file`, `set_header`, `set_status`,
  `set_cookie`, `delete_cookie`, `redirect`.
- Predicados: `is_sent`, `is_secure`, `wants_close`, `is_streaming`.
- Constructores: `new_application`, `new_router`, `new_config`, `handler_with`.
- Nada de `Ctx`, `req`, `res`, `cfg`, `mw`.

---

## 12. Cómo se ve una app

### 12.1 Hola mundo

```pengu
import portus
import portus.context as portus_context
import std.io

weave hola with context as ref to portus.Context into result of void to portus.HttpError:
    return calling context.send_text with "¡Hola, mundo!"

weave main into int:
    var app as portus.Application is calling portus.new_application
    calling app.get with "/", hola
    let server as result of portus.Server to portus.ListenError is calling app.listen
    if server.is_ok is false:
        calling io.write_stderr with "no se pudo escuchar\n"
        return 1
    var srv as portus.Server is server.value
    calling srv.run
    banish srv
    banish app
    return 0
```

### 12.2 API JSON

```pengu
import portus
import std.json

weave listar with context as ref to portus.Context into result of void to portus.HttpError:
    let items as list of json.JsonValue is [...]
    return calling context.send_json with (calling json.array_of with items)

weave crear with context as ref to portus.Context into result of void to portus.HttpError:
    let body as result of json.JsonValue to portus.BodyError is calling context.body_json
    if body.is_ok is false:
        return calling portus.err_of_status with portus.HttpError.BadRequest with message is "json inválido"
    ...
```

### 12.3 Middleware

```pengu
import portus
import portus.middleware.cors
import portus.middleware.logger

var app as portus.Application is calling portus.new_application
let cors_options as portus.cors.Options is with origins is ["https://miapp.dev"], credentials is true
calling portus.cors.use_cors with sigil of app, cors_options
calling portus.logger.use_logger with sigil of app
calling app.use_at with "/admin", requiere_auth
```

### 12.4 WebSocket

```pengu
import portus
import portus.websocket

rune Chat:
    sala as string

bind Chat with portus.websocket.Handler

enchanting Chat:
    weave on_open with connection as ref to portus.websocket.Connection into void:
        calling connection.join with self->sala
    weave on_message with connection as ref to portus.websocket.Connection, message as string into void:
        calling portus.websocket.broadcast_room with self->sala, message

var chat as Chat is with sala is "general"
calling portus.websocket.get with sigil of app, "/chat", sigil of chat
```

---

## 13. Limitaciones declaradas (van en el README)

1. Handlers síncronos: no bloquees el loop.
2. Sin TLS nativo en v1.
3. Sin HTTP/2.
4. Sin plantillas.
5. `Application` no se copia tras registrar.
6. wasm no soportado.
7. Los `data` de middleware viven en la arena de la app: no los liberes tú.
