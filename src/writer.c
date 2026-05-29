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
    int err;             /* 0 = ok; <0 propagated to caller */
    bool sort_unordered; /* deterministic ordering of map entries and set elements (byte-wise) */
    bool emit_metadata;  /* emit ^... prefixes for values with attached metadata */
    bool escape_unicode; /* escape non-ASCII bytes in strings as \uXXXX (BMP only) */
    bool indent;         /* pretty-print: hanging-indent collections, one item per line */
    size_t column;       /* current 0-based byte column since last '\n' in the output */
} emit_ctx_t;

static int serialize_key_to_heap(const edn_value_t* v, bool sort_unordered, bool escape_unicode,
                                 char** out_buf, size_t* out_len);

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
        return e->err;
    }
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            e->column = 0;
        } else {
            e->column++;
        }
    }
    return 0;
}

static int emit_cstr(emit_ctx_t* e, const char* s) {
    return emit(e, s, strlen(s));
}

static int emit_newline_indent(emit_ctx_t* e, size_t col) {
    if (emit(e, "\n", 1) != 0) {
        return e->err;
    }
    char spaces[64];
    memset(spaces, ' ', sizeof(spaces));
    while (col > 0) {
        size_t chunk = col < sizeof(spaces) ? col : sizeof(spaces);
        if (emit(e, spaces, chunk) != 0) {
            return e->err;
        }
        col -= chunk;
    }
    return 0;
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

/* Emit a bigint from its parts (sign, digit string sans prefix, radix).
 * Shared by the value-tree writer and the streaming emitter; the latter
 * pre-validates the digit string against the radix character class. */
static int emit_bigint_parts(emit_ctx_t* e, const char* digits, size_t len, bool negative,
                             uint8_t radix) {
    if (negative) {
        if (emit(e, "-", 1) != 0)
            return e->err;
    }

    /* The 'N' suffix is only accepted by the parser on decimal, hex, and
     * octal forms; radix notation (`NrDDD`) explicitly rejects it. Choose
     * the correct prefix and whether to emit the suffix from a single
     * classification of `radix`. The parser strips the literal prefix
     * (`0x` for hex, `0` for octal) from the stored digit string, so the
     * writer re-emits it symmetrically here. */
    bool emit_n_suffix = true;
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (radix == 16) {
        if (emit(e, "0x", 2) != 0)
            return e->err;
    } else if (radix == 8) {
        if (emit(e, "0", 1) != 0)
            return e->err;
    } else if (radix != 10) {
        char prefix[16];
        int pl = snprintf(prefix, sizeof(prefix), "%ur", (unsigned) radix);
        if (pl < 0) {
            e->err = -EDN_ERROR_OUT_OF_MEMORY;
            return e->err;
        }
        if (emit(e, prefix, (size_t) pl) != 0)
            return e->err;
        emit_n_suffix = false; /* parser rejects N on `NrDDD` form */
    }
#else
    (void) radix;
#endif

    if (emit(e, digits, len) != 0)
        return e->err;
    if (emit_n_suffix)
        return emit(e, "N", 1);
    return 0;
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
    return emit_bigint_parts(e, digits, len, neg, radix);
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

/* The parser produces this shape only when one of the components
 * exceeds int64_t range; for the fits-in-int64 case it produces
 * EDN_TYPE_RATIO instead. */
static int emit_bigratio(emit_ctx_t* e, const edn_value_t* v) {
    size_t num_len, denom_len;
    bool num_neg;
    const char* num;
    const char* denom;
    if (!edn_bigratio_get(v, &num, &num_len, &num_neg, &denom, &denom_len)) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    if (num_neg) {
        if (emit(e, "-", 1) != 0)
            return e->err;
    }
    if (emit(e, num, num_len) != 0)
        return e->err;
    if (emit(e, "/", 1) != 0)
        return e->err;
    return emit(e, denom, denom_len);
}

/* True iff `v` is a bare (non-namespaced) keyword whose name equals
 * `name[0..name_len)`. Used to classify metadata maps for short-form
 * emission (e.g. `:tag`, `:param-tags`). */
static bool keyword_is_bare(const edn_value_t* v, const char* name, size_t name_len) {
    return v != NULL && v->type == EDN_TYPE_KEYWORD && v->as.keyword.ns_length == 0 &&
           v->as.keyword.name_length == name_len && memcmp(v->as.keyword.name, name, name_len) == 0;
}

/* Emit the `^<META>` prefix for a value's metadata map. Short forms:
 *   {:tag Sym}            -> ^Sym
 *   {:param-tags [...]}   -> ^[...]
 *   {:any-kw true}        -> ^:any-kw         (including namespaced keys)
 * Anything else (including empty map and non-matching single entries) emits
 * the full map form `^{...}`. Caller is expected to emit a separating space
 * between the prefix and the value. */
static int emit_metadata_prefix(emit_ctx_t* e, const edn_value_t* meta) {
    if (emit(e, "^", 1) != 0)
        return e->err;
    if (meta->as.map.count == 1) {
        const edn_value_t* k = meta->as.map.keys[0];
        const edn_value_t* val = meta->as.map.values[0];
        if (keyword_is_bare(k, "tag", 3) && val != NULL && val->type == EDN_TYPE_SYMBOL) {
            return emit_value(e, val);
        }
        if (keyword_is_bare(k, "param-tags", 10) && val != NULL && val->type == EDN_TYPE_VECTOR) {
            return emit_value(e, val);
        }
        if (k != NULL && k->type == EDN_TYPE_KEYWORD && val != NULL && val->type == EDN_TYPE_BOOL &&
            val->as.boolean) {
            return emit_value(e, k);
        }
    }
    return emit_value(e, meta);
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

    /* Supplementary codepoints can
     * only reach this path via programmatic construction in those builds. */
#ifndef EDN_ENABLE_EXPERIMENTAL_EXTENSION
    if (cp > 0xFFFF) {
        e->err = -EDN_ERROR_UNSUPPORTED_TYPE;
        return e->err;
    }
#endif

    char buf[16];
    int len = snprintf(buf, sizeof(buf), "u%04X", cp);
    if (len < 0) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    return emit(e, buf, (size_t) len);
}

/* Strict UTF-8 decoder for one multi-byte sequence at `p`. Returns the byte
 * count consumed (2/3/4) with the codepoint in *out_cp, or 0 on malformation
 * (overlong, surrogate U+D800..U+DFFF, > U+10FFFF, or short input). */
static size_t decode_utf8(const unsigned char* p, size_t len, uint32_t* out_cp) {
    if (len == 0) {
        return 0;
    }
    unsigned char b0 = p[0];
    if ((b0 & 0xE0) == 0xC0) {
        if (len < 2 || (p[1] & 0xC0) != 0x80) {
            return 0;
        }
        uint32_t cp = ((uint32_t) (b0 & 0x1F) << 6) | (uint32_t) (p[1] & 0x3F);
        if (cp < 0x80) {
            return 0; /* overlong */
        }
        *out_cp = cp;
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (len < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) {
            return 0;
        }
        uint32_t cp = ((uint32_t) (b0 & 0x0F) << 12) | ((uint32_t) (p[1] & 0x3F) << 6) |
                      (uint32_t) (p[2] & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return 0; /* overlong or surrogate */
        }
        *out_cp = cp;
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (len < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) {
            return 0;
        }
        uint32_t cp = ((uint32_t) (b0 & 0x07) << 18) | ((uint32_t) (p[1] & 0x3F) << 12) |
                      ((uint32_t) (p[2] & 0x3F) << 6) | (uint32_t) (p[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) {
            return 0; /* overlong or out-of-range */
        }
        *out_cp = cp;
        return 4;
    }
    return 0;
}

static int emit_bmp_escape(emit_ctx_t* e, uint32_t cp) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "\\u%04X", cp);
    if (len < 0) {
        e->err = -EDN_ERROR_OUT_OF_MEMORY;
        return e->err;
    }
    return emit(e, buf, (size_t) len);
}

/* Escape pass: emit ASCII runs verbatim; convert each well-formed non-ASCII
 * UTF-8 sequence to either `\uXXXX` (BMP) or raw UTF-8 (supplementary). */
static int emit_string_bytes_escaped(emit_ctx_t* e, const char* data, size_t len) {
    const unsigned char* bytes = (const unsigned char*) data;
    size_t run_start = 0;
    size_t i = 0;
    while (i < len) {
        if (bytes[i] < 0x80) {
            i++;
            continue;
        }
        if (i > run_start) {
            if (emit(e, data + run_start, i - run_start) != 0)
                return e->err;
        }
        uint32_t cp = 0;
        size_t n = decode_utf8(bytes + i, len - i, &cp);
        if (n == 0) {
            e->err = -EDN_ERROR_INVALID_STRING;
            return e->err;
        }
        if (cp <= 0xFFFF) {
            if (emit_bmp_escape(e, cp) != 0)
                return e->err;
        } else {
            /* Supplementary plane: pass through as raw UTF-8 (parser does
             * not accept surrogate pairs). */
            if (emit(e, data + i, n) != 0)
                return e->err;
        }
        i += n;
        run_start = i;
    }
    if (run_start < len) {
        return emit(e, data + run_start, len - run_start);
    }
    return 0;
}

static int emit_string(emit_ctx_t* e, const edn_value_t* v) {
    /* Strings store raw source bytes between the quote characters. The source
     * is already valid EDN (any required escapes are present in the bytes),
     * so verbatim emission between quotes round-trips. When escape_unicode
     * is set, non-ASCII UTF-8 sequences in the BMP are converted to \uXXXX
     * on the way out; ASCII bytes (including pre-existing backslash escapes)
     * pass through unchanged. */
    if (emit(e, "\"", 1) != 0)
        return e->err;
    size_t len = edn_string_get_length(v);
    if (e->escape_unicode) {
        if (emit_string_bytes_escaped(e, v->as.string.data, len) != 0)
            return e->err;
    } else {
        if (emit(e, v->as.string.data, len) != 0)
            return e->err;
    }
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
    size_t indent_col = e->column;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (e->indent) {
                if (emit_newline_indent(e, indent_col) != 0)
                    return e->err;
            } else {
                if (emit(e, " ", 1) != 0)
                    return e->err;
            }
        }
        if (emit_value(e, elements[i]) != 0)
            return e->err;
    }
    return emit(e, &close, 1);
}

