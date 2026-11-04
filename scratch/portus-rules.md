# Portus — Reglas de código y configuración

> Reglas obligatorias para el código de portus. Salen de
> [`scratch/PENGUNIC.es.md`](PENGUNIC.es.md) y de los hallazgos de los spikes.
> Si una regla choca con el compilador, se documenta la excepción y se abre un
> bug.

---

## 1. `pengu.toml` del proyecto

```toml
[project]
name = "portus"
version = "0.1.0"
output = "static"
output_name = "portus"

[build]
src_dir = "src"
include_dir = "include"
c_dir = "c"
strict_c99 = true
links = []
lib_dirs = []
include_dirs = []
cflags = ["-Wall", "-Wextra", "-Wno-unused-parameter"]
ldflags = []
defines = []

[profiles.release]
cflags = ["-O2", "-DNDEBUG", "-fvisibility=hidden"]
```

- `output = "static"`: portus es una librería; nunca `exe` en `main`.
- Los ejemplos que necesiten binario usan `psc build --exe`.
- `c_dir` queda reservado para C auxiliar; v1 es header-only.
- `strict_c99 = true` desde el día uno.

### Cómo lo consume un proyecto

```toml
[dependencies.portus]
url = "https://github.com/TuGatito/portus"
version = "^0.1"
```

```sh
psc add https://github.com/TuGatito/portus --trust
```

> Hasta que se arregle el bug P0 del `derive_name`, usar
> `psc add <url> --name portus --trust`. El README lo indica.

---

## 2. Estructura del repositorio

```
portus/
├── pengu.toml
├── .pengufmt.toml
├── README.md
├── CHANGELOG.md
├── LICENSE
├── THIRD_PARTY.md
├── src/
│   ├── portus.pengu
│   └── portus/
│       ├── native.d.pengu
│       ├── transport.pengu
│       ├── ... (ver portus-design.md §10)
│       └── middleware/
├── include/
│   ├── portus_native.h
│   └── llhttp/            # vendorizado: LICENSE, SOURCE, VERSION
├── examples/
│   ├── hello-world/
│   ├── params/
│   └── ...
├── tools/
│   ├── fetch_llhttp.sh    # regenera include/llhttp
│   └── amalgamate.sh      # .c → llhttp_amalgam.c
└── tests/                 # e2e en bash (curl) además de `test "…"`
```

Regla: **el código de portus no vive en `std/`**. `std/` es del compilador; el
layout es el de un paquete (`src/portus/...`).

---

## 3. `.pengufmt.toml`

```toml
indent = 4
tab_size = 4
tabs = false
insert_spaces = true
blank_lines_max = 2
```

`psc fmt --check` en CI. Nunca mezclar formato y comportamiento en un commit.

---

## 4. Orden canónico de un módulo

```pengu
## Docstring del módulo: una frase de propósito, qué NO hace, ownership,
## y un ejemplo corto.

include "portus_native.h"     # solo en native.d.pengu
link "llhttp"

import std.io
import portus.context

const MAX_ALGO as int is 8    # solo si es de verdad constante

concept Handler:
    ...

rune Thing:
    ...

omen Flavor:
    ...

seal UserId

bind Thing with Handler:
    ...

enchanting Thing:
    weave ...

weave free_function with ...
    ...

# helpers privados (con `_` o simplemente no documentados)

test "descripción":
    ...
```

- Un módulo = una responsabilidad; límite blando **400 líneas** de cuerpo.
- Sin estado mutable de módulo. Si hace falta, `static var` **documentado**.
- Imports uno por línea; alias solo para colisión real.

---

## 5. Nombres (tabla aplicada)

