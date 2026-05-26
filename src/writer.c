/**
 * EDN.C - Writer (serializer)
 */

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"
#include "ryu/ryu.h"

typedef struct {
    edn_writer_callback_fn cb;
    void* ctx;
    int err; /* 0 = ok; <0 propagated to caller */
} emit_ctx_t;

static int emit(emit_ctx_t* e, const char* buf, size_t len) {
    if (e->err != 0) {
        return e->err;
    }
    if (len == 0) {
        return 0;
    }
    int r = e->cb(buf, len, e->ctx);
    if (r != 0) {
        e->err = (r < 0) ? r : -r;
    }
    return e->err;
}

static int emit_cstr(emit_ctx_t* e, const char* s) {
    return emit(e, s, strlen(s));
}

static int emit_value(emit_ctx_t* e, const edn_value_t* v);

/* --- scalar emitters --- */

static int emit_int(emit_ctx_t* e, int64_t n) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%" PRId64, n);
    if (len < 0) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    return emit(e, buf, (size_t) len);
}

/* Format a Ryu-shortest decimal per ECMA-262 §7.1.12.1 (Number toString),
 * with one EDN-specific tweak: integer-valued floats get a trailing ".0" so
 * they round-trip back to EDN_TYPE_FLOAT instead of EDN_TYPE_INT.
 */
static int emit_float(emit_ctx_t* e, double d) {
    if (isnan(d)) {
        return emit_cstr(e, "##NaN");
    }
    if (isinf(d)) {
        return emit_cstr(e, d < 0 ? "##-Inf" : "##Inf");
    }
    if (d == 0.0) {
        return emit_cstr(e, signbit(d) ? "-0.0" : "0.0");
    }

    char ryu[32];
    int ryu_len = d2s_buffered_n(d, ryu);
    ryu[ryu_len] = '\0';

    const char* p = ryu;
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }

    char digits[32];
    int k = 0;
    while (*p && *p != 'E') {
        if (*p != '.') {
            digits[k++] = *p;
        }
        p++;
    }
    p++; /* past 'E' */
    int ryu_exp = atoi(p);
    int n = ryu_exp + 1; /* digits before decimal point in normalized form */

    char out[48];
    char* o = out;
    if (negative)
        *o++ = '-';

    if (n >= 1 && n <= 21) {
        if (k <= n) {
            memcpy(o, digits, (size_t) k);
            o += k;
            for (int i = 0; i < n - k; i++)
                *o++ = '0';
            *o++ = '.';
            *o++ = '0';
        } else {
            memcpy(o, digits, (size_t) n);
            o += n;
            *o++ = '.';
            memcpy(o, digits + n, (size_t) (k - n));
            o += k - n;
        }
    } else if (n > -6 && n <= 0) {
        *o++ = '0';
        *o++ = '.';
        for (int i = 0; i < -n; i++)
            *o++ = '0';
        memcpy(o, digits, (size_t) k);
        o += k;
    } else {
        if (k == 1) {
            *o++ = digits[0];
        } else {
            *o++ = digits[0];
            *o++ = '.';
            memcpy(o, digits + 1, (size_t) (k - 1));
            o += k - 1;
        }
        *o++ = 'E';
        int written = snprintf(o, sizeof(out) - (size_t) (o - out), "%d", n - 1);
        if (written < 0) {
            e->err = -EDN_ERROR_OUT_OF_MEMORY;
            return e->err;
        }
        o += written;
    }

    return emit(e, out, (size_t) (o - out));
}

static int emit_bigint(emit_ctx_t* e, const edn_value_t* v) {
    size_t len;
    bool neg;
    uint8_t radix;
    const char* digits = edn_bigint_get(v, &len, &neg, &radix);
    if (!digits) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }

    if (neg) {
        if (emit(e, "-", 1) != 0)
            return e->err;
    }

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (radix != 10) {
        char prefix[16];
        if (radix == 16) {
            if (emit(e, "0x", 2) != 0)
                return e->err;
        } else if (radix == 8) {
            if (emit(e, "0", 1) != 0)
                return e->err;
        } else if (radix == 2) {
            /* 2rNN form is unambiguous for binary. */
            int pl = snprintf(prefix, sizeof(prefix), "%ur", (unsigned) radix);
            if (emit(e, prefix, (size_t) pl) != 0)
                return e->err;
        } else {
            int pl = snprintf(prefix, sizeof(prefix), "%ur", (unsigned) radix);
            if (emit(e, prefix, (size_t) pl) != 0)
                return e->err;
        }
    }
