# Portus — Auditoría de la idea (v1)

> Estado: auditoría completada. Este documento es la evidencia; las decisiones que
> necesito de ti están en [`portus.md`](portus.md) y el detalle en
> [`portus-decisions.md`](portus-decisions.md).
>
> Fecha: 2026-11-04 · Compilador auditado: PenguScript 0.28.0 (F13) · Repo: `portus`

---

## 1. Resumen ejecutivo

Tu idea es una **base filosófica correcta** y una **arquitectura ejecutable solo
después de 6 correcciones estructurales**. No está mal pensada: está pensada
"como stdlib de PenguScript" cuando lo que vamos a construir es un **paquete
instalable con `psc add`**, y eso cambia rutas, nombres, empaquetado y modelo de
extensiones.

Lo que se sostiene:

- La separación `Application` / `Router` / `Context` / `Handler` y el middleware
  opcional y componible.
- `result of void to HttpError` como modelo de error (con un helper, ver §5.2).
- Handlers como valores (punteros a función) sin closures: es exactamente el
  patrón que ya usa `std.functor` ("thick functions") en la stdlib.
- llhttp como parser HTTP.
- El plan por fases, con ajustes.

Lo que hay que cambiar:

1. **Rutas y empaquetado**: `std/portus/...` no funciona como paquete. La
   distribución válida es `src/portus.pengu` + `src/portus/*.pengu` +
   `include/*.h` (verificado con `psc add` real).
2. **Nombres**: `Ctx`, `req`, `res` violan PENGUNIC §5 (sin abreviaturas fuera
   del glosario). Van `Context`, `request`, `response`.
3. **Middleware**: el `concept Middleware` de la v2 **no es posible** en 0.28.0
   (no hay dispatch dinámico ni tipos existenciales; `concept` es estático y
   `bind` requiere una implementación fija por tipo). Se reemplaza por
   **dos fases**: capas `before` (pueden cortocircuitar) y `finishers` `after`
   (transforman la respuesta ya emitida: comprimir, ETag, logs, seguridad).
4. **uSockets**: no. Licencia OK (Apache-2.0) pero es un event loop + TLS
   completo; bindearlo cuesta más que escribir nuestro loop `poll(2)`
   (~800 líneas de C) y nos ata a OpenSSL. Se descarta para la v1.
5. **"Zero-copy"**: prematuro. En la v1 se copian path/headers/body a `string`
   propias (seguro, simple). Las vistas prestadas quedan como optimización
   medida de la fase 6, con contrato de vida documentado.
6. **`psc add https://github.com/Tuagatito/portus --trust`** — el comando exacto
   que pediste — **está roto en 0.28.0** por un use-after-free en
   `psc add` cuando no se pasa `--name` (bug P0, §6.1). Hay que parchear el
   compilador (o pedir el fix upstream) antes de que la experiencia de
   instalación funcione.

Además encontré **5 bugs del compilador** que afectan directamente al diseño
(§6). Uno es bloqueante para la instalación, dos son bloqueantes para escribir
la librería de forma idiomática, y dos tienen workaround.

---

## 2. Método

No me limité a leer `portus-idea.md`. Verifiqué cada supuesto contra el
compilador real:

- Leí `LANGUAGE.es.md` (2161 líneas) y `PENGUNIC.es.md` (3052) completos.
- Leí el código de la stdlib relevante (`net`, `filum`, `functor`, `digest`,
  `cipher`, `compressus`, `json`, `chronicle`, `arena`, `memoria`, `regulus`,
  `archivum`, `secrets`, `web/url`, `web/websocket`).
- Leí el compilador: `src/psc/cmd/deps.pengu`, `src/psc/pkg/build_script.pengu`,
  `src/psc/driver/build.pengu`, `src/psc/sema/infer/core.pengu`,
  `src/psc/codegen/context.pengu`.
- Escribí 7 spikes en `scratch/spikes/` y un shim C en `include/spike_native.h`
  para probar: handlers, alias de tipos función, callbacks C→Pengu, FFI de
  `ref to void`, paso de weaves como valores, ownership con `banish`.
- Empaqueté un paquete de prueba (`/tmp/demo`) con C header-only y lo instalé
  con `psc add` en un proyecto consumidor (`/tmp/demo-consumer`), incluyendo el
  layout con namespaces (`src/demo/native.d.pengu`).
- Verifiqué licencias de llhttp, uSockets, mbedTLS, OpenSSL, miniz y SDS.

