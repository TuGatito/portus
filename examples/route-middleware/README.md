# route-middleware

Un middleware que corre antes de la ruta y añade una cabecera a la respuesta.

```sh
psc run src/main.pengu
curl -i http://127.0.0.1:3000/
# HTTP/1.1 200 OK
# X-Ruta: con-middleware
```
