# 🌊 `std.portus` — El framework web de PenguScript

## 1. El nombre

**`portus`** (latín: *puerto, refugio, desembarcadero*). Es la metáfora exacta: un puerto donde llegan las naves (peticiones) y son recibidas, clasificadas y dirigidas a su muelle (handler). Es corto, evoca la palabra "puerto" de red, y encaja con la convención latina de la stdlib (`petitio`, `archivum`, `compass`, `scrolls`, `oracle`…).

**Alternativas** por si quieres otra dirección:

| Nombre | Latín | Por qué |
|---|---|---|
| `ianua` | puerta | la puerta de entrada a tu aplicación |
| `atrium` | sala de recepción romana | donde se reciben y enrutan los invitados |
| `limen` | umbral | la frontera entre el mundo y tu app |
| `statio` | estación/puesto | el servidor como puesto de guardia |

La simetría con `std.petitio` (que significa *petición* en latín y es el cliente HTTP) es perfecta: `petitio` envía peticiones, `portus` las recibe. Recomiendo **`std.portus`** sin dudarlo.

---

## 2. Filosofía de diseño

Antes de la API, los principios que la guían, para que cada decisión posterior sea coherente:

1. **Compuesto, no acoplado.** `portus` no es un monolito; es un núcleo pequeño (`App`, `Router`, `Ctx`, `Handler`) + middleware opcional. Importas solo lo que usas; el DCE elimina el resto.
2. **Explícito como PenguScript.** Los handlers devuelven `result of void to HttpError`, no lanzan excepciones. La propiedad de cada `string` en una respuesta se documenta con `##`.
3. **Zero-copy donde se pueda.** El parser HTTP (`llhttp`) trabaja sobre el búfer de `uSockets`; el cuerpo de la petición se expone como `slice of byte` antes de parsearlo, y solo se copia si lo pides.
4. **Middleware componible sin closures.** Como las lambdas de PenguScript no capturan, el middleware se modela como `Handler` que *no* envían respuesta y dejan que el framework continúe (ver §4.4). Para envolver la respuesta se usa el patrón `concept Handler` (decorador), que sí es expresable.
5. **Tipos de valor en la API.** `App`, `Router`, `Server`, `SseStream` son `rune`s que posees y liberas con `banish`. El `Ctx` se pasa por `ref to` porque es efímero por petición.
6. **HTTP correcto antes que rápido.** Cubre el RFC 9110/9112 completo (Range, Conditional, Content Negotiation, Trailers) antes de microoptimizar. Después mide con `psc time` y `perf`.

---

## 3. Estructura de módulos

```
std/
  portus.pengu            ← API pública (re-exporta todo lo de abajo)
  portus.d.pengu          ← bindings C (uSockets + llhttp + mbedTLS/OpenSSL)
  portus/
    app.pengu             ← App, Server, ciclo de vida
    router.pengu          ← Router, patrones, grupos, mounts
    ctx.pengu             ← Ctx, Request, Response
    handler.pengu         ← Handler, Next, ErrorHandler, concept Handler
    middleware/
      logger.pengu
      cors.pengu
      compress.pengu
      static.pengu
      body.pengu          ← parsing JSON/form/multipart/text/binary
      session.pengu
      auth.pengu          ← Basic, Bearer, JWT
      csrf.pengu
      security.pengu      ← CSP, HSTS, X-Frame-Options, etc.
      rate_limit.pengu
      request_id.pengu
      timeout.pengu
    http/
      status.pengu        ← códigos de estado (constantes)
      mime.pengu          ← tabla MIME estática
      headers.pengu       ← helpers de cabeceras tipadas
      conditional.pengu   ← ETag, If-Modified-Since, Vary
      range.pengu         ← Range, Content-Range, multipart/byteranges
      sse.pengu           ← Server-Sent Events
      stream.pengu        ← chunked, backpressure
    ws.pengu              ← WebSocket (handshake + framing + rooms)
    tls.pengu             ← config TLS
    test_client.pengu     ← cliente de test sin red
    cli.pengu             ← helper para `serve --port --host`
```

`std/portus.pengu` es el único que importas en la mayoría de los casos:

```pengu
import std.portus
```