Todo lo que digo abajo está respaldado por una prueba que puedo reproducir.

---

## 3. Veredicto global

| Dimensión | Veredicto |
|---|---|
| Nombre y filosofía | ✅ Buenos. `portus` se queda. |
| Arquitectura de capas | ✅ Correcta, con el ajuste before/after. |
| Modelo de handlers | ⚠️ Correcto en espíritu; la v2 propuesta es imposible. |
| Árbol de módulos | ❌ Inservible como paquete; se reescribe. |
| Nombres públicos | ❌ Violan PENGUNIC; se renombran. |
| Librerías C | ⚠️ llhttp sí; uSockets/TLS/otras no en v1. |
| Licencias | ✅ Todas compatibles con MIT. |
| Viabilidad en 0.28.0 | ⚠️ Sí, con 5 bugs (1 bloqueante de instalación). |
| Plan por fases | ⚠️ Orden correcto; faltan F0 (compilador) y criterios de salida. |
| Testing | ✅ Enfoque válido; se concreta en `portus-testing.md`. |

**Conclusión:** adelante, pero no como "stdlib". Es un **paquete** con capa
nativa propia y una API que se parece a Hono/Express en lo que importa (verbos,
rutas, middleware, contexto) y a PenguScript en cómo se escribe (métodos,
`result`, ownership explícito).

---

## 4. Auditoría punto por punto

### 4.1 Nombre y ubicación

**Propuesta:** `std.portus`, archivos en `std/portus*.pengu`.

**Veredicto:** ⚠️ El nombre es bueno; la ubicación no.

- `std.*` está reservado a los módulos que vienen con el compilador
  (`PENGU_STD_DIR`). Un paquete de terceros no vive ahí y no debe pretenderlo:
  sería un conflicto el día que el compilador traiga su propio `std.portus`, y
  rompe la expectativa de que `import std.x` no requiere `psc add`.
- Como paquete, el id de módulo se deriva de la ruta bajo el `src_dir` del
  paquete (`src/portus.pengu` → `portus`; `src/portus/router.pengu` →
  `portus.router`). **Verificado**: un `import demo.native` solo resuelve si el
  archivo está en `src/demo/native.d.pengu`, no en `src/native.d.pengu`.
- Consecuencia práctica: el árbol de módulos del documento se reescribe bajo
  `src/portus/`, y el binding C pasa a `src/portus/native.d.pengu`.

### 4.2 Filosofía "compuesto, no acoplado"

**Veredicto:** ✅ Se mantiene tal cual. El núcleo no depende de ningún
middleware; cada middleware es un módulo que se importa aparte. Esto además es
obligatorio por DCE: importar `portus.cors` no debe arrastrar `portus.session`.

### 4.3 `result of void to HttpError`

**Veredicto:** ✅ Correcto, con una ergonomía que hay que resolver.

- `result of void to E` es legal y se comporta bien en handlers (probado).
- Pero construir el éxito es feo: `return calling ok_of with 0`. Un handler que
  termina bien y ya envió la respuesta no debería escribir eso.
- **Solución:** un weave público `ok_void` en el núcleo (`portus.ok_void`), y que
  los helpers `context.send_*` devuelvan `result of void to HttpError` para poder
  hacer `return calling context.send_text with "hola"`. Eso hace que el caso
  normal sea una sola línea.
- Alternativa descartada: `maybe HttpError` (invierte el significado y pierde el
  `try`/`or:` idiomático).

### 4.4 Zero-copy

**Veredicto:** ⚠️ Prematuro. No es un requisito de la v1.

- llhttp entrega cuerpo y headers como buffers que viven **solo** durante el
  callback. Exponerlos como `slice of byte`/`string` prestados es posible, pero
  un `banish` accidental o guardar la referencia en `context.state` produce
  use-after-free silencioso. PENGUNIC §19 es explícito: las vistas prestadas
  necesitan contrato de vida y no se pueden detectar.
- Decisión v1: **copiar** a `string` propias al construir `Request`. Coste:
  ~1 copia de path+headers+cuerpo por petición (cientos de bytes). Se mide en
  F6; si el perfil lo justifica, se añaden vistas con contrato documentado y
  flag de configuración.

### 4.5 Middleware sin closures

**Veredicto:** ⚠️ El diagnóstico es correcto; la solución v2 es imposible.

