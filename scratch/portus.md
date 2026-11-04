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

## 7. F0 en curso

Orden de trabajo (cero deuda técnica: cada cosa se verifica antes de pasar a la
siguiente):

1. Arreglar los bugs de PenguScript con test de regresión, versión de parche
   (`0.28.1`, `0.28.2`, …), `CHANGELOG.md` y commit.
2. Guía bilingüe `PenguScript/docs/LIBRARIES.md` / `LIBRARIES.es.md`.
3. Esqueleto del paquete (`pengu.toml`, `src/portus.pengu`, `src/portus/*`,
   `include/*.h`) y llhttp vendorizado.
4. Transporte C con `poll(2)` + llhttp + `portus_native.h`.
5. `psc add <repo> --trust` en un proyecto limpio y hello world con `curl`.

El progreso real se anota en [`portus-plan.md`](portus-plan.md).

---

## 8. Estado del repositorio

```
portus/
├── LICENSE, README.md, .gitignore, .gitattributes
├── pengu.toml, pengu/          # andamiaje inicial; F0 lo reemplaza
└── scratch/                    # todo el trabajo de auditoría y diseño
    ├── portus.md               # ESTE documento
    ├── portus-*.md             # 10 documentos
    ├── spikes/                 # 6 spikes de compilador (evidencia)
    └── LANGUAGE/PENGUNIC/STDLIB/portus-idea/README
```

Los spikes (`scratch/spikes/*.pengu`) e `include/spike_native.h` son pruebas de
laboratorio; se borran o se mueven a `tools/spikes/` al empezar F0.