---

## 4. Tipos y API núcleo

### 4.1 `App` — la aplicación

```pengu-fragment
rune App:
    router as Router
    middlewares as list of Handler
    config as Config
    server as maybe Server
    state as map of string to string   # estado compartido entre handlers

rune Config:
    host as string
    port as int
    tls as maybe TlsConfig
    max_body_bytes as usize
    request_timeout_ms as int
    idle_timeout_ms as int
    keep_alive as bool
    dev_mode as bool              # errores verbosos, source maps

# Ciclo de vida
weave new_app into App
weave new_app_with with config as Config into App
weave new_dev_app into App        # dev_mode = true, puerto 3000

# Registro de rutas (delegan en el Router interno)
weave get    with app as ref to App, pattern as string, h as Handler into void
weave post   with app as ref to App, pattern as string, h as Handler into void
weave put    with app as ref to App, pattern as string, h as Handler into void
weave delete with app as ref to App, pattern as string, h as Handler into void
weave patch  with app as ref to App, pattern as string, h as Handler into void
weave head   with app as ref to App, pattern as string, h as Handler into void
weave options with app as ref to App, pattern as string, h as Handler into void
weave any    with app as ref to App, pattern as string, h as Handler into void

weave use    with app as ref to App, h as Handler into void
weave mount  with app as ref to App, prefix as string, sub as ref to App into void
weave group  with app as ref to App, prefix as string into Router  # router con prefijo

# Ficheros estáticos
weave static with app as ref to App, prefix as string, dir as string into void
weave static_with with app as ref to App, prefix as string, dir as string, opts as StaticOptions into void

# Manejadores especiales
weave on_error      with app as ref to App, h as ErrorHandler into void
weave on_not_found  with app as ref to App, h as Handler into void
weave on_start      with app as ref to App, h as ref to weave into result of void to HttpError into void
weave on_shutdown   with app as ref to App, h as ref to weave into void into void

# Arranque
weave listen with app as ref to App, port as int into result of Server to HttpError
weave listen_at with app as ref to App, host as string, port as int into result of Server to HttpError
weave serve  with app as ref to App, port as int into result of void to HttpError  # listen + run + señales
weave run    with server as ref to Server into void                                  # bloquea
weave stop   with server as ref to Server into result of void to HttpError            # graceful
```

### 4.2 `Ctx` — contexto de petición/respuesta

```pengu-fragment
rune Ctx:
    req as Request
    res as Response
    params as map of string to string       # :id, :slug, *
    state as map of string to string        # por petición, para middleware
    app as ref to App
    conn as ref to Connection               # de uSockets
    request_id as string

# ── Request (lectura) ──
weave method   with ctx as ref to Ctx into string
weave path     with ctx as ref to Ctx into string
weave query    with ctx as ref to Ctx, key as string into maybe string
weave queries  with ctx as ref to Ctx into map of string to list of string
weave header   with ctx as ref to Ctx, name as string into maybe string
weave headers  with ctx as ref to Ctx into map of string to string
weave param    with ctx as ref to Ctx, name as string into maybe string
weave cookie   with ctx as ref to Ctx, name as string into maybe string
weave ip       with ctx as ref to Ctx into string
weave host     with ctx as ref to Ctx into string
weave scheme   with ctx as ref to Ctx into string
weave is_secure with ctx as ref to Ctx into bool

# Cuerpo (lazy: solo parsea cuando lo pides)
weave body_bytes  with ctx as ref to Ctx into result of slice of byte to HttpError
weave body_text   with ctx as ref to Ctx into result of string to HttpError
weave body_json   with ctx as ref to Ctx into result of json.JsonValue to HttpError
weave body_form   with ctx as ref to Ctx into result of map of string to string to HttpError
weave body_multipart with ctx as ref to Ctx into result of Multipart to HttpError

# ── Response (escritura) ──
weave status     with ctx as ref to Ctx, code as int into void
weave set_status with ctx as ref to Ctx, s as Status into void
weave set_header with ctx as ref to Ctx, name as string, value as string into void
weave append_header with ctx as ref to Ctx, name as string, value as string into void
weave set_cookie with ctx as ref to Ctx, name as string, value as string, opts as CookieOptions into void
weave delete_cookie with ctx as ref to Ctx, name as string into void

weave text     with ctx as ref to Ctx, s as string into result of void to HttpError
weave html     with ctx as ref to Ctx, s as string into result of void to HttpError
weave json     with ctx as ref to Ctx, v as json.JsonValue into result of void to HttpError
weave json_string with ctx as ref to Ctx, s as string into result of void to HttpError  # pre-serializado
weave bytes    with ctx as ref to Ctx, b as slice of byte into result of void to HttpError
weave file     with ctx as ref to Ctx, path as string into result of void to HttpError
weave redirect with ctx as ref to Ctx, url as string into result of void to HttpError
weave redirect_permanent with ctx as ref to Ctx, url as string into result of void to HttpError
weave no_content with ctx as ref to Ctx into result of void to HttpError
weave not_modified with ctx as ref to Ctx, etag as string into result of void to HttpError

# Streaming
weave stream_open with ctx as ref to Ctx, content_type as string into result of Stream to HttpError
weave sse_open    with ctx as ref to Ctx into result of SseStream to HttpError
```

