# Portus — Arreglos necesarios en PenguScript

> Bugs del compilador que portus encontró. Todos están confirmados con una
> reproducción mínima. Estado a 2026-11-04: **1–7, 11, 12, 21 y 23–26
> arreglados** y publicados en 0.28.1–0.29.1; **8, 9, 10, 14, 15, 16, 17, 18,
> 19 y 20 los arregla 0.29.0/0.29.1** (comprobado con `psc test` y las
> regresiones de portus); 13 resultó ser otra cara de 15 y no se reproduce con
> el receptor en un local; 16–20 son hallazgos de F3–F5; 22 es del propio
> bootstrap del compilador.
>
> Checkout: `/home/tugatito/Documentos/GitHub/PenguScript` (v0.29.1, `build/psc`).
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
| 10 | P1 | Un `omen` algebraico no se puede usar en posición de valor | codegen/sema de omens | abierto | `rune { kind, message }` |
| 11 | P2 | `[]` emite una temporal sin usar (`-Wunused-variable`) | `src/psc/codegen/expr.pengu` | 0.28.7 | ya no hace falta |
| 12 | P1 | `maybe Fn` / `result of … to …` con payload de tipo función emite C inválido | `src/psc/codegen/expr.pengu` (`result_ctor`) + `ctype.pengu` | 0.28.8 | ya no hace falta |
| 13 | P2 | Llamar un método a través de un campo `ref` emite `T**`; `set x.campo` sobre un `ref` | codegen (receptores) | no reproduce (ver 15) | enlazar a un local |
| 14 | P1 | Un módulo llamado `router`/`routes` no emite sus weaves libres | codegen/DCE (sin reducir) | abierto | módulo `portus.group` |
| 15 | P1 | Un nombre suelto resuelve a la función del módulo **consumidor**, no a la del importado | sema (resolución de nombres) | abierto | calificar `modulo.fn` |
| 16 | P2 | `"{m or else \"x\"}"` imprime el texto literal, sin error | parser de interpolación | abierto | enlazar antes de interpolar |
| 17 | P2 | Un `rune` local con el nombre de un tipo importado genera C inválido | codegen (nombres de tipos) | abierto | renombrar el rune |
| 18 | P2 | Un `alias` en posición de retorno no acepta el tipo subyacente | sema (retornos) | abierto | devolver el tipo concreto |
| 19 | P1 | Un `or:` tras una llamada se liga al último argumento y pasa `NULL` | parser (postfijos) | abierto | enlazar el resultado |
| 20 | P2 | La caché de `psc build` ignora cambios en headers C | driver (caché) | abierto | `--no-cache` tras tocar headers |
| 21 | **P0** | `psc add` de una dependencia `path` copia `lib/`/`build/` (recursión, disco lleno) y `upsert` duplica `[dependencies]` | `src/psc/pkg/source.pengu`, `src/psc/pkg/manifest.pengu` | 0.28.9 | ya no hace falta |
| 22 | P1 | Dentro del compilador, un nombre suelto resuelve a otra función homónima (`has_type_param` de `sema/types/compat`) | `src/psc/codegen/generics.pengu` | 0.29.1 | calificar `catalog.` |
| 23 | P1 | `if c then a else b`: el `else` se bajaba un nivel y perdía `to string` / `some` | `src/psc/codegen/expr.pengu` (`if_expr_expected`) | 0.29.1 | ya no hace falta |
| 24 | P1 | Llamada genérica sin `of T` en el mismo módulo de la plantilla emitía el nombre base | `src/psc/codegen/expr.pengu` | 0.29.1 | ya no hace falta |
| 25 | P1 | `return with Variante is campo` no cerraba el literal compuesto del omen | `src/psc/codegen/expr.pengu` (`omen_init_expr`) | 0.29.1 | ya no hace falta |
| 26 | P2 | `calling f of T` podía pisarse con `by_name` de otro módulo | `src/psc/codegen/expr.pengu` | 0.29.1 | ya no hace falta |

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

## 10. P1 — Un `omen` algebraico no se puede usar en posición de valor

**Impacto:** obliga a diseñar los errores como `rune { kind, … }` en vez de como
unión etiquetada. portus tuvo que rehacer `HttpError` y `PatternError` por esto.

