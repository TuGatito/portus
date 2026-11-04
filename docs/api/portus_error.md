# portus.error

> Source: `src/portus/error.pengu`

portus.error — fallos HTTP como valores.

Un handler devuelve `result of void to HttpError`: `ok` cuando ya escribió la
respuesta, `err` cuando quiere que portus genere la página de error. No se
usa `panic` para datos del usuario.

El error es un `rune` con una categoría (`HttpErrorKind`) y un mensaje: así
el despachador decide el código de estado sin inspeccionar texto.

## Ejemplo
```pengu
weave show with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "hola"
    return calling portus.ok_void
```

---

### `HttpErrorKind`

> omen HttpErrorKind

```pengu
    BadRequest
    Unauthorized
    Forbidden
    NotFound
    MethodNotAllowed
    Conflict
    PayloadTooLarge
    UnsupportedMediaType
    TooManyRequests
    Internal
```

_Undocumented._

### `HttpError`

> rune HttpError

```pengu
    kind as HttpErrorKind
    message as string
```

 Un fallo HTTP con su categoría y un mensaje para el cliente.

### `bad_request`

> **bad_request**(message: string) -> HttpError

 Error 400 con un mensaje concreto.

### `unauthorized`

> **unauthorized**() -> HttpError

 Error 401.

### `forbidden`

> **forbidden**() -> HttpError

 Error 403.

### `not_found`

> **not_found**() -> HttpError

 Error 404.

### `method_not_allowed`

> **method_not_allowed**(allow: string) -> HttpError

 Error 405 con la cabecera `Allow` ya formateada.

### `conflict`

> **conflict**(message: string) -> HttpError

 Error 409 con un mensaje concreto.

### `payload_too_large`

> **payload_too_large**() -> HttpError

 Error 413.

### `unsupported_media_type`

> **unsupported_media_type**() -> HttpError

 Error 415.

### `too_many_requests`

> **too_many_requests**() -> HttpError

 Error 429.

### `internal`

> **internal**() -> HttpError

 Error 500. El mensaje siempre es genérico: el detalle no sale al cliente.

### `status_of`

> **status_of**(failure: HttpError) -> int

 Código de estado HTTP de un error.

### `message_of`

> **message_of**(failure: HttpError) -> string

 Texto para el cuerpo de la página de error.

### `"el status corresponde a la categoría"`

> test "el status corresponde a la categoría"

_Undocumented._

### `"el mensaje del 400 es el que dio el handler"`

> test "el mensaje del 400 es el que dio el handler"

_Undocumented._

### `"el 500 nunca filtra el detalle interno"`

> test "el 500 nunca filtra el detalle interno"

_Undocumented._

### `"method_not_allowed guarda la lista de métodos"`

> test "method_not_allowed guarda la lista de métodos"

_Undocumented._

### `"conflict lleva su mensaje"`

> test "conflict lleva su mensaje"

_Undocumented._

### `"unauthorized y forbidden no filtran detalle"`

> test "unauthorized y forbidden no filtran detalle"

_Undocumented._

