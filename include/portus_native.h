/* portus_native.h — capa C de portus.
 *
 * Header-only a propósito: el proyecto del consumidor compila `portus` dentro
 * de su única unidad de traducción, así que `psc add` no necesita encontrar
 * archivos ni enlazar bibliotecas.
 *
 * Todo símbolo exportado lleva el prefijo `portus_` y `static` (o
 * `static inline`) para no colisionar con el consumidor.
 *
 * Modelo de ejecución: un hilo, un bucle `poll(2)`. Los handlers corren en el
 * hilo del bucle; mientras un handler trabaja no se atiende a nadie más.
 */
#ifndef PORTUS_NATIVE_H
#define PORTUS_NATIVE_H

/* Las funciones del header son `static`: el consumidor solo usa las que llama,
 * así que marcamos todas como "posiblemente sin usar" para no ensuciar -Wall. */
#if defined(__GNUC__) || defined(__clang__)
#define PORTUS_MAYBE_UNUSED __attribute__((unused))
#else
#define PORTUS_MAYBE_UNUSED
#endif

/* Adelanto: la usan los helpers de sesión antes de su definición. */
static PORTUS_MAYBE_UNUSED int portus_native_random_hex(unsigned char *out, int bytes);

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
/* Windows no está soportado todavía: se devuelve un error explicativo. */
#define PORTUS_NATIVE_WINDOWS 1
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <strings.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "llhttp/llhttp_impl.h"

#define PORTUS_NATIVE_OK 0
#define PORTUS_NATIVE_ERR (-1)

#define PORTUS_DEFAULT_MAX_HEADER_BYTES (16 * 1024)
#define PORTUS_DEFAULT_MAX_BODY_BYTES (1024 * 1024)
#define PORTUS_DEFAULT_IDLE_TIMEOUT_MS 60000
#define PORTUS_DEFAULT_REQUEST_TIMEOUT_MS 30000
/* Tamaño máximo de un mensaje WebSocket completo (RFC 6455 §7.4.1: 1009). */
#define PORTUS_WS_MAX_MESSAGE (1024u * 1024u)
#define PORTUS_READ_CHUNK 16384

/* Códigos de error de la capa nativa. */
enum {
    PORTUS_ERR_NONE = 0,
    PORTUS_ERR_LISTEN = 1,
    PORTUS_ERR_BIND = 2,
    PORTUS_ERR_POLL = 3,
    PORTUS_ERR_PLATFORM = 4,
    PORTUS_ERR_ALLOC = 5,
    PORTUS_ERR_CONN = 6
};

typedef struct portus_header {
    char *name;
    size_t name_len;
    char *value;
    size_t value_len;
} portus_header;

typedef struct portus_buffer {
    char *data;
    size_t len;
    size_t cap;
} portus_buffer;

struct portus_request {
    int method;
    char *path;
    size_t path_len;
    char *query;
    size_t query_len;
    char *body;
    size_t body_len;
    size_t body_cap;
    portus_header *headers;
    size_t header_count;
    size_t header_cap;
    int keep_alive;
    int http_major;
    int http_minor;
    int complete;
    int oversized; /* 0 = no, 1 = cabeceras, 2 = cuerpo */
    /* Estado del parser mientras llega la petición. */
    portus_buffer url;
    portus_buffer field;
    portus_buffer value;
    int in_value;
    size_t header_bytes;
};

struct portus_response {
    int status;
    portus_header *headers;
    size_t header_count;
    size_t header_cap;
    portus_buffer body;
    int sent;
    struct portus_conn *conn;
    int streaming;
    int stream_ended;
};

/* Los punteros se declaran como `void *` a propósito: así el binding de
 * PenguScript puede pasar un weave con `userdata as ref to void, request as
 * ref to void, response as ref to void` sin conversiones de tipo función. */
typedef void (*portus_handler_fn)(void *userdata, void *request, void *response);

/* Nombres estables para el binding (PenguScript no ve `struct portus_*`). */
typedef struct portus_request portus_request_t;
typedef struct portus_response portus_response_t;

/* Callback de WebSocket: opcode 1 texto, 2 binario, 8 cierre. */
typedef void (*portus_ws_fn)(void *userdata, void *response, int opcode,
                             const unsigned char *data, int len);

typedef struct portus_conn {
    int fd;
    int active;
    int poll_slot;
    int close_after_write;
    int client_closed;
    int max_header_bytes;
    int max_body_bytes;
    unsigned long long last_activity_ms;
    unsigned long long request_started_ms;
    llhttp_t parser;
    llhttp_settings_t settings;
    struct portus_request request;
    struct portus_response response;
    portus_buffer in;
    portus_buffer out;
    size_t out_written;
    int ws;
    portus_ws_fn ws_cb;
    void *ws_data;
    int ws_closed;
    /* Mensaje WebSocket en construcción (fragmentos). */
    unsigned char *ws_msg;
    size_t ws_msg_len;
    size_t ws_msg_cap;
    int ws_msg_opcode;
    /* Estado del validador UTF-8 incremental (texto fragmentado). */
    int ws_utf8_need;
    unsigned int ws_utf8_cp;
    unsigned int ws_utf8_min;
} portus_conn;

/* Adelanto: el streaming necesita vaciar la conexión. */
static int portus_conn_flush(portus_conn *c);

struct portus_server {
    int listen_fd;
    int max_connections;
    int max_header_bytes;
    int max_body_bytes;
    int idle_timeout_ms;
    int request_timeout_ms;
    int keep_alive;
    unsigned long long started_ms;
    struct portus_conn *connections;
} portus_server;

typedef struct portus_server portus_server_t;

/* ------------------------------------------------------------------ */
/* Buffers                                                             */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED int portus_buf_reserve(portus_buffer *b, size_t extra) {
    size_t need;
    size_t cap;
    char *data;
    if (extra > (size_t)-1 - b->len) {
        return PORTUS_NATIVE_ERR;
    }
    need = b->len + extra;
    if (need <= b->cap) {
        return PORTUS_NATIVE_OK;
    }
    cap = b->cap ? b->cap : 64;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2) {
            return PORTUS_NATIVE_ERR;
        }
        cap *= 2;
    }
    data = (char *)realloc(b->data, cap);
    if (data == NULL) {
        return PORTUS_NATIVE_ERR;
    }
    b->data = data;
    b->cap = cap;
    return PORTUS_NATIVE_OK;
}

