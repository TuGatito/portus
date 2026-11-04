# Portus — API de referencia (objetivo v1)

> Firmas **objetivo**. Se ajustan a lo que el compilador 0.28.0 acepte durante
> F0/F1; cualquier cambio se anota en [`portus-decisions.md`](portus-decisions.md).
>
> Convenciones:
> - `enchanting T` lista los métodos de `T` (se llaman `calling x.m with …`).
> - Un parámetro `handler as weave with context as ref to Context into result
>   of void to HttpError` acepta una función nombrada (thin handler).
> - Todo lo que devuelve `string`/`list`/`map`/`rune` es **owned** por el
>   llamador salvo que diga lo contrario.
> - Los strings de `context.request` son del contexto: no se guardan.
> - Los `send_*` **toman posesión** del cuerpo.

---

## 1. `portus` (fachada)

```pengu
import portus

# Tipos reexportados con `alias`
alias Application as portus.application.Application
alias Router      as portus.router.Router
alias Context     as portus.context.Context
alias Request     as portus.request.Request
alias Response    as portus.response.Response
alias Handler     as portus.handler.Handler
alias Config      as portus.config.Config
alias Server      as portus.server.Server
alias HttpError   as portus.error.HttpError
alias Headers     as portus.header.Headers
alias Cookie      as portus.cookie.Cookie
alias CookieJar   as portus.cookie.CookieJar
alias TestClient  as portus.test_client.TestClient
alias TestResponse as portus.test_client.TestResponse

omen ListenError:
    AddressInUse
    PermissionDenied
    BadAddress
    System with code as int

weave new_application into Application
weave new_application_with with config as Config into Application
weave new_router into Router
weave new_config into Config

weave ok_void into result of void to HttpError
weave err_void with error as HttpError into result of void to HttpError
weave handler_with with invoke as weave with data as ref to void, context as ref to Context into result of void to HttpError, data as ref to void into Handler

weave serve with app as ref to Application, port as int into result of void to ListenError
weave serve_at with app as ref to Application, host as string, port as int into result of void to ListenError
```

`serve` = `listen_at` + `run` + parada limpia con SIGINT/SIGTERM.

---

## 2. `portus.config`

```pengu
import portus.config

rune Config:
    host as string
    port as int
    max_body_bytes as int
    max_header_bytes as int
    max_connections as int
    request_timeout_ms as int
    idle_timeout_ms as int
    keep_alive as bool
    dev_mode as bool
    trust_proxy as bool
    strict_trailing_slash as bool
    case_sensitive as bool
    server_name as string

weave new_config into Config
weave config_for with host as string, port as int into Config
```

`new_config` devuelve los valores de la tabla del diseño. Se personaliza con
`set` campo a campo o con un literal `with`.

---

## 3. `portus.error`

```pengu
import portus.error

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

weave status_code with error as HttpError into int
weave reason with error as HttpError into string
weave public_message with error as HttpError into string
weave is_client_error with error as HttpError into bool
weave is_server_error with error as HttpError into bool
```

`public_message` devuelve `message` solo en `dev_mode`; en producción, un texto
genérico por código. El dispatcher la usa para el cuerpo.

---

## 4. `portus.status`

```pengu
import portus.status

weave reason_phrase with code as int into string

const STATUS_OK as int is 200
const STATUS_CREATED as int is 201
const STATUS_ACCEPTED as int is 202
const STATUS_NO_CONTENT as int is 204
const STATUS_MOVED_PERMANENTLY as int is 301
const STATUS_FOUND as int is 302
const STATUS_NOT_MODIFIED as int is 304
const STATUS_BAD_REQUEST as int is 400
const STATUS_UNAUTHORIZED as int is 401
const STATUS_FORBIDDEN as int is 403
const STATUS_NOT_FOUND as int is 404
const STATUS_METHOD_NOT_ALLOWED as int is 405
const STATUS_CONFLICT as int is 409
const STATUS_PAYLOAD_TOO_LARGE as int is 413
const STATUS_UNSUPPORTED_MEDIA_TYPE as int is 415
const STATUS_TOO_MANY_REQUESTS as int is 429
const STATUS_INTERNAL as int is 500
const STATUS_NOT_IMPLEMENTED as int is 501
const STATUS_SERVICE_UNAVAILABLE as int is 503

const METHOD_GET as int is 1
const METHOD_POST as int is 2
const METHOD_PUT as int is 3
const METHOD_PATCH as int is 4
const METHOD_DELETE as int is 5
const METHOD_HEAD as int is 6
const METHOD_OPTIONS as int is 7
const METHOD_ANY as int is 0

weave method_code with method as string into int
weave method_name with code as int into string
```

