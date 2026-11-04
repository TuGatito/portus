# multi-router

Dos grupos de rutas en la misma aplicación. En portus un "router montado" es un
`Group`: la aplicación y el prefijo, sin copiar la aplicación ni un despachador
aparte.

```sh
psc run src/main.pengu
curl http://127.0.0.1:3000/            # inicio
curl http://127.0.0.1:3000/api/users   # api: usuarios
curl http://127.0.0.1:3000/admin/users # admin: usuarios
```
