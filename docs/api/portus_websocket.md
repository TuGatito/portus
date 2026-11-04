# portus.websocket

> Source: `src/portus/websocket.pengu`

portus.websocket — WebSocket de servidor (RFC 6455).

`upgrade(context, handler)` responde `101 Switching Protocols`, calcula
`Sec-WebSocket-Accept` y entrega cada mensaje al handler. Sin fragmentación
ni extensiones (v1): un frame por mensaje, cierre con código 1000.

## Ejemplo
```pengu
weave on_message with conn as ref to portus.websocket.Conn, opcode as int, payload as ref to frozen byte, length as int into void:
    if opcode == 1:
        var text as string is calling portus.websocket.text_of with payload, length
        calling conn.send_text with text
        banish text

weave chat with context as ref to portus.Context into result of void to portus.HttpError:
    let opened as bool is calling portus.websocket.upgrade with context, (calling portus.websocket.handler_with with on_message, (transmute 0 to ref to void))
    return calling portus.ok_void
```

## Memoria
El handler se copia a un bloque del heap que se libera al cerrar la conexión.

---

### `Conn`

> rune Conn

```pengu
    _res as ref to void
```

 Una conexión WebSocket viva.

### `MessageFn`

> alias MessageFn

 Función que recibe cada mensaje: `opcode` 0 apertura, 1 texto, 2 binario, 8 cierre.

### `SocketHandler`

> rune SocketHandler

```pengu
    invoke as MessageFn
    data as ref to void
    conn as ref to Conn
```

 Handler con su estado (no hay closures: el estado va en `data`).

 Se llama `SocketHandler` y no `Handler` para no chocar con el handler fino
 de `portus.handler`.

### `handler_with`

> **handler_with**(invoke: MessageFn, data: ref to void) -> SocketHandler

 Construye un handler con estado.

### `text_of`

> **text_of**(payload: ref to frozen u8, length: int) -> string

 Texto de un mensaje (copia propia).

### `upgrade`

> **upgrade**(context: ref to Context, handler: SocketHandler) -> bool

 Acepta la conexión y empieza a entregar mensajes.

 ## Memoria
 Copia `handler` a un bloque nuevo; se libera al cerrar la conexión.

### `_raw_handler`

> **_raw_handler**(box: ref to SocketHandler) -> ref to void

_Undocumented._

### `_dispatch`

> **_dispatch**(userdata: ref to void, response: ref to void, opcode: int, payload: ref to frozen u8, length: int) -> void

 Trampolín C -> Pengu.

### `_send_text`

> **_send_text**(conn: ref to Conn, text: string) -> bool

 Envía un texto.

### `_send_bytes`

> **_send_bytes**(conn: ref to Conn, data: ref to frozen u8, length: int) -> bool

 Envía bytes binarios.

### `_close`

> **_close**(conn: ref to Conn) -> bool

 Cierra la conexión con código 1000.

### `send_text`

> **send_text**(body: string) -> bool

 Envía un texto.

### `send_bytes`

> **send_bytes**(data: ref to frozen u8, length: int) -> bool

 Envía bytes binarios.

### `close`

> **close**() -> bool

 Cierra la conexión.

### `is_text`

> **is_text**(opcode: int) -> bool

 `true` si el mensaje es de texto.

### `"text_of copia el payload"`

> test "text_of copia el payload"

_Undocumented._

### `"handler_with guarda el estado"`

> test "handler_with guarda el estado"

_Undocumented._

### `_noop`

> **_noop**(data: ref to void, conn: ref to Conn, opcode: int, payload: ref to frozen u8, length: int) -> void

_Undocumented._