---

## 5. `portus.handler`

```pengu
import portus.handler

omen HandlerShape:
    Thin
    Thick

rune Handler:
    shape as HandlerShape
    thin as weave with context as ref to Context into result of void to HttpError
    thick as weave with data as ref to void, context as ref to Context into result of void to HttpError
    data as ref to void

weave thin_handler with function as weave with context as ref to Context into result of void to HttpError into Handler
weave thick_handler with function as weave with data as ref to void, context as ref to Context into result of void to HttpError, data as ref to void into Handler
```

`thin_handler` es lo que usa `app.get` internamente. Un `Handler` copia el
puntero; no copia `data`. El campo que no se usa se inicializa a `null`; si el
compilador no acepta `null` en un campo de tipo función, se usa `_no_handler`
(un weave vacío) como centinela.

---

## 6. `portus.route`

```pengu
import portus.route

omen Segment:
    Literal with text as string
    Param with name as string
    Wildcard with name as string
    CatchAll

omen PatternError:
    Empty
    NotAbsolute with pattern as string
    BadParam with pattern as string
    BadWildcard with pattern as string

rune Pattern:
    raw as string
    segments as list of Segment
    is_prefix as bool
    score as int

weave compile_pattern with raw as string, is_prefix as bool into result of Pattern to PatternError
weave match_pattern with pattern as ref to Pattern, path as string into maybe map of string to string
weave path_matches with pattern as ref to Pattern, path as string into bool
```

`Pattern` es owned (`segments`); `match_pattern` devuelve `map` owned o `none`.

---

## 7. `portus.header`

```pengu
import portus.header

rune Headers:
    # privado: _entries as list of (string, string)  # nombre en minúsculas, valor

weave new_headers into Headers
weave parse_headers with raw as list of (string, string) into Headers

enchanting Headers:
    weave get with name as string into maybe string
    weave get_or with name as string, fallback as string into string
    weave set with name as string, value as string into void
    weave add with name as string, value as string into void
    weave remove with name as string into void
    weave has with name as string into bool
    weave count into int
    weave name_at with index as int into string
    weave value_at with index as int into string
    weave clear into void
```

Nombres **case-insensitive** (se comparan en minúsculas). `set` reemplaza;
`add` añade (para `Set-Cookie`, `Vary`). `Headers` posee sus strings.

---

## 8. `portus.request`

```pengu
import portus.request

rune Request:
    method as string
    method_code as int
    target as string
    path as string
    query as map of string to list of string
    version as string
    headers as Headers
    body as string
    peer_ip as string
    host as string
    is_secure as bool
    wants_close as bool
```

Solo datos; el comportamiento está en `Context`.

---

## 9. `portus.response`

```pengu
import portus.response

rune Response:
    status as int
    headers as Headers
    body as string
    sent as bool
```

---

## 10. `portus.context`

