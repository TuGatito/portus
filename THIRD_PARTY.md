# Third-party code in portus

portus distribuye código de terceros solo cuando es imprescindible y siempre
vendorizado en el repositorio (nunca descargado durante el build). Este archivo
registra qué hay, bajo qué licencia y de dónde salió.

## Vendorizado en `include/`

| Componente | Versión | Licencia | Ubicación | Uso |
| --- | --- | --- | --- | --- |
| llhttp | 9.4.3 | MIT (Copyright Fedor Indutny, 2018) | `include/llhttp/` | Parser HTTP/1.1 incremental |

`include/llhttp/SOURCE` documenta la procedencia exacta, la transformación
(amalgamado en una unidad de traducción) y cómo actualizarlo
(`tools/vendor_llhttp.sh`). La licencia MIT se conserva en
`include/llhttp/LICENSE`.

## Indirecto (lo aporta el runtime de PenguScript)

Estos componentes vienen con el bundle que genera `psc`; portus no los
vendoriza ni los modifica.

| Componente | Licencia | Uso |
| --- | --- | --- |
| SDS | BSD-2-Clause | Cadenas dinámicas del runtime |
| Convenient Containers | MIT | Mapas y contenedores del runtime |
| vaqt | BSD-3-Clause | Utilidades del runtime |
| miniz | Public domain / MIT | Compresión (`std.compressus`) |

## Reglas

1. Una dependencia nueva necesita: licencia permisiva compatible con MIT,
   versión fijada, archivo `SOURCE` con procedencia y fecha, y entrada en esta
   tabla.
2. Nada de licencias copyleft fuerte (GPL, AGPL) ni de código sin licencia.
3. El código vendorizado no se edita a mano: si hay que transformarlo, la
   transformación vive en un script reproducible dentro de `tools/`.
4. Actualizar una dependencia es un cambio aparte, con su propio commit y sus
   pruebas pasando.