static PORTUS_MAYBE_UNUSED int portus_buf_append(portus_buffer *b, const void *src, size_t n) {
    if (n == 0) {
        return PORTUS_NATIVE_OK;
    }
    if (portus_buf_reserve(b, n) != PORTUS_NATIVE_OK) {
        return PORTUS_NATIVE_ERR;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return PORTUS_NATIVE_OK;
}

static PORTUS_MAYBE_UNUSED int portus_buf_append_str(portus_buffer *b, const char *s) {
    return portus_buf_append(b, s, strlen(s));
}

static PORTUS_MAYBE_UNUSED void portus_buf_clear(portus_buffer *b) {
    b->len = 0;
}

static PORTUS_MAYBE_UNUSED void portus_buf_free(portus_buffer *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

/* Copia `src` a una cadena terminada en NUL. */
static PORTUS_MAYBE_UNUSED char *portus_strndup_n(const char *src, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (out == NULL) {
        return NULL;
    }
    if (n > 0) {
        memcpy(out, src, n);
    }
    out[n] = '\0';
    return out;
}

static PORTUS_MAYBE_UNUSED void portus_header_free(portus_header *h) {
    free(h->name);
    free(h->value);
    h->name = NULL;
    h->value = NULL;
    h->name_len = 0;
    h->value_len = 0;
}

static PORTUS_MAYBE_UNUSED void portus_headers_free(portus_header **list, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        portus_header_free(&(*list)[i]);
    }
    free(*list);
    *list = NULL;
}

static PORTUS_MAYBE_UNUSED int portus_headers_push(portus_header **list, size_t *count, size_t *cap,
                               const char *name, size_t name_len,
                               const char *value, size_t value_len) {
    portus_header *grown;
    if (*count == *cap) {
        size_t next = *cap ? *cap * 2 : 8;
        grown = (portus_header *)realloc(*list, next * sizeof(portus_header));
        if (grown == NULL) {
            return PORTUS_NATIVE_ERR;
        }
        *list = grown;
        *cap = next;
    }
    (*list)[*count].name = portus_strndup_n(name, name_len);
    (*list)[*count].value = portus_strndup_n(value, value_len);
    if ((*list)[*count].name == NULL || (*list)[*count].value == NULL) {
        portus_header_free(&(*list)[*count]);
        return PORTUS_NATIVE_ERR;
    }
    (*list)[*count].name_len = name_len;
    (*list)[*count].value_len = value_len;
    (*count)++;
    return PORTUS_NATIVE_OK;
}

/* ------------------------------------------------------------------ */
/* Tiempo                                                              */
/* ------------------------------------------------------------------ */

/* TLS opcional por dlopen.
 *
 * El paquete es header-only: no enlazamos OpenSSL para no obligar al
 * consumidor a -lssl. Si la biblioteca existe cargamos los simbolos que
 * necesitamos; si no, `tls_available` devuelve 0 y el servidor va en claro.
 */
typedef void *portus_tls_ctx;
typedef void *portus_tls_ssl;
typedef void *(*portus_fn_tls_method)(void);
typedef portus_tls_ctx (*portus_fn_ctx_new)(const void *);
typedef void (*portus_fn_ctx_free)(portus_tls_ctx);
typedef int (*portus_fn_use_cert)(portus_tls_ctx, const char *, int);
typedef int (*portus_fn_use_key)(portus_tls_ctx, const char *, int, void *);
typedef portus_tls_ssl (*portus_fn_ssl_new)(portus_tls_ctx);
typedef void (*portus_fn_ssl_free)(portus_tls_ssl);
typedef int (*portus_fn_set_fd)(portus_tls_ssl, int);
typedef int (*portus_fn_accept)(portus_tls_ssl);

struct portus_tls_ops {
    void *ssl_handle;
    portus_fn_tls_method server_method;
    portus_fn_ctx_new ctx_new;
    portus_fn_ctx_free ctx_free;
    portus_fn_use_cert use_cert;
    portus_fn_use_key use_key;
    portus_fn_ssl_new ssl_new;
    portus_fn_ssl_free ssl_free;
    portus_fn_set_fd set_fd;
    portus_fn_accept accept;
    int loaded;
};

static PORTUS_MAYBE_UNUSED int portus_tls_load(struct portus_tls_ops *ops)
{
    if (ops->loaded)
        return ops->ssl_handle != NULL;
    ops->loaded = 1;
    ops->ssl_handle = dlopen("libssl.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!ops->ssl_handle)
        ops->ssl_handle = dlopen("libssl.so", RTLD_NOW | RTLD_LOCAL);
    if (!ops->ssl_handle)
        ops->ssl_handle = dlopen("libssl.3.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!ops->ssl_handle)
        ops->ssl_handle = dlopen("libssl.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!ops->ssl_handle)
        return 0;
    ops->server_method = (portus_fn_tls_method)dlsym(ops->ssl_handle, "TLS_server_method");
    ops->ctx_new = (portus_fn_ctx_new)dlsym(ops->ssl_handle, "SSL_CTX_new");
    ops->ctx_free = (portus_fn_ctx_free)dlsym(ops->ssl_handle, "SSL_CTX_free");
    ops->use_cert = (portus_fn_use_cert)dlsym(ops->ssl_handle, "SSL_CTX_use_certificate_file");
    ops->use_key = (portus_fn_use_key)dlsym(ops->ssl_handle, "SSL_CTX_use_PrivateKey_file");
    ops->ssl_new = (portus_fn_ssl_new)dlsym(ops->ssl_handle, "SSL_new");
    ops->ssl_free = (portus_fn_ssl_free)dlsym(ops->ssl_handle, "SSL_free");
    ops->set_fd = (portus_fn_set_fd)dlsym(ops->ssl_handle, "SSL_set_fd");
    ops->accept = (portus_fn_accept)dlsym(ops->ssl_handle, "SSL_accept");
    if (!ops->server_method || !ops->ctx_new || !ops->ctx_free || !ops->use_cert
        || !ops->use_key || !ops->ssl_new || !ops->ssl_free || !ops->set_fd
        || !ops->accept) {
        dlclose(ops->ssl_handle);
        ops->ssl_handle = NULL;
        return 0;
    }
    return 1;
}

/* 1 si hay TLS disponible en el sistema (OpenSSL via dlopen). */
static PORTUS_MAYBE_UNUSED int portus_native_tls_available(void)
{
    struct portus_tls_ops ops;
    memset(&ops, 0, sizeof(ops));
    return portus_tls_load(&ops);
}

static PORTUS_MAYBE_UNUSED unsigned long long portus_now_ms(void) {
#if defined(PORTUS_NATIVE_WINDOWS)
    return 0;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)(ts.tv_nsec / 1000000L);
#endif
}

/* ------------------------------------------------------------------ */
/* Petición y respuesta                                                */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED void portus_request_clear(struct portus_request *r) {
    portus_headers_free(&r->headers, r->header_count);
    r->header_count = 0;
    r->header_cap = 0;
    free(r->path);
    r->path = NULL;
    r->path_len = 0;
    free(r->query);
    r->query = NULL;
    r->query_len = 0;
    r->body_len = 0;
    r->method = 0;
    r->keep_alive = 0;
    r->http_major = 0;
    r->http_minor = 0;
    r->complete = 0;
    r->oversized = 0;
    r->header_bytes = 0;
    r->in_value = 0;
    portus_buf_clear(&r->url);
    portus_buf_clear(&r->field);
    portus_buf_clear(&r->value);
}

static PORTUS_MAYBE_UNUSED void portus_response_clear(struct portus_response *res) {
    portus_headers_free(&res->headers, res->header_count);
    res->header_count = 0;
    res->header_cap = 0;
    res->status = 200;
    res->sent = 0;
    portus_buf_clear(&res->body);
}

static PORTUS_MAYBE_UNUSED const char *portus_reason_phrase(int status) {
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    default: return "Unknown";
    }
}

static PORTUS_MAYBE_UNUSED int portus_response_find_header(const struct portus_response *res,
                                       const char *name, size_t name_len) {
    size_t i;
    for (i = 0; i < res->header_count; i++) {
        if (res->headers[i].name_len == name_len &&
            strncasecmp(res->headers[i].name, name, name_len) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Callbacks de llhttp                                                 */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED int portus_on_url(llhttp_t *parser, const char *at, size_t length) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    if (portus_buf_append(&c->request.url, at, length) != PORTUS_NATIVE_OK) {
        return -1;
    }
    if (c->request.url.len > (size_t)c->max_header_bytes) {
        c->request.oversized = 1;
        return -1;
    }
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_flush_header(struct portus_request *r) {
    int rc;
    if (r->field.len == 0) {
        return 0;
    }
    rc = portus_headers_push(&r->headers, &r->header_count, &r->header_cap,
                             r->field.data ? r->field.data : "",
                             r->field.len, r->value.data ? r->value.data : "",
                             r->value.len);
    portus_buf_clear(&r->field);
    portus_buf_clear(&r->value);
    r->in_value = 0;
    return rc == PORTUS_NATIVE_OK ? 0 : -1;
}

static PORTUS_MAYBE_UNUSED int portus_on_header_field(llhttp_t *parser, const char *at,
                                  size_t length) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    if (c->request.in_value) {
        if (portus_flush_header(&c->request) != 0) {
            return -1;
        }
    }
    if (portus_buf_append(&c->request.field, at, length) != PORTUS_NATIVE_OK) {
        return -1;
    }
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_on_header_value(llhttp_t *parser, const char *at,
                                  size_t length) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    if (portus_buf_append(&c->request.value, at, length) != PORTUS_NATIVE_OK) {
        return -1;
    }
    c->request.header_bytes =
        c->request.field.len + c->request.value.len + c->request.header_bytes;
    /* Solo se cuenta una vez por trozo; el tope exacto se comprueba al cerrar
     * la cabecera y con el tamaño total del búfer de entrada. */
    if (c->request.header_bytes > (size_t)c->max_header_bytes) {
        c->request.oversized = 1;
        return -1;
    }
    c->request.in_value = 1;
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_on_headers_complete(llhttp_t *parser) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    struct portus_request *r = &c->request;
    char *url;
    size_t i;
    size_t split;
    if (portus_flush_header(r) != 0) {
        return -1;
    }
    r->method = (int)parser->method;
    r->keep_alive = llhttp_should_keep_alive(parser);
    r->http_major = parser->http_major;
    r->http_minor = parser->http_minor;
    split = r->url.len;
    for (i = 0; i < r->url.len; i++) {
        if (r->url.data[i] == '?') {
            split = i;
            break;
        }
    }
    url = r->url.data ? r->url.data : (char *)"";
    r->path = portus_strndup_n(url, split);
    if (r->path == NULL) {
        return -1;
    }
    r->path_len = split;
    if (split < r->url.len) {
        size_t qlen = r->url.len - split - 1;
        r->query = portus_strndup_n(url + split + 1, qlen);
        if (r->query == NULL) {
            return -1;
        }
        r->query_len = qlen;
    } else {
        r->query = portus_strndup_n("", 0);
        if (r->query == NULL) {
            return -1;
        }
    }
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_on_body(llhttp_t *parser, const char *at, size_t length) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    struct portus_request *r = &c->request;
    if (r->body_len + length > (size_t)c->max_body_bytes) {
        r->oversized = 2;
        return -1;
    }
    if (r->body_len + length > r->body_cap) {
        size_t next = r->body_cap ? r->body_cap : 256;
        char *grown;
        while (next < r->body_len + length) {
            next *= 2;
        }
        grown = (char *)realloc(r->body, next);
        if (grown == NULL) {
            return -1;
        }
        r->body = grown;
        r->body_cap = next;
    }
    memcpy(r->body + r->body_len, at, length);
    r->body_len += length;
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_on_message_complete(llhttp_t *parser) {
    struct portus_conn *c = (struct portus_conn *)parser->data;
    c->request.complete = 1;
    /* Se pausa para responder y, si quedan bytes, seguir con la siguiente
     * petición de la misma conexión (pipelining). */
    return HPE_PAUSED;
}

static PORTUS_MAYBE_UNUSED void portus_settings_init(llhttp_settings_t *settings) {
    llhttp_settings_init(settings);
    settings->on_url = portus_on_url;
    settings->on_header_field = portus_on_header_field;
    settings->on_header_value = portus_on_header_value;
    settings->on_headers_complete = portus_on_headers_complete;
    settings->on_body = portus_on_body;
    settings->on_message_complete = portus_on_message_complete;
}

/* ------------------------------------------------------------------ */
/* Respuesta                                                           */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED void portus_native_response_set_status(struct portus_response *res, int status) {
    res->status = status;
}

static PORTUS_MAYBE_UNUSED void portus_response_set_header(struct portus_response *res,
                                       const char *name, size_t name_len,
                                       const char *value, size_t value_len) {
    int found = portus_response_find_header(res, name, name_len);
    if (found >= 0) {
        char *copy = portus_strndup_n(value, value_len);
        if (copy == NULL) {
            return;
        }
        free(res->headers[found].value);
        res->headers[found].value = copy;
        res->headers[found].value_len = value_len;
        return;
    }
    (void)portus_headers_push(&res->headers, &res->header_count,
                              &res->header_cap, name, name_len, value,
                              value_len);
}

static PORTUS_MAYBE_UNUSED void portus_response_add_header(struct portus_response *res,
                                       const char *name, size_t name_len,
                                       const char *value, size_t value_len) {
    (void)portus_headers_push(&res->headers, &res->header_count,
                              &res->header_cap, name, name_len, value,
                              value_len);
}

static PORTUS_MAYBE_UNUSED void portus_response_write(struct portus_response *res, const char *data,
                                  size_t len) {
    (void)portus_buf_append(&res->body, data, len);
}

static PORTUS_MAYBE_UNUSED int portus_http_date(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_utc;
#if defined(PORTUS_NATIVE_WINDOWS)
    if (gmtime_s(&tm_utc, &now) != 0) {
        return PORTUS_NATIVE_ERR;
    }
#else
    if (gmtime_r(&now, &tm_utc) == NULL) {
        return PORTUS_NATIVE_ERR;
    }
#endif
    if (strftime(out, out_size, "%a, %d %b %Y %H:%M:%S GMT", &tm_utc) == 0) {
        return PORTUS_NATIVE_ERR;
    }
    return PORTUS_NATIVE_OK;
}

/* Serializa la respuesta en `out`. Devuelve 0 si todo fue bien. */
static PORTUS_MAYBE_UNUSED int portus_response_serialize(struct portus_response *res,
                                     struct portus_request *req,
                                     int keep_alive, portus_buffer *out) {
    char head[64];
    char date[64];
    size_t i;
    int has_date = 0;
    int has_length = 0;
    int has_connection = 0;
    int is_head = req != NULL && req->method == HTTP_HEAD;
    int no_body = res->status == 204 || res->status == 304;

    portus_buf_clear(out);
    snprintf(head, sizeof(head), "HTTP/1.1 %d %s\r\n", res->status,
             portus_reason_phrase(res->status));
    if (portus_buf_append_str(out, head) != PORTUS_NATIVE_OK) {
        return PORTUS_NATIVE_ERR;
    }
    for (i = 0; i < res->header_count; i++) {
        if (res->headers[i].name_len == 4 &&
            strncasecmp(res->headers[i].name, "date", 4) == 0) {
            has_date = 1;
        }
        if (res->headers[i].name_len == 14 &&
            strncasecmp(res->headers[i].name, "content-length", 14) == 0) {
            has_length = 1;
        }
        if (res->headers[i].name_len == 10 &&
            strncasecmp(res->headers[i].name, "connection", 10) == 0) {
            has_connection = 1;
        }
        if (portus_buf_append(out, res->headers[i].name,
                              res->headers[i].name_len) != PORTUS_NATIVE_OK ||
            portus_buf_append_str(out, ": ") != PORTUS_NATIVE_OK ||
            portus_buf_append(out, res->headers[i].value,
                              res->headers[i].value_len) != PORTUS_NATIVE_OK ||
            portus_buf_append_str(out, "\r\n") != PORTUS_NATIVE_OK) {
            return PORTUS_NATIVE_ERR;
        }
    }
    if (!has_date && portus_http_date(date, sizeof(date)) == PORTUS_NATIVE_OK) {
        if (portus_buf_append_str(out, "Date: ") != PORTUS_NATIVE_OK ||
            portus_buf_append_str(out, date) != PORTUS_NATIVE_OK ||
            portus_buf_append_str(out, "\r\n") != PORTUS_NATIVE_OK) {
            return PORTUS_NATIVE_ERR;
        }
    }
    if (!has_length) {
        snprintf(head, sizeof(head), "Content-Length: %zu\r\n", res->body.len);
        if (portus_buf_append_str(out, head) != PORTUS_NATIVE_OK) {
            return PORTUS_NATIVE_ERR;
        }
    }
    if (!has_connection) {
        if (portus_buf_append_str(out, keep_alive ? "Connection: keep-alive\r\n"
                                                 : "Connection: close\r\n") !=
            PORTUS_NATIVE_OK) {
            return PORTUS_NATIVE_ERR;
        }
    }
    if (portus_buf_append_str(out, "\r\n") != PORTUS_NATIVE_OK) {
        return PORTUS_NATIVE_ERR;
    }
    if (!is_head && !no_body && res->body.len > 0) {
        if (portus_buf_append(out, res->body.data, res->body.len) !=
            PORTUS_NATIVE_OK) {
            return PORTUS_NATIVE_ERR;
        }
    }
    return PORTUS_NATIVE_OK;
}


/* ------------------------------------------------------------------ */
/* Streaming / SSE                                                     */
/* ------------------------------------------------------------------ */

/* Empieza una respuesta en streaming (chunked). Devuelve 0 o -1. */
static PORTUS_MAYBE_UNUSED int portus_native_response_stream_begin(void *res,
                                                                  const unsigned char *content_type,
                                                                  int content_type_len) {
    struct portus_response *r = (struct portus_response *)res;
    portus_conn *c = r->conn;
    char date[64];
    if (c == NULL || r->streaming) {
        return -1;
    }
    r->status = 200;
    r->sent = 1;
    r->streaming = 1;
    r->stream_ended = 0;
    portus_response_set_header(r, "Content-Type", 12, (const char *)content_type,
                               (size_t)content_type_len);
    portus_response_set_header(r, "Cache-Control", 13, "no-cache", 8);
    portus_buf_clear(&c->out);
    if (portus_buf_append_str(&c->out, "HTTP/1.1 200 OK\r\n") != PORTUS_NATIVE_OK) {
        return -1;
    }
    {
        size_t i;
        for (i = 0; i < r->header_count; i++) {
            if (portus_buf_append_str(&c->out, r->headers[i].name) != PORTUS_NATIVE_OK ||
                portus_buf_append_str(&c->out, ": ") != PORTUS_NATIVE_OK ||
                portus_buf_append_str(&c->out, r->headers[i].value) != PORTUS_NATIVE_OK ||
                portus_buf_append_str(&c->out, "\r\n") != PORTUS_NATIVE_OK) {
                return -1;
            }
        }
    }
    if (portus_http_date(date, sizeof(date)) == PORTUS_NATIVE_OK) {
        if (portus_buf_append_str(&c->out, "Date: ") != PORTUS_NATIVE_OK ||
            portus_buf_append_str(&c->out, date) != PORTUS_NATIVE_OK ||
            portus_buf_append_str(&c->out, "\r\n") != PORTUS_NATIVE_OK) {
            return -1;
        }
    }
    if (portus_buf_append_str(&c->out, "Transfer-Encoding: chunked\r\n") != PORTUS_NATIVE_OK ||
        portus_buf_append_str(&c->out, "Connection: keep-alive\r\n\r\n") != PORTUS_NATIVE_OK) {
        return -1;
    }
    (void)portus_conn_flush(c);
    return 0;
}

/* Escribe un trozo de streaming como un chunk. Devuelve 0 o -1. */
static PORTUS_MAYBE_UNUSED int portus_native_response_stream_write(void *res,
                                                                  const unsigned char *data,
                                                                  int len) {
    struct portus_response *r = (struct portus_response *)res;
    portus_conn *c = r->conn;
    char head[32];
    if (c == NULL || !r->streaming || len < 0) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    snprintf(head, sizeof(head), "%zx\r\n", (size_t)len);
    if (portus_buf_append_str(&c->out, head) != PORTUS_NATIVE_OK ||
        portus_buf_append(&c->out, data, (size_t)len) != PORTUS_NATIVE_OK ||
        portus_buf_append_str(&c->out, "\r\n") != PORTUS_NATIVE_OK) {
        return -1;
    }
    (void)portus_conn_flush(c);
    return 0;
}

/* Termina el streaming con el chunk final. Devuelve 0 o -1. */
static PORTUS_MAYBE_UNUSED int portus_native_response_stream_end(void *res) {
    struct portus_response *r = (struct portus_response *)res;
    portus_conn *c = r->conn;
    if (c == NULL || !r->streaming) {
        return -1;
    }
    if (portus_buf_append_str(&c->out, "0\r\n\r\n") != PORTUS_NATIVE_OK) {
        return -1;
    }
    r->streaming = 0;
    r->stream_ended = 1;
    (void)portus_conn_flush(c);
    return 0;
}


/* Identificador de sesión: 32 hex en un buffer estático (no reentrante). */
static PORTUS_MAYBE_UNUSED const char *portus_native_session_id(void) {
    static char id[33];
    if (portus_native_random_hex((unsigned char *)id, 16) != 0) {
        memcpy(id, "00000000000000000000000000000000", 32);
        id[32] = '\0';
    }
    return id;
}

/* Compara dos buffers en tiempo constante (longitudes incluidas). */
static PORTUS_MAYBE_UNUSED int portus_native_constant_time_eq(const unsigned char *a, int a_len,
                                                              const unsigned char *b, int b_len) {
    unsigned char diff = 0;
    int i;
    if (a_len != b_len) {
        return 0;
    }
    for (i = 0; i < a_len; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/* ------------------------------------------------------------------ */
/* WebSocket (RFC 6455, sin extensiones ni fragmentación)              */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED void portus_sha1(const unsigned char *data, size_t len,
                                            unsigned char out[20]) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    size_t total = len;
    size_t padded = ((len + 8) / 64 + 1) * 64;
    unsigned char *buf = (unsigned char *)malloc(padded);
    size_t i;
    uint64_t bits = (uint64_t)len * 8u;
    if (buf == NULL) {
        memset(out, 0, 20);
        return;
    }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    memset(buf + len + 1, 0, padded - len - 1);
    for (i = 0; i < 8; i++) {
        buf[padded - 1 - i] = (unsigned char)((bits >> (8u * i)) & 0xFFu);
    }
    for (i = 0; i < padded; i += 64) {
        uint32_t w[80];
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
        uint32_t e;
        size_t j;
        for (j = 0; j < 16; j++) {
            w[j] = ((uint32_t)buf[i + j * 4] << 24) | ((uint32_t)buf[i + j * 4 + 1] << 16) |
                   ((uint32_t)buf[i + j * 4 + 2] << 8) | (uint32_t)buf[i + j * 4 + 3];
        }
        for (j = 16; j < 80; j++) {
            uint32_t v = w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16];
            w[j] = (v << 1) | (v >> 31);
        }
        a = h[0];
        b = h[1];
        c = h[2];
        d = h[3];
        e = h[4];
        for (j = 0; j < 80; j++) {
            uint32_t f;
            uint32_t k;
            uint32_t t;
            if (j < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (j < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (j < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            t = ((a << 5) | (a >> 27)) + f + e + k + w[j];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = t;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    free(buf);
    for (i = 0; i < 5; i++) {
        out[i * 4] = (unsigned char)((h[i] >> 24) & 0xFFu);
        out[i * 4 + 1] = (unsigned char)((h[i] >> 16) & 0xFFu);
        out[i * 4 + 2] = (unsigned char)((h[i] >> 8) & 0xFFu);
        out[i * 4 + 3] = (unsigned char)(h[i] & 0xFFu);
    }
    (void)total;
}

static PORTUS_MAYBE_UNUSED int portus_base64(const unsigned char *in, int len, char *out,
                                             size_t out_cap) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i;
    size_t o = 0;
    if (out_cap < (size_t)(((len + 2) / 3) * 4 + 1)) {
        return -1;
    }
    for (i = 0; i + 2 < len; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        out[o++] = table[(v >> 18) & 63u];
        out[o++] = table[(v >> 12) & 63u];
        out[o++] = table[(v >> 6) & 63u];
        out[o++] = table[v & 63u];
    }
    if (i < len) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < len) {
            v |= (uint32_t)in[i + 1] << 8;
        }
        out[o++] = table[(v >> 18) & 63u];
        out[o++] = table[(v >> 12) & 63u];
        out[o++] = (i + 1 < len) ? table[(v >> 6) & 63u] : '=';
        out[o++] = '=';
    }
    out[o] = 0;
    return (int)o;
}

static PORTUS_MAYBE_UNUSED int portus_conn_header_contains(portus_conn *c, const char *name,
                                                           const char *needle) {
    size_t i;
    size_t nlen = strlen(name);
    for (i = 0; i < c->request.header_count; i++) {
        if (c->request.headers[i].name_len == nlen &&
            strncasecmp(c->request.headers[i].name, name, nlen) == 0) {
            return strcasestr(c->request.headers[i].value, needle) != NULL;
        }
    }
    return 0;
}

/* Envía un frame WebSocket. */
static PORTUS_MAYBE_UNUSED int portus_ws_send_frame(portus_conn *c, int opcode,
                                                    const unsigned char *data, int len) {
    unsigned char head[10];
    size_t head_len = 0;
    if (c == NULL || c->fd < 0) {
        return -1;
    }
    head[0] = (unsigned char)(0x80 | (opcode & 0x0F));
    if (len < 126) {
        head[1] = (unsigned char)len;
        head_len = 2;
    } else if (len < 65536) {
        head[1] = 126;
        head[2] = (unsigned char)((len >> 8) & 0xFF);
        head[3] = (unsigned char)(len & 0xFF);
        head_len = 4;
    } else {
        head[1] = 127;
        memset(head + 2, 0, 4);
        head[6] = (unsigned char)((len >> 24) & 0xFF);
        head[7] = (unsigned char)((len >> 16) & 0xFF);
        head[8] = (unsigned char)((len >> 8) & 0xFF);
        head[9] = (unsigned char)(len & 0xFF);
        head_len = 10;
    }
    if (portus_buf_append(&c->out, head, head_len) != PORTUS_NATIVE_OK ||
        (len > 0 && portus_buf_append(&c->out, data, (size_t)len) != PORTUS_NATIVE_OK)) {
        return -1;
    }
    return portus_conn_flush(c);
}

/* ¿Es `s` (de `len` bytes) UTF-8 válido? RFC 3629. */
static PORTUS_MAYBE_UNUSED int portus_ws_utf8_valid(const unsigned char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char b = s[i];
        unsigned int cp;
        int extra;
        int k;
        if (b < 0x80) {
            i++;
            continue;
        } else if ((b & 0xE0) == 0xC0) {
            cp = (unsigned int)(b & 0x1Fu);
            extra = 1;
        } else if ((b & 0xF0) == 0xE0) {
            cp = (unsigned int)(b & 0x0Fu);
            extra = 2;
        } else if ((b & 0xF8) == 0xF0) {
            cp = (unsigned int)(b & 0x07u);
            extra = 3;
        } else {
            return 0;
        }
        if (len - i <= (size_t)extra) {
            return 0;
        }
        for (k = 1; k <= extra; k++) {
            unsigned char cc = s[i + (size_t)k];
            if ((cc & 0xC0) != 0x80) {
                return 0;
            }
            cp = (cp << 6) | (unsigned int)(cc & 0x3Fu);
        }
        if (extra == 1 && cp < 0x80u) {
            return 0;
        }
        if (extra == 2 && cp < 0x800u) {
            return 0;
        }
        if (extra == 3 && cp < 0x10000u) {
            return 0;
        }
        if (cp >= 0xD800u && cp <= 0xDFFFu) {
            return 0;
        }
        if (cp > 0x10FFFFu) {
            return 0;
        }
        i += (size_t)extra + 1;
    }
    return 1;
}

/* Alimenta el validador UTF-8 incremental; 0 si el flujo ya es inválido. */
static PORTUS_MAYBE_UNUSED int portus_ws_utf8_feed(portus_conn *c, const unsigned char *s,
                                                   size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        unsigned char b = s[i];
        if (c->ws_utf8_need > 0) {
            if ((b & 0xC0) != 0x80) {
                return 0;
            }
            c->ws_utf8_cp = (c->ws_utf8_cp << 6) | (unsigned int)(b & 0x3Fu);
            c->ws_utf8_need--;
            if (c->ws_utf8_need == 0) {
                if (c->ws_utf8_cp < c->ws_utf8_min) {
                    return 0;
                }
                if (c->ws_utf8_cp >= 0xD800u && c->ws_utf8_cp <= 0xDFFFu) {
                    return 0;
                }
                if (c->ws_utf8_cp > 0x10FFFFu) {
                    return 0;
                }
            }
            continue;
        }
        if (b < 0x80) {
            continue;
        } else if (b >= 0xC2 && b <= 0xDF) {
            c->ws_utf8_need = 1;
            c->ws_utf8_cp = (unsigned int)(b & 0x1Fu);
            c->ws_utf8_min = 0x80u;
        } else if (b >= 0xE0 && b <= 0xEF) {
            c->ws_utf8_need = 2;
            c->ws_utf8_cp = (unsigned int)(b & 0x0Fu);
            c->ws_utf8_min = 0x800u;
        } else if (b >= 0xF0 && b <= 0xF4) {
            c->ws_utf8_need = 3;
            c->ws_utf8_cp = (unsigned int)(b & 0x07u);
            c->ws_utf8_min = 0x10000u;
        } else {
            return 0;
        }
    }
    return 1;
}

/* Cierra el WebSocket con `code` y un motivo corto. */
static PORTUS_MAYBE_UNUSED void portus_ws_fail(portus_conn *c, int code, const char *reason) {
    unsigned char payload[125];
    size_t n = 2;
    payload[0] = (unsigned char)((code >> 8) & 0xFF);
    payload[1] = (unsigned char)(code & 0xFF);
    if (reason != NULL) {
        size_t rl = strlen(reason);
        if (rl > sizeof(payload) - 2) {
            rl = sizeof(payload) - 2;
        }
        memcpy(payload + 2, reason, rl);
        n += rl;
    }
    (void)portus_ws_send_frame(c, 0x8, payload, (int)n);
    c->ws_closed = 1;
    c->close_after_write = 1;
}

/* Devuelve 0 si el mensaje ya es demasiado grande (1009). */
static PORTUS_MAYBE_UNUSED int portus_ws_msg_append(portus_conn *c, const unsigned char *data,
                                                    size_t len) {
    size_t need;
    if (len > PORTUS_WS_MAX_MESSAGE || c->ws_msg_len > PORTUS_WS_MAX_MESSAGE - len) {
        return 0;
    }
    need = c->ws_msg_len + len;
    if (need > c->ws_msg_cap) {
        size_t cap = c->ws_msg_cap == 0 ? 4096 : c->ws_msg_cap;
        unsigned char *nb;
        while (cap < need) {
            cap *= 2;
        }
        nb = (unsigned char *)realloc(c->ws_msg, cap);
        if (nb == NULL) {
            return 0;
        }
        c->ws_msg = nb;
        c->ws_msg_cap = cap;
    }
    memcpy(c->ws_msg + c->ws_msg_len, data, len);
    c->ws_msg_len += len;
    return 1;
}

/* Procesa los frames completos que haya en `c->in`. */
static PORTUS_MAYBE_UNUSED void portus_ws_consume(portus_conn *c) {
    while (c->in.len >= 2) {
        unsigned char *p = (unsigned char *)c->in.data;
        int fin = (p[0] & 0x80) != 0;
        int rsv = p[0] & 0x70;
        int opcode = p[0] & 0x0F;
        int masked = (p[1] & 0x80) != 0;
        unsigned long long len = (unsigned long long)(p[1] & 0x7F);
        size_t off = 2;
        unsigned char mask[4];
        unsigned char *payload;
        int i;
        int is_control = opcode >= 0x8;

        if (rsv != 0) {
            portus_ws_fail(c, 1002, "rsv");
            return;
        }
        if (opcode != 0x0 && opcode != 0x1 && opcode != 0x2 && opcode != 0x8 && opcode != 0x9 &&
            opcode != 0xA) {
            portus_ws_fail(c, 1002, "opcode");
            return;
        }
        if (is_control && (!fin || (p[1] & 0x7F) > 125)) {
            portus_ws_fail(c, 1002, "control frame");
            return;
        }
        if (opcode == 0x0 && c->ws_msg_opcode == 0) {
            portus_ws_fail(c, 1002, "continuation");
            return;
        }
        if ((opcode == 0x1 || opcode == 0x2) && c->ws_msg_opcode != 0) {
            portus_ws_fail(c, 1002, "new message inside fragment");
            return;
        }
        if (len == 126) {
            if (c->in.len < 4) {
                return;
            }
            len = ((unsigned long long)p[2] << 8) | p[3];
            off = 4;
        } else if (len == 127) {
            if (c->in.len < 10) {
                return;
            }
            if ((p[2] & 0x80) != 0) {
                portus_ws_fail(c, 1002, "length");
                return;
            }
            len = 0;
            for (i = 2; i < 10; i++) {
                len = (len << 8) | p[i];
            }
            off = 10;
        }
        if (!masked) {
            /* El cliente debe enmascarar: 1002. */
            portus_ws_fail(c, 1002, "unmasked");
            return;
        }
        if (c->in.len < off + 4) {
            return;
        }
        memcpy(mask, p + off, 4);
        off += 4;
        if (len > (unsigned long long)PORTUS_WS_MAX_MESSAGE) {
            portus_ws_fail(c, 1009, "too big");
            return;
        }
        if (c->in.len < off + (size_t)len) {
            return;
        }
        payload = (unsigned char *)malloc((size_t)len + 1);
        if (payload == NULL) {
            portus_ws_fail(c, 1011, "oom");
            return;
        }
        for (i = 0; i < (int)len; i++) {
            payload[i] = (unsigned char)(p[off + (size_t)i] ^ mask[i % 4]);
        }
        payload[len] = 0;

        /* Control: se procesan en medio de un mensaje fragmentado. */
        if (opcode == 0x8) {
            int code = 1000;
            if (len == 1) {
                free(payload);
                portus_ws_fail(c, 1002, "close payload");
                return;
            }
            if (len >= 2) {
                code = ((int)payload[0] << 8) | (int)payload[1];
                if (code < 1000 || code == 1004 || code == 1005 || code == 1006 || code == 1015 ||
                    (code >= 1016 && code <= 2999)) {
                    free(payload);
                    portus_ws_fail(c, 1002, "close code");
                    return;
                }
                if (len > 2 && !portus_ws_utf8_valid(payload + 2, (size_t)len - 2)) {
                    free(payload);
                    portus_ws_fail(c, 1007, "close reason");
                    return;
                }
            }
            {
                unsigned char echo[2];
                echo[0] = (unsigned char)((code >> 8) & 0xFF);
                echo[1] = (unsigned char)(code & 0xFF);
                (void)portus_ws_send_frame(c, 0x8, echo, 2);
            }
            c->ws_closed = 1;
            c->close_after_write = 1;
            if (c->ws_cb != NULL) {
                c->ws_cb(c->ws_data, (void *)&c->response, 8, payload, (int)len);
            }
            free(payload);
            memmove(c->in.data, c->in.data + off + (size_t)len,
                    c->in.len - (off + (size_t)len));
            c->in.len -= off + (size_t)len;
            return;
        }
        if (opcode == 0x9) {
            (void)portus_ws_send_frame(c, 0xA, payload, (int)len);
            free(payload);
            memmove(c->in.data, c->in.data + off + (size_t)len,
                    c->in.len - (off + (size_t)len));
            c->in.len -= off + (size_t)len;
            continue;
        }
        if (opcode == 0xA) {
            free(payload);
            memmove(c->in.data, c->in.data + off + (size_t)len,
                    c->in.len - (off + (size_t)len));
            c->in.len -= off + (size_t)len;
            continue;
        }

        /* Datos: 0x1/0x2 abren mensaje, 0x0 continúa. */
        if (opcode == 0x1 || opcode == 0x2) {
            c->ws_msg_opcode = opcode;
            c->ws_msg_len = 0;
            c->ws_utf8_need = 0;
            c->ws_utf8_cp = 0;
            c->ws_utf8_min = 0;
        }
        if (c->ws_msg_opcode == 0x1 && !portus_ws_utf8_feed(c, payload, (size_t)len)) {
            free(payload);
            portus_ws_fail(c, 1007, "utf8");
            return;
        }
        if (!portus_ws_msg_append(c, payload, (size_t)len)) {
            free(payload);
            portus_ws_fail(c, 1009, "message too big");
            return;
        }
        free(payload);
        memmove(c->in.data, c->in.data + off + (size_t)len,
                c->in.len - (off + (size_t)len));
        c->in.len -= off + (size_t)len;
        if (!fin) {
            continue;
        }
        if (c->ws_msg_opcode == 0x1 && c->ws_utf8_need != 0) {
            portus_ws_fail(c, 1007, "utf8");
            return;
        }
        if (c->ws_cb != NULL) {
            c->ws_cb(c->ws_data, (void *)&c->response, c->ws_msg_opcode, c->ws_msg,
                     (int)c->ws_msg_len);
        }
        c->ws_msg_len = 0;
        c->ws_msg_opcode = 0;
        if (c->ws_closed || c->close_after_write) {
            return;
        }
    }
}

/* Acepta la conexión WebSocket de la petición actual. */
static PORTUS_MAYBE_UNUSED int portus_native_ws_upgrade(void *res, portus_ws_fn cb,
                                                        void *userdata) {
    static const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    struct portus_response *r = (struct portus_response *)res;
    portus_conn *c = r->conn;
    unsigned char key[128];
    unsigned char digest[20];
    char accept[40];
    char head[256];
    size_t i;
    size_t key_len = 0;
    if (c == NULL || c->fd < 0) {
        return -1;
    }
    if (!portus_conn_header_contains(c, "Upgrade", "websocket")) {
        return -1;
    }
    for (i = 0; i < c->request.header_count; i++) {
        if (c->request.headers[i].name_len == 17 &&
            strncasecmp(c->request.headers[i].name, "Sec-WebSocket-Key", 17) == 0) {
            key_len = c->request.headers[i].value_len;
            if (key_len >= sizeof(key)) {
                return -1;
            }
            memcpy(key, c->request.headers[i].value, key_len);
            break;
        }
    }
    if (key_len == 0) {
        return -1;
    }
    memcpy(key + key_len, guid, strlen(guid));
    portus_sha1(key, key_len + strlen(guid), digest);
    if (portus_base64(digest, 20, accept, sizeof(accept)) < 0) {
        return -1;
    }
    snprintf(head, sizeof(head),
             "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept);
    portus_buf_clear(&c->out);
    if (portus_buf_append_str(&c->out, head) != PORTUS_NATIVE_OK) {
        return -1;
    }
    c->ws = 1;
    c->ws_cb = cb;
    c->ws_data = userdata;
    c->ws_closed = 0;
    r->sent = 1;
    r->streaming = 0;
    /* El flush puede quedar pendiente (EAGAIN): la conexión ya está
     * actualizada y el bucle reintenta la escritura. */
    (void)portus_conn_flush(c);
    return 0;
}

static PORTUS_MAYBE_UNUSED int portus_native_ws_send(void *res, int opcode,
                                                     const unsigned char *data, int len) {
    struct portus_response *r = (struct portus_response *)res;
    return portus_ws_send_frame(r->conn, opcode, data, len);
}

/* Cierra el WebSocket. */
static PORTUS_MAYBE_UNUSED int portus_native_ws_close(void *res) {
    struct portus_response *r = (struct portus_response *)res;
    unsigned char payload[2];
    payload[0] = 0x03;
    payload[1] = 0xE8;
    return portus_ws_send_frame(r->conn, 0x8, payload, 2);
}

/* ------------------------------------------------------------------ */
/* Conexiones                                                          */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED void portus_conn_reset_request(struct portus_conn *c) {
    portus_request_clear(&c->request);
    llhttp_init(&c->parser, HTTP_REQUEST, &c->settings);
    c->parser.data = c;
    c->request_started_ms = portus_now_ms();
}

static PORTUS_MAYBE_UNUSED void portus_conn_close(struct portus_conn *c) {
    if (c->ws_msg != NULL) {
        free(c->ws_msg);
        c->ws_msg = NULL;
        c->ws_msg_len = 0;
        c->ws_msg_cap = 0;
    }
    c->ws_utf8_need = 0;
    c->ws_utf8_cp = 0;
    c->ws_utf8_min = 0;
    if (c->active && c->fd >= 0) {
        close(c->fd);
    }
    c->active = 0;
    c->fd = -1;
    c->close_after_write = 0;
    c->client_closed = 0;
    c->poll_slot = -1;
    portus_buf_clear(&c->in);
    portus_buf_clear(&c->out);
    c->out_written = 0;
    portus_request_clear(&c->request);
    portus_response_clear(&c->response);
}

static PORTUS_MAYBE_UNUSED int portus_conn_init(struct portus_conn *c, int fd,
                            const struct portus_server *srv) {
    memset(c, 0, sizeof(*c));
    c->fd = fd;
    c->active = 1;
    c->poll_slot = -1;
    c->max_header_bytes = srv->max_header_bytes;
    c->max_body_bytes = srv->max_body_bytes;
    c->last_activity_ms = portus_now_ms();
    c->request_started_ms = c->last_activity_ms;
    portus_settings_init(&c->settings);
    llhttp_init(&c->parser, HTTP_REQUEST, &c->settings);
    c->parser.data = c;
    return PORTUS_NATIVE_OK;
}

/* ------------------------------------------------------------------ */
/* Servidor                                                            */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED int portus_socket_nonblocking(int fd) {
#if defined(PORTUS_NATIVE_WINDOWS)
    (void)fd;
    return PORTUS_NATIVE_ERR;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return PORTUS_NATIVE_ERR;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 ? PORTUS_NATIVE_OK
                                                      : PORTUS_NATIVE_ERR;
#endif
}

static PORTUS_MAYBE_UNUSED int portus_socket_open(const char *host, int port, int *out_port) {
#if defined(PORTUS_NATIVE_WINDOWS)
    (void)host;
    (void)port;
    (void)out_port;
    return PORTUS_NATIVE_ERR;
#else
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *it;
    char port_text[16];
    int fd = -1;
    int last_errno = 0;
    snprintf(port_text, sizeof(port_text), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(host, port_text, &hints, &result) != 0) {
        return PORTUS_NATIVE_ERR;
    }
    for (it = result; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            last_errno = errno;
            continue;
        }
        {
            int one = 1;
            (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        }
        if (portus_socket_nonblocking(fd) != PORTUS_NATIVE_OK) {
            last_errno = errno;
            close(fd);
            fd = -1;
            continue;
        }
        if (bind(fd, it->ai_addr, it->ai_addrlen) != 0) {
            last_errno = errno;
            close(fd);
            fd = -1;
            continue;
        }
        if (listen(fd, 128) != 0) {
            last_errno = errno;
            close(fd);
            fd = -1;
            continue;
        }
        {
            struct sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);
            if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) == 0) {
                if (addr.ss_family == AF_INET) {
                    *out_port =
                        (int)ntohs(((struct sockaddr_in *)&addr)->sin_port);
                } else if (addr.ss_family == AF_INET6) {
                    *out_port =
                        (int)ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
                }
            }
        }
        break;
    }
    freeaddrinfo(result);
    errno = last_errno;
    return fd;
#endif
}

/* Escribe una respuesta de error mínima y marca la conexión para cerrar. */
static PORTUS_MAYBE_UNUSED void portus_fail(portus_conn *c, int status, const char *message,
                        portus_buffer *scratch) {
    portus_response_clear(&c->response);
    c->response.status = status;
    (void)portus_buf_append_str(&c->response.body, message);
    portus_response_set_header(&c->response, "Content-Type", 12,
                               "text/plain; charset=utf-8", 25);
    if (portus_response_serialize(&c->response, &c->request, 0, scratch) ==
        PORTUS_NATIVE_OK) {
        (void)portus_buf_append(&c->out, scratch->data, scratch->len);
    }
    c->close_after_write = 1;
}

/* Llama al handler y encola su respuesta. */
static PORTUS_MAYBE_UNUSED void portus_dispatch(portus_conn *c, portus_handler_fn handler,
                            void *userdata, int keep_alive,
                            portus_buffer *scratch) {
    portus_response_clear(&c->response);
    c->response.status = 200;
    c->response.conn = c;
    handler(userdata, (void *)&c->request, (void *)&c->response);
    if (c->ws) {
        /* El upgrade ya escribió el 101 y cambió el protocolo. */
        return;
    }
    if (c->response.streaming) {
        if (!c->response.stream_ended) {
            c->close_after_write = 1;
        }
        return;
    }
    if (!c->response.sent) {
        portus_fail(c, 500, "el handler no respondió", scratch);
        return;
    }
    if (!keep_alive) {
        c->close_after_write = 1;
    }
    if (portus_response_serialize(&c->response, &c->request, keep_alive,
                                  scratch) != PORTUS_NATIVE_OK) {
        c->close_after_write = 1;
        return;
    }
    (void)portus_buf_append(&c->out, scratch->data, scratch->len);
}

/* Procesa los bytes disponibles en `c->in`. */
static PORTUS_MAYBE_UNUSED void portus_conn_process(portus_conn *c, int keep_alive_enabled,
                                portus_handler_fn handler, void *userdata,
                                portus_buffer *scratch) {
    size_t off = 0;
    if (c->ws) {
        portus_ws_consume(c);
        return;
    }
    while (off < c->in.len) {
        llhttp_errno_t err;
        size_t consumed;
        err = llhttp_execute(&c->parser, c->in.data + off, c->in.len - off);
        if (err == HPE_PAUSED) {
            consumed = (size_t)(c->parser.error_pos - (c->in.data + off));
            off += consumed;
            portus_dispatch(c, handler, userdata,
                            c->request.keep_alive && keep_alive_enabled,
                            scratch);
            if (c->close_after_write) {
                break;
            }
            portus_conn_reset_request(c);
            if (c->ws) {
                if (off < c->in.len) {
                    memmove(c->in.data, c->in.data + off, c->in.len - off);
                    c->in.len -= off;
                } else {
                    c->in.len = 0;
                }
                portus_ws_consume(c);
                return;
            }
            continue;
        }
        if (err != HPE_OK) {
            if (c->request.oversized == 2) {
                portus_fail(c, 413, "cuerpo demasiado grande", scratch);
            } else if (c->request.oversized == 1) {
                portus_fail(c, 431, "cabeceras demasiado grandes", scratch);
            } else {
                portus_fail(c, 400, "petición inválida", scratch);
            }
            break;
        }
        /* llhttp consumió todo: la petición sigue incompleta. */
        off = c->in.len;
    }
    if (off > 0 && off < c->in.len) {
        memmove(c->in.data, c->in.data + off, c->in.len - off);
    }
    if (off >= c->in.len) {
        c->in.len = 0;
    } else {
        c->in.len -= off;
    }
}

/* Drena el socket de entrada. Devuelve 0 si el cliente cerró. */
static PORTUS_MAYBE_UNUSED int portus_conn_drain(portus_conn *c) {
    char chunk[PORTUS_READ_CHUNK];
    for (;;) {
        ssize_t n = recv(c->fd, chunk, sizeof(chunk), 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
#if defined(PORTUS_NATIVE_WINDOWS)
            return 1;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1;
            }
            if (errno == EINTR) {
                continue;
            }
            return 0;
#endif
        }
        if (portus_buf_append(&c->in, chunk, (size_t)n) != PORTUS_NATIVE_OK) {
            return 0;
        }
        c->last_activity_ms = portus_now_ms();
        if (c->in.len >
            (size_t)(c->max_header_bytes) + (size_t)(c->max_body_bytes) +
                (size_t)PORTUS_READ_CHUNK) {
            c->request.oversized = 2;
            return 1;
        }
    }
}

/* Vuelca la salida pendiente. Devuelve 0 si la conexión debe cerrarse. */
static PORTUS_MAYBE_UNUSED int portus_conn_flush(portus_conn *c) {
    while (c->out_written < c->out.len) {
        ssize_t n = send(c->fd, c->out.data + c->out_written,
                         c->out.len - c->out_written, 0);
        if (n < 0) {
#if defined(PORTUS_NATIVE_WINDOWS)
            return 0;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1;
            }
            if (errno == EINTR) {
                continue;
            }
            return 0;
#endif
        }
        if (n == 0) {
            return 0;
        }
        c->out_written += (size_t)n;
    }
    portus_buf_clear(&c->out);
    c->out_written = 0;
    if (c->close_after_write || c->client_closed) {
        return 0;
    }
    return 1;
}

static PORTUS_MAYBE_UNUSED int portus_accept_ready(portus_server_t *srv) {
#if defined(PORTUS_NATIVE_WINDOWS)
    return PORTUS_NATIVE_ERR;
#else
    for (;;) {
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        int fd = accept(srv->listen_fd, (struct sockaddr *)&addr, &addr_len);
        int i;
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return PORTUS_NATIVE_OK;
            }
            if (errno == EINTR) {
                continue;
            }
            return PORTUS_NATIVE_ERR;
        }
        if (portus_socket_nonblocking(fd) != PORTUS_NATIVE_OK) {
            close(fd);
            continue;
        }
        for (i = 0; i < srv->max_connections; i++) {
            if (!srv->connections[i].active) {
                (void)portus_conn_init(&srv->connections[i], fd, srv);
                break;
            }
        }
        if (i == srv->max_connections) {
            close(fd);
        }
    }