**Repro A (variante sin payload como valor):**

```pengu
omen E:
    Empty
    Many with count as int

weave f into result of int to E:
    return calling err_of with E.Empty
```

C generado: `E _p = E_Empty;` — asignar la constante del enum a la estructura
`E` no compila (`inicializador no válido`). La forma correcta sería
`(E){.tag = E_Empty}`.

**Repro B (variante con payload en un local):**

```pengu
var e as E is with Many is with count is 3
```

C generado: `E e = (E){.Many = (E){.count = 3}};` — el compuesto interno usa el
tipo del omen en vez del tipo del payload (`E_Many`) y el externo no lleva
`.tag`/`.data`. gcc: "no tiene un miembro llamado 'Many'".

**Repro C (`judge` sobre el omen algebraico):**

```pengu
return judge e:
    when Empty -> 404
    when Many -> 500
```

C generado: `switch ((int64_t)(e)) { case Empty: … }` — usa el nombre de la
variante como etiqueta (no existe) y convierte la estructura a entero. Sólo
funciona el `judge` sobre omens numéricos o de string.

**Lo que sí funciona:** la expresión `with V is with campo is valor` **como
argumento directo** de una función cuyo parámetro es el omen (`respond_error(res,
(with V is with x is y))`) emite el literal correcto
`(E){.tag = E_V, .data.V = {.x = y}}`.

**Workaround (aplicado):** errores como `rune HttpError { kind as HttpErrorKind,
message as string }` con un `omen` **numérico** para la categoría, y constructores
(`not_found`, `bad_request(msg)`, …). Es el patrón de `std.json.JsonError`, así
que además es idiomático.

**Estado:** abierto (sema+codegen). Arreglo: `codegen/expr.pengu` debe emitir
`(T){.tag = T_V}` para variantes sin payload y `(T){.tag = T_V, .data.V = …}`
para las que lo llevan en **todos** los contextos; el `judge` debe bajar a
`switch (v.tag)` con las constantes `T_V`.

---

## 11. P2 — Lista vacía `[]` emite una variable temporal sin usar

**Impacto:** `-Wunused-variable` en cada build del consumidor que use `-Wall`
(y un fallo con `-Werror`). Cero riesgo, arreglo trivial.

**Repro:** `var xs as list of int is []` genera
`PenguList xs = __extension__(({ PenguList _ll = pengu_list_new(sizeof(int32_t), 0); int32_t _le; _ll; }));`
— `_le` no se usa nunca.

**Arreglo (aplicado en 0.28.7):** en `src/psc/codegen/expr.pengu`, emitir la
variable de elemento sólo cuando `n > 0`, en los dos constructores de lista
(`list_literal_expr` y la ruta de `list_init_expr`).

**Regresión (0.28.7):** `tests/regress_empty_list.sh` compila con
`cflags = ["-Wall", "-Werror"]` un proyecto que declara `[]` y hace `push`;
antes fallaba con `-Wunused-variable`, ahora imprime `1`. Verificado también
que portus (que usa `[]` en varios sitios) compila sin avisos.

---

## 12. P1 — `maybe`/`result` con payload de puntero a función emiten C inválido

Reproducción (`/tmp/b12/src/main.pengu`):

```pengu
alias Fn as weave with x as int into int

rune App:
    call as maybe Fn

weave twice with x as int into int:
    return x * 2

weave main into int:
    var app as App is with call is maybe none
    set app.call is some (twice)
    if app.call is present:
        let f as Fn is app.call.value
        calling spark.println with "{calling f with 21}"
    return 0
```

C generado (recortado):

```c
app.call = __extension__(({ int32_t (*)(int32_t) _p = (main_twice); int32_t (*)(int32_t)* _h = (int32_t (*)(int32_t)*)malloc(sizeof(int32_t (*)(int32_t))); ... }));
```

`gcc` falla con `expected identifier or '(' before ')' token` (el declarador de
`_p` no lleva nombre dentro de `(*)`), luego `_h`/`_p` «no se declararon» y
`app.call.value` da `expected ')' before '*'`.