/* --- deterministic ordering of unordered collections (maps, sets) --- */

/* Sortable view of a collection element: its serialized bytes plus the
 * original index. */
typedef struct {
    char* repr;
    size_t len;
    size_t idx;
} key_sort_item_t;

static int compare_key_sort_items(const void* a, const void* b) {
    const key_sort_item_t* ia = a;
    const key_sort_item_t* ib = b;
    size_t min_len = ia->len < ib->len ? ia->len : ib->len;
    int c = (min_len > 0) ? memcmp(ia->repr, ib->repr, min_len) : 0;
    if (c != 0)
        return c;
    if (ia->len < ib->len)
        return -1;
    if (ia->len > ib->len)
        return 1;
    return 0;
}

static void free_key_sort_items(key_sort_item_t* items, size_t count) {
    if (items == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        free(items[i].repr);
    }
    free(items);
}

/* Build a sorted permutation of [0, count) for `elements`, comparing
 * elements by their byte-wise EDN serialization. On success returns 0 and
 * sets *out_items to a freshly allocated array of `count` entries whose
 * `.idx` field gives the sorted order; the caller must release it with
 * free_key_sort_items. On failure returns a negative EDN_ERROR_* and sets
 * *out_items to NULL. */
static int build_sorted_indices(edn_value_t* const* elements, size_t count, bool sort_unordered,
                                bool escape_unicode, key_sort_item_t** out_items) {
    *out_items = NULL;
    key_sort_item_t* items = calloc(count, sizeof(*items));
    if (items == NULL) {
        return -EDN_ERROR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < count; i++) {
        int r = serialize_key_to_heap(elements[i], sort_unordered, escape_unicode, &items[i].repr,
                                      &items[i].len);
        if (r != 0) {
            free_key_sort_items(items, i);
            return r;
        }
        items[i].idx = i;
    }
    qsort(items, count, sizeof(*items), compare_key_sort_items);
    *out_items = items;
    return 0;
}

