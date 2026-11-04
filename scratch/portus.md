# Portus — documento central

> **Qué es esto.** El punto de entrada a todo lo que hemos planeado para
> `portus`: una librería HTTP para PenguScript, al estilo Express/Hono,
> instalable con `psc add`.
>
> **Estado:** decisiones **aprobadas el 2026-11-04**. Desarrollo en curso por
> F0 (compilador, paquete, transporte C). Lo que se pospone vive en
> [`portus-deferred.md`](portus-deferred.md).
>
> Lectura: 5 minutos aquí; el resto está enlazado.

---

## 1. Veredicto de la auditoría

Tu idea es **correcta en lo grande** y tenía tres aciertos importantes:

1. **Componer, no acoplar**: núcleo pequeño + middleware opcional.
2. **Errores como valores** (`result of void to HttpError`), no excepciones ni
   `panic`.
3. **Reutilizar la stdlib** (`std.json`, `std.compressus`, `std.regulus`,
   `std.archivum`) en vez de reescribir.

Pero **no era ejecutable tal cual**. Estas son las correcciones estructurales
que la auditoría encontró (verificadas contra el compilador o contra la guía
del lenguaje):

| # | Lo que decía el documento | Lo que hay que hacer | Por qué |
|---|---|---|---|
| 1 | `std/portus.pengu` dentro de `std/` | paquete con `src/portus.pengu` + `src/portus/*` + `include/*.h` | `psc add` instala en `lib/` y resuelve `import <basename>`; un paquete no puede escribir en `std/` |
| 2 | `concept Middleware` con `wrap` | dos fases: `use` (antes) y `finish` (después) | no hay dispatch dinámico ni tipos existenciales; un `concept` no cabe en una lista heterogénea |
| 3 | `Ctx`, `req`, `res` | `Context`, `request`, `response` | PENGUNIC §5 prohíbe abreviaturas |
| 4 | zero-copy con `slice of byte` | copiar en v1; optimizar después | el buffer del shim no vive lo que el handler; la API no cambia |
| 5 | bindear uSockets | loop propio con `poll(2)` en C | uSockets es un event loop completo con callbacks y TLS; envolverlo cuesta más que escribirlo |
| 6 | `psc add <url> --trust` | igual, pero hoy necesita `--name` | bug P0 del compilador (use-after-free en `derive_name`) |

Y cinco bugs del compilador, uno bloqueante, documentados con reproducción y
parche propuesto en [`portus-compiler-fixes.md`](portus-compiler-fixes.md).

### Librerías C: solo una

| Librería | Veredicto |
|---|---|
| **llhttp** (MIT) | ✅ **sí**. Parser HTTP streaming, el que usa Node. Vendorizado. |
| uSockets (Apache-2.0) | ❌ no. Escribimos el loop nosotros. |
| OpenSSL / mbedTLS | ⏸ fuera de v1 (TLS lo termina un proxy). |
| zlib, libsodium, libargon2, multipart-parser, libmagic, pcre2 | ❌ no hacen falta (la stdlib o 200 líneas propias cubren). |
| mongoose (GPL) | ❌ rechazado por licencia. |

Lo que sí hay que escribir en C (porque el lenguaje no lo tiene): **SHA-1**
(handshake WS), **bytes aleatorios**, **comparación en tiempo constante**,
**stat/mtime** (ETag), **señales** y el **loop de red**. Nada de eso es una
librería externa.

Todas las licencias son compatibles con MIT; obligaciones en
[`portus-licenses.md`](portus-licenses.md).

---

## 2. Lo que ya está probado (no es teoría)

Con el compilador 0.28.0 real y proyectos de prueba:

- ✅ Un paquete instalado con `psc add … --trust` en un proyecto limpio,
  importado con `import demo` y llamando funciones (`/tmp/demo`).
- ✅ C header-only dentro de un paquete, sin `-l` ni rutas frágiles.
- ✅ Handlers con `result of void to HttpError`.
- ✅ **Handlers "thick"**: puntero a función + `ref to void` de datos +
  `transmute` de vuelta → el middleware con configuración funciona.
- ✅ Config en `Arena` con `set essence of cfg is with …` + `banish`.
- ✅ C llamando a Pengu (callbacks) para el loop del servidor.
- ✅ `alias` entre módulos para la fachada (`alias Widget as demo.core.Widget`),
  incluidos métodos del tipo aliasado.
- ✅ Lo que **no** funciona hoy: alias de tipo función (bug 2), genérico que
  reenvía `of T` (bug 3), llamar un campo-función (bug 4), nombres keyword C
  (bug 5), `psc add` sin `--name` (bug 1). Todos con workaround.