**Causa.** `result_ctor` en `src/psc/codegen/expr.pengu` (~línea 1339) monta el
payload como `{c_type} _p = {valor};` y `{c_type}* _h = …`; con un tipo función
`c_type` es `R (*)(A)`, y el nombre tiene que ir dentro del `(*)`. Ya existe el
helper `name_fn_declarator` en `src/psc/codegen/function.pengu:171`, que inserta
el nombre en el hueco `(*)` (`has_fn_slot` en `:196` detecta si hay hueco).

**Arreglo propuesto.** En los dos constructores (`maybe`/`some` ~1339-1355 y
`ok_of`/`err_of` ~1430-1450):

```pengu
var paydecl as string is pt
if calling has_fn_slot with pt:
    set paydecl is calling name_fn_declarator with pt, "_p"
else:
    set paydecl is "{pt} _p"
```

y para el hueco del heap, `name_fn_declarator with pt, "*_h"` cuando hay hueco
(si no, el `{pt}* _h` actual). Alternativa más simple y uniforme: `void *_h` +
`memcpy` del payload, que además es lo que documenta el lenguaje («`some`
memcpy the payload»).

**Workaround en portus:** campos planos con un flag `_has_on_error`,
`_has_on_not_found`, `_has_on_start`, `_has_on_shutdown`.

---

## 13. P2 — Llamar un método a través de un campo `ref` emite `T**`

Síntomas vistos en `context.pengu`:

```pengu
calling ctx->response.is_sent     # emite &(ctx->response) → Response**
set ctx.params is …               # emite ctx.params sobre un puntero
```

Errores: `se pasa el argumento 1 de ‘Response_is_sent’ desde un tipo de puntero
incompatible` y `‘ctx’ es un puntero; ¿pretendía utilizar ‘->’?`. El mismo patrón
aparece con `calling ctx->request.path`.

**Workaround en portus:** enlazar el campo a un local
(`var response as ref to transport.Response is self->response`) o exponer
métodos en `Context` (`calling ctx.is_sent`). Hay que revisar `method_receiver`
en `src/psc/codegen/expr.pengu` para que desenvuelva también el `TkRef` del
*campo* antes de decidir si hace falta `&`.

---

## 14. P1 — Un módulo llamado `router`/`routes` no emite sus weaves libres

En el proyecto portus, `src/portus/router.pengu` (id `portus.router`) compila
(`psc check` limpio) pero sus **weaves libres** no aparecen en el bundle y las
llamadas desde la fachada salen sin prefijo:

```c
/* src/portus.pengu */  return router_with_prefix(app, prefix);
/* no existe ni portus_router_router_with_prefix ni router_with_prefix */
```

Los métodos de `enchanting` de ese módulo sí se emiten (`Router_get`,
`Router_full_pattern`). Pasa igual con `--no-dce` y tras borrar `build/`.

Matriz observada (mismo contenido, solo cambia el nombre del archivo/módulo):

| id del módulo | ¿emite sus weaves libres? |
|---|---|
| `portus.router` | **no** |
| `portus.routes` | **no** |
| `portus.routing` | sí |
| `portus.route_group` | sí |
| `portus.router2` | sí |
| `portus.group` | sí |
| `portus.prefix` | sí |

Un proyecto mínimo aparte (`/tmp/mr`, módulo `portus.router` + fachada que lo
importa) **sí** emite `portus_router_*`, así que el fallo depende de una
interacción con el resto del proyecto portus (posiblemente el módulo
`portus.route`, cuyo id es prefijo de `portus.router`, o el DCE). No se ha
reducido todavía.

**Workaround en portus:** el módulo se llama `portus.group` y el tipo `Group`.
Pendiente: reducirlo con una copia del repo e incluirlo aquí como repro.

---

## 15. P1 — Un nombre suelto resuelve a la función del módulo consumidor

Reproducción (`/tmp/b15`):

```pengu
# src/err.pengu
weave not_found into int:
    return 404

# src/app.pengu
import err
weave handler into int:
    return calling not_found

# src/main.pengu
import app
weave not_found with x as int into int:
    return x
```

`psc build` falla con `error[E0005]: too few arguments in call` señalando
`app.pengu:4` (`calling not_found`): la llamada se resolvió contra la `not_found`
de `main.pengu` (1 parámetro) en vez de la de `err.pengu` (0 parámetros).