static int emit_map_sorted(emit_ctx_t* e, edn_value_t* const* keys, edn_value_t* const* values,
                           size_t count) {
    key_sort_item_t* items = NULL;
    int r = build_sorted_indices(keys, count, e->sort_unordered, e->escape_unicode, &items);
    if (r != 0) {
        e->err = r;
        return e->err;
    }

    if (emit(e, "{", 1) != 0)
        goto done;
    size_t indent_col = e->column;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (e->indent) {
                if (emit_newline_indent(e, indent_col) != 0)
                    goto done;
            } else {
                if (emit(e, ", ", 2) != 0)
                    goto done;
            }
        }
        size_t idx = items[i].idx;
        if (emit_value(e, keys[idx]) != 0)
            goto done;
        if (emit(e, " ", 1) != 0)
            goto done;
        if (emit_value(e, values[idx]) != 0)
            goto done;
    }
    emit(e, "}", 1);

done:
    free_key_sort_items(items, count);
    return e->err;
}

static int emit_set_sorted(emit_ctx_t* e, edn_value_t* const* elements, size_t count) {
    key_sort_item_t* items = NULL;
    int r = build_sorted_indices(elements, count, e->sort_unordered, e->escape_unicode, &items);
    if (r != 0) {
        e->err = r;
        return e->err;
    }

    if (emit(e, "#{", 2) != 0)
        goto done;
    size_t indent_col = e->column;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (e->indent) {
                if (emit_newline_indent(e, indent_col) != 0)
                    goto done;
            } else {
                if (emit(e, " ", 1) != 0)
                    goto done;
            }
        }
        if (emit_value(e, elements[items[i].idx]) != 0)
            goto done;
    }
    emit(e, "}", 1);

done:
    free_key_sort_items(items, count);
    return e->err;
}

static int emit_set(emit_ctx_t* e, edn_value_t* const* elements, size_t count) {
    if (e->sort_unordered && count > 1) {
        return emit_set_sorted(e, elements, count);
    }
    if (emit(e, "#{", 2) != 0)
        return e->err;
    size_t indent_col = e->column;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (e->indent) {
                if (emit_newline_indent(e, indent_col) != 0)
                    return e->err;
            } else {
                if (emit(e, " ", 1) != 0)
                    return e->err;
            }
        }
        if (emit_value(e, elements[i]) != 0)
            return e->err;
    }
    return emit(e, "}", 1);
}

static int emit_map(emit_ctx_t* e, edn_value_t* const* keys, edn_value_t* const* values,
                    size_t count) {
    if (e->sort_unordered && count > 1) {
        return emit_map_sorted(e, keys, values, count);
    }
    if (emit(e, "{", 1) != 0)
        return e->err;
    size_t indent_col = e->column;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            if (e->indent) {
                if (emit_newline_indent(e, indent_col) != 0)
                    return e->err;
            } else {
                if (emit(e, ", ", 2) != 0)
                    return e->err;
            }
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

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (e->emit_metadata && v->metadata != NULL) {
        bool save_indent = e->indent;
        e->indent = false;
        int rc = emit_metadata_prefix(e, v->metadata);
        e->indent = save_indent;
        if (rc != 0)
            return e->err;
        if (emit(e, " ", 1) != 0)
            return e->err;
    }
#endif

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
        case EDN_TYPE_BIGRATIO:
            return emit_bigratio(e, v);
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
            return -EDN_ERROR_INVALID_ARGUMENT;
        }
        return 0;
    }
#ifndef EDN_ENABLE_CLOJURE_EXTENSION
    if (opts->emit_metadata)
        return -EDN_ERROR_UNSUPPORTED_TYPE;
#endif
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
        return -EDN_ERROR_INVALID_ARGUMENT;
    }
    int v = validate_options(options);
    if (v != 0)
        return v;

    bool sort_unordered = (options != NULL && options->struct_size != 0 && options->sort_unordered);
    bool emit_metadata = (options != NULL && options->struct_size != 0 && options->emit_metadata);
    bool escape_unicode = (options != NULL && options->struct_size != 0 && options->escape_unicode);
    bool indent = (options != NULL && options->struct_size != 0 && options->indent != 0);
    emit_ctx_t e = {.cb = cb,
                    .ctx = ctx,
                    .err = 0,
                    .sort_unordered = sort_unordered,
                    .emit_metadata = emit_metadata,
                    .escape_unicode = escape_unicode,
                    .indent = indent,
                    .column = 0};
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
        return -EDN_ERROR_OUT_OF_MEMORY;

    if (h->len + n + 1 > h->cap) {
        size_t new_cap = h->cap ? h->cap : 64;
        while (new_cap < h->len + n + 1) {
            size_t doubled = new_cap * 2;
            if (doubled < new_cap) {
                h->failed = true;
                return -EDN_ERROR_OUT_OF_MEMORY;
            }
            new_cap = doubled;
        }
        char* nb = realloc(h->buf, new_cap);
        if (!nb) {
            h->failed = true;
            return -EDN_ERROR_OUT_OF_MEMORY;
        }
        h->buf = nb;
        h->cap = new_cap;
    }
    memcpy(h->buf + h->len, data, n);
    h->len += n;
    return 0;
}

/* Serialize one value into a fresh malloc'd buffer (NOT null-terminated;
 * length returned via *out_len). Returns 0 on success, a negative
 * EDN_ERROR_* on failure (in which case *out_buf is set to NULL). */
static int serialize_key_to_heap(const edn_value_t* v, bool sort_unordered, bool escape_unicode,
                                 char** out_buf, size_t* out_len) {
    heap_ctx_t h = {.buf = NULL, .len = 0, .cap = 0, .failed = false};
    emit_ctx_t e = {.cb = heap_cb,
                    .ctx = &h,
                    .err = 0,
                    .sort_unordered = sort_unordered,
                    .emit_metadata = false,
                    .escape_unicode = escape_unicode};
    emit_value(&e, v);
    if (e.err != 0 || h.failed) {
        free(h.buf);
        *out_buf = NULL;
        *out_len = 0;
        return (e.err != 0) ? e.err : -EDN_ERROR_OUT_OF_MEMORY;
    }
    *out_buf = h.buf;
    *out_len = h.len;
    return 0;
}