#else
    (void) radix;
#endif

    if (emit(e, digits, len) != 0)
        return e->err;
    return emit(e, "N", 1);
}

static int emit_bigdec(emit_ctx_t* e, const edn_value_t* v) {
    size_t len;
    bool neg;
    const char* dec = edn_bigdec_get(v, &len, &neg);
    if (!dec) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    if (neg) {
        if (emit(e, "-", 1) != 0)
            return e->err;
    }
    if (emit(e, dec, len) != 0)
        return e->err;
    return emit(e, "M", 1);
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
static int emit_ratio(emit_ctx_t* e, const edn_value_t* v) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%" PRId64 "/%" PRId64, v->as.ratio.numerator,
                       v->as.ratio.denominator);
    if (len < 0) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    return emit(e, buf, (size_t) len);
}
#endif

static int emit_character(emit_ctx_t* e, uint32_t cp) {
    if (emit(e, "\\", 1) != 0)
        return e->err;

    switch (cp) {
        case 0x0A:
            return emit_cstr(e, "newline");
        case 0x09:
            return emit_cstr(e, "tab");
        case 0x20:
            return emit_cstr(e, "space");
        case 0x0D:
            return emit_cstr(e, "return");
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        case 0x0C:
            return emit_cstr(e, "formfeed");
        case 0x08:
            return emit_cstr(e, "backspace");
#endif
        default:
            break;
    }

    if (cp >= 0x21 && cp <= 0x7E) {
        char c = (char) cp;
        return emit(e, &c, 1);
    }

    char buf[16];
    int len = snprintf(buf, sizeof(buf), "u%04X", cp);
    if (len < 0) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    return emit(e, buf, (size_t) len);
}

static int emit_string(emit_ctx_t* e, const edn_value_t* v) {
    /* Strings store raw source bytes between the quote characters. The source
     * is already valid EDN (any required escapes are present in the bytes),
     * so verbatim emission between quotes round-trips. */
    if (emit(e, "\"", 1) != 0)
        return e->err;
    size_t len = edn_string_get_length(v);
    if (emit(e, v->as.string.data, len) != 0)
        return e->err;
    return emit(e, "\"", 1);
}

static int emit_symbol(emit_ctx_t* e, const edn_value_t* v) {
    if (v->as.symbol.ns_length > 0) {
        if (emit(e, v->as.symbol.namespace, v->as.symbol.ns_length) != 0)
            return e->err;
        if (emit(e, "/", 1) != 0)
            return e->err;
    }
    return emit(e, v->as.symbol.name, v->as.symbol.name_length);
}

static int emit_keyword(emit_ctx_t* e, const edn_value_t* v) {
    if (emit(e, ":", 1) != 0)
        return e->err;
    if (v->as.keyword.ns_length > 0) {
        if (emit(e, v->as.keyword.namespace, v->as.keyword.ns_length) != 0)
            return e->err;
        if (emit(e, "/", 1) != 0)
            return e->err;
    }
    return emit(e, v->as.keyword.name, v->as.keyword.name_length);
}

static int emit_sequence(emit_ctx_t* e, edn_value_t* const* elements, size_t count, char open,
                         char close) {
    if (emit(e, &open, 1) != 0)
        return e->err;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (emit(e, " ", 1) != 0)
                return e->err;
        }
        if (emit_value(e, elements[i]) != 0)
            return e->err;
    }
    return emit(e, &close, 1);
}

static int emit_set(emit_ctx_t* e, edn_value_t* const* elements, size_t count) {
    if (emit(e, "#{", 2) != 0)
        return e->err;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (emit(e, " ", 1) != 0)
                return e->err;
        }
        if (emit_value(e, elements[i]) != 0)
            return e->err;
    }
    return emit(e, "}", 1);
}