#endif
}

static PORTUS_MAYBE_UNUSED const char *portus_last_error_text = "sin error";

static PORTUS_MAYBE_UNUSED const char *portus_native_last_error(void) {
    return portus_last_error_text;
}

/* Bucle principal. Bloquea hasta que el proceso muera. */
static PORTUS_MAYBE_UNUSED int portus_native_serve(const unsigned char *host, int port, int max_connections,
                        int max_header_bytes, int max_body_bytes,
                        int idle_timeout_ms, int keep_alive,
                        portus_handler_fn handler, void *userdata,
                        int *out_port) {
#if defined(PORTUS_NATIVE_WINDOWS)
    (void)host;
    (void)port;
    (void)max_connections;
    (void)max_header_bytes;
    (void)max_body_bytes;
    (void)idle_timeout_ms;
    (void)keep_alive;
    (void)handler;
    (void)userdata;
    (void)out_port;
    portus_last_error_text = "portus no soporta Windows todavía";
    return PORTUS_ERR_PLATFORM;
#else
    portus_server_t srv;
    struct pollfd *fds = NULL;
    portus_buffer scratch;
    const char *host_c = (const char *)host;
    int i;
    int slot;

    if (max_connections <= 0 || max_connections > 100000) {
        max_connections = 1024;
    }
    if (max_header_bytes <= 0) {
        max_header_bytes = PORTUS_DEFAULT_MAX_HEADER_BYTES;
    }
    if (max_body_bytes <= 0) {
        max_body_bytes = PORTUS_DEFAULT_MAX_BODY_BYTES;
    }
    if (idle_timeout_ms <= 0) {
        idle_timeout_ms = PORTUS_DEFAULT_IDLE_TIMEOUT_MS;
    }
    memset(&srv, 0, sizeof(srv));
    memset(&scratch, 0, sizeof(scratch));
    srv.max_connections = max_connections;
    srv.max_header_bytes = max_header_bytes;
    srv.max_body_bytes = max_body_bytes;
    srv.idle_timeout_ms = idle_timeout_ms;
    srv.request_timeout_ms = PORTUS_DEFAULT_REQUEST_TIMEOUT_MS;
    srv.keep_alive = keep_alive;
    srv.started_ms = portus_now_ms();
    srv.connections = (struct portus_conn *)calloc(
        (size_t)max_connections, sizeof(struct portus_conn));
    fds = (struct pollfd *)calloc((size_t)max_connections + 1,
                                  sizeof(struct pollfd));
    if (srv.connections == NULL || fds == NULL) {
        free(srv.connections);
        free(fds);
        portus_last_error_text = "sin memoria";
        return PORTUS_ERR_ALLOC;
    }
    {
        int chosen = port;
        srv.listen_fd = portus_socket_open(host_c, port, &chosen);
        if (srv.listen_fd < 0) {
            free(srv.connections);
            free(fds);
            portus_last_error_text = "no se pudo abrir el socket de escucha";
            return PORTUS_ERR_LISTEN;
        }
        if (out_port != NULL) {
            *out_port = chosen;
        }
    }
    (void)signal(SIGPIPE, SIG_IGN);

    while (1) {
        int fd_count = 0;
        int ready;
        unsigned long long now;
        fds[fd_count].fd = srv.listen_fd;
        fds[fd_count].events = POLLIN;
        fds[fd_count].revents = 0;
        fd_count++;
        for (i = 0; i < srv.max_connections; i++) {
            struct portus_conn *c = &srv.connections[i];
            if (!c->active) {
                c->poll_slot = -1;
                continue;
            }
            c->poll_slot = fd_count;
            fds[fd_count].fd = c->fd;
            fds[fd_count].events = POLLIN;
            if (c->out.len > c->out_written) {
                fds[fd_count].events |= POLLOUT;
            }
            fds[fd_count].revents = 0;
            fd_count++;
        }
        ready = poll(fds, (nfds_t)fd_count, 500);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            portus_last_error_text = "poll falló";
            break;
        }
        if (fds[0].revents & POLLIN) {
            (void)portus_accept_ready(&srv);
        }
        now = portus_now_ms();
        for (i = 0; i < srv.max_connections; i++) {
            struct portus_conn *c = &srv.connections[i];
            short revents;
            if (!c->active) {
                continue;
            }
            revents = c->poll_slot >= 0 ? fds[c->poll_slot].revents : 0;
            if (revents & (POLLERR | POLLNVAL)) {
                portus_conn_close(c);
                continue;
            }
            if (revents & POLLIN) {
                if (!portus_conn_drain(c)) {
                    c->client_closed = 1;
                }
                if (c->request.oversized == 2 && c->in.len >
                        (size_t)(c->max_header_bytes) +
                            (size_t)(c->max_body_bytes)) {
                    portus_fail(c, 413, "cuerpo demasiado grande", &scratch);
                } else {
                    portus_conn_process(c, srv.keep_alive, handler, userdata,
                                        &scratch);
                }
            }
            if (c->active && (c->out.len > c->out_written ||
                              c->close_after_write)) {
                if (!portus_conn_flush(c)) {
                    portus_conn_close(c);
                    continue;
                }
            }
            if (!c->active) {
                continue;
            }
            if (c->client_closed) {
                portus_conn_close(c);
                continue;
            }
            if (c->in.len == 0 &&
                c->last_activity_ms + (unsigned long long)srv.idle_timeout_ms <
                    now) {
                portus_conn_close(c);
                continue;
            }
            if (c->in.len > 0 && c->request_started_ms +
                                         (unsigned long long)
                                             srv.request_timeout_ms <
                                     now) {
                portus_fail(c, 408, "tiempo de espera agotado", &scratch);
                (void)portus_conn_flush(c);
                portus_conn_close(c);
                continue;
            }
        }
    }
    for (i = 0; i < srv.max_connections; i++) {
        struct portus_conn *c = &srv.connections[i];
        if (c->active) {
            portus_conn_close(c);
        }
        portus_buf_free(&c->in);
        portus_buf_free(&c->out);
        portus_buf_free(&c->request.url);
        portus_buf_free(&c->request.field);
        portus_buf_free(&c->request.value);
        portus_buf_free(&c->response.body);
        free(c->request.body);
        portus_headers_free(&c->request.headers, c->request.header_count);
        portus_headers_free(&c->response.headers, c->response.header_count);
    }
    close(srv.listen_fd);
    free(srv.connections);
    free(fds);
    portus_buf_free(&scratch);
    (void)slot;
    return PORTUS_NATIVE_OK;