```pengu
import portus.context

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
    # privados: _native, _streaming, _sent

enchanting Context:
    # --- petición ---
    weave method into string
    weave path into string
    weave query with name as string into maybe string
    weave queries with name as string into list of string
    weave header with name as string into maybe string
    weave cookie with name as string into maybe string
    weave param with name as string into maybe string
    weave param_or with name as string, fallback as string into string
    weave host into string
    weave peer_ip into string
    weave is_secure into bool
    weave is_sent into bool
    weave is_streaming into bool
    weave request_id into string
    weave elapsed_ms into long

    # --- cuerpo ---
    weave body_bytes into slice of byte
    weave body_text into string
    weave body_json into result of json.JsonValue to body.BodyError
    weave body_form into result of map of string to string to body.BodyError
    weave body_multipart into result of list of body.Part to body.BodyError

    # --- estado ---
    weave get_state with key as string into maybe string
    weave set_state with key as string, value as string into void

    # --- respuesta ---
    weave set_status with code as int into void
    weave set_header with name as string, value as string into void
    weave add_header with name as string, value as string into void
    weave remove_header with name as string into void
    weave set_content_type with value as string into void
    weave set_cookie with cookie as Cookie into void
    weave delete_cookie with name as string into void

    # --- envío ---
    weave send_text with body as string into result of void to HttpError
    weave send_html with body as string into result of void to HttpError
    weave send_json with value as json.JsonValue into result of void to HttpError
    weave send_json_text with text as string into result of void to HttpError
    weave send_bytes with data as slice of byte into result of void to HttpError
    weave send_body with status as int, content_type as string, body as string into result of void to HttpError
    weave send_status with code as int into result of void to HttpError
    weave no_content into result of void to HttpError
    weave not_modified with etag as string into result of void to HttpError
    weave redirect with location as string into result of void to HttpError
    weave redirect_permanent with location as string into result of void to HttpError
    weave send_file with file_path as string into result of void to HttpError

    # --- streaming ---
    weave stream_open with status as int, content_type as string into result of Stream to HttpError
    weave sse_open into result of sse.Stream to HttpError
```

Reglas:

- Llamar dos veces a un `send_*` es un error de programación: el segundo
  devuelve `err_of with HttpError.Internal` (se detecta por `_sent`).
- `send_text` fija `Content-Type: text/plain; charset=utf-8` y `Content-Length`.
- `send_json` serializa con `std.json`.
- `send_file` necesita ETag/mtime del shim (F3).

---

## 11. `portus.application`

```pengu
import portus.application

rune Application:
    # privados: _config, _befores, _routes, _finishers, _arena, _state,
    #           _on_error, _on_not_found, _on_start, _on_shutdown, _server, _sealed

enchanting Application:
    # --- rutas ---
    weave get with pattern as string, handler as weave with context as ref to Context into result of void to HttpError into void
    weave post with pattern as string, handler as ...
    weave put with pattern as string, handler as ...
    weave patch with pattern as string, handler as ...
    weave delete with pattern as string, handler as ...
    weave head with pattern as string, handler as ...
    weave options with pattern as string, handler as ...
    weave any with pattern as string, handler as ...
    weave route with method as string, pattern as string, handler as ... into void

    # --- middleware ---
    weave use with handler as ... into void
    weave use_at with prefix as string, handler as ... into void
    weave use_handler with handler as Handler into void
    weave use_handler_at with prefix as string, handler as Handler into void
    weave finish_handler with handler as Handler into void
    weave finish_handler_at with prefix as string, handler as Handler into void
    weave mount with prefix as string, router as Router into void

    # --- hooks ---
    weave on_error with handler as ... into void
    weave on_not_found with handler as ... into void
    weave on_start with callback as weave with app as ref to Application into void into void
    weave on_shutdown with callback as weave with app as ref to Application into void into void

    # --- estado y configuración ---
    weave config into Config
    weave arena into ref to arena_module.Arena   # import std.arena as arena_module
    weave get_state with key as string into maybe string
    weave set_state with key as string, value as string into void
    weave remove_state with key as string into void
    weave is_sealed into bool

    # --- ciclo de vida ---
    weave listen into result of Server to ListenError
    weave listen_at with host as string, port as int into result of Server to ListenError
```

`get`/`post`/… son **thin**. Para un handler configurado: `use_handler`.
`mount` consume el `Router` (queda vacío).

---

