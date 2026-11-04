# portus.application

> Source: `src/portus/application.pengu`

portus.application — la aplicación: rutas, middlewares, despacho y errores.

Una `Application` guarda rutas (método + patrón + handler) ordenadas por
especificidad, middlewares que corren antes y después de la ruta, y los
hooks de error y ciclo de vida. El despacho construye un `Context` por
petición y lo pasa siempre por `ref to`.

## Ejemplo
```pengu
var app as portus.Application is calling portus.new_application
calling portus.get with sigil of app, "/", hello
let served as result of int to string is calling portus.serve with sigil of app, 3000
```

## Memoria
`free_application` libera las rutas, los middlewares y la arena de
configuración. Los handlers y los hooks son código: no se copian.

---

### `Context`

> alias Context

_Undocumented._

### `ErrorHandler`

> alias ErrorHandler

_Undocumented._

### `Handler`

> alias Handler

_Undocumented._

### `Middleware`

> alias Middleware

_Undocumented._

### `AppHook`

> alias AppHook

 Hook de ciclo de vida (`on_start`, `on_shutdown`).

### `RouteEntry`

> rune RouteEntry

```pengu
    method as string
    pattern as Pattern
    handler as Handler
    invoke as MiddlewareFn
    data as ref to void
    is_thick as bool
    score as int
```

 Una ruta registrada.

### `MiddlewareEntry`

> rune MiddlewareEntry

```pengu
    prefix as string
    middleware as Middleware
```

 Un middleware registrado con el prefijo de rutas al que aplica.

### `Application`

> rune Application

```pengu
    _routes as list of RouteEntry
    _befores as list of MiddlewareEntry
    _finishers as list of MiddlewareEntry
    _arena as Arena
    _on_error as ErrorHandler
    _has_on_error as bool
    _on_not_found as Handler
    _has_on_not_found as bool
    _on_start as AppHook
    _has_on_start as bool
    _on_shutdown as AppHook
    _has_on_shutdown as bool
    _sealed as bool
```

 La aplicación completa.

### `_no_error_handler`

> **_no_error_handler**(ctx: ref to Context, failure: HttpError) -> result of void to HttpError

 Handler de error por defecto: deja que portus escriba la respuesta.

### `_no_not_found`

> **_no_not_found**(ctx: ref to Context) -> result of void to HttpError

 Handler de 404 por defecto: no escribe nada; lo hace `respond_error`.

### `_no_thick`

> **_no_thick**(data: ref to void, ctx: ref to Context) -> result of void to HttpError

 Middleware de relleno para las rutas finas.

### `_no_handler`

> **_no_handler**(ctx: ref to Context) -> result of void to HttpError

 Handler fino de relleno para las rutas gruesas.

### `_no_hook`

> **_no_hook**(app: ref to Application) -> void

 Hook vacío.

### `new_application`

> **new_application**() -> Application

 Crea una aplicación vacía.

### `route_count`

> **route_count**(app: ref to Application) -> int

 Cuántas rutas hay registradas.

### `arena_of`

> **arena_of**(app: ref to Application) -> ref to Arena

 La arena donde viven las configuraciones de middleware.

 ## Memoria
 Devuelve un préstamo; la arena se libera con `free_application`.

### `use`

> **use**(app: ref to Application, middleware: Middleware) -> void

 Registra un middleware que corre antes de las rutas en todo el árbol.

### `use_at`

> **use_at**(app: ref to Application, prefix: string, middleware: Middleware) -> void

 Registra un middleware sólo para las rutas que empiezan por `prefix`.

### `after`

> **after**(app: ref to Application, middleware: Middleware) -> void

 Registra un middleware que corre después de la ruta (para métricas, logs…).

### `on_error`

> **on_error**(app: ref to Application, handler: ErrorHandler) -> void

 Registra el handler que convierte un `HttpError` en respuesta.

### `on_not_found`

> **on_not_found**(app: ref to Application, handler: Handler) -> void

 Registra el handler de rutas no encontradas.

### `on_start`

