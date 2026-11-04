# Portus — Trabajo diferido (post-v1)

> **Qué es esto.** Todo lo que decidimos **no** hacer en v1, por qué, y qué
> tiene que pasar antes para poder hacerlo. Si algo no está aquí ni en el plan,
> no existe.
>
> Regla: lo diferido no se implementa "a medias". O está fuera, o está
> terminado y probado.

---

## 1. Concurrencia real (hilos)

**Estado:** fuera de v1. Depende del lenguaje, no de portus.

Un handler es síncrono y corre en el hilo del event loop: si tarda, bloquea
todas las conexiones. Es el modelo de Node, y es honesto mientras el lenguaje
no tenga `async`/`await`.

Para tener un pool de hilos hacen falta, en este orden:

1. **Hilos en PenguScript.** Hoy no existe `spawn`; `std.filum` tiene
   `Mutex`/`Chan`/`WaitGroup` pero el lenguaje no puede crear un hilo. Hay que
   añadir una primitiva (`spawn weave(...) into Thread`, por ejemplo) al
   compilador y a la stdlib.
2. **Contexto por hilo.** Cada hilo necesita su propio `Context` y su propia
   arena por petición (hoy `app.arena` es compartida y de vida larga).
3. **Estado compartido seguro.** `app.state` es un `map` sin candado; habría
   que añadir un `Mutex` al contenedor o una API `state_get`/`state_set`
   atómica.
4. **Reparto del socket.** Aceptar en un hilo y repartir `Connection` entre
   workers (necesita `epoll`/`kqueue` con `EPOLLEXCLUSIVE` o `SO_REUSEPORT`).

**Se reevalúa cuando el lenguaje tenga hilos.** No antes.

## 2. Varios procesos (`SO_REUSEPORT` + `fork`)

**Estado:** F7.

La forma barata de escalar sin tocar el modelo: `N` procesos, cada uno con su
event loop, compartiendo el puerto con `SO_REUSEPORT`. Requiere:

- `fork(2)` en el shim C y un `supervisor` que reinicie hijos (o `systemd`).
- Decidir qué estado es compartido (sesiones en memoria **no** valen; haría
  falta un store externo o `mmap`).
- `std.rites` todavía no expone señales; el shim ya las captura para el
  apagado ordenado.

## 3. TLS nativo

**Estado:** fuera de v1. `is_secure` se deduce de `X-Forwarded-Proto` con
`trust_proxy = true`; en producción se termina TLS en un proxy (nginx, Caddy,
Traefik).

Motivos: +3 semanas, una dependencia pesada (mbedTLS u OpenSSL, Apache-2.0) y
una superficie de bugs de seguridad que no queremos mantener en v1.

Para hacerlo: `mbedtls` (elegida sobre OpenSSL por tamaño y licencia) envuelta
en el shim, `portus.tls` con `TlsConfig`, y el handshake en el loop (no
bloqueante). Requiere revisar el contrato de `native` (hoy no hay `void*` de
TLS en `Server`).

## 4. HTTP/2

**Estado:** fuera de v1 y probablemente de v2.

Requiere un parser distinto (nghttp2 o HPACK propio), multiplexación por
stream y un modelo de respuesta por stream que rompe el `Response` actual
(una respuesta por petición). Si algún día se hace, es un transporte nuevo, no
un parche.

## 5. `permessage-deflate` (WebSocket comprimido)

Fuera. El handshake y el framing son de v1 (F4); la extensión de compresión
añade estado por conexión y un caso de uso marginal. Si se pide, es un
`Upgrade` con `Sec-WebSocket-Extensions` y un `deflate` por conexión.

## 6. Brotli

Fuera. `std.compressus` da gzip/deflate con miniz (dominio público). Brotli
(MIT) es una dependencia C más y un 15–20 % de ahorro en texto; no compensa en
v1. `Content-Encoding: br` se documenta como no soportado.

## 7. WebAssembly / navegador

Fuera. `std.web` ya tiene un servidor de mentira en el navegador; portus es de
servidor. Si se quiere un `TestClient` que corra en wasm, se puede portar el
dispatcher (no el transporte) más adelante.

## 8. Windows

Fuera de v1. El shim usa `poll(2)`; en Windows haría falta `WSAPoll` o IOCP.
`std.net` ya devuelve error en Windows. Se documenta como "Linux/macOS"; si
alguien lo pide, es un `#ifdef _WIN32` en el shim, no un rediseño.

## 9. Métricas y tracing

`GET /metrics` en formato Prometheus y `traceparent` (W3C) quedan para v1.x.
La instrumentación interna (`request_id`, tiempos por petición) sí entra en
v1; exportarlos, no.

## 10. CLI (`portus`)

Un binario para `portus new`/`portus routes`/`portus bench` no entra en v1; el
"route inspector" en `dev_mode` (imprime la tabla al arrancar) sí. Un CLI
necesitaría un subcomando en `psc` o un ejecutable aparte; se decide después de
F6.

## 11. HTTP/1.1 avanzado

- **Trailers**: se parsean y se ignoran (documentado). Enviarlos, no.
- **`Expect: 100-continue`**: se responde 100 automáticamente; el handler no
  lo ve. Bien para v1.
- **Pipelining**: se atiende en orden, sin paralelismo (correcto, lento). No se
  optimiza.
- **`multipart/byteranges`**: solo `Range` de un rango y `If-Range`. El resto,
  v1.x.

## 12. Sesiones y stores externos

El store de v1 es memoria del proceso (`memory_store`). Redis/Postgres/SQLite
son plugins v1.x: la interfaz `SessionStore` (concept) ya los admite, pero no
se implementan.

## 13. Cifrado de sesiones y JWT asimétrico

v1: cookie firmada con HMAC-SHA256, JWT HS256. RS256/ES256 quedan fuera porque
requieren parsear/validar claves públicas (PKCS#8/X.509) y `std.digest` no
tiene RSA. Si se pide, se hace con mbedTLS o libsodium, con su licencia.

## 14. Compresión de respuestas en streaming

v1 comprime cuerpos completos (sabe el tamaño) y hace `chunked` sin comprimir.
Comprimir un `stream` en vuelo requiere un `deflate` incremental por respuesta;
se puede añadir sin romper la API (`send_stream_compressed`).

## 15. Autenticación de cliente / mTLS / OCSP

Fuera. Depende de TLS nativo (§3).

---

## Cómo se usa este documento

- Al cerrar cada fase, se revisa: ¿algo de aquí se desbloqueó? Si sí, se mueve
  al plan con su fase.
- Si el usuario pide algo de aquí, se anota la fecha y se planifica; no se
  implementa "de paso".
- Nada de aquí puede colarse en un PR de v1.