## 12. `portus.router`

```pengu
import portus.router

rune Router:
    # privados: _layers

weave new_router into Router

enchanting Router:
    weave get with pattern as string, handler as ... into void
    weave post with pattern as string, handler as ... into void
    weave put with pattern as string, handler as ... into void
    weave patch with pattern as string, handler as ... into void
    weave delete with pattern as string, handler as ... into void
    weave head with pattern as string, handler as ... into void
    weave options with pattern as string, handler as ... into void
    weave any with pattern as string, handler as ... into void
    weave use with handler as ... into void
    weave use_at with prefix as string, handler as ... into void
    weave mount with prefix as string, router as Router into void
    weave is_empty into bool
```

---

## 13. `portus.server`

```pengu
import portus.server

rune Server:
    # privados: _handle, _app, _port

enchanting Server:
    weave port into int
    weave run into result of void to ServerError
    weave stop into void
    weave is_running into bool

omen ServerError:
    BindFailed with code as int
    AlreadyRunning
    System with code as int
```

`run` bloquea hasta `stop` o señal. `stop` es seguro desde un handler.

---

## 14. `portus.test_client`

```pengu
import portus.test_client

rune TestClient:
    # privado: _app

rune TestResponse:
    status as int
    headers as Headers
    body as string

weave new_test_client with app as ref to Application into TestClient

enchanting TestClient:
    weave request with method as string, path as string into TestResponse
    weave request_with with method as string, path as string, headers as Headers, body as string into TestResponse
    weave get with path as string into TestResponse
    weave post_json with path as string, body as string into TestResponse
    weave cookie with request as ref to TestResponse, name as string into maybe string
```

En proceso: no abre sockets. `TestResponse.headers`/`body` son owned.

---

## 15. `portus.cookie`

```pengu
import portus.cookie

omen SameSite:
    Strict
    Lax
    None

rune Cookie:
    name as string
    value as string
    path as string
    domain as string
    max_age as int
    expires as string
    is_secure as bool
    is_http_only as bool
    same_site as SameSite

weave new_cookie with name as string, value as string into Cookie
weave parse_cookies with header as string into map of string to string
weave sign with value as string, secret as string into string
weave unsign with value as string, secret as string into maybe string
```

`sign` = HMAC-SHA256 + base64url + prefijo `v1.`. `unsign` compara en tiempo
constante.

---

## 16. `portus.mime`

```pengu
import portus.mime

weave mime_for with path as string into string
weave is_text with mime as string into bool
weave charset_for with mime as string into string
```

---

## 17. `portus.body`

```pengu
import portus.body

omen BodyError:
    TooLarge
    BadJson with message as string
    BadForm with message as string
    BadMultipart with message as string
    Unsupported with content_type as string

rune Part:
    name as string
    file_name as string
    content_type as string
    data as string

weave parse_form with body as string, content_type as string into result of map of string to string to BodyError
weave parse_multipart with body as string, boundary as string into result of list of Part to BodyError
```

---

## 18. `portus.static_file`

```pengu
import portus.static_file

rune StaticOptions:
    root as string
    prefix as string
    index as string
    max_age_seconds as int
    is_immutable as bool
    etag as bool
    last_modified as bool
    dotfiles as bool

weave new_static_options with root as string into StaticOptions
weave use_static with app as ref to Application, options as StaticOptions into void
```

`use_static` monta un finisher/antes que sirve ficheros y no toca las rutas
ajenas.

---

## 19. `portus.sse`

```pengu
import portus.sse

rune Stream:
    # privado: _native, _id

weave sse_open with context as ref to Context into result of Stream to HttpError

enchanting Stream:
    weave send with event as string, data as string into result of void to HttpError
    weave send_json with event as string, value as json.JsonValue into result of void to HttpError
    weave comment with text as string into result of void to HttpError
    weave close into void
```

---

## 20. `portus.websocket`

