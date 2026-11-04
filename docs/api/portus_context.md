# portus.context

> Source: `src/portus/context.pengu`

portus.context — la petición y la respuesta de un handler.

Es el único objeto que ve un handler. Los strings de la petición pertenecen al
contexto (no los banishes); lo que construyas para la respuesta sí es tuyo.

## Ejemplo
```pengu
weave hello with context as ref to portus.Context into result of void to portus.HttpError:
    calling context.set_header with "Content-Type", "text/plain"
    calling context.send_text with "hola"
    return calling portus.ok_void
```

## Memoria
El despachador crea y libera el contexto; no lo guardes.

---

### `Context`

> rune Context

```pengu
    request as ref to Request
    response as ref to Response
    params as Params
    started_ms as i64
```

 La petición, la respuesta y lo que el despachador capturó de la ruta.

### `param`

> **param**(name: string) -> maybe string

 Valor de un parámetro de ruta.

 ## Retorno
 Copia propia, o `none` si el patrón no lo capturó.

### `method`

> **method**() -> string

 Método HTTP de la petición (`GET`, `POST`, …).

### `path`

> **path**() -> string

 Ruta pedida, sin query string.

### `query`

> **query**() -> string

 Query string cruda (`a=1&b=2`) o `""`.

### `set_header`

> **set_header**(name: string, value: string) -> void

 Fija una cabecera de la respuesta.

 ## Memoria
 `name` y `value` se copian en la respuesta.

### `set_content_type`

> **set_content_type**(value: string) -> void

 Añade la cabecera `Content-Type`.

### `send_body_with_type`

> **send_body_with_type**(content_type: string, body: string) -> void

 Responde con un cuerpo y el `Content-Type` indicado.

 ## Memoria
 El cuerpo se copia en la respuesta; `content_type` es prestado.

### `send_text`

> **send_text**(body: string) -> void

 Responde con texto y `Content-Type: text/plain; charset=utf-8`.

 ## Memoria
 El cuerpo se copia en la respuesta.

### `send_html`

> **send_html**(body: string) -> void

 Responde con HTML.

### `set_status`

> **set_status**(code: int) -> void

 Fija el código de estado sin enviar cuerpo todavía.

### `send_status`

> **send_status**(code: int, body: string) -> void

 Responde con un código y un cuerpo de texto.

### `redirect`

> **redirect**(location: string) -> void

 Redirige con 302 y `Location: location`.

### `redirect_permanent`

> **redirect_permanent**(location: string) -> void

 Redirección permanente (301).

### `no_content`

> **no_content**() -> void

 Responde 204 sin cuerpo.

### `not_modified`

> **not_modified**() -> void

 Responde 304 sin cuerpo.

### `status_code`

> **status_code**() -> int

 Código de estado actual de la respuesta.

### `sse_open`

> **sse_open**() -> bool

 ¿El handler ya respondió?
 Abre un flujo SSE (`text/event-stream`).

### `sse_send`

> **sse_send**(event: string, data: string) -> bool

 Envía un evento SSE (`event: …` + `data: …`).

### `sse_close`

> **sse_close**() -> bool

 Cierra el flujo SSE.

### `stream_begin`

> **stream_begin**(content_type: string) -> bool

 Empieza una respuesta en streaming con `content_type`.

### `stream_write`

> **stream_write**(text: string) -> bool

 Escribe un trozo de texto en streaming.

### `stream_end`

> **stream_end**() -> bool

 Cierra el streaming.

### `mark_sent`

> **mark_sent**() -> void

 Marca la respuesta como enviada sin escribir cuerpo.

### `is_sent`

> **is_sent**() -> bool

_Undocumented._

### `header`

> **header**(name: string) -> maybe string

 Valor de una cabecera de la petición (sin distinguir mayúsculas).

 ## Retorno
 Copia propia, o `none` si la cabecera no vino.

### `queries`

> **queries**() -> Query

 Query string de la petición, ya decodificado.

 ## Memoria
 Devuelve un valor propio; libéralo con `portus.query.free_query`.

### `query_param`

> **query_param**(name: string) -> maybe string

 Primer valor de un parámetro del query string.

 ## Memoria
 Copia propia, o `none` si no vino.

### `body_text`

> **body_text**() -> string

 Cuerpo de la petición como texto.

 ## Memoria
 Copia propia; los strings de la petición no se pueden banir.

### `body_json`

> **body_json**() -> result of JsonValue to JsonError

 Cuerpo de la petición interpretado como JSON.

 ## Retorno
 El valor JSON (propio) o el error del parser. Un cuerpo vacío es un
 `JsonError` de tipo `UnexpectedEof`.

### `body_form`

> **body_form**() -> Query

 Cuerpo `application/x-www-form-urlencoded` como pares.

 ## Memoria
 Devuelve un valor propio; libéralo con `portus.query.free_query`.

### `body_multipart`

> **body_multipart**() -> maybe list of Part

 Cuerpo `multipart/form-data` como partes.

 ## Memoria
 Devuelve partes propias; libéralas con `multipart.free_parts`.

### `cookie`

> **cookie**(name: string) -> maybe string

 Valor de una cookie de la petición.

 ## Retorno
 Copia propia, o `none` si no vino.

### `set_cookie`

> **set_cookie**(name: string, value: string) -> void

 Añade una cookie a la respuesta con `SESSION_OPTIONS`.

 ## Memoria
 El valor se copia en la respuesta.

### `set_cookie_with`

> **set_cookie_with**(name: string, value: string, options: CookieOptions) -> void

 Añade una cookie a la respuesta con atributos explícitos.

### `delete_cookie`

> **delete_cookie**(name: string) -> void

 Pide al navegador que borre una cookie.

### `send_json`

> **send_json**(value: JsonValue) -> void

 Responde con un valor JSON y `Content-Type: application/json`.

 ## Memoria
 El texto serializado se copia en la respuesta y se libera aquí.

