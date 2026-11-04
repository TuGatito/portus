# portus.csrf

> Source: `src/portus/csrf.pengu`

portus.csrf — protección CSRF por doble envío.

El servidor pone una cookie `portus_csrf` (legible por JavaScript) y espera
que el cliente la repita en la cabecera `X-CSRF-Token` en cada petición que
cambia estado. `guard` rechaza con 403 las que no coinciden.

## Memoria
`token` devuelve un string propio; libéralo con `banish`. `guard` no
devuelve datos.

---

### `HEADER_NAME`

> **HEADER_NAME** : string

 Cabecera esperada.

### `COOKIE_NAME`

> **COOKIE_NAME** : string

 Cookie donde vive el token.

### `_same`

> **_same**(a: string, b: string) -> bool

 Compara en tiempo constante.

### `token`

> **token**(context: ref to Context) -> string

 Token de la petición: reutiliza la cookie o crea una nueva.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`.

### `check`

> **check**(context: ref to Context) -> bool

 `true` si la cabecera y la cookie coinciden.

### `guard`

> **guard**(context: ref to Context) -> result of void to HttpError

 Rechaza (403) las peticiones que cambian estado sin token válido.

### `"compara tokens"`

> test "compara tokens"

_Undocumented._

