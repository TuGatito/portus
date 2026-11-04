# portus.session

> Source: `src/portus/session.pengu`

portus.session — sesiones con cookie firmada y almacén en memoria.

Una sesión es un `id` aleatorio de 32 hex en una cookie `HttpOnly` firmada
con HMAC-SHA-256. Los datos viven en memoria del proceso, con expiración;
al cerrar el proceso se pierden (para persistencia, ver
`portus-deferred.md`).

## Ejemplo
```pengu
weave login with context as ref to portus.Context into result of void to portus.HttpError:
    var session as portus.session.Session is calling portus.session.start with context
    calling portus.session.set with sigil of session, "user", "ada"
    calling portus.session.save with context, sigil of session
    calling context.send_text with "dentro"
    return calling portus.ok_void
```

## Memoria
`load` devuelve un `Session` prestado (no lo liberes); `start` crea uno que
debes `save` o dejar caducar. Las cadenas que devuelve `get` son copias
propias.

---

### `COOKIE_NAME`

> **COOKIE_NAME** : string

 Nombre de la cookie de sesión.

### `DEFAULT_TTL_MS`

> **DEFAULT_TTL_MS** : i64

 Vida por defecto de una sesión (1 hora).

### `Store`

> rune Store

```pengu
    entries as map of string to string
    expiry as map of string to i64
    secret as string
    ttl_ms as i64
```

 Entrada del almacén: datos y caducidad.

### `Session`

> rune Session

```pengu
    id as string
    is_new as bool
```

 Sesión de una petición.

### `store`

> **store**() -> ref to Store

 Almacén del proceso. Es el sujeto de este módulo: por eso es `static var`.

### `configure`

> **configure**(secret: string, ttl_ms: i64) -> void

 Fija el secreto de firma y la vida de las sesiones.

### `sign`

> **sign**(id: string) -> string

 Firma un `id` con el secreto actual (hex).

### `verify`

> **verify**(id: string, signature: string) -> bool

 Comprueba una firma en tiempo constante.

### `new_id`

> **new_id**() -> string

 Genera un identificador nuevo (32 hex).

### `sid_of`

> **sid_of**(context: ref to Context) -> maybe string

 Extrae el `sid` de la cookie firmada; `maybe none` si no hay o no cuadra.

### `load`

> **load**(context: ref to Context) -> maybe Session

 Carga la sesión de la petición, si la cookie es válida y no ha caducado.

### `start`

> **start**(context: ref to Context) -> Session

 Empieza una sesión nueva (todavía no está guardada).

### `get`

> **get**(session: ref to Session, name: string) -> maybe string

 Valor de la sesión, o `none`.

### `set`

> **set**(session: ref to Session, name: string, value: string) -> void

 Fija un valor.

### `save`

> **save**(context: ref to Context, session: ref to Session) -> void

 Guarda la sesión y manda la cookie firmada.

### `destroy`

> **destroy**(context: ref to Context, session: ref to Session) -> void

 Borra la sesión del almacén y la cookie.

### `drop`

> **drop**(id: string) -> void

 Borra todos los valores de un `sid`.

### `"firma y verificación"`

> test "firma y verificación"

_Undocumented._

### `"ida de sesión con datos"`

> test "ida de sesión con datos"

_Undocumented._

