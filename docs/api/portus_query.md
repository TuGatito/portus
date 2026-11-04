# portus.query

> Source: `src/portus/query.pengu`

portus.query — lectura del query string (`?a=1&b=2`).

Decodifica porcentajes y `+` como espacio (form encoding) y devuelve pares
en orden de aparición. Los nombres repetidos se conservan.

## Ejemplo
```pengu
var query as portus.query.Query is calling portus.query.parse_query with "a=1&b=hola+mundo"
let a as maybe string is calling portus.query.query_get with query, "a"
```

## Memoria
`parse_query` devuelve un valor con memoria propia; libéralo con
`free_query` (o `query_drop` si solo la usaste).

---

### `QueryPair`

> rune QueryPair

```pengu
    name as string
    value as string
```

 Un par nombre/valor del query string.

### `Query`

> rune Query

```pengu
    pairs as list of QueryPair
```

 El query string de una petición, en orden.

### `parse_query`

> **parse_query**(raw: string) -> Query

 Decodifica `raw` (`a=1&b=2`) en pares.

 Un par sin `=` se guarda con valor `""`. Un porcentaje inválido deja el
 texto tal cual (no falla la petición entera).

 ## Memoria
 Devuelve un valor propio; libéralo con `free_query`.

### `query_get`

> **query_get**(query: Query, name: string) -> maybe string

 Primer valor de `name`, o `none`.

 ## Memoria
 Copia propia; libérala o pásala a `banish`.

### `query_has`

> **query_has**(query: Query, name: string) -> bool

 `true` si el query string trae `name`.

### `free_query`

> **free_query**(query: Query) -> void

 Libera el query string y todos sus pares.

### `_decode`

> **_decode**(raw: string) -> string

 Decodifica un componente (`%20`, `+` como espacio).

 Nunca falla: si el porcentaje es inválido devuelve una copia tal cual.

### `"parse_query decodifica y conserva el orden"`

> test "parse_query decodifica y conserva el orden"

_Undocumented._

### `"parse_query acepta un par sin igual"`

> test "parse_query acepta un par sin igual"

_Undocumented._

