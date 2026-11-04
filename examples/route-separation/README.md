# route-separation

Las rutas viven en su propio módulo (`src/routes.pengu`) y `main` solo arranca la
aplicación, como la carpeta `routes/` de Express.

```sh
psc run src/main.pengu
curl http://127.0.0.1:3000/       # inicio
curl http://127.0.0.1:3000/about  # acerca de
```
