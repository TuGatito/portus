# Chat WebSocket

Cada mensaje de texto se reenvía a todas las conexiones abiertas.

## Ejecutar

```bash
psc add ../.. --trust      # solo la primera vez
psc run src/main.pengu
```

Abre varias pestañas o usa un cliente WebSocket contra
`ws://127.0.0.1:3000/chat` y envía texto: lo recibirán todas.

## Qué demuestra

- `portus.ws_upgrade` con un handler que lleva su propio estado (`data`).
- Varias conexiones simultáneas en el mismo bucle `poll`.
- Cierre limpio: al recibir el `opcode` 8 la conexión se quita de la lista.