---

## 3. Cómo se verá

```pengu
import portus

weave hola with context as ref to portus.Context into result of void to portus.HttpError:
    return calling context.send_text with "¡Hola, mundo!"

weave main into int:
    var app as portus.Application is calling portus.new_application
    calling app.get with "/", hola
    let served as result of void to portus.ListenError is calling portus.serve with sigil of app, 3000
    if served.is_ok is false:
        return 1
    banish app
    return 0
```

```pengu
calling app.use_at with "/admin", requiere_auth
calling portus.static_file.use_static with sigil of app, opciones
calling portus.websocket.get with sigil of app, "/chat", sigil of chat
```

Detalle completo en [`portus-design.md`](portus-design.md) y
[`portus-api.md`](portus-api.md).

---

## 4. Decisiones (aprobadas)

El 2026-11-04 se aprobaron las recomendaciones de D1–D12, D15 y D16; D13 se
amplió al paquete completo de seguridad y D14 añadió la guía bilingüe en
PenguScript. El detalle y las condiciones están en
[`portus-decisions.md`](portus-decisions.md).

| # | Decisión | Recomendación |
|---|---|---|
| D1 | Nombre y forma de instalación | paquete `portus` (`import portus`), no `std.portus` |
| D2 | Idioma de la API | inglés; docs en español |
| D3 | Modelo de middleware | dos fases: `use` + `finish` (sin `concept`) |
| D4 | Retorno de handler | `result of void to HttpError` |
| D5 | `Context` mutable | `ref to Context` normal |
| D6 | Orden de rutas | especificidad primero |
| D7 | Cuerpo de respuesta | `send_*` toma posesión (cero copias) |
| D8 | TLS en v1 | no; lo termina un proxy |
| D9 | Concurrencia | 1 hilo + event loop; procesos en F7 |
| D10 | Bugs del compilador | arreglar upstream + workarounds |
| D11 | Zero-copy | copiar en v1 |
| D12 | WebSocket en v1 | sí |
| D13 | Seguridad en v1 | sesiones + auth + CSRF + rate limit + multipart |
| D14 | Layout del repo | `src/portus.pengu` + `src/portus/*` + `include/*.h` |
| D15 | Docs | `scratch/` ahora; `docs/` en release |
| D16 | Nombre en `pengu.toml` | `portus` |

Menores (Q1–Q5: puerto 3000, `port=0` para tests, `TestClient` público…) las
decido yo salvo que digas lo contrario.

---

## 5. Plan en una tabla

| Fase | Qué | Semanas | Gate |
|---|---|---|---|
| F0 | Compilador, esqueleto, transporte C | 2–3 | `psc add` + hello world real |
| F1 | Núcleo: app, router, context, dispatch | 3 | REST con params y middleware |
| F2 | Cuerpos, cookies, estáticos, CORS, compresión | 3 | 10 ejemplos de Express |
| F3 | Streaming, SSE, ficheros | 1–2 | descarga sin cargar en RAM |
| F4 | WebSocket | 2–3 | chat + Autobahn |
| F5 | Sesiones, auth, CSRF, rate limit, multipart | 3 | login + JWT + subida |
| F6 | Ejemplos, docs, release 0.1.0 | 2 | 25 ejemplos, `psc doc` limpio |

Total ~16–19 semanas. Detalle, riesgos y criterios en
[`portus-plan.md`](portus-plan.md).

---

## 6. Documentos

| Documento | Para qué |
|---|---|
| [`portus-audit.md`](portus-audit.md) | Auditoría completa: qué estaba bien, qué no, librerías C, licencias, bugs, riesgos |
| [`portus-decisions.md`](portus-decisions.md) | **Decisiones a tomar**, con opciones y consecuencias |
| [`portus-design.md`](portus-design.md) | Diseño: capas, modelo de ejecución, tipos, pipeline, router, frontera C |
| [`portus-api.md`](portus-api.md) | Firmas de toda la API pública |
| [`portus-plan.md`](portus-plan.md) | Fases, tareas, gates, riesgos |
| [`portus-testing.md`](portus-testing.md) | Pirámide de tests, CI, fuzzing, benchmarks |
| [`portus-examples.md`](portus-examples.md) | Paridad con los ejemplos de Express + los propios |
| [`portus-licenses.md`](portus-licenses.md) | Licencias, obligaciones, checklist de dependencias |
| [`portus-rules.md`](portus-rules.md) | Reglas de código, nombres, docs, ownership, PR |
| [`portus-compiler-fixes.md`](portus-compiler-fixes.md) | Los 5 bugs del compilador, con repro y parche |
| [`portus-deferred.md`](portus-deferred.md) | Lo que queda para después de v1 y de qué depende (hilos, TLS, HTTP/2…) |

