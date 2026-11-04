# portus

> Source: `src/portus.pengu`

portus — framework HTTP para PenguScript.

Un servidor HTTP/1.1 completo sobre `poll(2)` y llhttp, con la API de un
framework de rutas: `Application`, `Context`, `Handler` y un `HttpError` como
valor. Sin closures: los handlers son `weave`s con nombre.

## Ejemplo
```pengu
import std.spark
import portus

weave hello with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "hola"
    return calling portus.ok_void

weave main into int:
    var app as portus.Application is calling portus.new_application
    let added as result of void to portus.route.PatternError is calling portus.get with sigil of app, "/", hello
    let served as result of int to string is calling portus.serve with sigil of app, 3000
    return 0
```

## Memoria
`Application` es dueña de sus rutas: libérala con `portus.application.free_application`.
El servidor presta la aplicación mientras corre.

---

### `Application`

> alias Application

_Undocumented._

### `Context`

> alias Context

_Undocumented._

### `Handler`

> alias Handler

_Undocumented._

### `Params`

> alias Params

_Undocumented._

### `PatternError`

> alias PatternError

_Undocumented._

### `Request`

> alias Request

_Undocumented._

### `Response`

> alias Response

_Undocumented._

### `ServeOptions`

> alias ServeOptions

_Undocumented._

### `HttpError`

> alias HttpError

_Undocumented._

### `ErrorHandler`

> alias ErrorHandler

_Undocumented._

### `Middleware`

> alias Middleware

_Undocumented._

### `MiddlewareFn`

> alias MiddlewareFn

_Undocumented._

### `AppHook`

> alias AppHook

_Undocumented._

### `Group`

> alias Group

_Undocumented._

### `Cookies`

> alias Cookies

_Undocumented._

### `CookieOptions`

> alias CookieOptions

_Undocumented._

### `Queries`

> alias Queries

_Undocumented._

### `text_copy`

> **text_copy**(value: string) -> string

 Copia propia de una cadena (para guardar datos que sobreviven a la petición).

### `text_index`

> **text_index**(value: string, needle: string) -> int

 Posición de `needle` en `value`, o `-1`.

### `text_before`

> **text_before**(value: string, index: int) -> string

 Prefijo de `value` hasta `index`.

### `text_after`

> **text_after**(value: string, index: int) -> string

 Sufijo de `value` tras `index`.

### `PORTUS_VERSION`

> **PORTUS_VERSION** : string

 Versión del paquete.

### `new_application`

> **new_application**() -> Application

 Crea una aplicación vacía.

### `default_options`

> **default_options**() -> ServeOptions

 Opciones de arranque por defecto (`127.0.0.1:3000`).

### `ok_void`

> **ok_void**() -> result of void to HttpError

 `result of void to HttpError` con éxito.

### `err_void`

> **err_void**(failure: HttpError) -> result of void to HttpError

 `result of void to HttpError` con fallo.

### `get`

> **get**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `GET`.

### `options`

> **options**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `OPTIONS`.

### `post`

> **post**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `POST`.

### `put`

> **put**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `PUT`.

### `patch`

> **patch**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `PATCH`.

### `delete`

> **delete**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `DELETE`.

### `head`

> **head**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `HEAD`.

### `any`