### 4.3 `Router` — enrutador

```pengu-fragment
rune Router:
    routes as list of Route
    middlewares as list of Handler

weave new_router into Router
weave get_on     with r as ref to Router, pattern as string, h as Handler into void
weave post_on    with r as ref to Router, pattern as string, h as Handler into void
# ... (mismos verbos que App)
weave use_on     with r as ref to Router, h as Handler into void
weave group_on   with r as ref to Router, prefix as string into Router
weave mount_on   with r as ref to Router, prefix as string, sub as ref to Router into void
```

**Patrones de ruta** (P0):

| Patrón | Significado |
|---|---|
| `/users` | literal |
| `/users/:id` | parámetro `id` |
| `/files/*path` | comodín, captura el resto en `path` |
| `/users/:id/posts/:post_id` | múltiples params |
| `/api/*` | comodín sin nombre |
| `/:tenant/users` | prefijo de tenant |

**Reglas de matching** (P1): prioridad por especificidad (literal > param > comodín), case-sensitive por defecto (configurable), trailing slash configurable.

### 4.4 `Handler` y middleware

```pengu-fragment
# Un handler es una función que recibe el contexto y devuelve un resultado.
# Si quiere responder, llama a ctx.text/json/...; si no, el framework continúa.
weave Handler with ctx as ref to Ctx into result of void to HttpError

# Error handler global
weave ErrorHandler with ctx as ref to Ctx, err as HttpError into result of void to HttpError
```

**Modelo de middleware (v1):** el framework ejecuta la cadena de `Handler`s en orden. Después de cada uno, comprueba `ctx.res.sent`. Si es `true`, para. Si no, continúa. Un middleware que quiere "pasar" simplemente no escribe en `ctx.res`. Un middleware que quiere "cortar" escribe una respuesta.

```pengu-fragment
weave logger with ctx as ref to Ctx into result of void to HttpError:
    let start as isize is calling chrono.monotonic_nanos
    # no escribe respuesta → el framework continúa
    calling whisper.info with "{ctx.method} {ctx.path}"
    return calling ok_of with 0

weave auth_required with ctx as ref to Ctx into result of void to HttpError:
    let token as maybe string is calling header with ctx, "authorization"
    if token is not present:
        calling text with ctx, "unauthorized"   # escribe respuesta → corta
        calling status with ctx, 401
        return calling ok_of with 0
    return calling ok_of with 0
```

**Modelo avanzado (v2):** para middleware que envuelve la respuesta (compresión, transformación), se usa un `concept Handler` decorador:

```pengu-fragment
concept Middleware:
    weave wrap with next as Handler into Handler

rune CompressMiddleware:
    min_bytes as usize

bind CompressMiddleware with Middleware:
    weave wrap with next as Handler into Handler:
        # devuelve un Handler que llama a next y luego comprime
        ...
```

Esto es potente pero verboso; solo se usa cuando hace falta. El 90% de los middlewares son del primer tipo.

### 4.5 `Server` y `Connection`