#endif
}

/* ------------------------------------------------------------------ */
/* Accesores para PenguScript                                          */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED const char *portus_native_request_method_name(const struct portus_request *r,
                                              int *len) {
    const char *name = llhttp_method_name((llhttp_method_t)r->method);
    if (len != NULL) {
        *len = (int)strlen(name);
    }
    return name;
}

static PORTUS_MAYBE_UNUSED const char *portus_native_request_path(const struct portus_request *r,
                                       int *len) {
    if (len != NULL) {
        *len = (int)r->path_len;
    }
    return r->path != NULL ? r->path : "";
}

static PORTUS_MAYBE_UNUSED const char *portus_native_request_query(const struct portus_request *r,
                                        int *len) {
    if (len != NULL) {
        *len = (int)r->query_len;
    }
    return r->query != NULL ? r->query : "";
}

static PORTUS_MAYBE_UNUSED const char *portus_native_request_body(const struct portus_request *r,
                                       int *len) {
    if (len != NULL) {
        *len = (int)r->body_len;
    }
    return r->body;
}

static PORTUS_MAYBE_UNUSED int portus_native_request_header_count(const struct portus_request *r) {
    return (int)r->header_count;
}

static PORTUS_MAYBE_UNUSED const char *portus_native_request_header_name(const struct portus_request *r,
                                              int index, int *len) {
    if (index < 0 || (size_t)index >= r->header_count) {
        if (len != NULL) {
            *len = 0;
        }
        return "";
    }
    if (len != NULL) {
        *len = (int)r->headers[index].name_len;
    }
    return r->headers[index].name;
}

