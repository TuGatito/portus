# portus.text

> Source: `src/portus/text.pengu`

portus.text — utilidades de texto compartidas por los módulos de portus.

No es API pública: vive aquí para no duplicar el recorrido de listas de
strings en cada módulo.

## Memoria
`free_strings` libera cada elemento y el buffer de la lista.

---

### `free_strings`

> **free_strings**(items: list of string) -> void

 Libera una lista de strings y sus elementos.

 ## Memoria
 Libera `items` por completo; no la uses después.

### `free_borrowed`

> **free_borrowed**(items: list of string) -> void

 Libera una lista de strings sin tocar sus elementos (los strings son
 prestados, p. ej. una vista de la petición).

### `equals_ignore_case`

> **equals_ignore_case**(a: string, b: string) -> bool

 `true` si `a` y `b` son iguales sin distinguir mayúsculas.

 ## Memoria
 Libera las copias intermedias; `a` y `b` son prestados.

### `pengu_string_from_cstr_len`

> **pengu_string_from_cstr_len**(data: ref to frozen char, count: int) -> string

 Declaración del runtime: copia `count` bytes y devuelve un string propio.

### `from_bytes`

> **from_bytes**(data: ref to frozen u8, count: int) -> string

 Construye un string propio a partir de `count` bytes.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`.

### `copy`

> **copy**(value: string) -> string

 Copia un string a un buffer propio.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`. `value` es prestado.

### `from_cstr`

> **from_cstr**(data: ref to frozen char) -> string

 Copia un C string (`\0` terminado) a un string propio.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`.

### `_cstr_len`

> **_cstr_len**(data: ref to frozen char) -> int

 Longitud de un C string.

### `index_of`

> **index_of**(value: string, needle: string) -> int

 Posición de `needle` dentro de `value`, o `-1` si no aparece.

### `take_before`

> **take_before**(value: string, index: int) -> string

 Copia de `value` desde el principio hasta `index` (sin incluirlo).

 ## Memoria
 Devuelve una cadena nueva; libérala con `banish`.

### `take_after`

> **take_after**(value: string, index: int) -> string

 Copia de `value` desde `index + 1` hasta el final.

 ## Memoria
 Devuelve una cadena nueva; libérala con `banish`.

