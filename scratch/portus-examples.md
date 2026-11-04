# Portus — Ejemplos (paridad con Express)

> Referencia: <https://github.com/expressjs/express/tree/master/examples>.
> Estado: **planificación**. Cada ejemplo es un proyecto Pengu completo dentro
> de `examples/<nombre>/` con su `pengu.toml`, `src/main.pengu` y `README.md`.
>
> Durante el desarrollo el ejemplo depende de portus por ruta:
> `[dependencies.portus] path = "../.."`. En la release, por URL.

---

## 1. Estructura de un ejemplo

```
examples/hello-world/
├── pengu.toml
├── src/main.pengu
└── README.md      # qué hace, cómo correrlo, qué APIs muestra
```

```toml
[project]
name = "hello-world"
version = "0.1.0"
output = "exe"

[build]
src_dir = "src"

[dependencies.portus]
path = "../.."
```

Build y ejecución:

```sh
cd examples/hello-world
psc build
./build/hello-world
```

---

## 2. Tabla de paridad

Leyenda: ✅ factible · 🟡 adaptado (no idéntico) · ⛔ no aplica en v1.

| # | Ejemplo de Express | Estado | Fase | APIs de portus que ejercita |
|---|---|---|---|---|
| 1 | `hello-world` | ✅ | F1 | `get`, `send_text`, `listen`, `run` |
| 2 | `params` | ✅ | F1 | `:param`, `*`, `param`, `queries` |
| 3 | `route-separation` | ✅ | F1 | módulos separados, `Router` |
| 4 | `multi-router` | ✅ | F1 | `new_router`, `mount` |
| 5 | `route-middleware` | 🟡 | F1 | `use_at`, orden, cortar sin enviar |
| 6 | `route-map` | 🟡 | F1 | `Router`, `any`, tabla de handlers |
| 7 | `error` | ✅ | F1 | `err_of`, `on_error`, finishers |
| 8 | `error-pages` | ✅ | F2 | `on_not_found`, `send_html` |
| 9 | `cookies` | ✅ | F2 | `set_cookie`, `cookie`, `Cookie` |
| 10 | `static-files` | ✅ | F2 | `use_static`, `StaticOptions` |
| 11 | `downloads` | ✅ | F2 | `send_file`, `Content-Disposition` |
| 12 | `content-negotiation` | ✅ | F2 | `Accept`, `send_*`, `Vary` |
| 13 | `search` | ✅ | F2 | `body_form`, `queries`, `send_html` |
| 14 | `resource` | ✅ | F2 | CRUD, `redirect`, `set_status` |
| 15 | `web-service` | ✅ | F2 | JSON, auth por cabecera |
| 16 | `vhost` | ✅ | F2 | `Request.host`, prefijos |
| 17 | `session` | ✅ | F5 | `use_session`, `Session` |
| 18 | `cookie-sessions` | ✅ | F5 | cookie firmada |
| 19 | `auth` | ✅ | F5 | `use_basic_auth` |
| 20 | `online` | 🟡 | F5 | sesiones + SSE (sin Redis) |
| 21 | `file-upload` (propio) | ✅ | F5 | multipart |
| 22 | `auth-jwt` (propio) | ✅ | F5 | `use_bearer`, JWT |
| 23 | `websocket-chat` (propio) | ✅ | F4 | `websocket.get`, rooms |
| 24 | `sse-dashboard` (propio) | ✅ | F3 | `sse_open` |
| 25 | `rest-api` (propio) | ✅ | F6 | todo junto |

⛔ **No aplica** (y por qué):

| Ejemplo | Motivo |
|---|---|
| `ejs` | requiere motor de plantillas; no hay en PenguScript |
| `markdown` | requiere parser de Markdown (satélite inexistente) |
| `mvc` | requiere vistas/plantillas |
| `view-constructor` | ídem |
| `view-locals` | ídem |

---

## 3. Notas por ejemplo (los que tienen trampa)

### `params`

```pengu
calling app.get with "/usuario/:nombre", usuario
calling app.get with "/usuario/:nombre/:accion", accion
calling app.get with "/ficheros/*ruta", fichero
```

Comprueba:

- `%C3%A9` se decodifica a `é`.
- `:nombre` vacío (`/usuario/`) → 404.
- `*ruta` vacío (`/ficheros/`) → 200 con `ruta` vacío.
- `/usuario/a/b` no matchea `/usuario/:nombre`.