char* edn_write_string(const edn_value_t* value, const edn_write_options_t* options,
                       size_t* out_len) {
    heap_ctx_t h = {.buf = NULL, .len = 0, .cap = 0, .failed = false};
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
    buffer_ctx_t b = {.buf = buf, .cap = cap, .pos = 0, .needed = 0};
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
    return (w == n) ? 0 : -EDN_ERROR_IO_FAILURE;
}

int edn_write_file(const edn_value_t* value, FILE* fp, const edn_write_options_t* options) {
    if (fp == NULL) {
        return -EDN_ERROR_INVALID_ARGUMENT;
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

/* ========================================================================
 * Streaming emitter
 * ========================================================================
 *
 * Push-style emitter built on top of the same `emit_ctx_t` chokepoint used
 * by the value-tree writer. The emitter owns a single emit_ctx_t whose
 * callback is `emitter_dispatch_cb`; the dispatcher writes either to the
 * user callback or into a capture buffer (used to buffer streamed
 * metadata payloads). The capture path saves/restores column tracking so
 * indent behavior is identical to the value-tree path.
 */

typedef enum {
    EMITTER_FRAME_LIST,
    EMITTER_FRAME_VECTOR,
    EMITTER_FRAME_SET,
    EMITTER_FRAME_MAP
} emitter_frame_kind_t;

typedef struct {
    emitter_frame_kind_t kind;
    size_t item_count; /* items emitted so far (map: keys+values combined) */
    size_t indent_col; /* column at which inner items align */
} emitter_frame_t;

typedef struct emitter_prefix {
    char* bytes;
    size_t len;
    struct emitter_prefix* next;
} emitter_prefix_t;

/* Classification used to gate metadata-payload validation. */
typedef enum {
    PAYLOAD_NIL,
    PAYLOAD_BOOL,
    PAYLOAD_INT,
    PAYLOAD_DOUBLE,
    PAYLOAD_STRING,
    PAYLOAD_KEYWORD,
    PAYLOAD_SYMBOL,
    PAYLOAD_CHAR,
    PAYLOAD_BIGNUM,
    PAYLOAD_LIST,
    PAYLOAD_VECTOR,
    PAYLOAD_SET,
    PAYLOAD_MAP,
    PAYLOAD_EMBED
} payload_kind_t;

struct edn_emitter {
    /* Output context: cb routes through emitter_dispatch_cb. */
    emit_ctx_t e;
    edn_writer_callback_fn user_cb;
    void* user_ctx;

    /* Cached options (only fields that survive create-time validation). */
    bool newline_at_end;
    bool emit_metadata_enabled;
    bool indent_enabled;

    /* Frame stack. Inline first slot avoids alloc for shallow output. */
    emitter_frame_t* frames;
    size_t frames_count;
    size_t frames_cap;
    emitter_frame_t inline_frames[16];

    /* Pending prefix list (tag/meta), emitted in call order before the
     * next non-prefix value. */
    emitter_prefix_t* pending_head;
    emitter_prefix_t* pending_tail;
    bool tag_pending; /* exactly one tag may be pending at a time */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    /* Meta payload state. */
    bool expecting_meta_payload;
    bool capturing;
    bool capture_is_scalar;
    size_t capture_start_depth; /* frames_count when capture began */

    /* Capture buffer. */
    char* capture_buf;
    size_t capture_len;
    size_t capture_cap;

    /* Saved state during capture. */
    size_t saved_column;
    bool saved_indent;
#endif

    /* Lifecycle / state flags. */
    bool produced_top_value;
    bool finished;
    bool poisoned; /* sticky error flag: any failure marks the emitter unusable */
};

/* --- dispatch + capture --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
static int emitter_capture_append(edn_emitter_t* em, const char* data, size_t n) {
    if (em->capture_len + n > em->capture_cap) {
        size_t new_cap = em->capture_cap ? em->capture_cap : 64;
        while (new_cap < em->capture_len + n) {
            size_t doubled = new_cap * 2;
            if (doubled < new_cap)
                return -EDN_ERROR_OUT_OF_MEMORY;
            new_cap = doubled;
        }
        char* nb = realloc(em->capture_buf, new_cap);
        if (!nb)
            return -EDN_ERROR_OUT_OF_MEMORY;
        em->capture_buf = nb;
        em->capture_cap = new_cap;
    }
    memcpy(em->capture_buf + em->capture_len, data, n);
    em->capture_len += n;
    return 0;
}
#endif

static int emitter_dispatch_cb(const char* data, size_t n, void* ctx) {
    edn_emitter_t* em = ctx;
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (em->capturing) {
        return emitter_capture_append(em, data, n);
    }
#endif
    return em->user_cb(data, n, em->user_ctx);
}

/* --- frame stack --- */

static int emitter_push_frame(edn_emitter_t* em, emitter_frame_kind_t kind, size_t indent_col) {
    if (em->frames_count >= em->frames_cap) {
        size_t new_cap = em->frames_cap * 2;
        emitter_frame_t* new_frames;
        if (em->frames == em->inline_frames) {
            new_frames = malloc(new_cap * sizeof(*new_frames));
            if (!new_frames)
                return -EDN_ERROR_OUT_OF_MEMORY;
            memcpy(new_frames, em->inline_frames, em->frames_count * sizeof(*new_frames));
        } else {
            new_frames = realloc(em->frames, new_cap * sizeof(*new_frames));
            if (!new_frames)
                return -EDN_ERROR_OUT_OF_MEMORY;
        }
        em->frames = new_frames;
        em->frames_cap = new_cap;
    }
    emitter_frame_t* f = &em->frames[em->frames_count++];
    f->kind = kind;
    f->item_count = 0;
    f->indent_col = indent_col;
    return 0;
}

static emitter_frame_t* emitter_top_frame(edn_emitter_t* em) {
    return em->frames_count > 0 ? &em->frames[em->frames_count - 1] : NULL;
}

/* --- pending prefix list --- */

static void emitter_free_prefixes(edn_emitter_t* em) {
    emitter_prefix_t* p = em->pending_head;
    while (p) {
        emitter_prefix_t* nx = p->next;
        free(p->bytes);
        free(p);
        p = nx;
    }
    em->pending_head = em->pending_tail = NULL;
}

static int emitter_push_prefix(edn_emitter_t* em, char* bytes, size_t len) {
    emitter_prefix_t* p = malloc(sizeof(*p));
    if (!p) {
        free(bytes);
        return -EDN_ERROR_OUT_OF_MEMORY;
    }
    p->bytes = bytes;
    p->len = len;
    p->next = NULL;
    if (em->pending_tail) {
        em->pending_tail->next = p;
    } else {
        em->pending_head = p;
    }
    em->pending_tail = p;
    return 0;
}

static int emitter_flush_prefixes(edn_emitter_t* em) {
    while (em->pending_head) {
        emitter_prefix_t* p = em->pending_head;
        if (emit(&em->e, p->bytes, p->len) != 0)
            return em->e.err;
        em->pending_head = p->next;
        if (!em->pending_head)
            em->pending_tail = NULL;
        free(p->bytes);
        free(p);
    }
    em->tag_pending = false;
    return 0;
}

/* --- validation --- */

/* Single identifier segment: non-empty, no delimiter bytes, no slash, no
 * adjacent colons. Mirrors the reader's scan_identifier validity rules for
 * a chunk that does not contain a namespace separator. */
static bool valid_ident_chunk(const char* s, size_t len) {
    if (s == NULL || len == 0)
        return false;
    bool prev_colon = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) s[i];
        if (is_delimiter(c) || c == '/')
            return false;
        if (c == ':') {
            if (prev_colon)
                return false;
            prev_colon = true;
        } else {
            prev_colon = false;
        }
    }
    return true;
}