- Correcto: los lambdas de PenguScript no capturan (`_pengu_lambda_N`, solo
  parámetros y decls de módulo). Por eso no se puede hacer `(req,res,next) =>
  ...` como en Express.
- Correcto: la cadena de handlers que comprueba "¿ya se envió?" es el modelo.
- **Incorrecto:** `concept Middleware { weave wrap with next as Handler into
  Handler }`. En 0.28.0 los `concept` son estáticos, no hay vtable ni tipos
  existenciales: no puedes tener una `list of Middleware` heterogénea ni
  despachar por interfaz. `bind` fija una implementación por tipo en tiempo de
  compilación (§10 de LANGUAGE). El "decorador" no existe.
- **Reemplazo:** dos fases.
  - **Antes** (`before`): capas en orden; reciben `ref to Context`; si envían,
    la cadena se detiene; si devuelven error, va al manejador de errores; si no,
    continúa. Aquí viven CORS, auth, sesión (carga), rate limit, request id.
  - **Después** (`finishers`): corren **solo si la respuesta fue enviada** y
    antes de escribir al socket; reciben `ref to Context` y pueden transformar
    `response` (comprimir, ETag/304, cabeceras de seguridad, log con duración).
  - Esto cubre todo lo que el "decorador" pretendía, sin dispatch dinámico, y es
    más simple de razonar: el orden es una lista.
- Los middlewares con configuración (CORS, static, sesión) **no son closures**:
  son runas de configuración + una función `invoke` con `data as ref to void`.
  El patrón está validado (spike s3: `Handler { invoke, data }`, `transmute` de
  vuelta, llamada indirecta). La configuración se aloja en una **arena** que
  posee la `Application` y se libera con `banish app`.

### 4.6 Tipos owned + `banish`

**Veredicto:** ✅ Correcto, pero hay que definir la copia de `Application`.

- `Application`, `Server`, `Router`, `Context` como runas con `derive Nexus` es
  lo idiomático.
- Problema: un `rune` se copia por valor; copiar una `Application` con rutas
  registradas duplica punteros de handlers y buffers de strings. Regla:
  **`Application` se crea una vez, se registra, se presta (`ref to`) y se
  banea una vez; no se copia después de registrar**. Se documenta y se añade un
  chequeo en debug (`_sealed` flag) que aborta con mensaje claro si se registra
  sobre una copia.
- `Context` se crea en el stack del dispatcher y se pasa por `ref to`: nunca se
  devuelve ni se guarda.

### 4.7 Árbol de módulos

**Veredicto:** ❌ Se reescribe.

- `std/portus.d.pengu` (bindings uSockets+llhttp+TLS) mezcla tres cosas en un
  archivo; el binding debe ser mínimo y solo declarar.
- `http/{status,mime,headers,conditional,range,sse,stream}.pengu` es una
  categoría, no un módulo; varios caben en `response`, `request` y `static_file`.
- `middleware/compress` no puede ser un `before` (necesita el cuerpo ya
  construido): va como finisher.
- `session` y `auth` no son middleware "puros": necesitan estado (store) y
  configuración; se modelan como runas de configuración + capas.
- Nuevo árbol propuesto en [`portus-design.md`](portus-design.md) §10 y firmas
  en [`portus-api.md`](portus-api.md).

### 4.8 API propuesta

**Veredicto:** ⚠️ Buena forma, nombres y estilo a corregir.

- `Ctx` → `Context`, `req` → `request`, `res` → `response` (PENGUNIC §5).
- Funciones libres (`portus.get(app, ...)`) → **métodos** (`calling app.get with
  ...`). Es lo idiomático en Pengu (`enchanting`), evita repetir `app` y permite
  que el facade reexporte tipos sin reexportar 40 funciones.
- `mount(app, prefix, sub as ref to App)`: la vida del sub queda colgando de un
  `ref to`. Se cambia a **`mount` consume el `Router`** (lo mueve dentro de la
  app), o a `Router` como constructor y `app.use_router` con movimiento. Sin
  referencias colgantes.
- `listen(...) into result of Server to HttpError`: bien; el error de listen es
  de socket (`ListenError`), no HTTP. Se separa.
- `ctx.body_json` debe devolver `result of JsonValue to BodyError` (el error de
  parseo importa).
- Falta todo el modelo de **respuesta en dos fases** (§4.5) y el de **hooks**.

### 4.9 Router

**Veredicto:** ✅ Los patrones son los correctos. Faltan detalles.

