# portus.multipart

> Source: `src/portus/multipart.pengu`

portus.multipart — parser de `multipart/form-data`.

Pensado para formularios (campos de texto y ficheros pequeños). No valida
cuotas por parte ni decodifica `Content-Transfer-Encoding`; el límite de
tamaño lo pone `Config.max_body_bytes` antes de llegar aquí.

## Memoria
Las partes que devuelve `parse` son propias: `free_parts` libera la lista,
cada nombre, cada nombre de fichero y cada cuerpo.

---

### `Part`

> rune Part

```pengu
    name as string
    filename as string
    content_type as string
    data as string
```

 Una parte del formulario.

### `free_part`

> **free_part**(part: Part) -> void

 Libera una parte.

### `free_parts`

> **free_parts**(parts: list of Part) -> void

 Libera una lista de partes.

### `boundary_of`

> **boundary_of**(content_type: string) -> string

 Extrae el `boundary` de un `Content-Type`.

### `_param`

> **_param**(header: string, key: string) -> string

 Valor de un parámetro `clave="valor"` dentro de una cabecera de parte.

### `_fill_header`

> **_fill_header**(part: ref to Part, header: string) -> void

 Interpreta las cabeceras de una parte.

### `parse`

> **parse**(content_type: string, body: string) -> maybe list of Part

 Parsea un cuerpo `multipart/form-data`; `none` si no hay `boundary`.

 ## Memoria
 Devuelve partes propias; libéralas con `free_parts`.

### `part_named`

> **part_named**(parts: list of Part, name: string) -> maybe Part

 Primera parte con ese nombre.

### `"parametros de cabecera"`

> test "parametros de cabecera"

_Undocumented._

