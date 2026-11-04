# sse-dashboard

Panel HTML con `EventSource` que consume un flujo SSE.

```sh
psc run src/main.pengu   # abre http://127.0.0.1:3000
```

- `GET /` sirve la página.
- `GET /events` abre `text/event-stream`, envía tres latidos y cierra.

```sh
curl -N http://127.0.0.1:3000/events
```

portus es de un solo hilo: si el flujo se quedara abierto para siempre, ese
handler acapararía el servidor. Un panel real usa un hilo de fondo (ver
`scratch/portus-deferred.md`).
