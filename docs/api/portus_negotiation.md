# portus.negotiation

> Source: `src/portus/negotiation.pengu`

portus.negotiation — negociación de contenido por `Accept`.

`best(context, offered)` devuelve el primer tipo de `offered` que el cliente
acepta, respetando `q=` (a igualdad de `q` gana el orden de `Accept`).
`accepts(context, type)` responde sí/no.

## Ejemplo
```pengu
let chosen as maybe string is calling portus.negotiation.best with ctx, ["application/json", "text/html"]
```

---

### `best`

> **best**(ctx: ref to Context, offered: list of string) -> maybe string

 Mejor tipo ofrecido, o `none` si el cliente no acepta ninguno.

 ## Memoria
 El resultado es una copia propia; libéralo con `banish`.

### `accepts`

> **accepts**(ctx: ref to Context, media_type: string) -> bool

 `true` si el cliente acepta `media_type`.

### `_best_with`

> **_best_with**(header: string, offered: list of string) -> maybe string

 Implementación sobre el valor de `Accept`.

### `_matches`

> **_matches**(spec: string, candidate: string) -> bool

 `true` si `spec` (de `Accept`) cubre `candidate`.

### `_q_of`

> **_q_of**(item: string) -> int

 `q` en milésimas (1000 por defecto).

### `_end_of_number`

> **_end_of_number**(item: string, start: int) -> int

 Índice donde acaba el número decimal que empieza en `start`.

### `_is_digit`

> **_is_digit**(piece: string) -> bool

 `true` si `piece` es un dígito.

### `_decimal_to_milli`

> **_decimal_to_milli**(text: string) -> int

 Convierte "0.5" (o "1") a milésimas.

### `_digits_to_int`

> **_digits_to_int**(text: string) -> int

 Dígitos a entero (0 si no hay).

### `_fraction_to_milli`

> **_fraction_to_milli**(text: string) -> int

 Parte decimal a milésimas (`0.5` → 500).

### `_digit_of`

> **_digit_of**(piece: string) -> int

 Valor 0-9 de un carácter, o -1.

### `_without_params`

> **_without_params**(item: string) -> string

 Sin parámetros (`; q=…`, `; charset=…`).

### `_split_commas`

> **_split_commas**(header: string) -> list of string

 Separa por comas y recorta cada trozo.

### `_free_items`

> **_free_items**(items: list of string) -> void

 Libera la lista de trozos y sus cadenas.

### `"accept simple elige el primero"`

> test "accept simple elige el primero"

_Undocumented._

### `"accept con q prefiere el mayor"`

> test "accept con q prefiere el mayor"

_Undocumented._

### `"accept de tipo cubre subtipos"`

> test "accept de tipo cubre subtipos"

_Undocumented._

### `"accept sin coincidencia no elige nada"`

> test "accept sin coincidencia no elige nada"

_Undocumented._