static int emit_map(emit_ctx_t* e, edn_value_t* const* keys, edn_value_t* const* values,
                    size_t count) {
    if (emit(e, "{", 1) != 0)
        return e->err;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (emit(e, ", ", 2) != 0)
                return e->err;
        }
        if (emit_value(e, keys[i]) != 0)
            return e->err;
        if (emit(e, " ", 1) != 0)
            return e->err;
        if (emit_value(e, values[i]) != 0)
            return e->err;
    }
    return emit(e, "}", 1);
}

static int emit_tagged(emit_ctx_t* e, const edn_value_t* v) {
    if (emit(e, "#", 1) != 0)
        return e->err;
    if (emit(e, v->as.tagged.tag, v->as.tagged.tag_length) != 0)
        return e->err;
    if (emit(e, " ", 1) != 0)
        return e->err;
    return emit_value(e, v->as.tagged.value);
}

static int emit_value(emit_ctx_t* e, const edn_value_t* v) {
    if (e->err != 0)
        return e->err;
    if (v == NULL) {
        return emit_cstr(e, "nil");
    }

    switch (v->type) {
        case EDN_TYPE_NIL:
            return emit_cstr(e, "nil");
        case EDN_TYPE_BOOL:
            return emit_cstr(e, v->as.boolean ? "true" : "false");
        case EDN_TYPE_INT:
            return emit_int(e, v->as.integer);
        case EDN_TYPE_BIGINT:
            return emit_bigint(e, v);
        case EDN_TYPE_FLOAT:
            return emit_float(e, v->as.floating);
        case EDN_TYPE_BIGDEC:
            return emit_bigdec(e, v);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        case EDN_TYPE_RATIO:
            return emit_ratio(e, v);
#endif
        case EDN_TYPE_CHARACTER:
            return emit_character(e, v->as.character);
        case EDN_TYPE_STRING:
            return emit_string(e, v);
        case EDN_TYPE_SYMBOL:
            return emit_symbol(e, v);
        case EDN_TYPE_KEYWORD:
            return emit_keyword(e, v);
        case EDN_TYPE_LIST:
            return emit_sequence(e, v->as.list.elements, v->as.list.count, '(', ')');
        case EDN_TYPE_VECTOR:
            return emit_sequence(e, v->as.vector.elements, v->as.vector.count, '[', ']');
        case EDN_TYPE_SET:
            return emit_set(e, v->as.set.elements, v->as.set.count);
        case EDN_TYPE_MAP:
            return emit_map(e, v->as.map.keys, v->as.map.values, v->as.map.count);
        case EDN_TYPE_TAGGED:
            return emit_tagged(e, v);
        case EDN_TYPE_EXTERNAL:
            e->err = -EDN_ERROR_UNSUPPORTED_TYPE;
            return e->err;
        default:
            e->err = -EDN_ERROR_UNSUPPORTED_TYPE;
            return e->err;
    }
}

static int validate_options(const edn_write_options_t* opts) {
    if (opts == NULL) {
        return 0;
    }
    if (opts->struct_size < sizeof(edn_write_options_t)) {
        if (opts->struct_size != 0) {
            return -EDN_ERROR_UNSUPPORTED_TYPE;
        }
        return 0;
    }
    if (opts->indent != 0)
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    if (opts->sort_keys)
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    if (opts->emit_metadata)
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    if (opts->escape_unicode)
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    return 0;
}

static bool opt_newline_at_end(const edn_write_options_t* opts) {
    if (opts == NULL || opts->struct_size == 0)
        return false;
    return opts->newline_at_end;
}

/* ========================================================================
 * Public streaming primitive
 * ======================================================================== */

int edn_write_stream(const edn_value_t* value, edn_writer_callback_fn cb, void* ctx,
                     const edn_write_options_t* options) {
    if (cb == NULL) {
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    }
    int v = validate_options(options);
    if (v != 0)
        return v;

    emit_ctx_t e = {cb, ctx, 0};
    emit_value(&e, value);
    if (e.err != 0)
        return e.err;

    if (opt_newline_at_end(options)) {
        emit(&e, "\n", 1);
        if (e.err != 0)
            return e.err;
    }
    return 0;
}

/* ========================================================================
 * Heap-string wrapper
 * ======================================================================== */

typedef struct {
    char* buf;
    size_t len;
    size_t cap;
    bool failed;
} heap_ctx_t;