```pengu-fragment
rune Server:
    loop as ref to UsLoop
    ctx as ref to UsSocketContext
    app as ref to App
    running as bool

rune Connection:
    socket as ref to UsSocket
    id as u64
    ip as string
    remote_port as int
    is_tls as bool
```

### 4.6 WebSocket — `std.portus.ws`

```pengu-fragment
import std.portus.ws

rune Server:
    rooms as map of string to list of ref to Conn

rune Conn:
    id as u64
    ip as string
    headers as map of string to string
    room as maybe string
    state as map of string to string

concept Handler:
    weave on_open    with conn as ref to ws.Conn into void
    weave on_message with conn as ref to ws.Conn, msg as string into void
    weave on_binary  with conn as ref to ws.Conn, data as slice of byte into void
    weave on_close   with conn as ref to ws.Conn, code as int, reason as string into void
    weave on_error   with conn as ref to ws.Conn, err as string into void
    weave on_pong    with conn as ref to ws.Conn into void

# Registro
weave get with app as ref to portus.App, pattern as string, h as ref to Handler into void

# API de conexión
weave send        with conn as ref to ws.Conn, msg as string into result of void to WsError
weave send_binary with conn as ref to ws.Conn, data as slice of byte into result of void to WsError
weave close       with conn as ref to ws.Conn, code as int, reason as string into void
weave ping        with conn as ref to ws.Conn into result of void to WsError
weave join_room   with conn as ref to ws.Conn, room as string into void
weave leave_room  with conn as ref to ws.Conn into void

# Broadcast
weave broadcast       with srv as ref to ws.Server, msg as string into void
weave broadcast_room  with srv as ref to ws.Server, room as string, msg as string into void
weave broadcast_except with srv as ref to ws.Server, skip as ref to ws.Conn, msg as string into void
weave connections     with srv as ref to ws.Server into list of ref to ws.Conn
weave connections_in  with srv as ref to ws.Server, room as string into list of ref to ws.Conn
```

Uso típico de un chat:

```pengu-fragment
import std.portus
import std.portus.ws

rune ChatHandler:
    server as ref to ws.Server

bind ChatHandler with ws.Handler:
    weave on_open with conn as ref to ws.Conn into void:
        calling ws.join_room with conn, "general"
        calling ws.broadcast_room with self->server, "general", "user joined"

    weave on_message with conn as ref to ws.Conn, msg as string into void:
        calling ws.broadcast_room with self->server, "general", msg

    weave on_close with conn as ref to ws.Conn, code as int, reason as string into void:
        calling ws.broadcast_room with self->server, "general", "user left"

    weave on_binary with conn as ref to ws.Conn, data as slice of byte into void: ...
    weave on_error  with conn as ref to ws.Conn, err as string into void: ...
    weave on_pong   with conn as ref to ws.Conn into void: ...

weave main into int:
    var app as portus.App is calling portus.new_app
    defer banish app
    var h as ChatHandler is with server is sigil of app.ws_server
    calling ws.get with sigil of app, "/chat", sigil of h
    return calling portus.serve with sigil of app, 8080
```

---

## 5. Feature checklist completo

Organizado por categoría y con prioridad: **P0** = imprescindible para v1, **P1** = completo para v1, **P2** = v2/v3.

### A. Aplicación y ciclo de vida
- **P0** `App` con `new_app`, `listen`, `run`, `serve`, `stop`
- **P0** Configuración (`Config`): host, puerto, timeouts, límites
- **P0** `on_start`, `on_shutdown` hooks
- **P0** Graceful shutdown con SIGINT/SIGTERM (via `std.rites`)
- **P1** `on_error` global y `on_not_found`
- **P1** Estado compartido (`app.state`)
- **P1** Sub-apps (`mount`)
- **P2** Múltiples servidores (HTTP + HTTPS simultáneos)
- **P2** Cluster / multi-worker (coordinar con `std.filum` + `fork`)