```pengu
import portus.websocket

rune Connection:
    # privados: _handle, _room

concept Handler:
    weave on_open with connection as ref to Connection into void
    weave on_message with connection as ref to Connection, message as string into void
    weave on_binary with connection as ref to Connection, data as slice of byte into void
    weave on_close with connection as ref to Connection, code as int into void

weave get shard H where H: Handler with app as ref to Application, pattern as string, handler as ref to H into void
weave handshake with request as ref to Request into result of string to HttpError

enchanting Connection:
    weave send with message as string into result of void to HttpError
    weave send_binary with data as slice of byte into result of void to HttpError
    weave close with code as int into void
    weave ping with data as string into result of void to HttpError
    weave join with room as string into void
    weave leave into void
    weave room into string
    weave ip into string

weave broadcast with message as string into void
weave broadcast_room with room as string, message as string into void
weave broadcast_except with connection as ref to Connection, message as string into void
weave connection_count into int
weave connection_count_in with room as string into int
```

El `handler` debe vivir más que la app (normalmente una variable de `main`).
`get` guarda un `ref to H`; no lo copia.

---

## 21. Middlewares

Todos registran con `use_handler`/`finish_handler` y alojan su config en la
arena. Firma común: `weave use_<nombre> with app as ref to Application, … into void`.

### `portus.middleware.cors`

```pengu
rune CorsOptions:
    origins as list of string
    methods as list of string
    headers as list of string
    expose_headers as list of string
    credentials as bool
    max_age_seconds as int

weave new_cors_options into CorsOptions
weave use_cors with app as ref to Application, options as CorsOptions into void
```

### `portus.middleware.logger`

```pengu
weave use_logger with app as ref to Application into void
weave use_logger_with with app as ref to Application, format as string into void
```

Formato: `%m %p %s %b %Dms %i`. Escribe a stdout con `io.write_stdout`.

### `portus.middleware.request_id`

```pengu
weave use_request_id with app as ref to Application into void
weave use_request_id_with with app as ref to Application, header as string into void
```

Reutiliza `X-Request-Id` si viene; si no, 16 bytes aleatorios en hex.

### `portus.middleware.security_headers`

```pengu
weave use_security_headers with app as ref to Application into void
```

`X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy`,
`Content-Security-Policy` (configurable por `use_security_headers_with`),
`Strict-Transport-Security` solo si `is_secure`.

### `portus.middleware.compress`

```pengu
rune CompressOptions:
    threshold_bytes as int
    gzip as bool
    deflate as bool

weave new_compress_options into CompressOptions
weave use_compress with app as ref to Application, options as CompressOptions into void
```

### `portus.middleware.session`

```pengu
rune SessionOptions:
    cookie_name as string
    secret as string
    max_age_seconds as int
    is_secure as bool
    same_site as cookie.SameSite

rune Session:
    # privado: _id, _data

weave new_session_options with secret as string into SessionOptions
weave use_session with app as ref to Application, options as SessionOptions into void
weave session_of with context as ref to Context into maybe ref to Session

enchanting Session:
    weave get with key as string into maybe string
    weave set with key as string, value as string into void
    weave remove with key as string into void
    weave destroy into void
```

Store en memoria (`map`), con expiración perezosa.

### `portus.middleware.auth`

```pengu
weave use_basic_auth with app as ref to Application, verify as weave with user as string, password as string into bool into void
weave use_bearer with app as ref to Application, verify as weave with token as string into bool into void
weave bearer_token with context as ref to Context into maybe string
weave basic_credentials with context as ref to Context into maybe (string, string)
```

### `portus.middleware.csrf`

```pengu
rune CsrfOptions:
    cookie_name as string
    header_name as string
    secret as string

weave new_csrf_options with secret as string into CsrfOptions
weave use_csrf with app as ref to Application, options as CsrfOptions into void
weave csrf_token with context as ref to Context into string
```

### `portus.middleware.rate_limit`

