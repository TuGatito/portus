# portus.mime

> Source: `src/portus/mime.pengu`

portus.mime — tipo de contenido por extensión.

Tabla pequeña y explícita; el resto cae en `application/octet-stream`. No
consulta el sistema de ficheros ni `/etc/mime.types`.

---

### `for_path`

> **for_path**(path: string) -> string

 Tipo de contenido para `path` (mira solo la extensión).

 ## Memoria
 Devuelve un literal prestado; no lo banishes.

### `_last_dot`

> **_last_dot**(path: string) -> int

 Posición del último `.` del nombre, o -1.

### `_lookup`

> **_lookup**(ext: string) -> string

 Tabla de extensiones.

