# Portus — Licencias y terceros

> Portus se distribuye bajo **MIT** (ver `LICENSE`). Todo lo que se vendorice o
> se enlace debe ser compatible con esa licencia y quedar acreditado.
>
> Regla de oro: si una dependencia obliga a cambiar la licencia de portus o de
> las apps que lo usan, **no entra**.

---

## 1. Librerías evaluadas

| Librería | Licencia | ¿Compatible? | Uso en portus | Notas |
|---|---|---|---|---|
| **llhttp** | MIT | ✅ | parser HTTP (vendorizado) | requiere conservar copyright + permiso |
| **uSockets** | Apache-2.0 | ✅ | ❌ descartado | compatible, pero no lo necesitamos |
| **OpenSSL** | Apache-2.0 | ✅ | ❌ descartado v1 | pesado; TLS en F7 |
| **mbedTLS** | Apache-2.0 **o** GPL-2.0-or-later | ✅ (elegir Apache-2.0) | F7 (TLS) | hay que declarar la elección en THIRD_PARTY |
| **miniz** | dominio público / MIT | ✅ | ya en `std.compressus` | no lo vendorizamos |
| **libsodium** | ISC | ✅ | opcional (`std.secrets`) | no dependemos de él |
| **libargon2** | CC0 / Apache-2.0 | ✅ | ❌ descartado v1 | Argon2 fuera de v1 |
| **multipart-parser** | MIT | ✅ | ❌ descartado | parser propio (~200 líneas) |
| **libmagic** | BSD-2 | ✅ | ❌ descartado | tabla MIME propia |
| **pcre2** | BSD-3 | ✅ | ❌ descartado | `std.regulus` ya enlaza POSIX `re` |
| **picohttpparser** | MIT + Perl | ✅ | ❌ descartado | no es streaming; llhttp gana |
| **brotli** | MIT | ✅ | ❌ descartado v1 | gzip/deflate bastan |
| **zlib** | zlib | ✅ | ❌ no hace falta | miniz ya está |
| **civetweb** | MIT | ✅ | ❌ descartado | framework completo; no lo usamos |
| **libuv** | MIT | ✅ | ❌ descartado | event loop propio |
| **mongoose** | **GPL-2.0** | ❌ | — | copyleft fuerte: contamina el binario |
| **Redis / Postgres** | — | n/a | — | servicios externos, no se enlazan |

### Por qué se descartó cada una

- **uSockets**: Apache-2.0 es compatible, pero es un event loop completo con su
  propio modelo de callbacks y TLS. Envolverlo cuesta más que escribir un
  `poll` de ~800 líneas y añade una dependencia que el usuario tendría que
  compilar. Además, su API no está pensada para un único callback síncrono.
- **OpenSSL / mbedTLS**: TLS no está en v1 (D8). Cuando entre, mbedTLS es más
  pequeño y su licencia dual permite elegir Apache-2.0.
- **libargon2**: Argon2 es lo correcto para contraseñas, pero es un satélite
  más y v1 no lo necesita (auth Basic/Bearer/JWT). Se reevalúa en F7.
- **multipart-parser**: el parser es sencillo y así controlamos límites y
  errores; una dependencia más no compensa.
- **libmagic**: enorme por una tabla de extensiones. `portus.mime` es una tabla
  de ~60 entradas.
- **pcre2**: `std.regulus` ya usa `re.h` del sistema; añadir pcre2 duplicaría.
- **mongoose**: GPL-2.0. Un binario que enlace mongoose debe distribuirse bajo
  GPL-2.0; incompatible con portus MIT y con las apps de los usuarios.

---

## 2. Obligaciones por licencia

### MIT (llhttp)

Al vendorizar `include/llhttp/` hay que incluir:

- el texto completo de la licencia MIT de llhttp;
- el aviso de copyright (`Copyright (c) 2018 Fedor Indutny`).

Se hace en `include/llhttp/LICENSE` + entrada en `THIRD_PARTY.md`.

### Apache-2.0 (mbedTLS, si algún día)

- incluir el texto de Apache-2.0;
- incluir/modificar `NOTICE` si el proyecto lo trae;
- **declarar cambios** en los ficheros modificados;
- no usar las marcas del proyecto.

### Dominio público / CC0 (miniz, libargon2)

- sin obligaciones, pero se acredita igual en `THIRD_PARTY.md` por cortesía y
  trazabilidad.

---

## 3. `THIRD_PARTY.md` (plantilla)

```markdown
# Terceros

| Componente | Versión | Licencia | Copyright | Ubicación |
|---|---|---|---|---|
| llhttp | 9.x.y | MIT | Fedor Indutny | `include/llhttp/` |

## llhttp

Texto de la licencia MIT reproducido en `include/llhttp/LICENSE`.

## Cambios sobre llhttp

- `llhttp_amalgam.c`: concatenación de `api.c`, `http.c`, `llhttp.c` con
  `#include` relativo; sin cambios de lógica.
```

---

## 4. Checklist antes de añadir una dependencia C

- [ ] Licencia en la lista permitida: MIT, BSD-2/3, Apache-2.0, ISC, CC0,
      dominio público, zlib.
- [ ] No es GPL/LGPL/AGPL/MPL (o hay aprobación explícita y se documenta el
      impacto).
- [ ] Se puede compilar en Linux x86_64 y macOS arm64 sin dependencias del
      sistema raras.
- [ ] Es header-only o se puede amalgamar (para que `psc add` funcione).
- [ ] Tamaño razonable (< 500 KiB de fuente).
- [ ] Sin estado global mutable ni hilos propios (o se documenta).
- [ ] Entra en `THIRD_PARTY.md` con versión, licencia, copyright y ubicación.
- [ ] Se actualiza `tools/license_check.sh` si aplica.
- [ ] Decisión anotada en `portus-decisions.md`.

---

## 5. Licencia del propio portus

- **MIT**, igual que PenguScript. `LICENSE` ya está en el repo.
- Los ejemplos y la documentación también MIT.
- No se aceptan contribuciones con licencias incompatibles.
