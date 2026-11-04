/* portus_native.h — capa C de portus.
 *
 * Header-only a propósito: el proyecto del consumidor compila `portus` dentro
 * de su única unidad de traducción, así que `psc add` no necesita encontrar
 * archivos ni enlazar bibliotecas.
 *
 * Todo símbolo exportado lleva el prefijo `portus_` y `static` (o
 * `static inline`) para no colisionar con el consumidor.
 */
#ifndef PORTUS_NATIVE_H
#define PORTUS_NATIVE_H

#include <stdint.h>

/* Devuelve `n + 1`. Prueba de humo del puente C: si esto compila desde un
 * proyecto consumidor, el layout y el `-Iinclude` funcionan. */
static inline int32_t portus_native_ping(int32_t n) {
    return n + 1;
}

#endif /* PORTUS_NATIVE_H */
