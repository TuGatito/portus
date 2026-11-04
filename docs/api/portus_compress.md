# portus.compress

> Source: `src/portus/compress.pengu`

portus.compress — comprime la respuesta con gzip si el cliente lo acepta.

`use_compress(app)` registra un *finisher*: cuando el handler ya escribió,
mira `Accept-Encoding`, el `Content-Type` y el tamaño, comprime el cuerpo y
añade `Content-Encoding: gzip`. No toca imágenes, vídeo, `application/zip`
ni respuestas de menos de 860 bytes.

## Ejemplo
```pengu
calling portus.use_compress with sigil of app
```

---

### `MIN_BYTES`

> **MIN_BYTES** : int

 Tamaño mínimo para que compense comprimir.

### `_compress`

> **_compress**(data: ref to void, ctx: ref to Context) -> result of void to HttpError

 Compresión en el *finisher*.

### `_accepts_gzip`

> **_accepts_gzip**(ctx: ref to Context) -> bool

 `true` si el `Accept-Encoding` de la petición incluye gzip.

### `_content_type`

> **_content_type**(response: ref to Response) -> string

 `Content-Type` de la respuesta (vacío si no hay).

### `_response_header`

> **_response_header**(response: ref to Response, name: string) -> maybe string

 Valor de una cabecera de la respuesta.

### `_is_compressible`

> **_is_compressible**(content_type: string) -> bool

 `true` si merece la pena comprimir ese tipo.

