# session

Sesiones firmadas con cookie `portus_sid` y almacén en memoria.

```sh
psc run src/main.pengu
curl -c jar -b jar "http://127.0.0.1:3000/login?user=ada"
curl -b jar http://127.0.0.1:3000/me
curl -b jar http://127.0.0.1:3000/logout
```

El secreto es de ejemplo y el almacén vive en el proceso: al reiniciar se pierden las sesiones.
