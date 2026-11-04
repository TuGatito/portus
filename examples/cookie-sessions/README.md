# cookie-sessions

```sh
psc run src/main.pengu
curl -c jar -b jar "http://127.0.0.1:3000/cart?add=manzana"
curl -c jar -b jar "http://127.0.0.1:3000/cart?add=pera"
```

Demostración: el carrito vive en la cookie, sin firmar. Para sesiones de verdad,
usa `examples/session` (`portus.sessions_*`).
