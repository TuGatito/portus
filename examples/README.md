# Ejemplos de portus

Los mismos ejemplos que trae Express, adaptados a PenguScript. Cada carpeta es
una aplicación independiente que usa el portus de este repo como dependencia
`path` (no hace falta `psc add`).

## Cómo ejecutar uno

```sh
cd examples/hello-world
psc run src/main.pengu          # http://127.0.0.1:3000
```

o todos de golpe (solo compila; no los arranca):

```sh
./examples/run.sh               # compila los 5 ejemplos
```

## Ejemplos

| Carpeta | Qué muestra | Equivalente en Express |
|---|---|---|
| [`hello-world`](hello-world/) | una ruta y `send_text` | `hello-world` |
| [`params`](params/) | `:param` y query string | `params` |
| [`multi-router`](multi-router/) | grupos de rutas (`new_group`) | `multi-router` |
| [`route-middleware`](route-middleware/) | `Middleware` con `use` | `route-middleware` |
| [`error`](error/) | `HttpError`, `on_error`, `on_not_found` | `error` |

## Ejemplos de Express que no están

- `ejs`, `markdown`, `mvc`, `view-constructor`, `view-locals`: necesitan un
  motor de plantillas que PenguScript todavía no tiene.
- `online`: necesita Redis.
- `auth`, `cookies`, `session`, `static-files`, `downloads`, `vhost`,
  `content-negotiation`, `search`, `resource`, `web-service`, `cookie-sessions`:
  llegan con F2/F3 (cookies, sesiones, estáticos, negociación de contenido).
- `file-upload`: multipart, F2.
- `auth-jwt`, `websocket-chat`, `sse-dashboard`, `rest-api`: ejemplos propios
  planificados en [`../scratch/portus-examples.md`](../scratch/portus-examples.md).
