# rest-api

API REST en memoria con JSON de entrada y salida.

```sh
psc run src/main.pengu
```

| Método | Ruta | Qué hace |
|---|---|---|
| `GET` | `/items` | Lista en JSON |
| `POST` | `/items` | Crea; exige un objeto JSON (`{"nombre":"x"}`), devuelve `{"id":N}` con 201 |
| `GET` | `/items/:id` | Devuelve el objeto o 404 |
| `DELETE` | `/items/:id` | Borra; 204 o 404 |

```sh
curl -i -X POST -d '{"nombre":"mesa"}' http://127.0.0.1:3000/items
curl http://127.0.0.1:3000/items
curl http://127.0.0.1:3000/items/1
```

El almacén es un `static var` de este módulo: se pierde al reiniciar, como en
`examples/online` y `examples/resource`.