Referencias del lenguaje: [`LANGUAGE.es.md`](LANGUAGE.es.md),
[`PENGUNIC.es.md`](PENGUNIC.es.md), [`STDLIB.es.md`](STDLIB.es.md),
[`portus-idea.md`](portus-idea.md) (la propuesta original, sin modificar).

---

## 7. F0 — cerrado

Orden de trabajo (cero deuda técnica: cada cosa se verifica antes de pasar a la
siguiente). Todo hecho y verificado:

1. Bugs del compilador arreglados con test de regresión, versión de parche y
   commit: `0.28.1` (`psc add` sin `--name`), `0.28.2` (alias de tipo función),
   `0.28.3` (mangle de keyword C), `0.28.4` (campo-función), `0.28.5` (API POSIX
   para el C generado), `0.28.6` (métodos de un tipo reexportado con `alias`).
   Quedan abiertos el genérico que reenvía `of T` (bug 3), la llamada
   cualificada que resuelve al símbolo local (bug 8) y los bloques en posición de
   valor (bug 9, paridad del compilador), todos con workaround y documentados en
   [`portus-compiler-fixes.md`](portus-compiler-fixes.md).
2. Guía bilingüe `PenguScript/docs/LIBRARIES.md` / `LIBRARIES.es.md`.
3. Esqueleto del paquete (`pengu.toml`, `src/portus.pengu`, `src/portus/*`,
   `include/*.h`) y llhttp 9.4.3 vendorizado con `tools/vendor_llhttp.sh`.
4. Transporte C con `poll(2)` + llhttp (`include/portus_native.h`) y binding
   Pengu (`src/portus/native.d.pengu`, `src/portus/transport.pengu`).
5. Gate real: `tests/e2e_hello.sh` — proyecto limpio, `psc add <repo> --trust`
   sin `--name`, build, HTTP 200 con cuerpo, segunda petición keep-alive.
   Resultado: `OK e2e hello`.

**Siguiente: F1** (router, contexto, dispatcher, middlewares) según
[`portus-plan.md`](portus-plan.md).

### F1a — cerrado (router + despacho)

Hecho y verificado por el gate e2e:

- `route.pengu`: `Pattern`/`Segment`/`Params`, `compile` (literal, `:param`,
  `*wildcard`; score por especificidad), `match_pattern`, `get_param`.
- `error.pengu`: `HttpError` como `rune { kind as HttpErrorKind, message }`
  (workaround del bug 10 de omens algebraicos).
- `handler.pengu`: `Handler` (alias de tipo función), `ok_void`, `err_void`.
- `context.pengu`: parámetros, método, ruta, query y respuestas de texto/HTML.
- `application.pengu`: `Application`, `add`, `dispatch`, 404/405, 500,
  `serve_with`.
- Fachada: `new_application`, `get/post/put/patch/delete/head/any`, `serve`.
- `tests/e2e_hello.sh` amplía el gate F0: `/`, `/users/:name`, 404 y keep-alive.

Bug 11 del compilador (lista vacía con `-Wunused-variable`) arreglado y
publicado en 0.28.7 con `tests/regress_empty_list.sh`.

Commit `4d23ed4 f1a: router, contexto y dispatcher con gate e2e de rutas`.

### F1b — cerrado (middlewares, hooks y grupos)

- `handler.pengu`: `Middleware` (thick: `invoke` + `data`), `MiddlewareFn`,
  `ErrorHandler`, `middleware`, `invoke_middleware`.
- `application.pengu`: `_befores`/`_finishers` con prefijo, hooks
  `on_error`/`on_not_found`/`on_start`/`on_shutdown` (campo no-op + flag
  `_has_*` por el bug 12), `use`/`use_at`/`after`, `_handle_failure`,
  `_join_methods`, 405 con `Allow`, `respond_error`.
- `group.pengu`: `Group` (aplicación + prefijo) con `get/post/put/patch/delete/any`
  y el ritual `full_pattern`; la fachada expone `new_group`.
- Fachada: `use`, `use_at`, `after`, `on_error`, `on_not_found`, `on_start`,
  `on_shutdown`, `middleware`, `bad_request`, `not_found`, `internal_error`,
  `new_group`, alias `Middleware`/`ErrorHandler`/`MiddlewareFn`/`AppHook`/`Group`.