| Elemento | Regla | Ejemplo |
|---|---|---|
| Fichero | `snake_case.pengu` | `static_file.pengu` |
| Binding | `X.d.pengu` | `native.d.pengu` |
| Test | `<módulo>_test.pengu` | `route_test.pengu` |
| Módulo | sustantivo concreto | `cookie`, nunca `utils` |
| Rune/omen/seal/concept | `PascalCase` singular | `Application`, `HttpError` |
| Variable | `snake_case` sustantivo | `max_body_bytes` |
| Constante | `SCREAMING_SNAKE` | `STATUS_OK` |
| Función | verbo base, sin `-ing`, sin tipo | `send_json`, `set_header` |
| Predicado | `is_*`, `has_*`, `can_*` | `is_sent` |
| Constructor | `new_*`, `from_*`, `with_*` | `new_application` |
| Constructor falible | `try_*` | `try_parse` |
| Getter trivial | sustantivo | `port`, `count`, `config` |
| Getter no trivial | `get_*` | `get_state` |
| Conversión infalible | `to_*` | `to_hex` |
| Conversión falible | `parse_*` | `parse_cookies` |
| I/O lineal | `read_*` / `write_*` | `read_text` |
| Fallback | sufijo `_or` | `param_or` |
| Índices | `i`, `j`, `k` | |
| Genéricos | `T`, `E`, `Item` | |

**Prohibido:** `Ctx`, `req`, `res`, `cfg`, `mw`, `utils`, `helpers`, `common`,
`misc`, `base`, `core`, `manager`, `…Helper`, `…Manager`. La abreviatura `app`
se permite **solo** como variable local en ejemplos y tests.

---

## 6. Documentación (`##`)

Todo símbolo público lleva `##`. Plantilla de función:

```pengu
## Envía `body` como `text/plain` y cierra la respuesta.
##
## Toma posesión de `body`: no lo banees después.
## Devuelve `Err(HttpError.Internal)` si la respuesta ya fue enviada.
weave send_text with body as string into result of void to HttpError:
```

Plantilla de rune:

```pengu
## Configuración de una `Application`.
##
## Se crea con `new_config` y se pasa a `new_application_with`.
## Todos los strings son prestados por el llamador.
rune Config:
```

Reglas:

- Primera línea: frase completa en imperativo.
- Documentar **ownership** de cada parámetro que se guarde o libere.
- Documentar **cada camino de fallo** de un `result`.
- Sin `##` no hay API pública: el símbolo es privado.

---

## 7. Errores

- `result of T to HttpError` en handlers y en la API de respuesta.
- `result of T to <OmenConcreto>` en parsers (`BodyError`, `PatternError`,
  `ListenError`, `ServerError`).
- **Nunca** `result of T` sin tipo de error.
- **Nunca** `maybe` para señalar un fallo con causa.
- **Nunca** `panic` para entrada del usuario.
- `panic` solo para invariantes internas rotas (p. ej. `_sealed` violado en
  debug).

---

## 8. Memoria

1. El que crea, libera. Documentado.
2. `send_*` **mueve** el cuerpo al `Response`.
3. La config de middleware se aloja en la arena de la app; el middleware no la
   libera.
4. Nunca se banea un parámetro prestado.
5. Baneo de `list of string` elemento a elemento (`pengu_banish_string_list`
   no está expuesto; usar bucle o helper documentado).
6. `ref to T` no se devuelve nunca desde portus salvo que el docs digan
   "prestado de X, válido mientras X viva".
7. Tras `banish app`, ningún `Server`/`Context` se usa.

---

## 9. Rendimiento (reglas de sentido común)

- Nada de `malloc` por petición en el shim; buffers por conexión.
- Nada de `malloc` por petición en Pengu si se puede reutilizar el buffer.
- Nada de regex por petición: patrones compilados al registrar.
- Nada de formatear strings en el logger si el nivel está apagado.
- Copiar es aceptable en v1; medir antes de optimizar.

---

## 10. Tests

- Bloques `test "…":` en el mismo módulo (unitarios).
- Tests de integración con `TestClient`.
- e2e en `tests/*.sh` con `curl` para lo que depende del socket real.
- Nombres de test en español, descriptivos: `test "un param no matchea vacío":`.
- Sin reloj ni red en unitarios; sin asserts sobre mensajes de panic.

---

## 11. Checklist de PR

- [ ] `psc fmt --check` limpio.
- [ ] `psc check` sin warnings.
- [ ] `psc test` verde.
- [ ] `psc doc` sin `_Undocumented._` en símbolos públicos nuevos.
- [ ] Tests nuevos para la lógica nueva.
- [ ] Ownership documentado en cada función que asigna.
- [ ] Sin dependencias nuevas (o aprobadas en `portus-decisions.md`).
- [ ] Sin `panic` nuevo para entrada de usuario.
- [ ] Si toca el compilador: test de regresión en PenguScript.