static PORTUS_MAYBE_UNUSED const char *portus_native_request_header_value(const struct portus_request *r,
                                               int index, int *len) {
    if (index < 0 || (size_t)index >= r->header_count) {
        if (len != NULL) {
            *len = 0;
        }
        return "";
    }
    if (len != NULL) {
        *len = (int)r->headers[index].value_len;
    }
    return r->headers[index].value;
}

static PORTUS_MAYBE_UNUSED int portus_native_request_keep_alive(const struct portus_request *r) {
    return r->keep_alive;
}

static PORTUS_MAYBE_UNUSED int portus_native_request_http_minor(const struct portus_request *r) {
    return r->http_minor;
}

static PORTUS_MAYBE_UNUSED int portus_native_request_is_oversized(const struct portus_request *r) {
    return r->oversized;
}

static PORTUS_MAYBE_UNUSED void portus_native_response_write_bytes(struct portus_response *res,
                                        const unsigned char *data, int len) {
    if (data != NULL && len > 0) {
        (void)portus_buf_append(&res->body, data, (size_t)len);
    }
}

static PORTUS_MAYBE_UNUSED void portus_native_response_header_bytes(struct portus_response *res,
                                         const unsigned char *name,
                                         int name_len,
                                         const unsigned char *value,
                                         int value_len) {
    portus_response_set_header(res, (const char *)name, (size_t)name_len,
                               (const char *)value, (size_t)value_len);
}