### `route-middleware`

Express usa `next()` con varios handlers por ruta. Portus no tiene `next`:
se usan capas `use_at` antes de la ruta.

```pengu
calling app.use_at with "/admin", autenticar
calling app.use_at with "/admin", registrar
calling app.get with "/admin/panel", panel
```

El ejemplo documenta la diferencia en su README: **orden de registro + `sent`**,
no `next()`.

### `route-map`

Express permite `app.get(map)`. Portus:

```pengu
let rutas as map of string to portus.Handler is {
    "/a": portus.thin_handler with handler_a,
    "/b": portus.thin_handler with handler_b
}
for ruta, handler in rutas then calling app.use_handler_at with ruta, handler
```

(El mapa de handlers se recorre al registrar; en runtime no hay dispatch por
mapa.)

### `error`

```pengu
weave falla with context as ref to portus.Context into result of void to portus.HttpError:
    return calling portus.err_void with portus.HttpError.Internal with message is "boom"

calling app.on_error with manejador_de_errores
```

El ejemplo demuestra:

- `on_error` recibe `context.error` puesto.
- Si `on_error` no envía, se responde el status por defecto.
- En `dev_mode` el mensaje aparece; en producción no.

### `static-files`

```pengu
let opciones as portus.static_file.StaticOptions is calling portus.static_file.new_static_options with "public"
calling portus.static_file.use_static with sigil of app, opciones
```

Comprueba:

- `GET /index.html` y `GET /` (index).
- `GET /../pengu.toml` → 404 (nunca sale de la raíz).
- `Range: bytes=0-99` → 206 + `Content-Range`.
- `If-None-Match` → 304.
- Extensión desconocida → `application/octet-stream`.

### `downloads`

`send_file` + `Content-Disposition: attachment; filename="…"`. El nombre se
sanea (sin `"` ni `\r\n`).

### `vhost`

Express usa el `Host`. Portus no tiene `vhost` en v1: se resuelve con un
`before` que lee `context.host` y reescribe `context.request.path` según el
subdominio, o con dos `Application` en dos puertos. El ejemplo muestra la
primera opción y lo documenta como diferencia.

### `online`

Express+Redis: lista de usuarios conectados. Portus: sesiones en memoria + SSE
para el estado. Se documenta que sin Redis no hay multi-proceso.

### `web-service`

API JSON con token en `Authorization`. Demuestra `body_json`, `send_json`,
`HttpError.Unauthorized`, `set_header`.

### `rest-api` (propio, el ejemplo grande)

Recursos `tareas` con:

- `GET /tareas`, `GET /tareas/:id`, `POST /tareas`, `PATCH /tareas/:id`,
  `DELETE /tareas/:id`.
- Validación con `result` + `HttpError.BadRequest`.
- Almacén en memoria (`map of string to Tarea`) protegido por un mutex de
  `std.filum` (aunque sea un hilo, documenta el patrón).
- Paginación con `queries`.
- ETag + 304.
- Logger + request id + CORS.
- Tests con `TestClient`.

Es el ejemplo que se usa en el README y en el benchmark.

---

## 4. Orden de implementación

1. F1: `hello-world`, `params`, `route-separation`, `multi-router`,
   `route-middleware`, `route-map`, `error`.
2. F2: `error-pages`, `cookies`, `static-files`, `downloads`,
   `content-negotiation`, `search`, `resource`, `web-service`, `vhost`.
3. F3: `sse-dashboard`.
4. F4: `websocket-chat`.
5. F5: `session`, `cookie-sessions`, `auth`, `online`, `file-upload`,
   `auth-jwt`.
6. F6: `rest-api` + README raíz + benchmarks.

---

## 5. README de cada ejemplo

Plantilla:

```markdown
# Ejemplo: <nombre>

Qué demuestra, en dos frases.

## Correr

    psc add ../../ --name portus --trust   # solo la primera vez
    psc run src/main.pengu

    curl http://127.0.0.1:3000/...

## Qué mirar

- `src/main.pengu:12` — ...
- `src/main.pengu:30` — ...

## Diferencias con Express

- ...
```

---

## 6. Lo que los ejemplos NO cubren

- Plantillas (EJS, Pug, Markdown).
- Subida a S3/almacenamiento externo.
- Redis/Postgres.
- TLS.
- HTTP/2.

Se listan en el README raíz como "fuera de alcance de v1".