static int heap_cb(const char* data, size_t n, void* ctx) {
    heap_ctx_t* h = ctx;
    if (h->failed)
        return -1;

    if (h->len + n + 1 > h->cap) {
        size_t new_cap = h->cap ? h->cap : 64;
        while (new_cap < h->len + n + 1) {
            size_t doubled = new_cap * 2;
            if (doubled < new_cap) {
                h->failed = true;
                return -1;
            }
            new_cap = doubled;
        }
        char* nb = realloc(h->buf, new_cap);
        if (!nb) {
            h->failed = true;
            return -1;
        }
        h->buf = nb;
        h->cap = new_cap;
    }
    memcpy(h->buf + h->len, data, n);
    h->len += n;
    return 0;
}

char* edn_write_string(const edn_value_t* value, const edn_write_options_t* options,
                       size_t* out_len) {
    heap_ctx_t h = {NULL, 0, 0, false};
    int r = edn_write_stream(value, heap_cb, &h, options);
    if (r != 0 || h.failed) {
        free(h.buf);
        if (out_len)
            *out_len = 0;
        return NULL;
    }
    /* Always null-terminate. */
    if (h.len + 1 > h.cap) {
        char* nb = realloc(h.buf, h.len + 1);
        if (!nb) {
            free(h.buf);
            if (out_len)
                *out_len = 0;
            return NULL;
        }
        h.buf = nb;
    }
    /* Edge case: empty value */
    if (h.buf == NULL) {
        h.buf = malloc(1);
        if (!h.buf) {
            if (out_len)
                *out_len = 0;
            return NULL;
        }
    }
    h.buf[h.len] = '\0';
    if (out_len)
        *out_len = h.len;
    return h.buf;
}

/* ========================================================================
 * User-buffer wrapper (snprintf semantics)
 * ======================================================================== */

typedef struct {
    char* buf;
    size_t cap;    /* total capacity available (including space for null) */
    size_t pos;    /* bytes actually written (excluding null) */
    size_t needed; /* total bytes that would have been written */
} buffer_ctx_t;

static int buffer_cb(const char* data, size_t n, void* ctx) {
    buffer_ctx_t* b = ctx;
    b->needed += n;
    if (b->cap == 0) {
        return 0;
    }
    size_t room = (b->pos + 1 < b->cap) ? (b->cap - 1 - b->pos) : 0;
    size_t copy = (n < room) ? n : room;
    if (copy > 0) {
        memcpy(b->buf + b->pos, data, copy);
        b->pos += copy;
    }
    return 0;
}

size_t edn_write_buffer(const edn_value_t* value, char* buf, size_t cap,
                        const edn_write_options_t* options) {
    if (cap > 0 && buf == NULL) {
        return (size_t) -1;
    }
    buffer_ctx_t b = {buf, cap, 0, 0};
    int r = edn_write_stream(value, buffer_cb, &b, options);
    if (r != 0) {
        return (size_t) -1;
    }
    if (cap > 0) {
        buf[b.pos] = '\0';
    }
    return b.needed;
}

/* ========================================================================
 * FILE* wrapper
 * ======================================================================== */

static int file_cb(const char* data, size_t n, void* ctx) {
    FILE* fp = ctx;
    if (n == 0)
        return 0;
    size_t w = fwrite(data, 1, n, fp);
    return (w == n) ? 0 : -1;
}

int edn_write_file(const edn_value_t* value, FILE* fp, const edn_write_options_t* options) {
    if (fp == NULL) {
        return -EDN_ERROR_UNSUPPORTED_TYPE;
    }
    return edn_write_stream(value, file_cb, fp, options);
}

/* ========================================================================
 * Convenience
 * ======================================================================== */

char* edn_write(const edn_value_t* value) {
    return edn_write_string(value, NULL, NULL);
}

/* ========================================================================
 * Writer registry (scaffold; not yet used by emitter)
 * ======================================================================== */

struct edn_writer_registry {
    int placeholder;
};

edn_writer_registry_t* edn_writer_registry_create(void) {
    edn_writer_registry_t* r = calloc(1, sizeof(*r));
    return r;
}

void edn_writer_registry_destroy(edn_writer_registry_t* r) {
    free(r);
}
