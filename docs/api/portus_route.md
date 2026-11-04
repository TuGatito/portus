# portus.route

> Source: `src/portus/route.pengu`

portus.route — patrones de ruta y sus parámetros.

Un patrón es una secuencia de segmentos separados por `/`. Cada segmento es
literal (`/users`), parámetro (`/users/:id`) o comodín (`/files/*path`).
Se compara por especificidad: literal > parámetro > comodín.

## Ejemplo
```pengu
var compiled as result of Pattern to PatternError is calling route.compile with "/users/:id"
var pattern as Pattern is compiled or:
    return 1
var found as maybe Params is calling route.match_pattern with pattern, "/users/42"
```

## Memoria
`compile` devuelve un `Pattern` dueño de sus segmentos: libéralo con
`free_pattern`. `match_pattern` devuelve `Params` dueño de sus cadenas:
libéralo con `free_params`.

---

### `SegmentKind`

> omen SegmentKind

```pengu
    Literal
    Param
    Wildcard
```

 Tipo de un segmento del patrón.

### `Segment`

> rune Segment

```pengu
    kind as SegmentKind
    text as string
```

 Un segmento ya compilado.

### `Pattern`

> rune Pattern

```pengu
    segments as list of Segment
    score as int
```

 Un patrón compilado.

### `PatternErrorKind`

> omen PatternErrorKind

```pengu
    Empty
    TooManyParams
    DuplicateParam
    WildcardNotLast
```

 Categoría de un patrón inválido.

### `PatternError`

> rune PatternError

```pengu
    kind as PatternErrorKind
    message as string
```

 Por qué un patrón no se pudo compilar.

### `_empty_error`

> **_empty_error**() -> PatternError

 Patrón vacío.

### `_too_many`

> **_too_many**(count: int) -> PatternError

 Demasiados parámetros.

### `_duplicate`

> **_duplicate**(name: string) -> PatternError

 Nombre de parámetro repetido.

### `_wildcard_last`

> **_wildcard_last**() -> PatternError

 Comodín que no es el último segmento.

### `MAX_PARAMS`

> **MAX_PARAMS** : int

 Máximo de parámetros por ruta.

### `_copy`

> **_copy**(text: string) -> string

 Copia propia de una cadena.

### `compile`

> **compile**(pattern: string) -> result of Pattern to PatternError

 Compila un patrón.

 Acepta con o sin `/` inicial; los segmentos vacíos se ignoran.

 ## Retorno
 `Pattern` dueño de sus cadenas, o el motivo del fallo.

### `_has_name`

> **_has_name**(segments: list of Segment, name: string) -> bool

 ¿Algún segmento ya usa este nombre?

### `free_strings`

> **free_strings**(items: list of string) -> void

 Libera una lista de cadenas propias.

### `free_segments`

> **free_segments**(segments: list of Segment) -> void

 Libera los segmentos y sus cadenas.

### `free_pattern`

> **free_pattern**(pattern: Pattern) -> void

 Libera un patrón compilado.

### `Params`

> rune Params

```pengu
    names as list of string
    values as list of string
```

 Parámetros capturados de una petición.

### `free_params`

> **free_params**(params: Params) -> void

 Libera los parámetros y sus cadenas.

### `get_param`

> **get_param**(params: Params, name: string) -> maybe string

 Busca un parámetro por nombre.

 ## Retorno
 Copia propia del valor, o `none` si no está.

### `_path_parts`

> **_path_parts**(path: string) -> list of string

 Parte una ruta en segmentos no vacíos.

### `match_pattern`

> **match_pattern**(pattern: Pattern, path: string) -> maybe Params

 Compara un patrón con una ruta.

 ## Retorno
 `Params` propio (aunque esté vacío) cuando coincide, `none` cuando no.

### `"compila la raíz como patrón vacío"`

> test "compila la raíz como patrón vacío"

_Undocumented._

### `"literal y parámetro puntúan distinto"`

> test "literal y parámetro puntúan distinto"

_Undocumented._

### `"captura el parámetro de ruta"`

> test "captura el parámetro de ruta"

_Undocumented._

### `"un literal no casa con otro literal"`

> test "un literal no casa con otro literal"

_Undocumented._

### `"una ruta más corta no casa"`

> test "una ruta más corta no casa"

_Undocumented._

### `"el comodín absorbe el resto"`

> test "el comodín absorbe el resto"

_Undocumented._

### `"rechaza parámetros duplicados"`

> test "rechaza parámetros duplicados"

_Undocumented._

### `"rechaza un comodín que no va al final"`

> test "rechaza un comodín que no va al final"

_Undocumented._

### `"los parámetros distinguen mayúsculas"`

> test "los parámetros distinguen mayúsculas"

_Undocumented._

### `"un parámetro que no existe devuelve none"`

> test "un parámetro que no existe devuelve none"

_Undocumented._

### `"el comodín desnudo absorbe el resto"`

> test "el comodín desnudo absorbe el resto"

_Undocumented._

### `"una ruta con barra final casa con el patrón sin barra"`

> test "una ruta con barra final casa con el patrón sin barra"

_Undocumented._

### `"un patrón vacío se rechaza"`

> test "un patrón vacío se rechaza"

_Undocumented._

### `"el wildcard puntúa menos que el parámetro y el literal"`

> test "el wildcard puntúa menos que el parámetro y el literal"

_Undocumented._

### `"los nombres y valores de Params van en orden"`

> test "los nombres y valores de Params van en orden"

_Undocumented._

### `"una ruta más larga que el patrón no casa"`

> test "una ruta más larga que el patrón no casa"

_Undocumented._

