# portus.group

> Source: `src/portus/group.pengu`

portus.group — un prefijo de rutas sobre una aplicación.

Un `Group` no tiene estado propio ni un despachador aparte: guarda la
aplicación y el prefijo, y al registrar una ruta le antepone el prefijo. Así
se agrupan rutas (`/api`, `/admin`) sin `mount` ni copias de la aplicación.

## Ejemplo
```pengu
var api as portus.Group is calling portus.new_group with sigil of app, "/api"
let ok as result of void to portus.PatternError is calling api.get with "/users", users
```

## Memoria
El `Group` presta la aplicación: no la libera ni la copia.

---

### `Application`

> alias Application

_Undocumented._

### `Handler`

> alias Handler

_Undocumented._

### `Group`

> rune Group

```pengu
    _app as ref to Application
    _prefix as string
```

 Un grupo de rutas: la aplicación donde registra y el prefijo que antepone.

### `group_with_prefix`

> **group_with_prefix**(app: ref to Application, prefix: string) -> Group

 Crea un grupo de rutas bajo `prefix` (`"/api"`, `"/api/v1"`, …).

### `full_pattern`

> **full_pattern**(group: ref to Group, pattern: string) -> string

 Patrón completo de una ruta del grupo (prefijo + patrón).

### `get`

> **get**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `GET` bajo el prefijo.

### `post`

> **post**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `POST` bajo el prefijo.

### `put`

> **put**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `PUT` bajo el prefijo.

### `patch`

> **patch**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `PATCH` bajo el prefijo.

### `delete`

> **delete**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta `DELETE` bajo el prefijo.

### `any`

> **any**(pattern: string, handler: Handler) -> result of void to PatternError

 Registra una ruta para cualquier método bajo el prefijo.

### `"el grupo antepone el prefijo"`

> test "el grupo antepone el prefijo"

_Undocumented._

### `"el prefijo se normaliza sin barra final"`

> test "el prefijo se normaliza sin barra final"

_Undocumented._

### `"un grupo sin prefijo deja el patrón igual"`

> test "un grupo sin prefijo deja el patrón igual"

_Undocumented._