/* Keyword name: chunk-valid AND does not start with ':' (the leading ':'
 * is the keyword sigil and cannot also appear at the start of the name). */
static bool valid_kw_name(const char* s, size_t len) {
    return valid_ident_chunk(s, len) && s[0] != ':';
}

/* Symbol name / namespace: chunk-valid, does not start with ':', and does
 * not look like the start of a number (leading digit, or +/- followed by
 * a digit). Note '/' alone is a valid symbol name per the reader's special
 * case, but emit_symbol always namespaces explicitly via emit_symbol_ns,
 * so we don't carve out the lone-slash case here. */
static bool valid_sym_chunk(const char* s, size_t len) {
    if (!valid_ident_chunk(s, len))
        return false;
    if (s[0] == ':')
        return false;
    if (s[0] >= '0' && s[0] <= '9')
        return false;
    if ((s[0] == '+' || s[0] == '-') && len >= 2 && s[1] >= '0' && s[1] <= '9')
        return false;
    return true;
}

static bool valid_utf8(const char* s, size_t len) {
    const unsigned char* p = (const unsigned char*) s;
    size_t i = 0;
    while (i < len) {
        if (p[i] < 0x80) {
            i++;
            continue;
        }
        uint32_t cp;
        size_t n = decode_utf8(p + i, len - i, &cp);
        if (n == 0)
            return false;
        i += n;
    }
    return true;
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
/* Validate a digit string and split off the optional leading '-'. */
static bool split_sign_and_validate_digits(const char* digits, uint8_t radix, const char** out_d,
                                           size_t* out_len, bool* out_neg) {
    if (digits == NULL || *digits == '\0')
        return false;
    bool neg = false;
    if (*digits == '-') {
        neg = true;
        digits++;
    } else if (*digits == '+') {
        digits++;
    }
    size_t len = strlen(digits);
    if (len == 0)
        return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) digits[i];
        int dv = -1;
        if (c >= '0' && c <= '9')
            dv = c - '0';
        else if (c >= 'a' && c <= 'z')
            dv = (int) (c - 'a') + 10;
        else if (c >= 'A' && c <= 'Z')
            dv = (int) (c - 'A') + 10;
        if (dv < 0 || dv >= radix)
            return false;
    }
    *out_d = digits;
    *out_len = len;
    *out_neg = neg;
    return true;
}

/* Validate a bigdec literal: optional '-', integer-part digits, optional
 * '.' fraction, optional 'eE' [+/-] exponent. */
static bool valid_bigdec_digits(const char* s, const char** out_payload, size_t* out_len,
                                bool* out_neg) {
    if (s == NULL || *s == '\0')
        return false;
    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    } else if (*s == '+') {
        s++;
    }
    const char* start = s;
    size_t int_digits = 0;
    while (*s >= '0' && *s <= '9') {
        int_digits++;
        s++;
    }
    if (int_digits == 0)
        return false;
    if (*s == '.') {
        s++;
        size_t frac_digits = 0;
        while (*s >= '0' && *s <= '9') {
            frac_digits++;
            s++;
        }
        if (frac_digits == 0)
            return false;
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '+' || *s == '-')
            s++;
        size_t exp_digits = 0;
        while (*s >= '0' && *s <= '9') {
            exp_digits++;
            s++;
        }
        if (exp_digits == 0)
            return false;
    }
    if (*s != '\0')
        return false;
    *out_payload = start;
    *out_len = (size_t) (s - start);
    *out_neg = neg;
    return true;
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

/* --- separator emission --- */

static int emitter_emit_separator(edn_emitter_t* em, emitter_frame_t* f) {
    if (f->item_count == 0) {
        return 0;
    }
    /* Map alternation: odd item_count = value position (single space, no
     * newline); even item_count > 0 = next key (pair separator). */
    if (f->kind == EMITTER_FRAME_MAP && (f->item_count & 1) == 1) {
        return emit(&em->e, " ", 1);
    }
    if (em->e.indent) {
        return emit_newline_indent(&em->e, f->indent_col);
    }
    if (f->kind == EMITTER_FRAME_MAP) {
        return emit(&em->e, ", ", 2);
    }
    return emit(&em->e, " ", 1);
}

/* --- capture lifecycle --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
static void emitter_start_capture(edn_emitter_t* em, bool is_scalar) {
    em->capturing = true;
    em->capture_is_scalar = is_scalar;
    em->capture_start_depth = em->frames_count;
    em->capture_len = 0;
    /* Force compact emission of the payload bytes; restore on stop. */
    em->saved_indent = em->e.indent;
    em->saved_column = em->e.column;
    em->e.indent = false;
}

/* Wrap the captured payload as `^<payload> ` and push onto pending prefix
 * list. Restores column/indent. */
