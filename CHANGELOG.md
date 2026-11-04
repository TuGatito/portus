# Changelog

Todas las versiones notables de portus. Formato basado en [Keep a
Changelog](https://keepachangelog.com/es-ES/1.1.0/) y versionado semántico.

## [0.1.0] — 2026-11-04

Primera versión completa: el mismo terreno que Express.js y Hono.js, en
PenguScript. Requiere **PenguScript ≥ 0.28.9**.

### Añadido

- **Núcleo**: `Application`, `Context`, `Handler` (`result of void to
  HttpError`), `Group`, `Params`, `PatternError`; `get/post/put/patch/delete/
  head/options/any`, `new_group`, `serve`.
- **Router**: rutas literales, `:param` y `*wildcard`, orden por especificidad,
  `HEAD` y `OPTIONS` automáticos, `405` con `Allow`, `404` configurable.
- **Contexto**: `param`, `query_param`, `header`, `cookie`, `body_text`,
  `body_json`, `body_form`, `body_multipart`, `set_header`, `set_status`,
  `send_text/html/json`, `send_body_with_type`, `send_status`, `redirect`,
  `redirect_permanent`, `no_content`, `not_modified`, `mark_sent`.
- **Middlewares**: `use`, `use_at`, `after`; `use_request_id`,
  `use_security_headers`, `use_cors`, `use_logger`, `use_compress` (gzip),
  `use_csp`, `use_rate_limit`, `use_rate_limit_at`.
- **Errores**: `HttpError` (`bad_request`, `unauthorized`, `forbidden`,
  `not_found`, `method_not_allowed`, `conflict`, `payload_too_large`,
  `unsupported_media_type`, `too_many_requests`, `internal`), `on_error`,
  `on_not_found`, `on_start`, `on_shutdown`, `status_of`.
- **Estáticos**: `mount_static` con `Content-Type`, `ETag`, `Last-Modified`,
  `If-None-Match` (304) y `Range` (206/416), sin salirse de la raíz.
- **Streaming**: `stream_begin`/`stream_write`/`stream_end` (chunked),
  `sse_open`/`sse_send`/`sse_close`.
- **WebSocket**: handshake RFC 6455, frames (texto, binario, ping/pong, close),
  `ws_upgrade` y `Conn.send_text/send_bytes/close`.
- **Sesiones**: cookie `portus_sid` firmada con HMAC-SHA-256, almacén en memoria
  con caducidad, `sessions_configure/start/load/get/set/save/destroy`.
- **Auth**: Basic, Bearer, JWT HS256 (`jwt_sign`, `jwt_verify`, `jwt_claims`,
  `jwt_bearer`) con comparación en tiempo constante.
- **Seguridad**: CSRF double-submit (`csrf_protect`), CSP, cabeceras de
  seguridad y rate limit de ventana fija por cliente.
- **HTTP/1.1**: `poll(2)` + llhttp 9.4.3 vendorizado, keep-alive, `chunked`,
  pipelining, límites de cabecera y cuerpo (`431`/`413`), timeouts (`408`) y
  cierre limpio.
- **Ejemplos**: 21 en `examples/` (los de Express adaptados) que compilan con
  `./examples/run.sh`.
- **Pruebas**: 42 unitarias (`psc test`) y tres gates e2e HTTP reales
  (`tests/e2e_hello.sh`, `tests/e2e_ws.sh`, `tests/e2e_security.sh`).
- **Docs**: `docs/api/` generado con `psc doc` (22 módulos).

### Notas

- Un solo hilo y handlers síncronos: un handler lento bloquea el proceso. El
  escalado horizontal y los hilos están en
  [`scratch/portus-deferred.md`](scratch/portus-deferred.md).
- Los bugs del compilador que portus encontró (y los workarounds mientras se
  arreglan) están en
  [`scratch/portus-compiler-fixes.md`](scratch/portus-compiler-fixes.md).