/* Copia el cuerpo de la respuesta a `out`; devuelve cuántos bytes caben. */
static PORTUS_MAYBE_UNUSED const unsigned char *portus_native_response_body(const void *res, int *out_len) {
    const struct portus_response *r = (const struct portus_response *)res;
    if (out_len != NULL) {
        *out_len = (int)r->body.len;
    }
    return (const unsigned char *)r->body.data;
}

/* Reemplaza el cuerpo de la respuesta (copia `len` bytes). */
static PORTUS_MAYBE_UNUSED void portus_native_response_set_body(void *res, const unsigned char *data, int len) {
    struct portus_response *r = (struct portus_response *)res;
    r->body.len = 0;
    if (len > 0 && data != NULL) {
        portus_buf_append(&r->body, data, (size_t)len);
    }
}

/* Valor de una cabecera de la respuesta, o NULL. */
static PORTUS_MAYBE_UNUSED const unsigned char *portus_native_response_header_value(const void *res,
                                                                                  const unsigned char *name,
                                                                                  int name_len,
                                                                                  int *out_len) {
    const struct portus_response *r = (const struct portus_response *)res;
    size_t i;
    for (i = 0; i < r->header_count; i++) {
        if ((int)r->headers[i].name_len == name_len &&
            memcmp(r->headers[i].name, name, (size_t)name_len) == 0) {
            if (out_len != NULL) {
                *out_len = (int)r->headers[i].value_len;
            }
            return (const unsigned char *)r->headers[i].value;
        }
    }
    return NULL;
}