**Causa.** La resolución de un nombre suelto de función consulta un registro
global donde el módulo de la aplicación (el consumidor) gana sobre el módulo
importado por el archivo que hace la llamada.

**Impacto real.** Rompió `examples/error` de portus: `application.pengu` llama
`not_found` de `portus.error` y el ejemplo define su propio handler `not_found`.

**Arreglo propuesto.** Si el nombre no está declarado en el módulo actual,
buscar solo en los módulos que ese módulo importa (en orden), nunca en módulos
ajenos al grafo de imports de ese archivo.

**Workaround en portus:** importar con alias y calificar siempre
(`import portus.error as error_module` + `calling error_module.not_found`).

---

## 16. Cómo se arreglan (proceso)

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

## 16. P2 — Interpolar un `or else` dentro de `{}` imprime el texto literal

**Impacto:** silencioso: el string sale con `{q or else "none"}` en vez del
valor. Afecta a cualquier mensaje con un `maybe` sin enlazar antes.

**Repro:**
```pengu
weave pick into int:
    let m as maybe string is some ("ada")
    return m or else "none"

weave main into int:
    let m as maybe string is some ("ada")
    calling spark.println with "hola {m or else \"none\"}"   # imprime el texto tal cual
    return 0
```

**Causa:** el parser de interpolación no acepta la gramática completa de
expresión (`or else` con varias palabras) y, en vez de diagnosticar, deja el
texto literal.

**Workaround (portus):** enlazar primero y interpolar el binding:
```pengu
let shown as string is m or else "none"
return "hola {shown}"
```

**Arreglo propuesto:** reportar `E0000` en la interpolación cuando no se puede
parsear la expresión, o aceptar `or else` (preferible) con paréntesis
obligatorios.

## 17. P2 — Un `rune` local con el mismo nombre que un tipo importado

**Impacto:** silencioso hasta el C. Si un módulo define `rune Handler` y ya
importa un `Handler` (el alias de `portus.handler`), hay posiciones en las que
`Handler` resuelve al importado y el C generado es inválido:

```
void* _raw_handler(PenguResult (*box)(Context*)*) { ... }
error: expected ‘;’, ‘,’ or ‘)’ before ‘*’ token
```

En otras posiciones resuelve al local, así que el error aparece lejos de la
declaración y solo al compilar.

**Repro:** módulo con `import portus.handler` y `rune Handler { invoke as ... }`;
una función `antiquus _raw_handler with box as ref to Handler` emite el tipo del
alias importado (`PenguResult (*)(Context*)`) en vez del rune local.

**Workaround (portus):** renombrar el rune local (`Handler` →
`SocketHandler`) y exportarlo con `alias` desde la fachada.

**Arreglo propuesto:** la resolución de nombres debe preferir la declaración del
módulo actual sobre un símbolo importado y, si hay ambigüedad real, emitir
`E0046`/`W0005` en vez de elegir en silencio.

## 18. P2 — Un `alias` de tipo en posición de retorno no acepta el subyacente

**Impacto:** bloquea fachadas idiomáticas: `alias WebSocketHandler as
websocket.SocketHandler` y `weave ws_handler ... into WebSocketHandler: return
calling websocket.handler_with ...` da

```
error[E0005]: type mismatch: expected 'WebSocketHandler', got 'SocketHandler'
```

aunque el alias es transparente en parámetros y en variables.

**Repro:**
```pengu
# a.pengu
rune Thing:
    n as int
weave make into Thing:
    return with n is 1

# main.pengu
import a
alias Wrapped as a.Thing
weave build into Wrapped:
    return calling a.make
```

**Workaround (portus):** declarar el retorno con el tipo concreto
(`websocket_module.SocketHandler`) y mantener el alias para los parámetros.

**Arreglo propuesto:** en la comprobación de retorno, desenvolver `TkAlias`
igual que ya se hace para llamadas (bug 2) y para `c_type` (0.28.2).

---

## 19. P1 — Un `or:` tras una llamada se liga al último argumento

**Impacto:** el programa compila y luego hace `call 0x0` (SIGSEGV) al despachar.
Costó una sesión de depuración con `gdb`.

