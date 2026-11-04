# cookies-and-json

Cookies de sesión (`context.set_cookie` / `context.cookie`), lectura de un
cuerpo JSON (`context.body_json`) y las cabeceras de `use_request_id` y
`use_security_headers`.

```sh
psc run src/main.pengu
curl -c jar -s "http://127.0.0.1:3000/login?user=ada"   # hola ada
curl -b jar -s http://127.0.0.1:3000/me                  # eres ada
curl -s -X POST -d '{"ok":true}' http://127.0.0.1:3000/echo
# {"ok":true}
```