### B. Enrutamiento
- **P0** Verbos HTTP: GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
- **P0** `any` (todos los verbos)
- **P0** Patrones: literales, `:param`, `*wildcard`
- **P0** Prioridad por especificidad
- **P0** Grupos con prefijo (`group`)
- **P0** Mount de sub-apps
- **P1** Rutas nombradas + reverse routing (`url_for`)
- **P1** 404 y 405 automáticos
- **P1** Auto-OPTIONS (Allow header)
- **P1** Auto-HEAD (GET sin cuerpo)
- **P1** Trailing slash configurable
- **P1** Case sensitivity configurable
- **P1** Host-based routing (virtual hosts)
- **P2** Regex routes (vía `std.regulus`)
- **P2** Route constraints (`:id` solo enteros)

### C. Contexto y petición
- **P0** `method`, `path`, `query`, `header`, `param`, `cookie`, `ip`, `host`, `scheme`
- **P0** `body_bytes`, `body_text`, `body_json`, `body_form`
- **P0** `request_id` (generado o desde header)
- **P0** `state` por petición (para middleware)
- **P1** `body_multipart` (ficheros)
- **P1** `queries` completo (multi-valor)
- **P1** `headers` completo
- **P1** IP real detrás de proxy (`X-Forwarded-For`)
- **P2** `body_xml`, `body_yaml`, `body_toml` (vía std)

### D. Respuesta
- **P0** `status`, `set_header`, `set_cookie`
- **P0** `text`, `html`, `json`, `json_string`, `bytes`, `file`, `redirect`, `no_content`
- **P0** Content-Length automático, Content-Type por defecto
- **P1** `append_header`, `delete_cookie`
- **P1** `not_modified` (304)
- **P1** Streaming (`stream_open`, chunked)
- **P1** SSE (`sse_open`, `SseStream`)
- **P1** `Content-Disposition` (attachment)
- **P2** HTTP/2 server push (si uSockets lo soporta)

### E. Middleware integrado
- **P0** `logger` (structured, vía `std.whisper`)
- **P0** `cors` (configurable: orígenes, métodos, headers, credentials)
- **P0** `request_id`
- **P0** `timeout` (corta peticiones lentas)
- **P0** `body_limit` (rechaza cuerpos > max_body_bytes)
- **P1** `compress` (gzip, deflate, br si disponible)
- **P1** `static` (ficheros estáticos con MIME, ETag, Range, cache)
- **P1** `session` (cookie firmada + store en memoria, pluggable)
- **P1** `auth_basic`, `auth_bearer`, `auth_jwt`
- **P1** `csrf` (double-submit cookie)
- **P1** `security_headers` (CSP, HSTS, X-Frame-Options, X-Content-Type-Options, Referrer-Policy, Permissions-Policy)
- **P1** `rate_limit` (token bucket, sliding window)
- **P2** `cache_control` helpers
- **P2** `etag` helper

### F. HTTP correctness (RFC 9110/9112)
- **P0** Parseo incremental con `llhttp`
- **P0** Content-Length y chunked transfer
- **P0** Keep-alive
- **P1** Range requests (`Range`, `Content-Range`, `multipart/byteranges`)
- **P1** Conditional requests (`ETag`, `If-None-Match`, `If-Modified-Since`, `If-Range`, `Vary`)
- **P1** Content negotiation (`Accept`, `Accept-Charset`, `Accept-Encoding`, `Accept-Language`)
- **P1** `Expect: 100-continue`
- **P1** Trailers
- **P2** Upgrade handling (para WebSocket y otros)
- **P2** `CONNECT` (proxy)

### G. Seguridad
- **P0** TLS/HTTPS (vía uSockets + OpenSSL/mbedTLS)
- **P0** Límite de tamaño de cuerpo y cabeceras
- **P0** Timeouts (request, idle, header)
- **P1** Password hashing (Argon2id vía `std.secrets` o binding a `libargon2`)
- **P1** JWT (sign/verify HS256, RS256) en PenguScript usando `std.digest` + `std.cipher`
- **P1** Session signing (HMAC)
- **P1** CSRF tokens
- **P2** HTTP/2 (si uSockets lo expone)
- **P2** OCSP stapling, ALPN