> **any**(app: ref to Application, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta para cualquier método.

### `serve`

> **serve**(app: ref to Application, port: int) -> result of int to string

 Arranca el servidor en `port` y bloquea. Devuelve el puerto real.

 ## Memoria
 La aplicación debe vivir mientras el servidor corre.

### `new_group`

> **new_group**(app: ref to Application, prefix: string) -> Group

 Crea un grupo de rutas bajo `prefix` (`"/api"`).

 ## Memoria
 El `Group` presta la aplicación: no la libera ni la copia.

### `middleware`

> **middleware**(invoke: MiddlewareFn) -> Middleware

 Envuelve una función de middleware en un `Middleware` con su `data`.

### `use`

> **use**(app: ref to Application, layer: Middleware) -> void

 Añade un middleware para todas las rutas.

### `use_at`

> **use_at**(app: ref to Application, prefix: string, layer: Middleware) -> void

 Añade un middleware solo bajo `prefix`.

### `after`

> **after**(app: ref to Application, layer: Middleware) -> void

 Añade un middleware que corre después de la ruta.

### `on_error`

> **on_error**(app: ref to Application, handler: ErrorHandler) -> void

 Registra el manejador de errores de la aplicación.

### `on_not_found`

> **on_not_found**(app: ref to Application, handler: Handler) -> void

 Registra el manejador de rutas no encontradas.

### `on_start`

> **on_start**(app: ref to Application, hook: AppHook) -> void

 Registra un hook que corre al arrancar.

### `on_shutdown`

> **on_shutdown**(app: ref to Application, hook: AppHook) -> void

 Registra un hook que corre al apagar.

### `status_of`

> **status_of**(failure: HttpError) -> int

 Código de estado HTTP de un `HttpError`.

### `bad_request`

> **bad_request**(message: string) -> HttpError

 Crea un `HttpError` 400.

### `not_found`

> **not_found**() -> HttpError

 Crea un `HttpError` 404.

### `internal_error`

> **internal_error**() -> HttpError

 Crea un `HttpError` 500.

### `use_request_id`

> **use_request_id**(app: ref to Application) -> void

 Añade `X-Request-Id` aleatorio a cada respuesta.

### `use_security_headers`

> **use_security_headers**(app: ref to Application) -> void

 Añade las cabeceras de seguridad habituales.

### `use_cors`

> **use_cors**(app: ref to Application, origin: string) -> void

 Añade CORS para `origin`.

### `use_logger`

> **use_logger**(app: ref to Application) -> void

 Añade un log de una línea por petición.

### `accepts`

> **accepts**(context: ref to Context, media_type: string) -> bool

 Ping del binding nativo (humo de instalación).
 Negociación: `true` si el cliente acepta `media_type`.

### `best`

> **best**(context: ref to Context, offered: list of string) -> maybe string

 Negociación: mejor tipo de `offered`, o `none`.

### `use_compress`

> **use_compress**(app: ref to Application) -> void

 Comprime con gzip las respuestas que lo acepten.

### `mount_static`

> **mount_static**(app: ref to Application, prefix: string, root: string) -> result of void to PatternError

 Sirve `root` bajo `prefix` (`"/static"`; `""` para la raíz).

### `ping`

> **ping**(n: int) -> int

_Undocumented._

### `WebSocketConn`

> alias WebSocketConn

 Conexión WebSocket viva.

### `WebSocketHandler`

> alias WebSocketHandler

 Handler WebSocket con estado.

### `WebSocketMessageFn`

> alias WebSocketMessageFn

 Función que recibe cada mensaje WebSocket.

### `ws_handler`

> **ws_handler**(invoke: WebSocketMessageFn, data: ref to void) -> SocketHandler

 Construye un handler WebSocket con estado.

### `ws_text`

> **ws_text**(payload: ref to frozen u8, length: int) -> string

 Texto de un mensaje WebSocket (copia propia).

### `ws_upgrade`

> **ws_upgrade**(context: ref to Context, handler: WebSocketHandler) -> bool

 Acepta la conexión WebSocket de esta petición.

### `Session`

> alias Session

 Sesión de una petición (cookie firmada).

### `sessions_configure`

> **sessions_configure**(secret: string, ttl_ms: i64) -> void

 Fija el secreto de firma y la vida de las sesiones.

### `sessions_load`

> **sessions_load**(context: ref to Context) -> maybe Session

 Carga la sesión de la petición, si la hay y es válida.

### `sessions_start`

> **sessions_start**(context: ref to Context) -> Session

 Empieza una sesión nueva.

### `sessions_get`

> **sessions_get**(session: ref to Session, name: string) -> maybe string

 Lee un valor de la sesión.

### `sessions_set`

> **sessions_set**(session: ref to Session, name: string, value: string) -> void

 Escribe un valor en la sesión.

### `sessions_save`

> **sessions_save**(context: ref to Context, session: ref to Session) -> void

 Guarda la sesión y manda la cookie.

### `sessions_destroy`

> **sessions_destroy**(context: ref to Context, session: ref to Session) -> void

 Borra la sesión y su cookie.

### `basic_auth`

> **basic_auth**(context: ref to Context, user: string, password: string) -> maybe string

 Autentica Basic; si falla deja `WWW-Authenticate: Basic`.

### `bearer_auth`

> **bearer_auth**(context: ref to Context) -> maybe string

 Token Bearer de la petición.

### `bearer_required`

> **bearer_required**(context: ref to Context, expected: string) -> bool

 Comprueba un Bearer fijo.

### `jwt_sign`

> **jwt_sign**(secret: string, claims: string) -> string

 Firma un JWT HS256.

### `jwt_verify`

> **jwt_verify**(secret: string, token: string) -> bool

 Verifica la firma de un JWT HS256.

### `jwt_claims`

> **jwt_claims**(secret: string, token: string) -> maybe string

 Claims de un JWT HS256 válido.

### `jwt_bearer`

> **jwt_bearer**(context: ref to Context, secret: string) -> maybe string

 Claims del Bearer JWT de la petición, si la firma cuadra.

### `issue_csrf_token`

> **issue_csrf_token**(context: ref to Context) -> string

 Token CSRF de la petición (crea cookie si falta).

### `csrf_ok`

> **csrf_ok**(context: ref to Context) -> bool

 `true` si la cabecera CSRF coincide con la cookie.

### `csrf_protect`

> **csrf_protect**(context: ref to Context) -> result of void to HttpError

 Rechaza con 403 las peticiones que cambian estado sin CSRF válido.

### `use_rate_limit`

> **use_rate_limit**(app: ref to Application, max_requests: int, window_ms: i64) -> void

 Limita las peticiones por cliente (ventana fija).

### `use_rate_limit_at`

> **use_rate_limit_at**(app: ref to Application, prefix: string, max_requests: int, window_ms: i64) -> void

 Limita las peticiones bajo `prefix`.

### `use_csp`

> **use_csp**(app: ref to Application, policy: string) -> void

 Añade la cabecera `Content-Security-Policy`.

### `MultipartPart`

> alias MultipartPart

 Parte de un formulario `multipart/form-data`.

### `multipart_part`

> **multipart_part**(parts: list of MultipartPart, name: string) -> maybe MultipartPart

 Primera parte con ese nombre.

### `free_multipart_part`

> **free_multipart_part**(part: MultipartPart) -> void

 Libera una parte.

### `free_multipart`

> **free_multipart**(parts: list of MultipartPart) -> void

 Libera una lista de partes.

