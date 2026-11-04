# portus.rate_limit

> Source: `src/portus/rate_limit.pengu`

portus.rate_limit — límite de peticiones por cliente en ventana fija.

El cliente se identifica por el primer valor de `X-Forwarded-For` (o
`"local"`). El estado vive en memoria del proceso y se recicla cuando pasa
la ventana; si la tabla crece por encima de `MAX_CLIENTS` se vacía entera
para acotar la memoria.

## Ejemplo
```pengu
calling portus.use_rate_limit with sigil of app, 60, 60000
```

---

### `MAX_CLIENTS`

> **MAX_CLIENTS** : int

 Clientes recordados antes de vaciar la tabla.

### `Rule`

> rune Rule

```pengu
    max_requests as int
    window_ms as i64
```

 Configuración del límite.

### `Slot`

> rune Slot

```pengu
    count as int
    reset_at as i64
```

 Contador de un cliente.

### `slots`

> **slots**() -> ref to map of string to Slot

 Tabla de contadores. Es el sujeto de este módulo.

### `client_key`

> **client_key**(context: ref to Context) -> string

 Identifica al cliente.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`.

### `_limit`

> **_limit**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Cuenta la petición y responde 429 si se pasa del límite.

### `_raw_rule`

> **_raw_rule**(rule: ref to Rule) -> ref to void

 Convierte la configuración en el `data` opaco del middleware.

### `use_at`

> **use_at**(app: ref to Application, prefix: string, max_requests: int, window_ms: i64) -> void

 Aplica el límite solo a las rutas bajo `prefix`.

### `use`

> **use**(app: ref to Application, max_requests: int, window_ms: i64) -> void

 Aplica el límite a toda la aplicación.

