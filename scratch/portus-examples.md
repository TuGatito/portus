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
| 1 | `hello-world` | ✅ `examples/hello-world` | F1 | `get`, `send_text`, `serve` |
| 2 | `params` | ✅ `examples/params` | F1 | `:param`, `param`, `query_param` |
| 3 | `route-separation` | ✅ `examples/route-separation` | F1 | módulos separados, `get` |
| 4 | `multi-router` | ✅ `examples/multi-router` | F1 | `new_group`, `Group` |
| 5 | `route-middleware` | ✅ `examples/route-middleware` | F1 | `middleware`, `use` |
| 6 | `route-map` | ✅ `examples/route-map` | F1 | `any "/*path"`, `context.path` |
| 7 | `error` | ✅ `examples/error` | F1 | `err_void`, `on_error`, `on_not_found`, `status_of` |
| 8 | `error-pages` | 🟡 cubierto por `examples/error` | F1 | `on_not_found`, `send_html` |
| 9 | `cookies` | ✅ `examples/cookies-and-json` | F2 | `set_cookie`, `cookie`, `body_json`, `use_cors` |
| 10 | `static-files` | ✅ `examples/static-files` | F2 | `mount_static` (ETag, Last-Modified, Range) |
| 11 | `downloads` | ✅ `examples/downloads` | F2 | `send_body_with_type`, `Content-Disposition` |
| 12 | `content-negotiation` | ✅ `examples/content-negotiation` | F2 | `portus.best`, `accepts` |
| 13 | `search` | ✅ `examples/search` | F2 | `query_param`, strings (`contains`) |
| 14 | `resource` | ✅ `examples/resource` | F2 | CRUD, `body_form`, `send_status` |
| 15 | `web-service` | ✅ `examples/web-service` | F2 | JSON, auth por cabecera |
| 16 | `vhost` | ✅ `examples/vhost` | F2 | `context.header "host"` |
| 17 | `session` | ✅ `examples/session` | F5 | `sessions_start/load/get/set/save/destroy` |
| 18 | `cookie-sessions` | ✅ `examples/cookie-sessions` | F5 | cookies firmadas por la aplicación |
| 19 | `auth` | ✅ `examples/auth-jwt` | F5 | `basic_auth`, `jwt_bearer` |
| 20 | `online` | 🟡 `examples/online` (sin Redis) | F5 | `static var` documentado + contador |
| 21 | `file-upload` (propio) | ✅ `examples/file-upload` | F5 | `body_multipart`, `multipart_part` |
| 22 | `auth-jwt` (propio) | ✅ `examples/auth-jwt` | F5 | `jwt_sign`, `jwt_verify`, `bearer_required` |
| 23 | `websocket-chat` (propio) | ✅ `examples/websocket-chat` | F3 | `ws_upgrade`, `Conn.send_text`, broadcast |
| 24 | `sse-dashboard` (propio) | ⬜ pendiente | F6 | `sse_open`, `sse_send` |
| 25 | `rest-api` (propio) | ⬜ pendiente | F6 | todo junto |

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
