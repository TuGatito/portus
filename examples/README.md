# Ejemplos de portus

Los mismos ejemplos que trae Express, adaptados a PenguScript. Cada carpeta es
una aplicación independiente que usa el portus de este repo como dependencia
`path`.

## Cómo ejecutar uno

```sh
cd examples/hello-world
psc add ../.. --trust          # solo la primera vez
psc run src/main.pengu         # http://127.0.0.1:3000
```

o todos de golpe (instala la dependencia si falta y solo compila; no los
arranca):

```sh
./examples/run.sh
```

## Ejemplos

| Carpeta | Qué muestra | Equivalente en Express |
|---|---|---|
| [`hello-world`](hello-world/) | una ruta y `send_text` | `hello-world` |
| [`params`](params/) | `:param` y query string | `params` |
| [`route-separation`](route-separation/) | rutas en un módulo aparte | `route-separation` |
| [`multi-router`](multi-router/) | grupos de rutas (`new_group`) | `multi-router` |
| [`route-middleware`](route-middleware/) | `Middleware` con `use` | `route-middleware` |
| [`route-map`](route-map/) | despacho propio con `any "/*path"` | `route-map` |
| [`error`](error/) | `HttpError`, `on_error`, `on_not_found` | `error` |
| [`cookies-and-json`](cookies-and-json/) | cookies, JSON, request id, CORS | `cookies` |
| [`cookie-sessions`](cookie-sessions/) | carrito en una cookie | `cookie-sessions` |
| [`session`](session/) | sesiones firmadas en memoria | `session` |
| [`auth-jwt`](auth-jwt/) | Basic y JWT HS256 | `auth` |
| [`static-files`](static-files/) | `mount_static` con ETag y Range | `static-files` |
| [`downloads`](downloads/) | `Content-Disposition` y bytes | `downloads` |
| [`file-upload`](file-upload/) | `multipart/form-data` | — (propio) |
| [`content-negotiation`](content-negotiation/) | `portus.best` | `content-negotiation` |
| [`vhost`](vhost/) | enrutado por `Host` | `vhost` |
| [`web-service`](web-service/) | API con `X-Api-Key` | `web-service` |
| [`search`](search/) | búsqueda sobre una lista | `search` |
| [`resource`](resource/) | CRUD con `body_form` | `resource` |
| [`online`](online/) | contador de conectados en memoria | `online` (sin Redis) |
| [`websocket-chat`](websocket-chat/) | WebSocket con broadcast | — (propio) |

## Ejemplos de Express que no están

- `ejs`, `markdown`, `mvc`, `view-constructor`, `view-locals`: necesitan un
  motor de plantillas que PenguScript todavía no tiene.
- `online` con Redis: la versión de este repo guarda el estado en memoria.
- `sse-dashboard` y `rest-api`: ejemplos propios planificados en
  [`../scratch/portus-examples.md`](../scratch/portus-examples.md).
