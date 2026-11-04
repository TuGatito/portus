# portus.cookie

> Source: `src/portus/cookie.pengu`

portus.cookie — lectura y escritura de cookies HTTP.

`parse_cookies` lee la cabecera `Cookie` de la petición (`a=1; b=2`) y
`serialize_cookie` produce un valor de `Set-Cookie` con sus atributos.

## Ejemplo
```pengu
calling context.set_header with "Set-Cookie", (calling portus.cookie.serialize_cookie with "sid", "abc", portus.cookie.session_options)
```

## Memoria
`parse_cookies` devuelve un valor propio; libéralo con `free_cookies`.
Los strings de `serialize_cookie` son propios.

---

### `CookiePair`

> rune CookiePair

```pengu
    name as string
    value as string
```

 Una cookie de la petición.

### `Cookies`

> rune Cookies

```pengu
    pairs as list of CookiePair
```

 Las cookies de una petición, en orden.

### `CookieOptions`

> rune CookieOptions

```pengu
    max_age as int
    path as string
    http_only as bool
    secure as bool
    same_site as string
```

 Atributos de una cookie de respuesta.

 `max_age` negativo omite `Max-Age`. `same_site` vacío omite `SameSite`;
 los valores habituales son `"Lax"`, `"Strict"` y `"None"`.

### `SESSION_OPTIONS`

> **SESSION_OPTIONS** : CookieOptions

```pengu
    max_age as int
    path as string
    http_only as bool
    secure as bool
    same_site as string
```

 Opciones de una cookie de sesión razonable: `HttpOnly`, `SameSite=Lax`.

### `parse_cookies`

> **parse_cookies**(header: string) -> Cookies

 Lee la cabecera `Cookie` completa.

 Ignora segmentos vacíos y espacios sobrantes. Un segmento sin `=` se guarda
 con valor `""`.

 ## Memoria
 Devuelve un valor propio; libéralo con `free_cookies`.

### `cookie_get`

> **cookie_get**(cookies: Cookies, name: string) -> maybe string

 Valor de la cookie `name`, o `none`.

 ## Memoria
 Copia propia; libérala o pásala a `banish`.

### `cookie_has`

> **cookie_has**(cookies: Cookies, name: string) -> bool

 `true` si la petición trae la cookie `name`.

### `free_cookies`

> **free_cookies**(cookies: Cookies) -> void

 Libera las cookies y sus strings.

### `serialize_cookie`

> **serialize_cookie**(name: string, value: string, options: CookieOptions) -> string

 Construye el valor de `Set-Cookie` para `name=value`.

 ## Memoria
 Devuelve un string propio.

### `clear_cookie`

> **clear_cookie**(name: string, path: string) -> string

 Construye el `Set-Cookie` que borra una cookie (`Max-Age=0`).

 ## Memoria
 Devuelve un string propio.

### `"parse_cookies lee pares y tolera espacios"`

> test "parse_cookies lee pares y tolera espacios"

_Undocumented._

### `"serialize_cookie escribe atributos"`

> test "serialize_cookie escribe atributos"

_Undocumented._

### `"clear_cookie pone Max-Age=0"`

> test "clear_cookie pone Max-Age=0"

_Undocumented._

