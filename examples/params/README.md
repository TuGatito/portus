# params

Parámetros de ruta (`:name`) y query string.

```sh
psc run src/main.pengu
curl http://127.0.0.1:3000/users/ada    # hola ada
curl "http://127.0.0.1:3000/users/ada?x=1"  # hola ada
curl http://127.0.0.1:3000/greet?name=ada   # query: name=ada
```
