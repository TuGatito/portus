#!/usr/bin/env bash
# Compila todos los ejemplos. Falla si alguno no compila.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PSC="${PSC:-psc}"

for dir in "$ROOT"/examples/*/; do
    name="$(basename "$dir")"
    [ -f "$dir/pengu.toml" ] || continue
    printf '%-18s ' "$name"
    ( cd "$dir" && "$PSC" build src/main.pengu >"$dir/build.log" 2>&1 ) \
        || { echo "FAIL"; tail -20 "$dir/build.log"; exit 1; }
    echo "ok"
    rm -f "$dir/build.log"
done
