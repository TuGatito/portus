#!/usr/bin/env bash
# Vendoriza llhttp en `include/llhttp/` como una sola unidad de traducción.
#
# llhttp se distribuye para compilarse en tres unidades (`api.c`, `http.c`,
# `llhttp.c`), pero portus viaja dentro del bundle C del consumidor: todo el C
# tiene que poder incluirse desde una cabecera. Este script baja una versión
# fija del tag oficial y genera el «amalgamado»:
#
#   1. copia `llhttp.h` y la licencia MIT,
#   2. concatena `api.c` + `http.c` + `llhttp.c` en `llhttp_impl.h`,
#   3. quita las declaraciones anticipadas de las funciones que define
#      `api.c`/`http.c` (en una sola unidad chocarían con su definición),
#   4. envuelve el bloque en `#pragma GCC diagnostic ignored "-Wpointer-sign"`
#      (las llamadas internas pasan `const unsigned char*` a las funciones de
#      `api.c`, que reciben `const char*`; el puntero es el mismo),
#   5. compila un programa de humo con `-Wall -Werror` para que el vendorizado
#      no introduzca avisos.
#
# Uso: tools/vendor_llhttp.sh [VERSION]   (por defecto 9.4.3)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:-9.4.3}"
TAG="release/v${VERSION}"
DEST="$ROOT/include/llhttp"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> llhttp $VERSION"
curl -fsSL "https://codeload.github.com/nodejs/llhttp/tar.gz/refs/tags/release/v${VERSION}" \
    -o "$WORK/llhttp.tgz"
tar xzf "$WORK/llhttp.tgz" -C "$WORK"
SRC="$WORK/llhttp-release-v${VERSION}"
for f in src/api.c src/http.c src/llhttp.c include/llhttp.h LICENSE-MIT; do
    [ -f "$SRC/$f" ] || { echo "FALTA: $f en el tarball de llhttp $VERSION" >&2; exit 1; }
done

mkdir -p "$DEST"
cp "$SRC/include/llhttp.h" "$DEST/llhttp.h"
cp "$SRC/LICENSE-MIT" "$DEST/LICENSE"

python3 - "$SRC" "$DEST" "$VERSION" <<'PY'
import re
import sys

src, dest, version = sys.argv[1], sys.argv[2], sys.argv[3]
api = open(f"{src}/src/api.c", encoding="utf-8").read()
http = open(f"{src}/src/http.c", encoding="utf-8").read()
gen = open(f"{src}/src/llhttp.c", encoding="utf-8").read()

# Las funciones que define api.c/http.c se declaran otra vez al principio de
# llhttp.c, con los tipos del generador (`const unsigned char*`). En una sola
# unidad de traducción chocarían con su definición, así que se quitan: la
# definición (que va antes en el amalgamado) ya sirve de declaración.
defined = set(re.findall(r"^int (llhttp__[A-Za-z0-9_]+)\(", api, re.M))
defined |= set(re.findall(r"^int (llhttp__[A-Za-z0-9_]+)\(", http, re.M))
removed = 0
for name in sorted(defined):
    gen, n = re.subn(r"\nint " + re.escape(name) + r"\(\n(?:[^;{}]*?)\);\n", "\n", gen)
    removed += n
print(f"declaraciones anticipadas quitadas: {removed} ({len(defined)} funciones)")

head = f"""/* llhttp {version} — amalgamado generado por tools/vendor_llhttp.sh.
 *
 * NO EDITAR A MANO. Origen, licencia y detalles: include/llhttp/SOURCE.
 */
#ifndef PORTUS_LLHTTP_IMPL_H
#define PORTUS_LLHTTP_IMPL_H

/* Las funciones de api.c reciben `const char*` y el generador las llama con
 * `const unsigned char*`. El puntero es el mismo; el aviso no aporta nada. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "llhttp.h"

"""
tail = """
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif /* PORTUS_LLHTTP_IMPL_H */
"""

body = "\n".join([api, "", http, "", gen])
open(f"{dest}/llhttp_impl.h", "w", encoding="utf-8").write(head + body + tail)
PY

cat >"$DEST/SOURCE" <<EOF
llhttp ${VERSION}
===============

Origen:    https://github.com/nodejs/llhttp (tag ${TAG})
Descarga:  https://codeload.github.com/nodejs/llhttp/tar.gz/refs/tags/${TAG}
Licencia:  MIT (Copyright Fedor Indutny, 2018). Ver include/llhttp/LICENSE.
Fecha:     $(date -u +%Y-%m-%d)

Archivos
--------
- \`llhttp.h\`: cabecera pública, copiada tal cual.
- \`llhttp_impl.h\`: amalgamado de \`src/api.c\`, \`src/http.c\` y
  \`src/llhttp.c\` (en ese orden), generado por \`tools/vendor_llhttp.sh\`.
- \`LICENSE\`: licencia MIT original.

Por qué amalgamado
------------------
llhttp se compila normalmente en tres unidades de traducción. Portus se
instala como dependencia y el consumidor compila un único bundle de C: todo
el código nativo tiene que entrar por una cabecera. El script quita las
declaraciones anticipadas que el generador repite en \`llhttp.c\` para las
funciones definidas en \`api.c\`/\`http.c\` (en una sola unidad chocarían) y
silencia \`-Wpointer-sign\` alrededor del bloque generado.

Actualizar
----------
\`\`\`sh
tools/vendor_llhttp.sh 9.5.0   # cambia la versión; revisa el diff
\`\`\`
La compilación de humo del script usa \`-Wall -Werror\`: si una versión nueva
introduce avisos o cambia el generador, el script falla y hay que adaptarlo.
EOF

# Compilación de humo: el amalgamado debe compilar sin avisos y parsear.
cat >"$WORK/smoke.c" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "llhttp_impl.h"

int main(void) {
    llhttp_t parser;
    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    llhttp_init(&parser, HTTP_REQUEST, &settings);
    const char *req = "GET /hola?x=1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    enum llhttp_errno err = llhttp_execute(&parser, req, strlen(req));
    if (err != HPE_OK || parser.method != HTTP_GET) {
        fprintf(stderr, "llhttp no parsea: %s\n", llhttp_errno_name(err));
        return 1;
    }
    return 0;
}
EOF
gcc -std=c99 -Wall -Wextra -Werror -O1 -I"$DEST" -o "$WORK/smoke" "$WORK/smoke.c"
"$WORK/smoke"
echo "==> OK llhttp $VERSION en include/llhttp/"
