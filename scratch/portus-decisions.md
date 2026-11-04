# Portus — Decisiones

> **Estado: APROBADAS el 2026-11-04.** El usuario aceptó las recomendaciones
> (D1–D12, D15, D16) y precisó D13 y D14. El desarrollo empieza por F0.
>
> Decisiones del usuario que añaden trabajo:
> - **D10 refuerza el lema "cero deuda técnica"**: cualquier bug del lenguaje o
>   del compilador que aparezca se arregla en el momento, con test de regresión,
>   subida de versión de parche (`x.x.+1`), `CHANGELOG.md` y commit en
>   PenguScript. Nada de workarounds permanentes.
> - **D13 = paquete completo** de seguridad en v1.
> - **D14 = A + guía bilingüe** en `PenguScript/docs/` sobre cómo crear una
>   librería de PenguScript con lo que está probado que funciona.
> - Cada fase se cierra solo cuando funciona de verdad (tests + e2e), no cuando
>   "parece" que funciona.

---

## D1 — Nombre y forma de instalación

**Contexto.** El documento original propone `std.portus` (dentro del árbol de
PenguScript). Pero `psc add` instala paquetes en `lib/<nombre>/` y resuelve
módulos como `import <basename>`; un paquete **no** puede añadir módulos a
`std.`.

| Opción | Qué implica |
|---|---|
| **A. Paquete `portus`** (recomendada) | `psc add <url> --trust`, `import portus`. Módulos `portus.*`. Es la única forma que funciona hoy. |
| B. `std.portus` | Obligaría a parchear el compilador o a copiar ficheros a `std/`. No es instalable. |
| C. Otro nombre latino (`ianua`, `atrium`) | Igual que A, solo cambia el nombre. |

**Recomendación: A.** El nombre `portus` ya está en el repo y comunica lo mismo.
**Consecuencia de A:** el "namespace" es `portus`, no `std.portus`; los
documentos que digan `std.portus` se actualizan.

---

## D2 — Idioma de la API

**Contexto.** Los documentos están en español; PenguScript y su stdlib usan
nombres en inglés (`Application`, `send_text`).

| Opción | Qué implica |
|---|---|
| **A. API en inglés, docs en español** (recomendada) | Consistente con `std.*`; los ejemplos se leen como Express/Hono. |
| B. API en español (`Aplicacion`, `enviar_texto`) | Coherente con los docs, pero rompe la convención del ecosistema y de `std`. |
| C. Bilingüe (alias) | Duplica superficie y documentación; no. |

**Recomendación: A.**

---

## D3 — Modelo de middleware

**Contexto.** El documento propone un `concept Middleware` con `wrap`. En
0.28.0 no hay dispatch dinámico ni tipos existenciales: un `concept` no puede
guardarse en una lista heterogénea. Los spikes confirman que lo que sí funciona
es: lista de capas + handler que decide si continúa.

| Opción | Qué implica |
|---|---|
| **A. Dos fases: `use` (antes) y `finish` (después)** (recomendada) | `use` puede cortar enviando; `finish` ve la respuesta final (comprimir, log, ETag). Sin closures; config en arena. |
| B. Solo `use`, sin finishers | `compress`/`logger` tendrían que envolver la respuesta a mano; más difícil y con más copias. |
| C. Intento de `concept Middleware` | No compila hoy; se puede revisar cuando el lenguaje tenga dispatch dinámico. |

**Recomendación: A.** Es la traducción honesta de "middleware" al modelo del
lenguaje.

---

## D4 — Tipo de retorno de un handler

| Opción | Qué implica |
|---|---|
| **A. `result of void to HttpError`** (recomendada) | Errores explícitos, `try`, `on_error`, sin flags. |
| B. `void` + comprobar `context.is_sent` | Se pierde el motivo del error; los 500 se adivinan. |
| C. `maybe HttpError` | Pierde la distinción entre "no envié" y "fallé". |

**Recomendación: A.** `ok_void`/`err_void` evitan el ruido.

---

## D5 — Mutabilidad del `Context`

**Contexto.** El documento proponía `ref to Context` (inmutable) con campos
`var` internos. PenguScript no tiene mutabilidad interior sin pasar por un
puntero mutable.

| Opción | Qué implica |
|---|---|
| **A. `ref to Context` normal (mutable)** (recomendada) | Los handlers pueden hacer `set context.response.status is 404`; el dispatcher también. Simple, cero fricción. |
| B. `ref to frozen Context` + `ref to void` nativo | Cumple "inmutable" pero cada setter necesita C; complica el 100 % del código. |

**Recomendación: A.** La inmutabilidad se garantiza por convención (los campos
`_` son privados) y por revisión, no por el tipo.

---

## D6 — Orden de rutas

| Opción | Qué implica |
|---|---|
| **A. Especificidad primero, registro como desempate** (recomendada) | `/usuarios/me` gana a `/usuarios/:id` sin importar el orden. Determinista. |
| B. Orden de registro (Express) | Compatible con Express, pero la trampa clásica. |

**Recomendación: A.** Se documenta la diferencia con Express en el README.

---

## D7 — Ownership del cuerpo de la respuesta

| Opción | Qué implica |
|---|---|
| **A. `send_*` toma posesión** (recomendada) | Cero copias; hay que documentarlo bien y no banear después. |
| B. `send_*` copia | Más seguro para el usuario, más lento y más memoria. |

**Recomendación: A** con documentación `##` explícita en cada `send_*`.

---

## D8 — TLS en v1

