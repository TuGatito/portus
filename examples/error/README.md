# error

Errores como valores: un handler devuelve `err_void` con un `HttpError`, y la
aplicación lo convierte en respuesta (400, 404, 405, 500). `on_not_found` y
`on_error` permiten personalizar el formato.

```sh
psc run src/main.pengu
curl -i http://127.0.0.1:3000/boom   # 400 + "algo salió mal"
curl -i http://127.0.0.1:3000/nope   # 404 + "no existe esa ruta"
```