```pengu
rune RateLimitOptions:
    max_requests as int
    window_ms as int
    key as weave with context as ref to Context into string

weave new_rate_limit_options with max_requests as int, window_ms as int into RateLimitOptions
weave use_rate_limit with app as ref to Application, options as RateLimitOptions into void
```

### `portus.middleware.timeout`

```pengu
weave use_timeout with app as ref to Application, timeout_ms as int into void
```

Marca el deadline en el contexto; el shim cierra la conexión si se supera.

---

## 22. Internos (no documentados para el usuario)

- `portus.transport`: `run_server`, `respond`, `write_chunk`, `accept_upgrade`.
- `portus.dispatcher`: `dispatch`.
- `portus.layer`: `Layer`, `new_layer`, `layer_matches`.
- `portus.application`: `add_layer`, `sorted_routes`.

---

## 23. `portus.native.d.pengu` (binding)

```pengu
include "portus_native.h"

alias ServerHandle as opaque
alias Connection as opaque

declare portus_server_new with host as ref to char, port as int, on_request as weave with userdata as ref to void, request as ref to void into void, userdata as ref to void into ref to ServerHandle
declare portus_server_run with server as ref to ServerHandle into int
declare portus_server_stop with server as ref to ServerHandle into void
declare portus_server_free with server as ref to ServerHandle into void
declare portus_server_port with server as ref to ServerHandle into int
declare portus_server_install_signals with server as ref to ServerHandle into void

declare portus_request_method with request as ref to void into ref to char
declare portus_request_target with request as ref to void, length as ref to int into ref to char
declare portus_request_version with request as ref to void into ref to char
declare portus_request_header_count with request as ref to void into int
declare portus_request_header_name with request as ref to void, index as int, length as ref to int into ref to char
declare portus_request_header_value with request as ref to void, index as int, length as ref to int into ref to char
declare portus_request_body with request as ref to void, length as ref to int into ref to char
declare portus_request_peer_ip with request as ref to void into ref to char
declare portus_request_is_secure with request as ref to void into int
declare portus_request_wants_close with request as ref to void into int
declare portus_request_is_upgrade with request as ref to void into int

declare portus_request_respond with request as ref to void, status as int, headers as ref to char, header_length as int, body as ref to char, body_length as int into void
declare portus_request_begin_stream with request as ref to void, status as int, headers as ref to char, header_length as int into int
declare portus_request_write_chunk with request as ref to void, data as ref to char, length as int into int
declare portus_request_end_stream with request as ref to void into void
declare portus_request_accept_upgrade with request as ref to void, headers as ref to char, header_length as int into int

declare portus_sha1 with data as ref to char, length as int, out as ref to byte into void
declare portus_random_bytes with buffer as ref to byte, length as int into int
declare portus_constant_time_eq with a as ref to char, b as ref to char, length as int into int
declare portus_now_ms into long
declare portus_stat with path as ref to char, size as ref to long, mtime_ms as ref to long into int
```

Las cabeceras se pasan como un blob ya serializado (`Nombre: valor\r\n` por
línea) para no cruzar la frontera N veces por respuesta.

## F2 — cuerpos, query, cookies y middlewares (implementado)

Añadidos a `Context`:

```pengu
weave queries into portus.query.Query
weave query_param with name as string into maybe string
weave body_text into string
weave body_json into result of portus.json.JsonValue to portus.json.JsonError
weave body_form into portus.query.Query
weave cookie with name as string into maybe string
weave set_cookie with name as string, value as string into void
weave set_cookie_with with name as string, value as string, options as portus.cookie.CookieOptions into void
weave delete_cookie with name as string into void
weave send_json with value as portus.json.JsonValue into void
weave send_body_with_type with content_type as string, body as string into void
weave status_code into int
```

Módulos nuevos:

- `portus.query`: `Query`, `QueryPair`, `parse_query`, `query_get`, `query_has`,
  `free_query`.
- `portus.cookie`: `Cookies`, `CookiePair`, `CookieOptions`, `SESSION_OPTIONS`,
  `parse_cookies`, `cookie_get`, `cookie_has`, `serialize_cookie`,
  `clear_cookie`, `free_cookies`.