| Opción | Qué implica |
|---|---|
| **A. Fuera de v1; el proxy termina TLS** (recomendada) | Ahorra 2–4 semanas y una dependencia pesada; `is_secure` se deduce de `X-Forwarded-Proto` con `trust_proxy`. |
| B. Incluir mbedTLS en v1 | +3 semanas, +dependencia, +superficie de bugs de seguridad. |

**Recomendación: A.** Se reevalúa en F7.

---

## D9 — Concurrencia

| Opción | Qué implica |
|---|---|
| **A. Un hilo, event loop, handlers síncronos** (recomendada) | Simple, predecible; un handler lento bloquea todo (documentado). |
| B. Pool de hilos en C | Complejidad alta (contexto por hilo, sesiones compartidas) y sin `async` en el lenguaje, los handlers seguirían bloqueando su hilo. |
| C. Varios procesos (F7) | Escala sin tocar el modelo; requiere `fork` en C. |

**Recomendación: A** en v1, C como camino de escala posterior.

---

## D10 — Qué hacer con los bugs del compilador

| Opción | Qué implica |
|---|---|
| **A. Arreglarlos upstream y usar workarounds mientras** (recomendada) | portus funciona con 0.28.0 + `--name`; los PRs mejoran el lenguaje. |
| B. Fork de PenguScript | Hay que mantenerlo; los usuarios tendrían que usar el fork. |
| C. Esperar a 0.28.1 | Bloquea el proyecto sin fecha. |

**Recomendación: A.** Detalle en
[`portus-compiler-fixes.md`](portus-compiler-fixes.md). **Aprobado con
condición:** cada fix sube la versión de parche (`0.28.0 → 0.28.1 → …`), se
anota en `CHANGELOG.md` y se commitea en PenguScript con su test de regresión.

---

## D11 — Zero-copy

| Opción | Qué implica |
|---|---|
| **A. Copiar en v1** (recomendada) | Cada petición copia target/cabeceras/cuerpo a `string`. Simple y seguro; se mide. |
| B. `slice of byte` sobre el buffer del shim | Cero copias, pero la vida del buffer es frágil y el cuerpo puede necesitar seguir vivo. Se puede añadir después sin romper la API. |

**Recomendación: A.** La API pública no cambia si luego se optimiza.

---

## D12 — WebSocket en v1

| Opción | Qué implica |
|---|---|
| **A. Sí, F4** (recomendada) | Chat y tiempo real son el 30 % de los casos de Express/Hono; sin esto no es "alternativa". |
| B. Fuera de v1 | Menos trabajo, menos paridad. |

**Recomendación: A.** Sin `permessage-deflate` (documentado).

---

## D13 — Alcance de seguridad en v1

Opciones: sesiones + auth Basic/Bearer/JWT + CSRF + rate limit + multipart
(**recomendada**, F5) vs solo lo mínimo (auth + cookies) y el resto después.

**Recomendación:** el paquete completo, porque sin él los ejemplos `auth`,
`session`, `web-service` no existen.

---

## D14 — Estructura del repositorio

| Opción | Qué implica |
|---|---|
| **A. `src/portus.pengu` + `src/portus/*` + `include/*.h`** (recomendada) | Es lo que `psc add` resuelve hoy (validado). |
| B. `std/portus/*` | No instalable como paquete. |

**Recomendación: A.** Ya está validado con un paquete de prueba. **Aprobado
con añadido:** además de este repo, se escribe una guía bilingüe (inglés y
español) en `PenguScript/docs/` — `LIBRARIES.md` / `LIBRARIES.es.md` — con el
proceso probado (layout, `pengu.toml`, módulos anidados, C header-only,
`psc add --trust`), para que cualquiera pueda crear y publicar una librería.

---

## D15 — Dónde viven los documentos

Ya están en `scratch/` (este repo). Opción alternativa: mover a `docs/` al
publicar. **Recomendación:** `scratch/` durante el desarrollo; `docs/` en la
release.

---

## D16 — Nombre del paquete en `pengu.toml`

`name = "portus"`. Sin cambios.

---

## 17. Preguntas abiertas (menores, puedo decidir yo)

| # | Pregunta | Propuesta |
|---|---|---|
| Q1 | ¿Puerto por defecto? | `3000` (como Express) |
| Q2 | ¿`Server` acepta `port = 0`? | Sí, para tests (puerto efímero) |
| Q3 | ¿`TestClient` público? | Sí, es parte de la promesa "testeable" |
| Q4 | ¿Benchmarks en CI? | No como gate; sí como job manual |
| Q5 | ¿`psc doc` commiteado? | Sí, `docs/api/` |

---

## 18. Reglas permanentes (aprobadas)

1. **Cero deuda técnica:** un bug que se encuentre se arregla, no se esquiva
   para siempre. Workaround solo mientras el fix está en revisión, y anotado.
2. Nada de dependencias C fuera de [`portus-licenses.md`](portus-licenses.md).
3. Cambios de API pública después de F1 se anotan aquí con fecha.
4. Los fixes de PenguScript van por rama propia, con test, versión de parche y
   commit; no se mezclan con features.
5. Nada entra a una fase si su gate no pasa de verdad (tests + e2e + ASan).
6. Lo que se pospone se anota en [`portus-deferred.md`](portus-deferred.md),
   nunca se olvida.

---

## 19. Cómo seguimos

1. F0 en curso: fixes del compilador, guía bilingüe, esqueleto del paquete,
   llhttp vendorizado, transporte C y `psc add` de punta a punta.
2. Cada gate de fase se revisa al cerrarla; el estado vive en
   [`portus-plan.md`](portus-plan.md) y en [`portus.md`](portus.md).