static int emitter_stop_capture_commit(edn_emitter_t* em) {
    em->capturing = false;
    em->e.indent = em->saved_indent;
    em->e.column = em->saved_column;

    size_t pl = em->capture_len;
    size_t total = 1 + pl + 1; /* '^' + payload + ' ' */
    char* buf = malloc(total);
    if (!buf)
        return -EDN_ERROR_OUT_OF_MEMORY;
    buf[0] = '^';
    if (pl > 0)
        memcpy(buf + 1, em->capture_buf, pl);
    buf[1 + pl] = ' ';
    em->capture_len = 0;
    return emitter_push_prefix(em, buf, total);
}
#endif

/* --- pre/post value hooks --- */

/* Called at the start of every value-producing public function.
 * Validates state, flushes prefixes (if not capturing), and emits the
 * inter-item separator within an open frame. */
static int emitter_pre_value(edn_emitter_t* em, payload_kind_t kind) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->poisoned || em->finished)
        return -EDN_ERROR_INVALID_STATE;

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (em->expecting_meta_payload) {
        bool ok = (kind == PAYLOAD_KEYWORD || kind == PAYLOAD_SYMBOL || kind == PAYLOAD_MAP ||
                   kind == PAYLOAD_VECTOR || kind == PAYLOAD_EMBED);
        if (!ok) {
            em->poisoned = true;
            return -EDN_ERROR_INVALID_STATE;
        }
        em->expecting_meta_payload = false;
        bool is_scalar =
            (kind == PAYLOAD_KEYWORD || kind == PAYLOAD_SYMBOL || kind == PAYLOAD_EMBED);
        emitter_start_capture(em, is_scalar);
        /* IMPORTANT: skip separator + prefix flush. The payload bytes are
         * conceptually part of a prefix on the upcoming real value, not a
         * value in the enclosing container; any pending tag/meta prefixes
         * apply to the real value and must remain queued. */
        return 0;
    }
#else
    (void) kind;
#endif

    emitter_frame_t* f = emitter_top_frame(em);
    if (f == NULL) {
        if (em->produced_top_value) {
            em->poisoned = true;
            return -EDN_ERROR_INVALID_STATE;
        }
    } else {
        if (emitter_emit_separator(em, f) != 0) {
            em->poisoned = true;
            return em->e.err ? em->e.err : -EDN_ERROR_OUT_OF_MEMORY;
        }
    }

    if (emitter_flush_prefixes(em) != 0) {
        em->poisoned = true;
        return em->e.err ? em->e.err : -EDN_ERROR_OUT_OF_MEMORY;
    }
    return 0;
}

/* Called at the end of a scalar emit (and after embedded edn_emit_value
 * completes). For collections, the equivalent logic lives in the end_X
 * functions because the depth-balance check fires there. */
static int emitter_post_scalar(edn_emitter_t* em) {
    if (em->poisoned || em->e.err != 0) {
        em->poisoned = true;
        return em->e.err ? em->e.err : -EDN_ERROR_INVALID_STATE;
    }

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (em->capturing && em->capture_is_scalar && em->frames_count == em->capture_start_depth) {
        int r = emitter_stop_capture_commit(em);
        if (r != 0) {
            em->poisoned = true;
            return r;
        }
        /* The captured scalar was the meta payload, not a real value. */
        return 0;
    }
#endif

    emitter_frame_t* f = emitter_top_frame(em);
    if (f != NULL) {
        f->item_count++;
    } else {
        em->produced_top_value = true;
    }
    return 0;
}

/* --- create / finish / destroy --- */

static int emitter_validate_options_for_create(const edn_write_options_t* opts) {
    if (opts == NULL)
        return 0;
    if (opts->struct_size != 0 && opts->struct_size < sizeof(edn_write_options_t))
        return -1;
    if (opts->struct_size == 0)
        return 0;
    if (opts->sort_unordered)
        return -1;
    if (opts->writer_registry != NULL)
        return -1;
#ifndef EDN_ENABLE_CLOJURE_EXTENSION
    if (opts->emit_metadata)
        return -1;
#endif
    return 0;
}

edn_emitter_t* edn_emitter_create(edn_writer_callback_fn cb, void* ctx,
                                  const edn_write_options_t* options) {
    if (cb == NULL)
        return NULL;
    if (emitter_validate_options_for_create(options) != 0)
        return NULL;

    edn_emitter_t* em = calloc(1, sizeof(*em));
    if (em == NULL)
        return NULL;

    bool has_opts = (options != NULL && options->struct_size != 0);
    em->user_cb = cb;
    em->user_ctx = ctx;
    em->newline_at_end = has_opts && options->newline_at_end;
    em->emit_metadata_enabled = has_opts && options->emit_metadata;
    em->indent_enabled = has_opts && options->indent != 0;

    em->e.cb = emitter_dispatch_cb;
    em->e.ctx = em;
    em->e.err = 0;
    em->e.sort_unordered = false;
    em->e.emit_metadata = em->emit_metadata_enabled;
    em->e.escape_unicode = has_opts && options->escape_unicode;
    em->e.indent = em->indent_enabled;
    em->e.column = 0;

    em->frames = em->inline_frames;
    em->frames_count = 0;
    em->frames_cap = sizeof(em->inline_frames) / sizeof(em->inline_frames[0]);
    return em;
}

void edn_emitter_destroy(edn_emitter_t* em) {
    if (em == NULL)
        return;
    if (em->frames != em->inline_frames)
        free(em->frames);
    emitter_free_prefixes(em);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    free(em->capture_buf);
#endif
    free(em);
}

int edn_emitter_finish(edn_emitter_t* em) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->poisoned || em->finished)
        return -EDN_ERROR_INVALID_STATE;
    if (em->frames_count != 0)
        return -EDN_ERROR_INVALID_STATE;
    if (em->tag_pending || em->pending_head != NULL)
        return -EDN_ERROR_INVALID_STATE;
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (em->expecting_meta_payload || em->capturing)
        return -EDN_ERROR_INVALID_STATE;
#endif
    if (!em->produced_top_value)
        return -EDN_ERROR_INVALID_STATE;
    if (em->newline_at_end) {
        if (emit(&em->e, "\n", 1) != 0)
            return em->e.err;
    }
    em->finished = true;
    return 0;
}