- Especificidad literal > parámetro > wildcard: correcto, pero hay que **ordenar
  las rutas al registrarlas** (una vez) y no en cada request.
- `:param` debe decodificar percent-encoding; `*` debe capturar el resto
  (incluido `/`), `*name` con nombre; param vacío no matchea.
- Doble barra, `..`, `.` y trailing slash: normalizar antes de matchear
  (configurable). Decisión D6.
- 405/OPTIONS/HEAD automáticos: sí, pero necesitan saber qué métodos existen
  para el path → el router guarda un índice por path normalizado.
- Host routing y regex (POSIX `re.h` vía `std.regulus`) quedan P2.

### 4.10 Librerías C

Ver §5.

### 4.11 Fases

**Veredicto:** ⚠️ El orden es razonable, pero falta la fase 0.

- Tu F0 ("bindear uSockets + echo") desaparece con el cambio de enfoque; en su
  lugar va **F0: arreglar el compilador + esqueleto del paquete + shim C
  probado en C puro**.
- Las fases 1–6 se conservan con criterios de salida medibles
  ([`portus-plan.md`](portus-plan.md)).
- Estimación: 9–11 semanas de núcleo (sin TLS nativo), no 11–16.

### 4.12 Testing

**Veredicto:** ✅ Bien encaminado. Se concreta:

- `test` blocks para unidad (router, headers, cookies, JWT, multipart).
- `TestClient` en proceso para integración (sin sockets).
- Servidor real en puerto efímero (`listen(0)`) para end-to-end con `std.net`.
- Autobahn para WS (job opcional de CI, requiere Python).
- Fuzzing del parser (harness C + `libFuzzer`) y ASan sobre el bundle generado.
- Benchmarks contra Express y Hono (Bun/Node) con `wrk`.

### 4.13 `psc add ... --trust`

**Veredicto:** ❌ Hoy no funciona tal cual (bug P0). Después del fix, sí, y con
una ventaja: si el paquete **no** trae `build.pengu`, `--trust` no ejecuta nada
y la instalación es puramente copia de `src/` + `include/`. Verificado que un
paquete con C header-only funciona sin compilar nada en el consumidor.

---

## 5. Librerías C y licencias

### 5.1 Qué se queda, qué se va

| Librería | Propuesta | Veredicto | Razón |
|---|---|---|---|
| **llhttp** | sí | ✅ **Sí** | MIT. Streaming, probado en Node, API de callbacks simple. Es el parser correcto. |
| **uSockets** | sí | ❌ **No** | Apache-2.0 (compatible), pero es un event loop + TLS + crypto; bindearlo es más trabajo que escribir el loop, y arrastra OpenSSL. |
| **OpenSSL** | sí | ⏸ **F6 opcional** | Apache-2.0. Solo si hacemos TLS nativo; en v1 TLS lo termina un proxy. |
| **mbedTLS** | alternativa | ⏸ **F6 opcional** | Dual Apache-2.0/GPL-2.0; elegir Apache-2.0. Más pequeño que OpenSSL, sin dependencia de sistema en macOS. |
| **zlib** | sí | ❌ **No** | `std.compressus` ya envuelve miniz 3.1.2 (public domain). |
| **libsodium** | opcional | ❌ **No obligatorio** | `std.secrets` ya lo usa si está; no podemos exigirlo. Argon2id queda P2 vía sodium opcional. |
| **multipart-parser** | opcional | ❌ **No** | Parsear multipart en Pengu son ~150 líneas y evita otra dep. |
| **libargon2** | opcional | ❌ **No** | Si hace falta, vía `std.secrets`; no en v1. |
| **libmagic** | opcional | ❌ **No** | MIME por extensión + tabla propia. |
| **pcre2** | — | ❌ **No** | `std.regulus` ya usa POSIX regex (`link "re"`). |

**Conclusión:** la única librería C nueva es **llhttp**, vendorizada y
amalgamada en `include/` (un solo TU). Todo lo demás que falta son funciones
pequeñas que van en nuestro shim (`portus_native.h`): SHA-1 (handshake WS),
bytes aleatorios, comparación en tiempo constante, señales, `stat`/mtime para
`Last-Modified`. Eso son ~200 líneas, no una dependencia.

### 5.2 Lo que la idea no cubría y hay que añadir al shim

