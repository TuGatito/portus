# portus.transport

> Source: `src/portus/transport.pengu`

Transporte HTTP/1.1 de portus sobre `poll(2)` y llhttp.

Envuelve las estructuras opacas del binding (`portus.native`) en dos runes
prestados, `Request` y `Response`, y expone `serve`, que bloquea hasta que el
proceso muere o el socket no se puede abrir.

Un `Request` y un `Response` **solo son válidos durante la llamada al
handler**: apuntan a memoria de la conexión y el bucle los reutiliza en la
petición siguiente. Si necesitas conservar algo, cópialo (`copy` del string).

No hace TLS, HTTP/2, ni atiende dos conexiones a la vez: un handler que se
bloquea bloquea a todos. Para escalar, varios procesos (F7).

## Memoria
Los `string` que devuelven los accesores son propios del llamador: libéralos
con `banish` cuando ya no los necesites.

---

### `_take_bytes`

> **_take_bytes**(data: ref to frozen char, count: int) -> string

 Copia `count` bytes del transporte a un `string` propio del llamador.

### `_take_cstr`

> **_take_cstr**(data: ref to frozen char) -> string

 Copia una cadena C terminada en cero a un `string` propio.

### `Request`

> rune Request

```pengu
    _raw as ref to portus_request_t
```

 Petición HTTP en curso.

 Es una vista prestada: no la guardes más allá del handler.

### `method`

> **method**() -> string

 Devuelve el método (`GET`, `POST`, …), en mayúsculas.

 ## Memoria
 El `string` es propio del llamador.

### `path`

> **path**() -> string

 Devuelve la ruta sin query (`/users/7`).

 ## Memoria
 El `string` es propio del llamador.

### `query`

> **query**() -> string

 Devuelve la query sin el `?` (puede ser vacía).

 ## Memoria
 El `string` es propio del llamador.

### `body_text`

> **body_text**() -> string

 Devuelve el cuerpo tal cual llegó, como texto.

 ## Memoria
 El `string` es propio del llamador.

### `header_count`

> **header_count**() -> int

 Número de cabeceras recibidas.

### `header_name`

> **header_name**(index: int) -> string

 Nombre de la cabecera `index` (0-based).

 ## Memoria
 El `string` es propio del llamador.

### `header_value`

> **header_value**(index: int) -> string

 Valor de la cabecera `index` (0-based).

 ## Memoria
 El `string` es propio del llamador.

### `keep_alive`

> **keep_alive**() -> bool

 `true` si la conexión puede reutilizarse.

### `http_minor`

> **http_minor**() -> int

 Minor de la versión HTTP (0 para HTTP/1.0, 1 para HTTP/1.1).

### `oversized`

> **oversized**() -> int

 0 si la petición cabe en los límites, 1 si el cuerpo era enorme, 2 si
 lo eran las cabeceras.

### `Response`

> rune Response

```pengu
    _raw as ref to portus_response_t
    _sent as bool
```

 Respuesta HTTP en construcción.

 Los bytes se acumulan en la conexión; nada se envía hasta que el handler
 termina. Llama a `mark_sent` cuando hayas decidido la respuesta: si el
 handler vuelve sin marcarla, el transporte responde 500.

### `set_status`

> **set_status**(status: int) -> void

 Fija el código de estado.

### `set_header`

> **set_header**(name: string, value: string) -> void

 Fija (o reemplaza) una cabecera.

### `write_bytes`

> **write_bytes**(data: ref to frozen u8, count: int) -> void

 Añade bytes al cuerpo.

### `write_text`

> **write_text**(text: string) -> void

 Añade texto al cuerpo.

### `mark_sent`

> **mark_sent**() -> void

 Marca la respuesta como enviada.

### `stream_begin`

> **stream_begin**(content_type: string) -> bool

 Código de estado actual.
 Empieza una respuesta en streaming con `content_type`.

### `stream_write_text`

> **stream_write_text**(text: string) -> bool

 Escribe un trozo de texto en la respuesta en streaming.

### `stream_end`

> **stream_end**() -> bool

 Cierra la respuesta en streaming.

### `status`

> **status**() -> int

_Undocumented._

### `is_sent`

> **is_sent**() -> bool

 ¿El handler ya llamó a `mark_sent`?

### `ServeOptions`

> rune ServeOptions

```pengu
    host as string
    port as int
    max_connections as int
    max_header_bytes as int
    max_body_bytes as int
    idle_timeout_ms as int
    keep_alive as bool
```

 Opciones de arranque del servidor.

### `base_options`

> **base_options**() -> ServeOptions

 Opciones por defecto: `127.0.0.1:3000`, 1024 conexiones, 16 KiB de cabeceras,
 1 MiB de cuerpo, 60 s de inactividad y keep-alive.

### `_Bridge`

> rune _Bridge

```pengu
    handler as weave with request: ref to Request, response: ref to Response into void
```

 Estado que el bucle C pasa de vuelta al handler puente.

### `_bridge`

> **_bridge**(userdata: ref to void, request: ref to void, response: ref to void) -> void

 Puente C → PenguScript: convierte los punteros opacos en runes prestados.

### `serve_loop`

> **serve_loop**(options: ServeOptions, handler: weave with request: ref to Request, response: ref to Response into void) -> result of int to string

 Arranca el servidor y bloquea hasta que muere.

 Devuelve el puerto real cuando `options.port` es 0 (puerto efímero): útil en
 tests. En error devuelve el texto de `portus_last_error`.

 ## Memoria
 `options.host` se presta durante la llamada; el resultado está vacío en éxito.

### `_BridgeData`

> rune _BridgeData

```pengu
    handler as weave with data: ref to void, request: ref to Request, response: ref to Response into void
    data as ref to void
```

 Puente para handlers que necesitan un `data` propio (la aplicación).

### `_bridge_data`

> **_bridge_data**(userdata: ref to void, request: ref to void, response: ref to void) -> void

 Puente C → PenguScript con `data`: lo usa el despachador.

### `serve_loop_data`

> **serve_loop_data**(options: ServeOptions, handler: weave with data: ref to void, request: ref to Request, response: ref to Response into void, data: ref to void) -> result of int to string

 Igual que `serve_loop`, pero el handler recibe un `data` opaco.

 ## Memoria
 `data` debe seguir vivo mientras el servidor corre (el bucle bloquea).

### `native_ping`

> **native_ping**(n: int) -> int

 Versión de la capa nativa (ping de humo del binding).

