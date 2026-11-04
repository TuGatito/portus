# static-files

```sh
psc run src/main.pengu
curl -i http://127.0.0.1:3000/static/hello.txt
curl -i -H "If-None-Match: <etag>" http://127.0.0.1:3000/static/hello.txt
curl -i -H "Range: bytes=0-3" http://127.0.0.1:3000/static/hello.txt
```