**Repro:**
```pengu
weave hello with context as ref to Context into result of void to HttpError:
    return calling ok_void

weave main into int:
    var app as Application is calling new_application
    calling get with sigil of app, "/", hello or:
        return 1
    return 0
```

**Síntoma:** el C generado pasa `NULL` como último argumento
(`portus_application_add(&app, "GET", "/", 0)`) y el servidor muere al primer
request con `0x0000000000000000 in ?? ()` desde
`portus_application__invoke_entry`.

**Causa:** el parser acepta `or:` sobre una llamada pero lo asocia al último
argumento en vez de al resultado de la llamada.

**Workaround (portus y ejemplos):** enlazar el resultado antes:
```pengu
let added as result of void to PatternError is calling get with sigil of app, "/", hello
```

**Arreglo propuesto:** en el parseo de postfijos, `or:`/`or else`/`or return`
deben aplicarse al nodo de llamada completo, no al argumento.

---

## 20. P2 — La caché de `psc build` no ve cambios en headers C incluidos

**Impacto:** silencioso y peligroso: tras tocar `include/*.h` de una dependencia
`path`, el consumidor sigue compilando el bundle cacheado y enlaza el código
viejo. Produjo un `stack-buffer-overflow` fantasma durante una hora.

**Repro:** compilar un consumidor, cambiar una función `static` del header de la
dependencia, volver a `psc build` sin `--no-cache`: el binario no cambia.

**Causa:** la clave de caché no incluye el contenido/mtime de los headers que
entran por `include "..."`.

**Workaround:** `psc build … --no-cache` después de tocar headers; en el e2e de
seguridad el consumidor es nuevo, así que no hace falta.

**Arreglo propuesto:** añadir los headers resueltos (y los de `include_dir`) a
la clave de caché.

---

## 21. P0 — `psc add` de una dependencia `path` copia `lib/` y `build/`

**Impacto:** recursión exponencial y disco lleno. Un ejemplo con
`[dependencies.portus] path = "../.."` copiaba el repo entero, incluido su
propio `lib/portus`, que contenía otra copia del repo con ese `lib/`… Resultado:
`examples/downloads/lib` llegó a **14 GB** y el repo a 28 GB; el compilador se
quedaba minutos compilando (y terminaba en
`[PANIC] StringBuilder.reserve: capacity overflow`) porque escaneaba esos
árboles.

**Repro:** repo con `examples/x` que declara `path = "../.."`; `psc add ../..`
dentro de `examples/x`; `du -sh lib/x/examples` crece en cada `add`.

**Causa:** `src/psc/pkg/source.pengu` `copy_local` copia todos los ficheros de
`archivum.find_files src, true`, sin excluir artefactos generados.

**Arreglo:** helpers `contains`/`is_ignored_rel` en `source.pengu`: una ruta
relativa con un componente `.git`, `lib` o `build` no se copia. Así una
dependencia `path` instala solo su código.

**Test:** `tests/regress_path_dep_no_generated.sh` crea una dependencia con
`lib/junk.txt` y `build/junk.txt`, la instala con `psc add` y comprueba que
`lib/dep/src/dep.pengu` existe pero `lib/dep/lib` y `lib/dep/build` no.

---

## 22. Notas de ergonomía (no son bugs, pero duelen)

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
- Convertir un `ref to T` a `ref to void` con `transmute x to ref to void`
  emite `(void *)&x` (la dirección de la variable puntero), no el valor del
  puntero. Para el puente C de portus hizo falta un `antiquus` de una línea:
  `return (void *)app;`. Síntoma típico: ASan reporta `stack-buffer-overflow`
  leyendo dentro del marco del que pasó el puntero.
- `maybe none` es la única forma de escribir el vacío (`none` suelto es E0004).
- `calling a.b.c with …` (cadena cualificada) se resuelve mal si en el módulo
  actual hay una función con el mismo nombre final (bug 8): renombra el helper
  local (`new_group`, `new_router`) o el de la biblioteca.
- `psc add <ruta>` **no refresca** una dependencia de tipo `path` ya instalada:
  sigue usando `lib/<nombre>/` copiado. Para probar cambios locales hay que
  `rm -rf lib/<nombre>` (o `rm -rf lib` + `pengu.lock`) y volver a añadir.
  Conviene documentarlo en `docs/LIBRARIES.md`.
