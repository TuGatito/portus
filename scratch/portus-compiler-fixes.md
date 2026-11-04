# Portus — Arreglos necesarios en PenguScript

> Bugs del compilador que portus encontró. Todos están confirmados con una
> reproducción mínima. Estado a 2026-11-04: **1–5 arreglados** y publicados en
> 0.28.1–0.28.4; **6 arreglado** en 0.28.5; **7 arreglado** en 0.28.6;
> **8 pendiente** (workaround aplicado en portus).
>
> Checkout: `/home/tugatito/Documentos/GitHub/PenguScript` (v0.28.6, `build/psc`).
> Cada arreglo lleva su test en `tests/regress_*.sh`, su entrada en `CHANGELOG.md`
> y su bump de versión en todos los documentos.

---

## Resumen

| # | Severidad | Síntoma | Archivo | Estado | Workaround en portus |
|---|---|---|---|---|---|
| 1 | **P0** | `psc add` sin `--name` corrompe el manifiesto | `src/psc/cmd/deps.pengu` | 0.28.1 | ya no hace falta |
| 2 | P1 | Llamar un alias de tipo función da E0020 | `src/psc/sema/infer/core.pengu` | 0.28.2 | ya no hace falta |
| 3 | P1 | Genérico que reenvía un `of T` genera `void**` | codegen/sema de genéricos | abierto | no reenviar; tipo concreto |
| 4 | P2 | Llamar un campo-función da E0004 | `src/psc/sema/infer/core.pengu` | 0.28.4 | ya no hace falta |
| 5 | P2 | Nombre de variable = keyword C genera C inválido | `src/psc/codegen/context.pengu` | 0.28.3 | ya no hace falta |
| 6 | P1 | `-D_POSIX_C_SOURCE` ausente: el C no ve la API POSIX | `src/psc/driver/toolchain.pengu` | 0.28.5 | ya no hace falta |
| 7 | P1 | `ref to Alias` no resuelve métodos del tipo aliaseado | `src/psc/sema/infer/core.pengu` | 0.28.6 | ya no hace falta |
| 8 | P1 | `padre.hijo.fn` desde el módulo `padre` llama a la `fn` local | sema (resolución de llamadas) | abierto | renombrar la función |
| 9 | P1 | Bloque (`do:` / `if` multilínea) en posición de valor emite sólo la última expresión | parser+codegen (paridad) | abierto | `if c then a else b` |

---

## 1. P0 — `psc add` sin `--name` corrompe el manifiesto

**Impacto:** rompe exactamente el comando que el usuario quiere que funcione:
`psc add https://github.com/TuGatito/portus --trust`.

**Repro:**

```sh
mkdir -p /tmp/demo/src
cat > /tmp/demo/pengu.toml <<'EOF'
[project]
name = "demo"
version = "0.1.0"
output = "exe"
EOF
cat > /tmp/demo/src/main.pengu <<'EOF'
weave main into int:
    return 0
EOF

mkdir -p /tmp/demo-consumer/src
cd /tmp/demo-consumer
psc add /tmp/demo          # sin --name
cat pengu.toml
```

Salida:

```toml
[dependencies]
demo = { path = "demo      " }     # basura con padding
```

y el instalador falla con `cannot install dependency 'demo'`.

**Causa:** en `src/psc/cmd/deps.pengu`, el weave `derive_name` hace:

```pengu
var s as string is source          # comparte buffer con el argumento
# ... recorta/deriva el nombre ...
banish s                           # libera el buffer de `source`
# ... a partir de aquí se usa `d`, que apunta a memoria liberada ...
```

`banish` sobre un `string` libera el buffer; como `s` es una vista de `source`
(al no haber copia), el argumento del llamador queda colgando. El nombre
derivado se construye después leyendo esa memoria.

**Arreglo propuesto:** no banear un valor prestado.

```pengu
let s as string is calling scrolls.copy with source
defer banish s
```

o, si `s` no se modifica, eliminar el `banish` (el llamador es el dueño).

**Test de regresión:** un test del comando `add` que ejecute sin `--name` y
verifique el `pengu.toml` generado y la instalación en `lib/`.

