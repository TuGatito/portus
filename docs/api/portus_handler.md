# portus.handler

> Source: `src/portus/handler.pengu`

portus.handler — la firma de un handler.

Un handler recibe el contexto de la petición y devuelve `ok` si ya respondió
o `err` con el fallo que portus debe convertir en respuesta de error. No hay
`next()` ni closures: la composición se hace con la cadena de la aplicación.

## Ejemplo
```pengu
weave index with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.send_text with "hola"
    return calling portus.ok_void
```

## Memoria
El contexto lo presta el despachador: no lo guardes ni lo banishes.

---

### `Context`

> alias Context

_Undocumented._

### `Handler`

> alias Handler

 Firma de un handler de ruta o de middleware.

### `ok_void`

> **ok_void**() -> result of void to HttpError

 `result of void to HttpError` con éxito; azúcar para no escribir `ok_of`.

### `err_void`

> **err_void**(failure: HttpError) -> result of void to HttpError

 `result of void to HttpError` con fallo.

### `MiddlewareFn`

> alias MiddlewareFn

 Firma de un middleware: recibe su configuración y el contexto.

 Un middleware **no** tiene `next`: corre antes (o después) de las rutas y
 puede responder o devolver un fallo, que corta la cadena.

### `ErrorHandler`

> alias ErrorHandler

 Firma de un handler de error (`on_error`).

### `Middleware`

> rune Middleware

```pengu
    invoke as MiddlewareFn
    data as ref to void
```

 Un middleware con su configuración, sin closures.

 `data` apunta a memoria que posee la `Application` (su arena), no el
 middleware; `invoke` la recupera con `transmute data to ref to T`.

### `middleware`

> **middleware**(invoke: MiddlewareFn) -> Middleware

 Envuelve una función de middleware sin configuración.

### `middleware_with`

> **middleware_with**(invoke: MiddlewareFn, data: ref to void) -> Middleware

 Envuelve una función de middleware con su configuración.

 ## Memoria
 `data` es prestado: debe vivir tanto como el middleware (normalmente en la
 arena de la aplicación).

### `invoke`

> **invoke**(handler: Handler, context: ref to Context) -> result of void to HttpError

 Ejecuta un handler de ruta.

### `invoke_middleware`

> **invoke_middleware**(middleware: Middleware, context: ref to Context) -> result of void to HttpError

 Ejecuta un middleware con su configuración.