static PORTUS_MAYBE_UNUSED void portus_native_response_mark_sent(struct portus_response *res) {
    res->sent = 1;
}

static PORTUS_MAYBE_UNUSED int portus_native_response_status(const struct portus_response *res) {
    return res->status;
}


/* ------------------------------------------------------------------ */
/* Utilidades para middlewares                                         */
/* ------------------------------------------------------------------ */

/* Escribe `bytes` bytes aleatorios como hexadecimal (2*bytes chars) más el
 * terminador en `out`. `out` debe tener espacio para 2*bytes+1.
 * Devuelve 0 si pudo, -1 si no hay fuente de aleatoriedad. */
static PORTUS_MAYBE_UNUSED int portus_native_random_hex(unsigned char *out, int bytes) {
    unsigned char raw[64];
    int i;
    int fd;
    if (bytes <= 0 || bytes > 64 || out == NULL) {
        return -1;
    }
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    if (read(fd, raw, (size_t)bytes) != bytes) {
        close(fd);
        return -1;
    }
    close(fd);
    for (i = 0; i < bytes; i++) {
        static const char hex[] = "0123456789abcdef";
        out[i * 2] = (unsigned char)hex[(raw[i] >> 4) & 0x0F];
        out[i * 2 + 1] = (unsigned char)hex[raw[i] & 0x0F];
    }
    out[bytes * 2] = 0;
    return 0;
}