**Workaround en portus:** documentar `psc add <url> --name portus --trust`
hasta que salga 0.28.1.

---

## 2. P1 — Alias de tipo función: E0020 al llamar

**Impacto:** impide declarar `type Handler = weave with context as ref to
Context into result of void to HttpError` y usarlo como parámetro. Es la forma
idiomática de nombrar el tipo de handler.

**Repro** (`scratch/spikes/s1e_alias.pengu`):

```pengu
alias Fn as weave with x as int into int

weave apply with f as Fn, x as int into int:
    return calling f with x
```

```
error[E0020]: return value of type 'Fn' does not match 'int'
```

**Causa:** en `src/psc/sema/infer/core.pengu`, la inferencia de llamada
(~líneas 713–721) hace:

```pengu
var ft as tk.SemaType is calling types.at with s->types, fsv.type
if ft.kind is TkRef:
    set ft is ft.elem
# falta resolver TkAlias / alias de función
```

Al no resolver el alias, `calling f` devuelve el tipo del *alias* en vez del
tipo de retorno de la función.

**Arreglo propuesto:** antes de comprobar que `ft` es `TkFn`, resolver
alias/ref/seal hasta el tipo subyacente (usar el mismo helper que ya se usa en
otras rutas de inferencia, p. ej. `resolve_underlying`). Test de regresión:
el repro debe imprimir 42.

**Workaround en portus:** escribir el tipo inline en cada firma. Cuando el bug
se arregle, se puede introducir `alias Handler as weave …` (cambio compatible).

---

## 3. P1 — Genérico que reenvía `of T` genera `void**`

**Impacto:** impide los helpers genéricos sobre la arena
(`weave alloc_in shard T with a as ref to Arena into ref to T`).

**Repro** (`/tmp/s4/src/main.pengu`):

```pengu
weave gen_alloc shard T with a as ref to Arena into ref to T:
    return calling arena_alloc_one of T with a
```

C generado:

```c
error: returning 'void **' from a function with incompatible return type 'Thing *'
```

La llamada concreta (`arena_alloc_one of Thing`) sí funciona.

**Causa:** al monomorfizar, el tipo de retorno de la llamada se calcula con el
tipo genérico (`T` → `void*`) en lugar de sustituir `T` por el argumento
concreto antes de emitir el `return`.

**Arreglo propuesto:** en la emisión de la llamada genérica, sustituir los
parámetros de tipo en el tipo de retorno **antes** de generar C; mismo camino
que ya se usa para `memoria.alloc of T`.

**Workaround en portus:** la configuración de middleware se aloja con tipo
concreto en el sitio de llamada (`arena_alloc_one of MarcaConfig`), nunca
dentro de un `shard T`.

---

## 4. P2 — Llamar un campo-función da E0004

**Impacto:** un `Handler` guardado en un campo de runa no se puede invocar
directamente (`calling route.handler with context`).

**Repro:**

```pengu
rune Route:
    handler as weave with n as int into int

weave call_it with route as ref to Route into int:
    return calling route.handler with 5
```

```
error[E0004]: undefined function 'handler'
```

**Causa:** el camino de llamada con callee que es un *field access* se envía a
resolución de métodos en vez de probar primero el tipo del campo.

**Arreglo propuesto:** en `infer_call`, si el callee es `tk.TkFieldAccess` (o
equivalente), comprobar si el tipo del campo es `TkFn`; si lo es, emitir
llamada indirecta; si no, seguir con la resolución actual.

**Workaround en portus:** `let f as <tipo inline> is route.handler` y llamar
`f`. Documentado en `portus-rules.md` §8.

---

## 5. P2 — Nombre de variable = keyword C genera C inválido

**Impacto:** una variable llamada `double`, `long`, `int`, `char`, `signed`…
genera C que no compila.

**Repro:**

```pengu
weave main into int:
    let double as weave with x as int into int is lambda x as int into x * 2
    let n as int is calling double with 21
    return n
```

C generado:

```c
int32_t (*double_)(int32_t) = ...;   /* declaración: manglada */
int32_t n = double(21);              /* llamada: sin mangle */
```

