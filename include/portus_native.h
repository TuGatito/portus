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
#include <sys/socket.h>
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
};

/* Los punteros se declaran como `void *` a propósito: así el binding de
 * PenguScript puede pasar un weave con `userdata as ref to void, request as
 * ref to void, response as ref to void` sin conversiones de tipo función. */
typedef void (*portus_handler_fn)(void *userdata, void *request, void *response);

/* Nombres estables para el binding (PenguScript no ve `struct portus_*`). */
typedef struct portus_request portus_request_t;
typedef struct portus_response portus_response_t;

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
} portus_conn;

typedef struct portus_server {
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
/* Conexiones                                                          */
/* ------------------------------------------------------------------ */

static PORTUS_MAYBE_UNUSED void portus_conn_reset_request(struct portus_conn *c) {
    portus_request_clear(&c->request);
    llhttp_init(&c->parser, HTTP_REQUEST, &c->settings);
    c->parser.data = c;
    c->request_started_ms = portus_now_ms();
}

static PORTUS_MAYBE_UNUSED void portus_conn_close(struct portus_conn *c) {
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
    handler(userdata, (void *)&c->request, (void *)&c->response);
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

static PORTUS_MAYBE_UNUSED int portus_accept_ready(portus_server *srv) {
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
    portus_server srv;
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

static PORTUS_MAYBE_UNUSED void portus_native_response_mark_sent(struct portus_response *res) {
    res->sent = 1;
}

static PORTUS_MAYBE_UNUSED int portus_native_response_status(const struct portus_response *res) {
    return res->status;
}

static PORTUS_MAYBE_UNUSED int portus_native_ping(int n) {
    return n + 1;
}

#endif /* PORTUS_NATIVE_H */