| Necesidad | Por qué | Dónde |
|---|---|---|
| SHA-1 | Handshake WS (RFC 6455). `std.digest` solo tiene SHA-256. | shim C (~60 líneas) o contribuir a `std.digest` |
| `random_bytes` | request IDs, sesiones, CSRF. `std.lot` es PRNG no cripto. | shim C (`getrandom`/`arc4random`/`/dev/urandom`) |
| Comparación en tiempo constante | tokens, CSRF, firmas | shim C |
| Señales SIGINT/SIGTERM | graceful shutdown | shim C |
| `stat` (mtime, tamaño) | `Last-Modified`, ETag barato, `send_file` | shim C |
| Base64url | JWT (HS256) | Pengu sobre `std.cipher` (20 líneas) |
| HTTP date | cabecera `Date`, `Last-Modified`, `Expires` | `std.chronicle` con `%a, %d %b %Y %H:%M:%S GMT` |
| Percent-decoding de params | `:param` correcto | `std.web.url.decode_component` |

### 5.3 Licencias

Todas compatibles con MIT (la licencia del repo):

- **llhttp**: MIT. Obligación: conservar el aviso de copyright de Fedor Indutny
  en el código vendorizado y en `THIRD_PARTY.md`/`NOTICE`.
- **miniz** (ya dentro de PenguScript): public domain.
- **SDS** (runtime de PenguScript): BSD-2. **Convenient Containers**: MIT.
  **vaqt**: BSD-3. No las distribuimos nosotros, pero el binario final las
  incluye; el `NOTICE` del compilador ya las cubre.
- Si en F6 entra **OpenSSL** (Apache-2.0) o **mbedTLS** (Apache-2.0): compatible,
  con atribución y sin cláusulas adicionales.
- **Descartadas por licencia**: mongoose (GPLv2/comercial), cualquier GPL.

Riesgo: ninguna licencia impide publicar portus como MIT. Ver
[`portus-licenses.md`](portus-licenses.md) para el texto y el checklist.

---

## 6. Bugs del compilador 0.28.0 (evidencia)

Estos son los que bloquean o ensucian el diseño. Todos con repro mínimo.

### 6.1 P0 — `psc add` sin `--name` corrompe el nombre y el path

**Comando exacto que pediste:**
```
psc add https://github.com/Tuagatito/portus --trust
```
Sin `--name`, el nombre se deriva del source. Ese camino tiene un
**use-after-free**:

`src/psc/cmd/deps.pengu`, weave `derive_name`:
```pengu
var s as string is source      # comparte buffer con el parámetro
...
var n as string is s at (i + 1) to e
banish s                        # libera el buffer de `source` (¡prestado!)
return n
```
`banish s` libera el buffer que pertenece a `source`; el llamador sigue usando
`source` para construir el spec y el path. Resultado observado:
`demo = { path = "demo      " }` (basura con padding) y
`cannot install dependency 'demo'`.

**Repro:** `psc add /tmp/demo` en un proyecto limpio → falla; `psc add /tmp/demo
--name demo` funciona.

**Fix:** no banear `s` (es una vista prestada), o copiar antes. Una línea.
**Impacto:** sin esto no hay `psc add <url>` sin `--name`, que es tu requisito.

### 6.2 P1 — Llamar a través de un `alias` de tipo función devuelve el tipo equivocado

```pengu
alias Fn as weave with x as int into int

weave apply with f as Fn into int:
    return calling f with 5      # error[E0020]: return value of type 'Fn' does not match 'int'
```
Causa: `src/psc/sema/infer/core.pengu`, `infer_call` (~713-721) resuelve el
tipo de la expresión llamada y solo desenvuelve `TkRef`, no `underlying_type`;
un alias se queda como alias y se devuelve `fsv.type`.

**Fix:** resolver alias (y refs) hasta `TkFn` antes de calcular el tipo de
retorno y los parámetros.
**Impacto:** sin esto no se puede escribir `alias Handler as weave ...` y usarlo
como parámetro; hay que repetir la firma inline en cada sitio (funciona, pero es
ruido). Con el fix, la API pública queda limpia.

### 6.3 P1 — Llamar a un campo que es un puntero a función

```pengu
rune Route:
    handler as weave with context as ref to Context into result of void to HttpError

weave run with r as ref to Route:
    return calling r->handler with context   # error[E0004]: undefined function 'handler'
```
El camino de llamada trata `r->handler` como nombre de método. Workaround:
`let f as <firma inline> is r->handler` y llamar `f`. Con alias, cae en 6.2.