> **on_start**(app: ref to Application, hook: AppHook) -> void

 Registra un hook que corre al arrancar el servidor.

### `on_shutdown`

> **on_shutdown**(app: ref to Application, hook: AppHook) -> void

 Registra un hook que corre al detenerse el servidor.

### `add`

> **add**(app: ref to Application, method: string, pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta.

 `method` es `"GET"`, `"POST"`, … o `"*"`. El patrón se compila aquí; un
 patrón inválido es un error de programación y se devuelve como valor.

 ## Memoria
 La ruta guarda el `Pattern` compilado; el handler es código, no se copia.

### `_sort_last`

> **_sort_last**(app: ref to Application) -> void

 Reubica la última ruta en su sitio por especificidad.

### `free_application`

> **free_application**(app: ref to Application) -> void

 Libera las rutas, los middlewares y la arena. No libera el código.

### `dispatch`

> **dispatch**(app: ref to Application, request: ref to Request, response: ref to Response) -> void

 Despacha una petición ya parseada: befores, ruta, finishers y error.

 ## Memoria
 Presta `request` y `response`; libera el `Params` que construye.

### `_empty_params`

> **_empty_params**() -> Params

 Un `Params` vacío (para peticiones que no encajan en ninguna ruta).

### `_run_befores`

> **_run_befores**(app: ref to Application, ctx: ref to Context) -> maybe HttpError

 Corre los middlewares de antes. Devuelve el primer fallo, si lo hay.

### `_run_finishers`

> **_run_finishers**(app: ref to Application, ctx: ref to Context) -> void

 Corre los middlewares de después; sus fallos no cambian la respuesta.

### `_matches_prefix`

> **_matches_prefix**(ctx: ref to Context, prefix: string) -> bool

 ¿La ruta de la petición empieza por `prefix`?

### `_run_route`

> **_run_route**(app: ref to Application, ctx: ref to Context) -> maybe HttpError

 Busca la ruta y ejecuta su handler. Devuelve el fallo, si lo hay.

 Automatismos: `HEAD` cae en la ruta `GET` si no hay una `HEAD` propia (el
 transporte omite el cuerpo), y `OPTIONS` sin ruta propia responde `204` con
 `Allow`.

### `_invoke_entry`

> **_invoke_entry**(entry: RouteEntry, ctx: ref to Context) -> result of void to HttpError

 Ejecuta la ruta, sea fina o gruesa.

### `add_thick`

> **add_thick**(app: ref to Application, method: string, pattern: string, invoke: MiddlewareFn, data: ref to void) -> result of void to PatternError

 Registra una ruta con un handler que lleva configuración (`data`).

 ## Memoria
 `data` debe vivir tanto como la aplicación (normalmente en su arena).

### `_handle_failure`

> **_handle_failure**(app: ref to Application, ctx: ref to Context, failure: HttpError) -> void

 Convierte un fallo en respuesta (o delega en `on_error` / `on_not_found`).

### `_join_methods`

> **_join_methods**(methods: list of string) -> string

 Junta métodos permitidos en el valor de `Allow`.

### `respond_error`

> **respond_error**(response: ref to Response, failure: HttpError) -> void

 Escribe la respuesta de error (a menos que ya se haya enviado algo).

 ## Memoria
 El cuerpo se copia en la respuesta.

### `_as_raw_context`

> **_as_raw_context**(app: ref to Application) -> ref to void

 Devuelve el puntero de la aplicación como `ref to void` sin tomar la dirección
 del parámetro (el código generado para una conversión implícita usaría `&app`).

### `serve_with`

> **serve_with**(app: ref to Application, options: ServeOptions) -> result of int to string

 Arranca el servidor con la aplicación como despachador y unas opciones dadas.

 ## Memoria
 La aplicación debe vivir mientras el servidor corre (esta llamada bloquea).

### `_serve_bridge`

> **_serve_bridge**(data: ref to void, request: ref to Request, response: ref to Response) -> void

 Puente con `data`: reenvía al despachador de la aplicación.