### H. WebSocket
- **P0** Handshake RFC 6455 (SHA-1 + Base64; añadir SHA-1 a `std.digest`)
- **P0** Framing (texto, binario, close, ping, pong)
- **P0** `on_open`, `on_message`, `on_close`
- **P0** `send`, `close`
- **P1** Rooms (`join_room`, `leave_room`, `broadcast_room`)
- **P1** Broadcast global y `broadcast_except`
- **P1** `on_binary`, `on_error`, `on_pong`
- **P1** Backpressure (send queue, `pending_bytes`)
- **P1** Subprotocolos
- **P2** permessage-deflate (compresión)
- **P2** WebSocket sobre TLS (wss://)
- **P2** Rate limiting por conexión

### I. Observabilidad
- **P0** Logging estructurado por petición (`logger`)
- **P0** Request ID (correlación)
- **P0** Acceso a timings (duración por petición)
- **P1** Métricas: contadores, histogramas (interfaz pluggable)
- **P1** Health checks (`/health`, `/ready`)
- **P1** Route listing (endpoint de debug en dev_mode)
- **P2** Tracing (spans, OpenTelemetry-like interface)

### J. Developer Experience
- **P0** Modo dev (`dev_mode`) con errores verbosos y stack traces
- **P0** Test client (`TestClient` que hace peticiones sin red)
- **P0** Integración con `std.ward` para tests
- **P1** CLI helper (`serve --port --host --tls-cert --tls-key`)
- **P1** Auto-reload en dev (delegar a `psc watch` + reinicio)
- **P2** Pretty-print de errores HTTP
- **P2** Route inspector (`portus.routes with app`)

### K. Rendimiento
- **P0** Zero-copy del parser al body slice
- **P0** Pool de buffers para respuestas
- **P1** Límite de conexiones concurrentes
- **P1** Backpressure en streaming
- **P1** Timeouts duros por petición
- **P2** HTTP pipelining
- **P2** Buffer pooling configurable

---

## 6. Librerías C necesarias

### Obligatorias (bindear)

| Librería | Para qué | Notas |
|---|---|---|
| **`uSockets`** | event loop, TCP, UDP, TLS | el corazón; ya lo elegimos |
| **`llhttp`** | parser HTTP incremental | el parser de Node.js; C puro, sin dependencias, muy rápido |
| **OpenSSL** o **mbedTLS** | TLS | uSockets usa OpenSSL por defecto; mbedTLS es más ligero |

### Opcionales / ya en std

| Librería | Para qué | Estado |
|---|---|---|
| `zlib` | gzip/deflate | ya en `std.compressus` (miniz vendorizado) |
| `libsodium` | crypto, password hashing | ya en `std.secrets` |
| `multipart-parser` (C) | parseo multipart | o escribir en PenguScript |
| `libargon2` | Argon2id | bindear si no se quiere implementar |
| `libmagic` | MIME detection | o tabla estática en PenguScript |

### Ya disponible en la stdlib (reutilizar)

`std.json` (JSON), `std.cipher` (Base64/URL encoding), `std.digest` (SHA-256), `std.compressus` (gzip), `std.scrolls` (strings), `std.lingua` (UTF-8), `std.archivum` (ficheros), `std.compass` (rutas), `std.whisper` (logging), `std.ward` (asserts), `std.filum` (sync), `std.rites` (env, signals), `std.chronicle` (tiempo), `std.invoke` (CLI).

### Bindings necesarios (`.d.pengu`)

Dos archivos nuevos:

- `std/usockets.d.pengu` — event loop, context, listen, on_data, write, TLS
- `std/llhttp.d.pengu` — parser incremental

O uno solo: `std/portus.d.pengu` con `include` de ambas cabeceras y las `declare`s.

---

## 7. Orden de construcción (fases)

### Fase 0 — Fundación (1-2 semanas)
1. Bindear `uSockets` mínimo: `us_create_loop`, `us_create_socket_context`, `us_socket_context_listen`, `us_socket_context_on_open/on_data/on_close`, `us_socket_write`.
2. Servidor **echo TCP** que funcione end-to-end. Valida la cadena de compilación.
3. Bindear `llhttp` (parser incremental).
4. Parsear una petición HTTP/1.1 real y devolver una respuesta fija.

### Fase 1 — Núcleo de `portus` (2-3 semanas)
5. `App`, `Config`, `Server`, `Connection`.
6. `Router` con patrones literales, `:param`, `*wildcard`.
7. `Ctx`, `Request`, `Response` con accesores básicos.
8. `text`, `html`, `json`, `redirect`, `status`, `set_header`.
9. `Handler` + cadena de middleware (modelo `sent`).
10. `on_error`, `on_not_found`.
11. Graceful shutdown.

### Fase 2 — HTTP completo (2-3 semanas)
12. Body parsing: JSON, form-urlencoded, text, bytes.
13. Cookies (parse + set).
14. Static files (con MIME, ETag, Range).
15. Compression middleware.
16. CORS middleware.
17. Security headers middleware.
18. Conditional requests y Range.
19. Streaming y SSE.

### Fase 3 — WebSocket (2 semanas)
20. Handshake RFC 6455.
21. Framing completo (text, binary, close, ping, pong).
22. `ws.Conn`, `ws.Handler` concept.
23. Rooms y broadcast.
24. Backpressure.
25. Probar con Autobahn Testsuite.

### Fase 4 — Seguridad y sesiones (2 semanas)
26. TLS config (`listen_tls`).
27. Sessions (cookie firmada + store).
28. Auth middleware (Basic, Bearer, JWT).
29. CSRF middleware.
30. Rate limiting.
31. Password hashing (Argon2id).

### Fase 5 — DX y observabilidad (1-2 semanas)
32. Test client.
33. CLI helper (`serve`).
34. Modo dev con errores verbosos.
35. Métricas y health checks.
36. Route inspector.

### Fase 6 — Ejemplos y docs (1 semana)
37. Ejemplo chat (WebSocket + rooms).
38. Ejemplo API REST (JSON + validación).
39. Ejemplo juego simple (WS + broadcast de posiciones).
40. Documentar con `##` y generar `psc doc`.

---

## 8. Testing

- **Unit tests** por módulo: router (matching), body parsers, cookie signing, JWT.
- **Integration tests** con el `TestClient` (sin red): cada feature del framework.
- **Conformance HTTP**: probar contra la suite de `h2spec` (si HTTP/2) y casos de `llhttp`.
- **WebSocket conformance**: Autobahn Testsuite (obligatorio para producción).
- **Fuzzing**: el parser HTTP y el framing WS deben fuzzearse. `libFuzzer` sobre el C generado.
- **ASan** en CI para todos los módulos que asignan memoria (ver `PENGUNIC.es.md` §27.2).
- **Benchmarks**: `wrk` o `bombardier` contra el servidor echo; comparar con Node.js + Express como referencia.

---

## 9. Un ejemplo completo del resultado final

```pengu
import std.portus
import std.portus.ws
import std.spark
import std.json

rune ChatState:
    messages as list of string

bind ChatState with ws.Handler:
    weave on_open with conn as ref to ws.Conn into void:
        calling ws.broadcast with "joined"
    weave on_message with conn as ref to ws.Conn, msg as string into void:
        calling ws.broadcast with msg
    weave on_close with conn as ref to ws.Conn, code as int, reason as string into void:
        calling ws.broadcast with "left"
    weave on_binary with conn as ref to ws.Conn, data as slice of byte into void: ...
    weave on_error  with conn as ref to ws.Conn, err as string into void: ...
    weave on_pong   with conn as ref to ws.Conn into void: ...

weave list_messages with ctx as ref to portus.Ctx into result of void to portus.HttpError:
    calling portus.json with ctx, (with messages is ["hi", "there"])
    return calling ok_of with 0

weave main into int:
    var app as portus.App is calling portus.new_dev_app
    defer banish app

    calling portus.use with sigil of app, portus.middleware.logger
    calling portus.use with sigil of app, portus.middleware.cors_default
    calling portus.use with sigil of app, portus.middleware.security_headers

    calling portus.get with sigil of app, "/", list_messages
    calling portus.ws.get with sigil of app, "/chat", sigil of ChatState.new

    return calling portus.serve with sigil of app, 3000
```

Eso es lo que se siente al construir un backend con PenguScript: declarativo, tipado, con propiedad explícita y sin sorpresas. `portus` es la puerta; lo que hay detrás es tu aplicación.