**Fix:** en `infer_call`, si la callee es un field access cuyo tipo es `TkFn`,
emitir llamada indirecta.
**Impacto:** el dispatcher necesita esto o el workaround (una línea por capa).

### 6.4 P1 — Genérico que reenvía su parámetro de tipo a otro genérico

```pengu
weave gen_alloc shard T with a as ref to Arena into ref to T:
    return calling arena_alloc_one of T with a
```
C generado:
```
error: returning 'void **' from a function with incompatible return type 'Thing *'
```
La instanciación no sustituye `T` en el tipo de retorno de la llamada al genérico
de otra módulo. Afecta a `memoria.alloc of T`, `arena_alloc_one of T`, etc.

**Impacto:** no se pueden escribir helpers genéricos de configuración
(`use_configured shard C`). Workaround adoptado: cada middleware aloja su
configuración con un tipo concreto (`memoria.alloc of CorsConfig`), nunca con
`of T` dentro de un genérico.
**Fix:** en codegen de llamadas genéricas, sustituir los argumentos de tipo al
instanciar el callee (probablemente en `src/psc/codegen/expr.pengu`, búsqueda de
`type_args`/`subst`).

### 6.5 P2 — Variable con nombre de keyword de C: declaración y llamada no coinciden

```pengu
let double as weave with x as int into int is lambda x as int into x * 2
let n as int is calling double with 21
```
C generado:
```c
int32_t (*double_)(int32_t) = ...;   /* declaración con sufijo */
int32_t n = double(21);              /* llamada sin sufijo */
```
`gcc: expected expression before 'double'`.
Causa: `safe_c_name` (`src/psc/codegen/context.pengu:294`) añade `_` a keywords
de C, pero la emisión de la llamada no aplica el mismo mangle.

**Fix:** usar el nombre mangleado también en el call site (o manglear en un solo
punto).
**Impacto:** bajo (evitar nombres como `double`, `int`, `static`), pero es una
trampa silenciosa.

### 6.6 Notas de ergonomía (no bugs)

- `with campo is valor` **no** admite el tipo delante (`Widget with ...` no
  compila); el tipo se infiere del contexto. Para pasar una config a un genérico
  hace falta una variable local tipada. Afecta solo a helpers internos.
- `psc check --src-dir` exige `=` (`--src-dir=src`); con espacio falla.
- `ok_of with 0` para `result of void` funciona, pero es feo (ver §4.3).
- `std.digest` no tiene SHA-1; `std.chronicle` sí formatea HTTP dates.

---

## 7. Riesgos

| Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|
| El shim C (loop + parser + buffers) tiene bugs de memoria | Alta | Alto | Fase 0: probarlo **en C puro** con curl/ab + ASan + fuzz antes de bindearlo |
| El modelo single-thread + handlers síncronos bloquea el servidor con I/O lenta | Alta | Alto | Documentar; procesos múltiples (P2); ver D9 |
| Workarounds del compilador ensucian la API pública | Media | Medio | Arreglar 6.2/6.3 upstream; mientras, encapsular en `portus.handler` |
| El ownership de `send_*` (toma posesión) sorprende y causa dobles frees | Media | Alto | Documentar en cada `##`; tests de ownership; `TestClient` con ASan |
| llhttp amalgamado no compila con `--strict-c99` | Baja | Medio | Probar en CI desde F1; si falla, ajustar flags del paquete |
| `psc add` siga roto upstream | Media | Alto | Parche local + PR; mientras, instructivo con `--name` |
| Rendimiento inferior a Express | Baja | Bajo | Benchmark temprano (F1) con un handler trivial |

---

## 8. Recomendaciones

1. **Aprobar el cambio de empaquetado** (`src/portus/...`, `include/...`) y los
   renombres (`Context`, `request`, `response`).
2. **Aprobar el modelo before/after** y descartar el `concept Middleware`.
3. **Aprobar llhttp y descartar uSockets/TLS nativo en v1.**
4. **Arreglar los 5 bugs del compilador** en un fork local, con PRs upstream;
   sin el P0 no hay instalación limpia.
5. **Empezar por F0 con el shim en C puro**, no por Pengu: el riesgo está en el
   loop y el parser.
6. **Fijar el modelo de concurrencia** (D9) antes de escribir la API: condiciona
   todo lo demás.

El plan completo, con criterios de salida y entregables, está en
[`portus-plan.md`](portus-plan.md).
