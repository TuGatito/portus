# portus.middleware

> Source: `src/portus/middleware.pengu`

portus.middleware — middlewares listos para usar.

Cada `add_*` registra un middleware en la aplicación; el orden de llamada es
el de registro. Todos son "de antes": se ejecutan antes de la ruta y pueden
cortar la petición respondiendo y marcando el contexto como enviado.

## Ejemplo
```pengu
calling portus.middleware.add_request_id with sigil of app
calling portus.middleware.add_security_headers with sigil of app
calling portus.middleware.add_cors with sigil of app, "https://midominio.example"
```

## Memoria
La configuración se aloja en la arena de la aplicación; no la liberes.

---

### `CorsConfig`

> rune CorsConfig

```pengu
    origin as string
```

 Configuración del middleware CORS.

### `add_request_id`

> **add_request_id**(app: ref to Application) -> void

 Añade `X-Request-Id` con 16 bytes aleatorios en hexadecimal (32 chars).

 ## Memoria
 No aloja configuración.

### `_request_id`

> **_request_id**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Middleware que pone `X-Request-Id`.

### `add_security_headers`

> **add_security_headers**(app: ref to Application) -> void

 Añade las cabeceras de seguridad habituales.

 `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`,
 `Referrer-Policy: no-referrer` y `Cross-Origin-Opener-Policy: same-origin`.

### `_security_headers`

> **_security_headers**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Middleware de cabeceras de seguridad.

### `_raw_cors`

> **_raw_cors**(config: ref to CorsConfig) -> ref to void

_Undocumented._

### `add_cors`

> **add_cors**(app: ref to Application, origin: string) -> void

 Añade CORS para `origin`.

 Responde los preflight (`OPTIONS` con `Access-Control-Request-Method`) con
 `204` y las cabeceras `Access-Control-Allow-*`.

 ## Memoria
 `origin` se copia en la arena de la aplicación.

### `_cors`

> **_cors**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Middleware que aplica la configuración CORS.

### `add_logger`

> **add_logger**(app: ref to Application) -> void

 Registra un log de una línea por petición en `stdout`.

 Antes de la ruta imprime `-> METODO ruta`; al terminar, `<- codigo Nms`.

### `_log_before`

> **_log_before**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Middleware que abre la línea de log.

### `_log_after`

> **_log_after**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Finisher que cierra la línea de log con código y duración.

### `CspConfig`

> rune CspConfig

```pengu
    policy as string
```

 Configuración de CSP.

### `_raw_csp`

> **_raw_csp**(config: ref to CspConfig) -> ref to void

 Convierte la configuración en el `data` opaco del middleware.

### `_csp`

> **_csp**(data: ref to void, context: ref to Context) -> result of void to HttpError

 Añade `Content-Security-Policy` si la respuesta no trae una.

### `use_csp`

> **use_csp**(app: ref to Application, policy: string) -> void

 Aplica una política CSP a las respuestas de la aplicación.