/* Devuelve un identificador de petición de 32 caracteres hexadecimales en un
 * buffer estático (se sobrescribe en cada llamada: cópialo antes de la
 * siguiente). null si no hay fuente de aleatoriedad. Un solo hilo a la vez. */
static PORTUS_MAYBE_UNUSED const char *portus_native_request_id(void) {
    static char id[33];
    if (portus_native_random_hex((unsigned char *)id, 16) != 0) {
        return NULL;
    }
    return id;
}


/* Tamaño y mtime (segundos Unix) de un fichero regular.
 * Devuelve 0 si existe, -1 si no. `path` no necesita terminador. */
static PORTUS_MAYBE_UNUSED int portus_native_file_info(const unsigned char *path,
                                                     int path_len,
                                                     int64_t *out_size,
                                                     int64_t *out_mtime) {
    char buf[4096];
    struct stat st;
    if (path == NULL || path_len <= 0 || path_len >= (int)sizeof(buf)) {
        return -1;
    }
    memcpy(buf, path, (size_t)path_len);
    buf[path_len] = 0;
    if (stat(buf, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    if (out_size != NULL) {
        *out_size = (int64_t)st.st_size;
    }
    if (out_mtime != NULL) {
        *out_mtime = (int64_t)st.st_mtime;
    }
    return 0;
}

/* Milisegundos monótonos desde un origen arbitrario. */
static PORTUS_MAYBE_UNUSED long long portus_native_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

static PORTUS_MAYBE_UNUSED int portus_native_ping(int n) {
    return n + 1;
}

#endif /* PORTUS_NATIVE_H */