- `portus.middleware`: `add_request_id`, `add_security_headers`, `add_cors`,
  `add_logger`.
- `portus.text` (interno): `free_strings`, `equals_ignore_case`, `from_bytes`,
  `from_cstr`.

Fachada: `use_request_id`, `use_security_headers`, `use_cors(app, origin)`,
`use_logger`, y `handler.middleware_with(invoke, data)` para middlewares con
configuración.

## F2 — estáticos, compresión, negociación y streaming (implementado)

```pengu
# application
weave add_thick with app as ref to Application, method as string, pattern as string, invoke as MiddlewareFn, data as ref to void into result of void to route.PatternError

# portus.static_file
weave mount with app as ref to Application, prefix as string, root as string into result of void to route.PatternError

# portus.mime
weave for_path with path as string into string

# fachada
weave use_compress with app as ref to Application into void
weave mount_static with app as ref to Application, prefix as string, root as string into result of void to route.PatternError
weave accepts with context as ref to Context, media_type as string into bool
weave best with context as ref to Context, offered as list of string into maybe string

# Context
weave sse_open into bool
weave sse_send with event as string, data as string into bool
weave sse_close into bool
weave stream_begin with content_type as string into bool
weave stream_write with text as string into bool
weave stream_end into bool
weave mark_sent into void

# transport.Response
weave stream_begin with content_type as string into bool
weave stream_write_text with text as string into bool
weave stream_end into bool
```

Nativo añadido: `portus_native_response_body`, `portus_native_response_set_body`,
`portus_native_response_header_value`, `portus_native_response_stream_begin`,
`portus_native_response_stream_write`, `portus_native_response_stream_end`,
`portus_native_file_info` (tamaño y mtime para `ETag`/`Last-Modified`).

Comportamiento de los estáticos: `GET /static/*file`; `..` o rutas absolutas →
404; `index.html` cuando el comodín viene vacío; `ETag` = `"tamaño-mtime"`;
`If-None-Match` → 304; `Range: bytes=a-b` → 206 con `Content-Range`, rango
inválido → 416. `use_compress` es un finisher: `Accept-Encoding` con gzip,
cuerpo ≥ 860 bytes y tipo compresible.

## F3 — WebSocket

```pengu
# portus.websocket
rune Conn:
    _res as ref to void

alias MessageFn as weave with data as ref to void, conn as ref to Conn, opcode as int, payload as ref to frozen byte, length as int into void

rune SocketHandler:
    invoke as MessageFn
    data as ref to void

weave handler_with with invoke as MessageFn, data as ref to void into SocketHandler
weave text_of with payload as ref to frozen byte, length as int into string
weave upgrade with context as ref to Context, handler as SocketHandler into bool

# métodos de Conn
weave send_text with body as string into bool
weave send_bytes with data as ref to frozen byte, length as int into bool
weave close into bool
weave is_text with opcode as int into bool

# fachada
alias WebSocketConn as websocket.Conn
alias WebSocketHandler as websocket.SocketHandler
alias WebSocketMessageFn as websocket.MessageFn
weave ws_handler with invoke as WebSocketMessageFn, data as ref to void into websocket.SocketHandler
weave ws_text with payload as ref to frozen byte, length as int into string
weave ws_upgrade with context as ref to Context, handler as WebSocketHandler into bool
```

Nativo añadido: `portus_native_ws_upgrade`, `portus_native_ws_send` y
`portus_native_ws_close`, más el tipo `portus_ws_fn`. El mensaje es un frame
completo (sin fragmentación ni `permessage-deflate` en v1); `ping` se responde en
C; el cierre con código 1000 lo inicia el C o el cliente.

El callback recibe `opcode` 0 al abrir (registrar la conexión), 1 texto, 2
binario y 8 cierre. `Conn` es estable durante toda la conexión, así que se puede
guardar en una lista de conexiones para broadcast; se libera tras el `opcode` 8.