/* --- depth check shared by begin_X --- */

static int emitter_enter_depth(edn_emitter_t* em) {
    if (em->frames_count >= EDN_DEFAULT_MAX_DEPTH) {
        em->poisoned = true;
        return -EDN_ERROR_MAX_DEPTH_EXCEEDED;
    }
    return 0;
}

/* --- scalars --- */

int edn_emit_nil(edn_emitter_t* em) {
    int r = emitter_pre_value(em, PAYLOAD_NIL);
    if (r != 0)
        return r;
    if (emit_cstr(&em->e, "nil") != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_bool(edn_emitter_t* em, bool value) {
    int r = emitter_pre_value(em, PAYLOAD_BOOL);
    if (r != 0)
        return r;
    if (emit_cstr(&em->e, value ? "true" : "false") != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_int(edn_emitter_t* em, int64_t value) {
    int r = emitter_pre_value(em, PAYLOAD_INT);
    if (r != 0)
        return r;
    if (emit_int(&em->e, value) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_double(edn_emitter_t* em, double value) {
    int r = emitter_pre_value(em, PAYLOAD_DOUBLE);
    if (r != 0)
        return r;
    if (emit_float(&em->e, value) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_string(edn_emitter_t* em, const char* s, size_t len) {
    if (em == NULL || s == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (len == (size_t) -1)
        len = strlen(s);
    if (!valid_utf8(s, len))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_STRING);
    if (r != 0)
        return r;
    if (emit(&em->e, "\"", 1) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    int wr =
        em->e.escape_unicode ? emit_string_bytes_escaped(&em->e, s, len) : emit(&em->e, s, len);
    if (wr != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    if (emit(&em->e, "\"", 1) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

static int emit_kw_or_sym_raw(edn_emitter_t* em, bool is_keyword, const char* ns,
                              const char* name) {
    if (is_keyword) {
        if (emit(&em->e, ":", 1) != 0)
            return em->e.err;
    }
    if (ns != NULL) {
        if (emit(&em->e, ns, strlen(ns)) != 0)
            return em->e.err;
        if (emit(&em->e, "/", 1) != 0)
            return em->e.err;
    }
    return emit(&em->e, name, strlen(name));
}

int edn_emit_keyword(edn_emitter_t* em, const char* name) {
    if (em == NULL || name == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_kw_name(name, strlen(name)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_KEYWORD);
    if (r != 0)
        return r;
    if (emit_kw_or_sym_raw(em, true, NULL, name) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_keyword_ns(edn_emitter_t* em, const char* ns, const char* name) {
    if (em == NULL || ns == NULL || name == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_sym_chunk(ns, strlen(ns)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_kw_name(name, strlen(name)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_KEYWORD);
    if (r != 0)
        return r;
    if (emit_kw_or_sym_raw(em, true, ns, name) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_symbol(edn_emitter_t* em, const char* name) {
    if (em == NULL || name == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_sym_chunk(name, strlen(name)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_SYMBOL);
    if (r != 0)
        return r;
    if (emit_kw_or_sym_raw(em, false, NULL, name) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_symbol_ns(edn_emitter_t* em, const char* ns, const char* name) {
    if (em == NULL || ns == NULL || name == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_sym_chunk(ns, strlen(ns)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (!valid_sym_chunk(name, strlen(name)))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_SYMBOL);
    if (r != 0)
        return r;
    if (emit_kw_or_sym_raw(em, false, ns, name) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_character(edn_emitter_t* em, uint32_t cp) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_CHAR);
    if (r != 0)
        return r;
    if (emit_character(&em->e, cp) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
int edn_emit_bigint(edn_emitter_t* em, const char* digits, int radix) {
    if (em == NULL || digits == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (radix < 2 || radix > 36)
        return -EDN_ERROR_INVALID_ARGUMENT;
    const char* d;
    size_t dlen;
    bool neg;
    if (!split_sign_and_validate_digits(digits, (uint8_t) radix, &d, &dlen, &neg))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_BIGNUM);
    if (r != 0)
        return r;
    if (emit_bigint_parts(&em->e, d, dlen, neg, (uint8_t) radix) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_bigratio(edn_emitter_t* em, const char* numerator, const char* denominator) {
    if (em == NULL || numerator == NULL || denominator == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    const char* n;
    size_t nlen;
    bool nneg;
    if (!split_sign_and_validate_digits(numerator, 10, &n, &nlen, &nneg))
        return -EDN_ERROR_INVALID_ARGUMENT;
    /* Denominator must be positive: reject leading sign. */
    if (*denominator == '-' || *denominator == '+')
        return -EDN_ERROR_INVALID_ARGUMENT;
    const char* d;
    size_t dlen;
    bool dneg;
    if (!split_sign_and_validate_digits(denominator, 10, &d, &dlen, &dneg))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_BIGNUM);
    if (r != 0)
        return r;
    emit_ctx_t* e = &em->e;
    if (nneg) {
        if (emit(e, "-", 1) != 0) {
            em->poisoned = true;
            return e->err;
        }
    }
    if (emit(e, n, nlen) != 0 || emit(e, "/", 1) != 0 || emit(e, d, dlen) != 0) {
        em->poisoned = true;
        return e->err;
    }
    return emitter_post_scalar(em);
}

int edn_emit_bigdecimal(edn_emitter_t* em, const char* digits) {
    if (em == NULL || digits == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    const char* d;
    size_t dlen;
    bool neg;
    if (!valid_bigdec_digits(digits, &d, &dlen, &neg))
        return -EDN_ERROR_INVALID_ARGUMENT;
    int r = emitter_pre_value(em, PAYLOAD_BIGNUM);
    if (r != 0)
        return r;
    emit_ctx_t* e = &em->e;
    if (neg) {
        if (emit(e, "-", 1) != 0) {
            em->poisoned = true;
            return e->err;
        }
    }
    if (emit(e, d, dlen) != 0 || emit(e, "M", 1) != 0) {
        em->poisoned = true;
        return e->err;
    }
    return emitter_post_scalar(em);
}
#endif

/* --- collections --- */

static int emitter_begin_collection(edn_emitter_t* em, emitter_frame_kind_t kind, const char* open,
                                    size_t open_len, payload_kind_t pk) {
    int r = emitter_pre_value(em, pk);
    if (r != 0)
        return r;
    r = emitter_enter_depth(em);
    if (r != 0)
        return r;
    if (emit(&em->e, open, open_len) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    size_t indent_col = em->e.column;
    if (emitter_push_frame(em, kind, indent_col) != 0) {
        em->poisoned = true;
        return -EDN_ERROR_OUT_OF_MEMORY;
    }
    return 0;
}

static int emitter_end_collection(edn_emitter_t* em, emitter_frame_kind_t kind, const char* close) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->poisoned || em->finished)
        return -EDN_ERROR_INVALID_STATE;
    emitter_frame_t* f = emitter_top_frame(em);
    if (f == NULL || f->kind != kind) {
        em->poisoned = true;
        return -EDN_ERROR_INVALID_STATE;
    }
    if (kind == EMITTER_FRAME_MAP && (f->item_count & 1) == 1) {
        em->poisoned = true;
        return -EDN_ERROR_INVALID_STATE;
    }
    /* Pending tag at end of empty collection is a state error: the tag
     * would have nothing to attach to inside the just-closed container. */
    if (em->tag_pending || em->pending_head != NULL) {
        em->poisoned = true;
        return -EDN_ERROR_INVALID_STATE;
    }
    if (emit(&em->e, close, 1) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    em->frames_count--;

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    /* If this end balanced a captured collection payload, commit. */
    if (em->capturing && em->frames_count == em->capture_start_depth) {
        int rc = emitter_stop_capture_commit(em);
        if (rc != 0) {
            em->poisoned = true;
            return rc;
        }
        return 0; /* captured value is not a real value emission */
    }
#endif

    emitter_frame_t* parent = emitter_top_frame(em);
    if (parent != NULL) {
        parent->item_count++;
    } else {
        em->produced_top_value = true;
    }
    return 0;
}

int edn_emit_begin_list(edn_emitter_t* em) {
    return emitter_begin_collection(em, EMITTER_FRAME_LIST, "(", 1, PAYLOAD_LIST);
}
int edn_emit_end_list(edn_emitter_t* em) {
    return emitter_end_collection(em, EMITTER_FRAME_LIST, ")");
}
int edn_emit_begin_vector(edn_emitter_t* em) {
    return emitter_begin_collection(em, EMITTER_FRAME_VECTOR, "[", 1, PAYLOAD_VECTOR);
}
int edn_emit_end_vector(edn_emitter_t* em) {
    return emitter_end_collection(em, EMITTER_FRAME_VECTOR, "]");
}
int edn_emit_begin_set(edn_emitter_t* em) {
    return emitter_begin_collection(em, EMITTER_FRAME_SET, "#{", 2, PAYLOAD_SET);
}
int edn_emit_end_set(edn_emitter_t* em) {
    return emitter_end_collection(em, EMITTER_FRAME_SET, "}");
}
int edn_emit_begin_map(edn_emitter_t* em) {
    return emitter_begin_collection(em, EMITTER_FRAME_MAP, "{", 1, PAYLOAD_MAP);
}
int edn_emit_end_map(edn_emitter_t* em) {
    return emitter_end_collection(em, EMITTER_FRAME_MAP, "}");
}

/* --- tag --- */

int edn_emit_tag(edn_emitter_t* em, const char* tag) {
    if (em == NULL || tag == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->poisoned || em->finished)
        return -EDN_ERROR_INVALID_STATE;
    size_t tag_len = strlen(tag);
    if (!valid_sym_chunk(tag, tag_len))
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->tag_pending) {
        em->poisoned = true;
        return -EDN_ERROR_INVALID_STATE;
    }
    /* Build `#<tag> ` and push as a pending prefix. */
    size_t total = 1 + tag_len + 1;
    char* buf = malloc(total);
    if (buf == NULL)
        return -EDN_ERROR_OUT_OF_MEMORY;
    buf[0] = '#';
    memcpy(buf + 1, tag, tag_len);
    buf[1 + tag_len] = ' ';
    if (emitter_push_prefix(em, buf, total) != 0)
        return -EDN_ERROR_OUT_OF_MEMORY;
    em->tag_pending = true;
    return 0;
}

/* --- meta --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
int edn_emit_meta(edn_emitter_t* em) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    if (em->poisoned || em->finished)
        return -EDN_ERROR_INVALID_STATE;
    if (!em->emit_metadata_enabled)
        return -EDN_ERROR_INVALID_STATE;
    if (em->expecting_meta_payload) {
        em->poisoned = true;
        return -EDN_ERROR_INVALID_STATE;
    }
    em->expecting_meta_payload = true;
    return 0;
}
#endif

/* --- embed --- */

static payload_kind_t embed_payload_kind(const edn_value_t* v) {
    if (v == NULL)
        return PAYLOAD_NIL;
    switch (v->type) {
        case EDN_TYPE_MAP:
            return PAYLOAD_MAP;
        case EDN_TYPE_VECTOR:
            return PAYLOAD_VECTOR;
        case EDN_TYPE_SYMBOL:
            return PAYLOAD_SYMBOL;
        case EDN_TYPE_KEYWORD:
            return PAYLOAD_KEYWORD;
        default:
            return PAYLOAD_EMBED;
    }
}

int edn_emit_value(edn_emitter_t* em, const edn_value_t* v) {
    if (em == NULL)
        return -EDN_ERROR_INVALID_ARGUMENT;
    /* Use the value's actual type so that meta-payload validation rejects
     * non-meta-payload-eligible embedded values. */
    payload_kind_t kind = embed_payload_kind(v);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    if (em->expecting_meta_payload) {
        bool ok = (kind == PAYLOAD_KEYWORD || kind == PAYLOAD_SYMBOL || kind == PAYLOAD_MAP ||
                   kind == PAYLOAD_VECTOR);
        if (!ok) {
            em->poisoned = true;
            return -EDN_ERROR_INVALID_STATE;
        }
    }
#endif
    int r = emitter_pre_value(em, kind);
    if (r != 0)
        return r;
    if (emit_value(&em->e, v) != 0) {
        em->poisoned = true;
        return em->e.err;
    }
    return emitter_post_scalar(em);
}
