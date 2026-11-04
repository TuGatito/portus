# content-negotiation

```sh
psc run src/main.pengu
curl -H "Accept: application/json" http://127.0.0.1:3000/data
curl -H "Accept: text/html" http://127.0.0.1:3000/data
curl -H "Accept: image/png" http://127.0.0.1:3000/data
```