- Tests: 14 unitarios (`psc test` → `All 14 test(s) passed.`; 10 de `route`,
  3 de `error`, 1 de `group`).
- Gate: `tests/e2e_hello.sh` prueba `/`, `/users/:name`, `/api/ping` (grupo),
  `/boom` (400), `POST /only-post` (405 + `Allow`), middleware `X-Portus`,
  `on_not_found` propio y keep-alive → `OK e2e hello (puerto 52503)`.

Cierre de F1:

- `HEAD` automático: si no hay ruta `HEAD`, se usa la `GET` (el transporte omite
  el cuerpo). `OPTIONS` sin ruta propia responde `204` + `Allow`. `Allow` de un
  405 incluye `HEAD` cuando hay `GET`.
- `examples/`: `hello-world`, `params`, `multi-router`, `route-middleware`,
  `error`, cada uno con `pengu.toml` (dependencia `path` al repo), `README.md` y
  `examples/run.sh` que los compila todos. Los 5 arrancan y responden lo que
  dicen sus README.
- Tests: 25 unitarios (`All 25 test(s) passed.`).
- Fachada: `status_of`, `new_group`, `Group`, `Middleware`, `ErrorHandler`,
  `MiddlewareFn`, `AppHook`.
- Bugs nuevos documentados: 12 (`maybe` de puntero a función), 13
  (`ctx->campo.metodo` → `T**`), 14 (módulo `router` sin weaves libres), 15
  (nombre suelto resuelve al módulo consumidor).

**Decisión de diseño tomada al implementar:** no hay `mount`; un "router
montado" es un `Group` (aplicación + prefijo) y `use_at(prefix)` para middleware
con prefijo. Un `Application` dentro de otra aplicación no aporta nada que el
grupo no haga y complica el ownership.

F1 — **cerrado** con el gate: build + 25 tests + e2e + 6 ejemplos.

F2 — **en curso**. Hecho:
- Cuerpos: `context.body_text`, `body_json` (directo a `std.json`), `body_form`.
- Query string: `portus.query` (`parse_query`, `query_get`, `free_query`) y
  `context.queries` / `context.query_param`.
- Cookies: `portus.cookie` (`parse_cookies`, `cookie_get`, `serialize_cookie`,
  `clear_cookie`, `CookieOptions`) y `context.cookie` / `set_cookie` /
  `set_cookie_with` / `delete_cookie`.
- Respuestas: `send_json`, `send_body_with_type`, `status_code`.
- Middlewares: `portus.middleware.add_request_id`, `add_security_headers`,
  `add_cors`, `add_logger` (con `application.middleware_with` para configuración).
- Nativo: `portus_native_random_hex`, `portus_native_request_id`,
  `portus_native_now_ms` (para `X-Request-Id` y duración del logger).
- Estáticos: `portus.static_file.mount` (y `portus.mount_static`) sirve un
  directorio con MIME por extensión (`portus.mime`), `ETag`, `Last-Modified`,
  `If-None-Match` (304), `Range: bytes=a-b` (206 + `Content-Range`) y rechazo de
  rutas con `..` o absolutas.
- Compresión: `portus.use_compress` (finisher gzip si el cliente lo acepta, el
  tipo es compresible y el cuerpo pasa de 860 bytes).
- Negociación: `portus.best` / `portus.accepts` sobre el `Accept` con `q=`.
- Streaming: `context.sse_open` / `sse_send` / `sse_close` y
  `stream_begin` / `stream_write` / `stream_end`, con `Transfer-Encoding: chunked`.
- Handlers gruesos en rutas: `application.add_thick` + `RouteEntry{handler,
  invoke, data, is_thick}` para que los estáticos (y lo que venga) lleven
  configuración sin closures.
- Tests: 34 (`psc test`), e2e ampliado (JSON, cookies, request-id, cabeceras de
  seguridad, preflight CORS, estáticos con ETag/Range, gzip, negociación y SSE)
  y ejemplo `examples/cookies-and-json`.
Pendiente de F2: nada bloqueante; sesiones firmadas se difieren a F4 (necesitan
`HMAC-SHA256` de `std.digest` y un `Set-Cookie` firmado que ya cabe en
`portus.cookie`).

F3 — **cerrado (WebSocket)**. Hecho y verificado:

