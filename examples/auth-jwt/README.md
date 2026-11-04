# auth-jwt

Basic y JWT HS256 con `portus`.

```sh
psc run src/main.pengu
curl "http://127.0.0.1:3000/login?user=ada"
curl -H "Authorization: Bearer <token>" http://127.0.0.1:3000/private
curl -u ada:secreto http://127.0.0.1:3000/basic
```

El secreto es de ejemplo: cámbialo y léelo de una variable de entorno.
