# portus.static_file

> Source: `src/portus/static_file.pengu`

portus.static_file — sirve un directorio como estáticos.

`mount(app, "/static", "./public")` registra `GET /static/*file`. Resuelve
el archivo relativo a `root`, rechaza rutas con `..`, pone `Content-Type`
por extensión, `ETag` y `Last-Modified`, honra `If-None-Match` (304) y una
`Range: bytes=a-b` (206).

## Ejemplo
```pengu
let mounted as result of void to portus.route.PatternError is calling portus.static_file.mount with sigil of app, "/static", "./public"
```

## Memoria
La configuración se copia en la arena de la aplicación.

---

### `StaticConfig`

> rune StaticConfig

```pengu
    root as string
    prefix as string
```

 Configuración del montaje.

### `ByteRange`

> rune ByteRange

```pengu
    requested as bool
    valid as bool
    start as int
    last as int
```

 Un rango pedido por `Range: bytes=a-b`.

### `_raw_static`

> **_raw_static**(config: ref to StaticConfig) -> ref to void

_Undocumented._

### `mount`

> **mount**(app: ref to Application, prefix: string, root: string) -> result of void to PatternError

 Sirve `root` bajo `prefix` (`"/static"` o `""` para la raíz).

 ## Memoria
 Crea el patrón `"/static/*file"` y copia `root` en la arena.

### `_serve`

> **_serve**(data: ref to void, ctx: ref to Context) -> result of void to HttpError

 Handler de un archivo estático.

### `_has_dotdot`

> **_has_dotdot**(rel: string) -> bool

 `true` si `rel` intenta salir del directorio raíz.

### `_has_absolute`

> **_has_absolute**(rel: string) -> bool

 `true` si `rel` empieza por `/` (ruta absoluta).

### `_normalize_prefix`

> **_normalize_prefix**(prefix: string) -> string

 Normaliza un prefijo: quita la barra final y convierte `"/"` en `""`.

### `_parse_range`

> **_parse_range**(ctx: ref to Context, total: int) -> ByteRange

 Lee y valida `Range: bytes=a-b` de la petición.

### `_digit_of`

> **_digit_of**(c: string) -> int

 Valor 0-9 del carácter `c`, o -1 si no es un dígito ASCII.

### `_to_int`

> **_to_int**(text: string, fallback: int) -> int

 Convierte dígitos decimales; `-1` si no son todos dígitos.