- C (`include/portus_native.h`): SHA-1 y base64 propios (no hay SHA-1 en
  `std.digest`), `portus_native_ws_upgrade` (valida `Upgrade`/`Sec-WebSocket-Key`
  y responde `101` con `Sec-WebSocket-Accept`), `portus_native_ws_send` y
  `portus_native_ws_close`; `portus_ws_consume` parsea frames, exige máscara,
  contesta `ping` con `pong`, cierra con `1000`, rechaza fragmentación con
  `1003` y limita el mensaje a 1 MiB. `portus_conn_process` deriva a
  `portus_ws_consume` en cuanto la conexión es WebSocket (incluido el frame que
  llegue en el mismo `recv` que el handshake).
- Pengu: `src/portus/websocket.pengu` con `Conn` (`send_text`, `send_bytes`,
  `close`, `is_text`), `SocketHandler` (fn + `data`, sin closures), `handler_with`,
  `text_of` y `upgrade(context, handler)`. El handler se copia a la arena del
  heap con `memoria.alloc` y se libera con `banish` al recibir el cierre.
- La fachada expone `WebSocketConn`, `WebSocketHandler`, `WebSocketMessageFn`,
  `ws_handler`, `ws_text` y `ws_upgrade`.
- Callback con `opcode` 0 al abrir, para que el usuario registre la conexión;
  `Conn` es un bloque estable por conexión (no un wrapper temporal por mensaje).
- Ejemplo `examples/websocket-chat`: 10 clientes simultáneos, broadcast y cierre
  (probado con un cliente Python: 10/10 reciben el mensaje).
- Gate: `tests/e2e_ws.sh` — proyecto temporal con `psc add <repo> --trust`,
  handshake real, comprobación del `Sec-WebSocket-Accept`, eco de texto corto y
  de 300 bytes, `ping`/`pong` y cierre → `OK e2e websocket (puerto …)`.
- Tests: 36 (`psc test`), incluidos `text_of` y `handler_with`.

**Decisión de diseño:** un mensaje = un frame (sin fragmentación ni
extensiones en v1), y el estado por conexión va en el `data` del handler, no en
variables globales. El servidor sigue siendo de un hilo: el callback debe ser
rápido; para trabajo largo se difiere un pool de hilos (ver
[`portus-deferred.md`](portus-deferred.md)).

Bugs nuevos del compilador detectados aquí (documentados en
[`portus-compiler-fixes.md`](portus-compiler-fixes.md)): 17 (un `rune` local con
el mismo nombre que un tipo importado resuelve al importado en unas posiciones y
genera C inválido) y 18 (un `alias` de tipo en posición de retorno no acepta el
tipo subyacente).


---

## 8. Estado del repositorio

```
portus/
├── LICENSE, README.md, THIRD_PARTY.md, .gitignore, .gitattributes
├── pengu.toml                  # paquete estático (output = "static")
├── src/
│   ├── portus.pengu            # fachada pública (alias + serve)
│   └── portus/
│       ├── native.d.pengu      # declares del header C
│       ├── transport.pengu     # Request/Response/ServeOptions + puente
│       ├── route.pengu         # patrones y parámetros
│       ├── error.pengu         # HttpError y su status
│       ├── handler.pengu       # Handler, Middleware, ok_void/err_void
│       ├── context.pengu       # Context y sus métodos
│       ├── group.pengu         # Group (prefijo de rutas)
│       ├── text.pengu          # puente de strings desde C
│       ├── query.pengu         # query string
│       ├── cookie.pengu        # cookies y Set-Cookie
│       ├── middleware.pengu    # request-id, seguridad, CORS, logger
│       ├── mime.pengu          # extensión -> Content-Type
│       ├── static_file.pengu   # estáticos con ETag/Last-Modified/Range
│       ├── compress.pengu      # gzip de respuesta (finisher)
│       ├── negotiation.pengu   # Accept / q=
│       └── application.pengu   # Application, dispatch, serve
├── include/
│   ├── portus_native.h         # servidor poll(2) + llhttp, header-only
│   └── llhttp/                 # llhttp 9.4.3 vendorizado (LICENSE, SOURCE)
├── tools/vendor_llhttp.sh
├── tests/e2e_hello.sh          # gate de F0 (instalación + HTTP real)
└── scratch/                    # auditoría, diseño y planes
    ├── portus.md               # ESTE documento
    ├── portus-*.md             # 11 documentos
    └── spikes/                 # evidencia de compilador (ignorada por git)
```

Los spikes (`scratch/spikes/*.pengu`) son pruebas de laboratorio: no forman parte
del paquete y `scratch/spikes/` está en `.gitignore`.