```
error: expected expression before 'double'
```

**Causa:** `safe_c_name` (`src/psc/codegen/context.pengu:294`) añade `_` a los
keywords C, pero el sitio de la llamada emite el nombre crudo.

**Arreglo propuesto:** usar el mismo helper de mangla en la emisión de
llamadas (y en cualquier otro sitio que emita un identificador de variable:
declaración, uso, dirección, etc.).

**Workaround en portus:** ningún identificador interno es keyword C. Los
usuarios que lo hagan verán el error hasta 0.28.1.

---

## 6. P1 — El C generado no ve la API POSIX (−D_POSIX_C_SOURCE)

**Impacto:** cualquier binding de sockets, tiempo o red falla al compilar. Sin
esto no hay servidor HTTP posible.

**Repro:** proyecto con `include/probe.h` que haga `#include <time.h>` y use
`clock_gettime(CLOCK_MONOTONIC, …)`:

```
error: declaración implícita de la función ‘clock_gettime’
error: ‘CLOCK_MONOTONIC’ no se declaró aquí
```

Con `struct addrinfo`/`getaddrinfo` el error es «tipo desconocido».

**Causa:** `src/psc/driver/toolchain.pengu` añade siempre `-std=c11`; eso define
`__STRICT_ANSI__` y glibc/musl ocultan las declaraciones POSIX. Definir
`_POSIX_C_SOURCE` desde el header del proyecto llega tarde: el bundle incluye
`<pengu_runtime.h>` antes.

**Arreglo (0.28.5):** añadir a las cflags por defecto, tras `-std=c11` y `-Wall`:

```pengu
calling cflags.push with "-D_POSIX_C_SOURCE=200809L"
calling cflags.push with "-D_DEFAULT_SOURCE"
```

El C sigue siendo C11 estricto; solo se expone la API POSIX.

**Test:** `tests/regress_posix_c.sh` (un `#error` si falta la macro y una llamada
real a `clock_gettime`).

---

## 7. P1 — `ref to Alias` no resuelve los métodos del tipo aliaseado

**Impacto:** una fachada que reexporta un tipo con `alias` funciona por valor
pero no cuando el handler recibe `ref to Tipo` — justo la firma de un handler
HTTP.

**Repro:**

```pengu
# demo/core.pengu: rune Widget + enchanting Widget { weave label ... }
# demo.pengu:      alias Widget as demo.core.Widget
weave probe with w as ref to Widget into string:
    return calling w.label with "!"
```

`error[E0004]: undefined function 'label'`. Por valor (`w as Widget`) sí resuelve.

**Causa:** en `src/psc/sema/infer/core.pengu`, `infer_call` hacía
`set recv_lookup is rty.a` al desenvolver un `TkRef`, sin volver a resolver el
alias. `underlying_type` solo atraviesa `TkAlias`/`TkSeal`, no punteros.

**Arreglo (0.28.6):**

```pengu
if rty.kind == tk.SemaTypeKind.TkRef:
    set recv_lookup is calling underlying_type with s, rty.a
```

**Test:** `tests/regress_ref_alias_method.sh`.

---

## 8. P1 — `padre.hijo.fn` desde el módulo `padre` resuelve a la `fn` local

**Impacto:** una fachada no puede llamar a una función de su submódulo si ambas
se llaman igual (caso normal: `portus.serve` → `portus.transport.serve`). La
llamada se resuelve a la función local y el C generado se llama a sí mismo
(`-Winfinite-recursion`).

**Repro:**

```pengu
# demo/core.pengu: weave default_options into int: return 7
# demo.pengu:      weave default_options into int: return 1
#                  weave run into int: return calling demo.core.default_options
# esperado 7, imprime 1; el C generado llama a demo_default_options()
```

Con nombres distintos (`demo.core.other`) funciona.

**Causa:** en la resolución de llamadas cualificadas, cuando el primer segmento
coincide con el módulo actual, la búsqueda acaba encontrando el símbolo local
del mismo nombre antes que el del módulo importado.

**Arreglo propuesto:** en `infer_call`, si la llamada viene con cualificación de
módulo (`base_import >= 0`) y el símbolo del módulo no aparece, devolver E0004
en vez de caer al registro local; y resolver primero el módulo cualificado.

**Test:** pendiente (`tests/regress_qualified_local_name.sh`).

**Workaround en portus:** `portus.transport.serve` se llama `serve_loop` y
`portus.transport.default_options` se llama `base_options`.

---

## 9. P1 — Bloque en posición de valor: emite sólo la última expresión

**Impacto:** el patrón más natural de PenguScript (`let x as int is do:` …) produce
C que compila pero calcula mal, sin diagnóstico. Es peor que un error: pasa
desapercibido.

**Repro A (`do:`):**

```pengu
let v as int is do:
    var a as int is 5
    a + 4
```

C generado: `int32_t v = (a + 4);` — la sentencia `var a` se descarta y `a` queda
sin declarar (`C compiler failed`).

**Repro B (`if` multilínea):**

```pengu
let v as int is if flag:
    1
else:
    0
```

C generado: `int32_t v = 0;` — la condición y las ramas desaparecen; siempre 0.
La documentación de 0.28.0 lo presentaba como soportado.

**Causa:** el `if`/`unless` multilínea en posición de valor se parsea como
`if_stmt` (no como `if_expr`), y `emit_expr` no lo trata: cae en el `return "0"`
final. El sema, por su política de "constructo fuera del subconjunto modelado",
le asigna el tipo esperado y no se queja (`docs/PARITY.md`). Para `do:`
(`NdDoExpr`) el codegen emite **sólo** el último hijo, sin las sentencias previas.

**Estado:** **paridad pendiente del compilador**, no un arreglo de una línea.
Requiere que el emisor pueda capturar sentencias dentro de una expresión C
(`__extension__(({ … }))`), lo que hoy no existe: `emit.line` escribe en el
buffer global. Plan: añadir captura al emisor, emitir el bloque dentro de la
statement expression y asignar el valor de la última sentencia.

**Workaround (aplicado en portus):** usar la forma de una línea
`if c then a else b` (ver `src/portus/transport.pengu`, cálculo de `keep`), o un
`weave` auxiliar. La documentación del lenguaje ya se corrigió para 0.28.6.

**Test:** pendiente; el de regresión debe comprobar la forma correcta cuando se
implemente (no se añade uno que congele el comportamiento actual).

---

## 10. Cómo se arreglan (proceso)

1. Rama por bug en el checkout de PenguScript:
   `fix/deps-derive-name`, `fix/alias-fn-call`, `fix/generic-forward`,
   `fix/field-call`, `fix/c-keyword-mangle`.
2. Cada rama: repro mínimo como test en el runner del compilador + el arreglo.
3. `psc test` del compilador verde + `psc check` de los ejemplos.
4. PR upstream con: síntoma, repro, causa, arreglo, riesgo.
5. Mientras tanto, portus no depende de las ramas: los workarounds están en el
   código.

### Plantilla de PR

```markdown
## Síntoma
<error exacto + repro de 6 líneas>

## Causa
<archivo:línea + explicación>

## Arreglo
<qué cambia y por qué es seguro>

## Tests
- `tests/<archivo>`: <caso>

## Riesgo
<qué podría romper; por qué no>
```

---

## 11. Notas de ergonomía (no son bugs, pero duelen)

Detectadas escribiendo los spikes; no bloquean portus, pero conviene saberlas:

- `with` exige la forma `campo is valor`; `Runa with campo is v` (prefijado por
  tipo) es E0000. El tipo se infiere del destino.
- `at` es operador, no método: `lista at 0`, no `calling lista.at with 0`.
- Toda sentencia necesita prefijo `calling`/`let`/`var`/`set`/`return`; una
  llamada suelta no es sentencia.
- `let` produce `ref to frozen T`; para mutar hace falta `var`.
- El literal de lista vacía necesita tipo: `let xs as list of Handler is []`.
- Un literal de runa genérica necesita una variable tipada intermedia.
- `io.println` no existe; `io.write_stdout` sí (devuelve `bool`).
