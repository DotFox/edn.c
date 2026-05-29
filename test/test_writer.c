/* test_writer.c - Tests for the EDN writer. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Helper: parse `input`, write back, return heap string. Returns NULL on
 * either parse or write failure. Caller frees. */
static char* write_roundtrip(const char* input) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK || r.value == NULL) {
        edn_free(r.value);
        return NULL;
    }
    char* out = edn_write(r.value);
    edn_free(r.value);
    return out;
}

/* Helper: assert that input parses, writes, and re-parses equal to the
 * original. */
static bool roundtrip_equal(const char* input) {
    edn_result_t a = edn_read(input, 0);
    if (a.error != EDN_OK)
        return false;
    char* serialized = edn_write(a.value);
    if (!serialized) {
        edn_free(a.value);
        return false;
    }
    edn_result_t b = edn_read(serialized, 0);
    bool eq = false;
    if (b.error == EDN_OK) {
        eq = edn_value_equal(a.value, b.value);
    }
    free(serialized);
    edn_free(a.value);
    edn_free(b.value);
    return eq;
}

/* --- scalar output --- */

TEST(write_nil) {
    char* s = write_roundtrip("nil");
    assert(s != NULL);
    assert_str_eq(s, "nil");
    free(s);
}

TEST(write_bool_true) {
    char* s = write_roundtrip("true");
    assert(s != NULL);
    assert_str_eq(s, "true");
    free(s);
}

TEST(write_bool_false) {
    char* s = write_roundtrip("false");
    assert(s != NULL);
    assert_str_eq(s, "false");
    free(s);
}

TEST(write_int_positive) {
    char* s = write_roundtrip("42");
    assert(s != NULL);
    assert_str_eq(s, "42");
    free(s);
}

TEST(write_int_negative) {
    char* s = write_roundtrip("-9001");
    assert(s != NULL);
    assert_str_eq(s, "-9001");
    free(s);
}

TEST(write_int_zero) {
    char* s = write_roundtrip("0");
    assert(s != NULL);
    assert_str_eq(s, "0");
    free(s);
}

TEST(write_float_basic) {
    assert_true(roundtrip_equal("3.14"));
}

TEST(write_float_negative) {
    assert_true(roundtrip_equal("-0.5"));
}

TEST(write_float_scientific) {
    assert_true(roundtrip_equal("1.5e10"));
}

/* Shortest round-trip output formatted per ECMA-262, plus an
 * EDN-specific ".0" suffix on integer-valued floats. */
TEST(write_float_shortest_one) {
    char* s = write_roundtrip("1.0");
    assert(s != NULL);
    assert_str_eq(s, "1.0");
    free(s);
}

TEST(write_float_shortest_pi_approx) {
    char* s = write_roundtrip("3.14");
    assert(s != NULL);
    assert_str_eq(s, "3.14");
    free(s);
}

TEST(write_float_shortest_tenth) {
    char* s = write_roundtrip("0.1");
    assert(s != NULL);
    assert_str_eq(s, "0.1");
    free(s);
}

TEST(write_float_shortest_scientific_fixed) {
    /* 1.5e10 is within fixed-notation range (exp 10 < 21). */
    char* s = write_roundtrip("1.5e10");
    assert(s != NULL);
    assert_str_eq(s, "15000000000.0");
    free(s);
}

TEST(write_float_shortest_negative_zero) {
    char* s = write_roundtrip("-0.0");
    assert(s != NULL);
    assert_str_eq(s, "-0.0");
    free(s);
}

TEST(write_float_shortest_hundred) {
    char* s = write_roundtrip("100.0");
    assert(s != NULL);
    assert_str_eq(s, "100.0");
    free(s);
}

TEST(write_float_shortest_small) {
    char* s = write_roundtrip("0.0001");
    assert(s != NULL);
    assert_str_eq(s, "0.0001");
    free(s);
}

TEST(write_float_shortest_below_lower_bound) {
    char* s = write_roundtrip("0.0000001");
    assert(s != NULL);
    assert_str_eq(s, "1E-7");
    free(s);
}

TEST(write_float_shortest_above_upper_bound) {
    char* s = write_roundtrip("1e21");
    assert(s != NULL);
    assert_str_eq(s, "1E21");
    free(s);
}

TEST(write_float_shortest_negative) {
    char* s = write_roundtrip("-0.5");
    assert(s != NULL);
    assert_str_eq(s, "-0.5");
    free(s);
}

TEST(write_float_shortest_positive_zero) {
    char* s = write_roundtrip("0.0");
    assert(s != NULL);
    assert_str_eq(s, "0.0");
    free(s);
}

TEST(write_float_nan) {
    char* s = write_roundtrip("##NaN");
    assert(s != NULL);
    assert_str_eq(s, "##NaN");
    free(s);
}

TEST(write_float_pos_inf) {
    char* s = write_roundtrip("##Inf");
    assert(s != NULL);
    assert_str_eq(s, "##Inf");
    free(s);
}

TEST(write_float_neg_inf) {
    char* s = write_roundtrip("##-Inf");
    assert(s != NULL);
    assert_str_eq(s, "##-Inf");
    free(s);
}

TEST(write_bigint) {
    char* s = write_roundtrip("12345678901234567890N");
    assert(s != NULL);
    assert_str_eq(s, "12345678901234567890N");
    free(s);
}

static bool bigint_roundtrip_equal(const char* input) {
    edn_result_t a = edn_read(input, 0);
    if (a.error != EDN_OK)
        return false;
    char* serialized = edn_write(a.value);
    if (!serialized) {
        edn_free(a.value);
        return false;
    }
    edn_result_t b = edn_read(serialized, 0);
    bool ok = (b.error == EDN_OK) && edn_value_equal(a.value, b.value);
    free(serialized);
    edn_free(a.value);
    edn_free(b.value);
    return ok;
}

TEST(write_bigint_decimal_roundtrip) {
    char* s = write_roundtrip("123N");
    assert(s != NULL);
    assert_str_eq(s, "123N");
    free(s);
    assert_true(bigint_roundtrip_equal("123N"));
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
TEST(write_bigint_hex_roundtrip) {
    assert_true(bigint_roundtrip_equal("0xFFN"));
}

TEST(write_bigint_octal_roundtrip) {
    char* s = write_roundtrip("077N");
    assert(s != NULL);
    assert_str_eq(s, "077N");
    free(s);
    assert_true(bigint_roundtrip_equal("077N"));
}

TEST(write_bigint_octal_canonicalizes_leading_zeros) {
    char* s = write_roundtrip("00077N");
    assert(s != NULL);
    assert_str_eq(s, "077N");
    free(s);

    edn_result_t a = edn_read("00077N", 0);
    assert(a.error == EDN_OK);
    char* serialized = edn_write(a.value);
    assert(serialized != NULL);
    edn_result_t b = edn_read(serialized, 0);
    assert(b.error == EDN_OK);
    assert_true(edn_value_equal(a.value, b.value));
    free(serialized);
    edn_free(a.value);
    edn_free(b.value);
}

TEST(write_bigint_octal_large) {
    /* 24 octal digits = 72 bits, well past int64 range, so this exercises
     * the bigint path (not the int64 fast path) end-to-end. */
    const char* input = "0777777777777777777777777N";
    char* s = write_roundtrip(input);
    assert(s != NULL);
    assert_str_eq(s, input);
    free(s);
    assert_true(bigint_roundtrip_equal(input));
}

TEST(write_bigint_radix2_small) {
    /* Small radix-2 values fit in int64 and reparse to EDN_TYPE_INT;
     * edn_value_equal still holds across the int<->bigint bridge. */
    assert_true(bigint_roundtrip_equal("2r1010"));
}

TEST(write_bigint_radix5_arbitrary) {
    assert_true(bigint_roundtrip_equal("5r1234"));
}

TEST(write_bigint_radix2_large) {
    /* 71-digit binary literal forces the parser to take the bigint path on
     * re-parse, exercising radix preservation in the bigint slot. */
    const char* input = "2r10000000000000000000000000000000000000000000000000000000000000000000000";
    assert_true(bigint_roundtrip_equal(input));
}

TEST(write_bigint_radix2_negative) {
    assert_true(bigint_roundtrip_equal("-2r1111"));
}

/* Big ratios: numerator and/or denominator overflow int64 range and so go
 * through the EDN_TYPE_BIGRATIO storage path rather than EDN_TYPE_RATIO.
 * `edn_value_equal` currently has no BIGRATIO case, so structural round-trip
 * is verified by checking that re-serializing the parsed output yields the
 * same byte sequence (serialization is a fixed point on this input). */
static bool bigratio_serialization_fixed_point(const char* input) {
    char* once = write_roundtrip(input);
    if (once == NULL)
        return false;
    char* twice = write_roundtrip(once);
    bool ok = (twice != NULL) && strcmp(once, twice) == 0;
    free(once);
    free(twice);
    return ok;
}

TEST(write_bigratio_basic) {
    const char* input = "99999999999999999999/3";
    char* s = write_roundtrip(input);
    assert(s != NULL);
    assert_str_eq(s, input);
    free(s);
    assert_true(bigratio_serialization_fixed_point(input));
}

TEST(write_bigratio_huge_denominator) {
    const char* input = "3/99999999999999999999";
    char* s = write_roundtrip(input);
    assert(s != NULL);
    assert_str_eq(s, input);
    free(s);
    assert_true(bigratio_serialization_fixed_point(input));
}

TEST(write_bigratio_both_huge) {
    const char* input = "99999999999999999999/88888888888888888888";
    char* s = write_roundtrip(input);
    assert(s != NULL);
    assert_str_eq(s, input);
    free(s);
    assert_true(bigratio_serialization_fixed_point(input));
}

TEST(write_bigratio_negative) {
    /* Sign lives on the numerator slot only; the denominator slice is the
     * positive-magnitude digits. */
    const char* input = "-99999999999999999999/3";
    char* s = write_roundtrip(input);
    assert(s != NULL);
    assert_str_eq(s, input);
    free(s);
    assert_true(bigratio_serialization_fixed_point(input));
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

TEST(write_bigdec) {
    char* s = write_roundtrip("3.1415926535897932384626433832795028841971693993751M");
    assert(s != NULL);
    assert_str_eq(s, "3.1415926535897932384626433832795028841971693993751M");
    free(s);
}

TEST(write_string_simple) {
    char* s = write_roundtrip("\"hello\"");
    assert(s != NULL);
    assert_str_eq(s, "\"hello\"");
    free(s);
}

TEST(write_string_with_escapes) {
    /* "a\nb\tc" -> source bytes a\nb\tc are emitted verbatim. */
    char* s = write_roundtrip("\"a\\nb\\tc\"");
    assert(s != NULL);
    assert_str_eq(s, "\"a\\nb\\tc\"");
    free(s);
}

TEST(write_character_letter) {
    char* s = write_roundtrip("\\a");
    assert(s != NULL);
    assert_str_eq(s, "\\a");
    free(s);
}

TEST(write_character_newline) {
    char* s = write_roundtrip("\\newline");
    assert(s != NULL);
    assert_str_eq(s, "\\newline");
    free(s);
}

TEST(write_character_unicode) {
    char* s = write_roundtrip("\\u00e9");
    assert(s != NULL);
    assert_str_eq(s, "\\u00E9");
    free(s);
}

TEST(write_character_bmp_max) {
    /* Largest BMP codepoint. The 4-digit \u parser accepts it in every build
     * mode, so round-trip is required regardless of extension flags. */
    char* s = write_roundtrip("\\uFFFF");
    assert(s != NULL);
    assert_str_eq(s, "\\uFFFF");
    free(s);
}

TEST(write_character_control_codepoint) {
    /* Low control codepoint. The 4-digit \u parser accepts it in every build
     * mode, so round-trip is required regardless of extension flags. */
    char* s = write_roundtrip("\\u0001");
    assert(s != NULL);
    assert_str_eq(s, "\\u0001");
    free(s);
}

#ifndef EDN_ENABLE_EXPERIMENTAL_EXTENSION
/* In non-EXPERIMENTAL builds the character parser only accepts exactly 4 hex
 * digits, so a supplementary codepoint cannot arise from parsing. We
 * construct one directly to verify the writer refuses rather than emitting
 * input the parser would reject. The stack value owns no arena, so we must
 * NOT call edn_free on it. */
static int sink_cb(const char* data, size_t n, void* ctx) {
    (void) data;
    (void) n;
    (void) ctx;
    return 0;
}

TEST(write_character_supplementary_rejected_without_experimental) {
    edn_value_t v = {0};
    v.type = EDN_TYPE_CHARACTER;
    v.as.character = 0x1F600;

    char* s = edn_write_string(&v, NULL, NULL);
    assert(s == NULL);

    int rc = edn_write_stream(&v, sink_cb, NULL, NULL);
    assert_int_eq(rc, -EDN_ERROR_UNSUPPORTED_TYPE);
}
#endif

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
TEST(write_character_supplementary_emits_under_experimental) {
    /* The 4-6 digit \u parser accepts 5-digit supplementary codepoints, and
     * snprintf("%04X", cp) emits the natural-width form. */
    char* s = write_roundtrip("\\u1F600");
    assert(s != NULL);
    assert_str_eq(s, "\\u1F600");
    free(s);
    assert_true(roundtrip_equal("\\u1F600"));
}
#endif

TEST(write_symbol_simple) {
    char* s = write_roundtrip("foo");
    assert(s != NULL);
    assert_str_eq(s, "foo");
    free(s);
}

TEST(write_symbol_namespaced) {
    char* s = write_roundtrip("my.ns/foo");
    assert(s != NULL);
    assert_str_eq(s, "my.ns/foo");
    free(s);
}

TEST(write_keyword_simple) {
    char* s = write_roundtrip(":kw");
    assert(s != NULL);
    assert_str_eq(s, ":kw");
    free(s);
}

TEST(write_keyword_namespaced) {
    char* s = write_roundtrip(":my.ns/kw");
    assert(s != NULL);
    assert_str_eq(s, ":my.ns/kw");
    free(s);
}

/* --- collection output --- */

TEST(write_list_empty) {
    char* s = write_roundtrip("()");
    assert(s != NULL);
    assert_str_eq(s, "()");
    free(s);
}

TEST(write_list_basic) {
    char* s = write_roundtrip("(1 2 3)");
    assert(s != NULL);
    assert_str_eq(s, "(1 2 3)");
    free(s);
}

TEST(write_vector_empty) {
    char* s = write_roundtrip("[]");
    assert(s != NULL);
    assert_str_eq(s, "[]");
    free(s);
}

TEST(write_vector_basic) {
    char* s = write_roundtrip("[:a :b :c]");
    assert(s != NULL);
    assert_str_eq(s, "[:a :b :c]");
    free(s);
}

TEST(write_set_basic) {
    assert_true(roundtrip_equal("#{1 2 3}"));
}

TEST(write_map_basic) {
    assert_true(roundtrip_equal("{:a 1 :b 2}"));
}

TEST(write_map_nested) {
    assert_true(roundtrip_equal("{:a [1 2] :b {:c 3}}"));
}

TEST(write_tagged) {
    char* s = write_roundtrip("#inst \"2024-01-01\"");
    assert(s != NULL);
    assert_str_eq(s, "#inst \"2024-01-01\"");
    free(s);
}

/* --- adapters: buffer, file, streaming --- */

TEST(write_buffer_exact_size) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    char buf[32];
    size_t needed = edn_write_buffer(r.value, buf, sizeof(buf), NULL);
    assert(needed == 7);
    assert_str_eq(buf, "[1 2 3]");

    edn_free(r.value);
}

TEST(write_buffer_truncated) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    char buf[4];
    size_t needed = edn_write_buffer(r.value, buf, sizeof(buf), NULL);
    assert(needed == 7);
    /* cap-1 bytes written + null terminator */
    assert_str_eq(buf, "[1 ");

    edn_free(r.value);
}

TEST(write_buffer_zero_cap) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    size_t needed = edn_write_buffer(r.value, NULL, 0, NULL);
    assert(needed == 3);
    edn_free(r.value);
}

TEST(write_file_tmpfile) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    FILE* fp = tmpfile();
    assert(fp != NULL);
    int rc = edn_write_file(r.value, fp, NULL);
    assert(rc == 0);

    rewind(fp);
    char buf[32] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    assert_str_eq(buf, "[1 2 3]");
    fclose(fp);

    edn_free(r.value);
}

typedef struct {
    char buf[64];
    size_t len;
} capture_ctx_t;
static int capture_cb(const char* data, size_t n, void* ctx) {
    capture_ctx_t* c = ctx;
    if (c->len + n + 1 > sizeof(c->buf))
        return -EDN_ERROR_OUT_OF_MEMORY;
    memcpy(c->buf + c->len, data, n);
    c->len += n;
    c->buf[c->len] = '\0';
    return 0;
}

TEST(write_stream_callback) {
    edn_result_t r = edn_read(":foo", 0);
    assert(r.error == EDN_OK);

    capture_ctx_t c = {{0}, 0};
    int rc = edn_write_stream(r.value, capture_cb, &c, NULL);
    assert(rc == 0);
    assert_str_eq(c.buf, ":foo");

    edn_free(r.value);
}

TEST(write_stream_callback_abort) {
    /* Long string that overflows capture_ctx_t::buf (64 bytes), forcing
     * capture_cb to return its error code mid-emission. */
    edn_result_t r = edn_read("[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]",
                              0);
    assert(r.error == EDN_OK);

    capture_ctx_t c = {{0}, 0};
    int rc = edn_write_stream(r.value, capture_cb, &c, NULL);
    assert(rc == -EDN_ERROR_OUT_OF_MEMORY);
    assert_true(c.len > 0);
    assert_true(c.len < sizeof(c.buf));

    edn_free(r.value);
}

static int abort_with_oom_cb(const char* data, size_t n, void* ctx) {
    (void) data;
    (void) n;
    (void) ctx;
    return -EDN_ERROR_OUT_OF_MEMORY;
}

TEST(write_stream_callback_propagates_oom) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    int rc = edn_write_stream(r.value, abort_with_oom_cb, NULL, NULL);
    assert(rc == -EDN_ERROR_OUT_OF_MEMORY);
    edn_free(r.value);
}

static int abort_with_io_failure_cb(const char* data, size_t n, void* ctx) {
    (void) data;
    (void) n;
    (void) ctx;
    return -EDN_ERROR_IO_FAILURE;
}

TEST(write_stream_callback_propagates_io_failure) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    int rc = edn_write_stream(r.value, abort_with_io_failure_cb, NULL, NULL);
    assert(rc == -EDN_ERROR_IO_FAILURE);
    edn_free(r.value);
}

TEST(write_stream_null_cb_invalid_argument) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    int rc = edn_write_stream(r.value, NULL, NULL, NULL);
    assert(rc == -EDN_ERROR_INVALID_ARGUMENT);
    edn_free(r.value);
}

TEST(write_file_null_fp_invalid_argument) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    int rc = edn_write_file(r.value, NULL, NULL);
    assert(rc == -EDN_ERROR_INVALID_ARGUMENT);
    edn_free(r.value);
}

TEST(write_buffer_null_buf_returns_sentinel) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);
    size_t needed = edn_write_buffer(r.value, NULL, 16, NULL);
    assert(needed == (size_t) -1);
    edn_free(r.value);
}

TEST(write_options_bogus_struct_size_invalid_argument) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = 1; /* non-zero but smaller than sizeof(opts) */

    /* edn_write_string only signals failure via NULL; probe edn_write_stream
     * to observe the exact code. */
    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s == NULL);

    int rc = edn_write_stream(r.value, abort_with_oom_cb, NULL, &opts);
    assert(rc == -EDN_ERROR_INVALID_ARGUMENT);

    edn_free(r.value);
}

/* --- options --- */

TEST(write_newline_at_end) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.newline_at_end = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "42\n");

    free(s);
    edn_free(r.value);
}

/* --- indent (pretty-print) --- */

/* Pretty-print one source string with default indent on (any non-zero
 * value enables pretty-print; the numeric value itself is ignored). */
static char* write_pretty(const char* input) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK || r.value == NULL) {
        edn_free(r.value);
        return NULL;
    }
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    char* out = edn_write_string(r.value, &opts, NULL);
    edn_free(r.value);
    return out;
}

TEST(write_indent_empty_collections_stay_compact) {
    char* a = write_pretty("()");
    char* b = write_pretty("[]");
    char* c = write_pretty("{}");
    char* d = write_pretty("#{}");
    assert(a && b && c && d);
    assert_str_eq(a, "()");
    assert_str_eq(b, "[]");
    assert_str_eq(c, "{}");
    assert_str_eq(d, "#{}");
    free(a);
    free(b);
    free(c);
    free(d);
}

TEST(write_indent_single_element_collections_stay_inline) {
    char* a = write_pretty("(1)");
    char* b = write_pretty("[1]");
    char* c = write_pretty("{:a 1}");
    char* d = write_pretty("#{1}");
    assert(a && b && c && d);
    assert_str_eq(a, "(1)");
    assert_str_eq(b, "[1]");
    assert_str_eq(c, "{:a 1}");
    assert_str_eq(d, "#{1}");
    free(a);
    free(b);
    free(c);
    free(d);
}

TEST(write_indent_vector_multi) {
    char* s = write_pretty("[1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "[1\n 2\n 3]");
    free(s);
}

TEST(write_indent_list_multi) {
    char* s = write_pretty("(1 2 3)");
    assert(s != NULL);
    assert_str_eq(s, "(1\n 2\n 3)");
    free(s);
}

TEST(write_indent_set_multi_two_space_step) {
    char* s = write_pretty("#{1 2 3}");
    assert(s != NULL);
    assert_str_eq(s, "#{1\n  2\n  3}");
    free(s);
}

TEST(write_indent_map_multi) {
    char* s = write_pretty("{:a 1 :b 2}");
    assert(s != NULL);
    assert_str_eq(s, "{:a 1\n :b 2}");
    free(s);
}

TEST(write_indent_nested_vector_in_vector) {
    char* s = write_pretty("[[1 2] [3 4]]");
    assert(s != NULL);
    assert_str_eq(s, "[[1\n  2]\n [3\n  4]]");
    free(s);
}

TEST(write_indent_nested_value_in_map) {
    char* s = write_pretty("{:a [1 2] :b 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:a [1\n     2]\n :b 3}");
    free(s);
}

TEST(write_indent_nested_key_in_map) {
    char* s = write_pretty("{[1 2] :v :a :b}");
    assert(s != NULL);
    assert_str_eq(s, "{[1\n  2] :v\n :a :b}");
    free(s);
}

TEST(write_indent_set_with_nested_vector) {
    char* s = write_pretty("#{[1 2] 3}");
    assert(s != NULL);
    assert_str_eq(s, "#{[1\n   2]\n  3}");
    free(s);
}

TEST(write_indent_with_sort_unordered) {
    edn_result_t r = edn_read("{:c 1 :a 2 :b 3}", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    opts.sort_unordered = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "{:a 2\n :b 3\n :c 1}");
    free(s);
    edn_free(r.value);
}

TEST(write_indent_with_newline_at_end) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    opts.newline_at_end = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "[1\n 2\n 3]\n");
    free(s);
    edn_free(r.value);
}

TEST(write_indent_with_escape_unicode_in_collection) {
    edn_result_t r = edn_read("[\"café\" 42]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    opts.escape_unicode = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "[\"caf\\u00E9\"\n 42]");
    free(s);
    edn_free(r.value);
}

TEST(write_indent_tagged_with_collection) {
    /* `#foo ` then `[`, so the inner vector's indent column is 6. */
    char* s = write_pretty("#foo [1 2]");
    assert(s != NULL);
    assert_str_eq(s, "#foo [1\n      2]");
    free(s);
}

/* Round-trip: parse → write-with-indent → parse → write-compact yields
 * exactly what we'd get parsing the original and writing it compactly. */
TEST(write_indent_roundtrip_via_pretty) {
    const char* input = "{:a [1 2 3] :b #{4 5} :c \"hi\"}";

    edn_result_t r1 = edn_read(input, 0);
    assert(r1.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    char* pretty = edn_write_string(r1.value, &opts, NULL);
    assert(pretty != NULL);

    edn_result_t r2 = edn_read(pretty, 0);
    assert(r2.error == EDN_OK);

    char* compact_via_pretty = edn_write(r2.value);
    char* compact_direct = edn_write(r1.value);
    assert(compact_via_pretty != NULL);
    assert(compact_direct != NULL);
    assert_str_eq(compact_via_pretty, compact_direct);

    free(pretty);
    free(compact_via_pretty);
    free(compact_direct);
    edn_free(r1.value);
    edn_free(r2.value);
}

/* Backward compat: indent == 0 produces byte-identical compact output. */
TEST(write_indent_zero_matches_compact) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 0;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "[1 2 3]");
    free(s);
    edn_free(r.value);
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
TEST(write_indent_with_metadata_short_keyword) {
    edn_result_t r = edn_read("^:dynamic [1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    opts.emit_metadata = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "^:dynamic [1\n           2\n           3]");
    free(s);
    edn_free(r.value);
}

TEST(write_indent_with_metadata_full_map_stays_compact) {
    edn_result_t r = edn_read("^{:a 1 :b 2} [1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    opts.emit_metadata = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "^{:a 1, :b 2} [1\n               2\n               3]");
    free(s);
    edn_free(r.value);
}
#endif

/* --- sort_unordered --- */

static char* write_sorted(const char* input) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK || r.value == NULL) {
        edn_free(r.value);
        return NULL;
    }
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.sort_unordered = true;
    char* out = edn_write_string(r.value, &opts, NULL);
    edn_free(r.value);
    return out;
}

TEST(write_sort_unordered_basic) {
    char* s = write_sorted("{:b 1 :a 2 :c 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:a 2, :b 1, :c 3}");
    free(s);
}

TEST(write_sort_unordered_mixed_types) {
    /* Byte-wise lex order over serialized keys:
     *   "\"a\"" starts with 0x22, "1" starts with 0x31, ":b" starts with 0x3A
     * Expected order: "a", 1, :b */
    char* s = write_sorted("{:b 1 \"a\" 2 1 3}");
    assert(s != NULL);
    assert_str_eq(s, "{\"a\" 2, 1 3, :b 1}");
    free(s);
}

TEST(write_sort_unordered_nested) {
    char* s = write_sorted("{:b {:y 1 :x 2} :a 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:a 3, :b {:x 2, :y 1}}");
    free(s);
}

TEST(write_sort_unordered_empty_and_singleton) {
    char* sorted_empty = write_sorted("{}");
    assert(sorted_empty != NULL);
    assert_str_eq(sorted_empty, "{}");
    free(sorted_empty);

    char* sorted_one = write_sorted("{:a 1}");
    assert(sorted_one != NULL);
    assert_str_eq(sorted_one, "{:a 1}");
    free(sorted_one);

    char* unsorted_empty = write_roundtrip("{}");
    assert(unsorted_empty != NULL);
    assert_str_eq(unsorted_empty, "{}");
    free(unsorted_empty);

    char* unsorted_one = write_roundtrip("{:a 1}");
    assert(unsorted_one != NULL);
    assert_str_eq(unsorted_one, "{:a 1}");
    free(unsorted_one);
}

TEST(write_sort_unordered_off_preserves_order) {
    char* s = write_roundtrip("{:b 1, :a 2, :c 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:b 1, :a 2, :c 3}");
    free(s);
}

TEST(write_sort_unordered_set_basic) {
    char* s = write_sorted("#{:c :a :b}");
    assert(s != NULL);
    assert_str_eq(s, "#{:a :b :c}");
    free(s);
}

TEST(write_sort_unordered_set_mixed_types) {
    /* Same byte-wise rationale as write_sort_unordered_mixed_types:
     *   "\"a\"" -> 0x22, "1" -> 0x31, ":b" -> 0x3A. */
    char* s = write_sorted("#{:b \"a\" 1}");
    assert(s != NULL);
    assert_str_eq(s, "#{\"a\" 1 :b}");
    free(s);
}

TEST(write_sort_unordered_set_nested) {
    /* Inner set sorts to "#{:x :y}" (starts with '#' = 0x23), which is less
     * than ":a" (starts with ':' = 0x3A). */
    char* s = write_sorted("#{#{:y :x} :a}");
    assert(s != NULL);
    assert_str_eq(s, "#{#{:x :y} :a}");
    free(s);
}

TEST(write_sort_unordered_set_inside_map) {
    char* s = write_sorted("{:s #{:c :a :b}}");
    assert(s != NULL);
    assert_str_eq(s, "{:s #{:a :b :c}}");
    free(s);
}

TEST(write_sort_unordered_set_off_preserves_order) {
    char* s = write_roundtrip("#{:c :a :b}");
    assert(s != NULL);
    assert_str_eq(s, "#{:c :a :b}");
    free(s);
}

/* Idempotency: sorting an already-sorted output must be a fixed point. */

static void assert_sort_idempotent(const char* input) {
    char* s1 = write_sorted(input);
    assert(s1 != NULL);
    char* s2 = write_sorted(s1);
    assert(s2 != NULL);
    assert_str_eq(s1, s2);
    free(s1);
    free(s2);
}

TEST(write_sort_unordered_idempotent_map) {
    assert_sort_idempotent("{:c 1 :a 2 :b 3}");
}

TEST(write_sort_unordered_idempotent_set) {
    assert_sort_idempotent("#{:c :a :b}");
}

TEST(write_sort_unordered_idempotent_nested) {
    assert_sort_idempotent("{:b {:y 1 :x 2} :a [1 2 3] :c #{:c :a :b}}");
}

/* Emit-abort cleanup on sort paths. The assert checks the propagated
 * error code; ASan (`make DEBUG=1`) validates the cleanup itself. */

typedef struct {
    size_t budget;
    size_t consumed;
} budget_cb_ctx_t;

static int budget_cb(const char* data, size_t n, void* ctx) {
    (void) data;
    budget_cb_ctx_t* c = ctx;
    if (c->consumed + n > c->budget) {
        return -EDN_ERROR_IO_FAILURE;
    }
    c->consumed += n;
    return 0;
}

TEST(write_sort_unordered_map_abort_cleans_up) {
    /* 5-byte budget aborts emission mid-map. */
    edn_result_t r = edn_read("{:c 1 :a 2 :b 3}", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.sort_unordered = true;

    budget_cb_ctx_t ctx = {5, 0};
    int rc = edn_write_stream(r.value, budget_cb, &ctx, &opts);
    edn_free(r.value);
    assert_int_eq(rc, -EDN_ERROR_IO_FAILURE);
}

TEST(write_sort_unordered_set_abort_cleans_up) {
    edn_result_t r = edn_read("#{:c :a :b}", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.sort_unordered = true;

    budget_cb_ctx_t ctx = {5, 0};
    int rc = edn_write_stream(r.value, budget_cb, &ctx, &opts);
    edn_free(r.value);
    assert_int_eq(rc, -EDN_ERROR_IO_FAILURE);
}

/* Round-trip property corpus. */

TEST(write_roundtrip_corpus) {
    static const char* const corpus[] = {
        /* scalars */
        "nil",
        "true",
        "false",
        "0",
        "1",
        "-1",
        "42",
        "-9223372036854775808",
        "9223372036854775807",
        "0.0",
        "-0.0",
        "1.0",
        "3.14",
        "1.5e10",
        "##NaN",
        "##Inf",
        "##-Inf",
        "123N",
        "\"\"",
        "\"hello\"",
        "\"with spaces\"",
        "\"a\\nb\\tc\"",
        "\\a",
        "\\newline",
        "\\space",
        "\\u00E9",
        "foo",
        "my.ns/foo",
        ":kw",
        ":my.ns/kw",

        /* collections */
        "()",
        "(1)",
        "(1 2 3)",
        "(1 (2 (3)))",
        "[]",
        "[1]",
        "[1 2 3]",
        "[[1] [2 [3]]]",
        "#{}",
        "#{1 2 3}",
        "{}",
        "{:a 1}",
        "{:a 1 :b 2 :c 3}",

        /* mixed */
        "[1 :a \"s\" \\c true nil]",
        "{[:a :b] 1}",
        "#inst \"2024-01-01\"",
    };
    size_t n = sizeof(corpus) / sizeof(*corpus);
    for (size_t i = 0; i < n; i++) {
        if (!roundtrip_equal(corpus[i])) {
            printf("Round-trip failure for: %s\n", corpus[i]);
            assert_true(false);
        }
    }
}

#ifndef EDN_ENABLE_CLOJURE_EXTENSION
TEST(write_meta_rejects_when_extension_off) {
    edn_result_t r = edn_read("nil", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s == NULL);

    edn_free(r.value);
}
#endif

/* --- escape_unicode --- */

static char* write_with_escape_unicode(const char* input) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK || r.value == NULL) {
        edn_free(r.value);
        return NULL;
    }
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.escape_unicode = true;
    char* out = edn_write_string(r.value, &opts, NULL);
    edn_free(r.value);
    return out;
}

TEST(write_escape_unicode_ascii_passthrough) {
    char* s = write_with_escape_unicode("\"hello world\"");
    assert(s != NULL);
    assert_str_eq(s, "\"hello world\"");
    free(s);
}

TEST(write_escape_unicode_bmp_2byte) {
    /* U+00E9 (é) -> \u00E9 */
    char* s = write_with_escape_unicode("\"café\"");
    assert(s != NULL);
    assert_str_eq(s, "\"caf\\u00E9\"");
    free(s);
}

TEST(write_escape_unicode_bmp_3byte) {
    /* U+65E5 (日) and U+672C (本) */
    char* s = write_with_escape_unicode("\"日本\"");
    assert(s != NULL);
    assert_str_eq(s, "\"\\u65E5\\u672C\"");
    free(s);
}

TEST(write_escape_unicode_supplementary_passthrough) {
    /* U+1F600 (😀) -> raw UTF-8 bytes, not surrogate pair. */
    char* s = write_with_escape_unicode("\"😀\"");
    assert(s != NULL);
    assert_str_eq(s, "\"\xF0\x9F\x98\x80\"");
    free(s);
}

TEST(write_escape_unicode_preserves_existing_escapes) {
    char* s = write_with_escape_unicode("\"a\\nb\\u00E9c\"");
    assert(s != NULL);
    assert_str_eq(s, "\"a\\nb\\u00E9c\"");
    free(s);
}

TEST(write_escape_unicode_mixed) {
    char* s = write_with_escape_unicode("\"hello, café! 😀\"");
    assert(s != NULL);
    assert_str_eq(s, "\"hello, caf\\u00E9! \xF0\x9F\x98\x80\"");
    free(s);
}

TEST(write_escape_unicode_off_preserves_raw_utf8) {
    char* s = write_roundtrip("\"café\"");
    assert(s != NULL);
    assert_str_eq(s, "\"café\"");
    free(s);
}

TEST(write_escape_unicode_empty_string) {
    char* s = write_with_escape_unicode("\"\"");
    assert(s != NULL);
    assert_str_eq(s, "\"\"");
    free(s);
}

TEST(write_escape_unicode_inside_collection) {
    char* s = write_with_escape_unicode("[\"café\" 42]");
    assert(s != NULL);
    assert_str_eq(s, "[\"caf\\u00E9\" 42]");
    free(s);
}

TEST(write_escape_unicode_does_not_affect_keyword_with_utf8) {
    /* EDN identifiers have no escape syntax — raw UTF-8 only. */
    char* s = write_with_escape_unicode(":café");
    assert(s != NULL);
    assert_str_eq(s, ":café");
    free(s);
}

TEST(write_escape_unicode_does_not_affect_character) {
    /* Character emitter already escapes non-ASCII; flag must not double-process. */
    char* s = write_with_escape_unicode("\\u00E9");
    assert(s != NULL);
    assert_str_eq(s, "\\u00E9");
    free(s);
}

TEST(write_escape_unicode_with_sort_unordered) {
    /* Sort key for "é" becomes "\u00E9" with escape_unicode on. Byte-wise:
     * both start with `"`, then `\` (0x5C) < `a` (0x61), so the escaped
     * string sorts first — opposite of the raw-UTF-8 byte order. */
    edn_result_t r = edn_read("{\"é\" 1 \"a\" 2}", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.escape_unicode = true;
    opts.sort_unordered = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "{\"\\u00E9\" 1, \"a\" 2}");
    free(s);
    edn_free(r.value);
}

/* --- emit_metadata (Clojure extension only) --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
/* Like write_roundtrip but with emit_metadata=true. */
static char* write_with_metadata(const char* input) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK || r.value == NULL) {
        edn_free(r.value);
        return NULL;
    }
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;
    char* out = edn_write_string(r.value, &opts, NULL);
    edn_free(r.value);
    return out;
}

/* Round-trip property check that also verifies metadata structure is
 * preserved (recall edn_value_equal ignores metadata). */
static bool meta_roundtrip_ok(const char* input) {
    edn_result_t a = edn_read(input, 0);
    if (a.error != EDN_OK)
        return false;
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;
    char* s = edn_write_string(a.value, &opts, NULL);
    if (!s) {
        edn_free(a.value);
        return false;
    }
    edn_result_t b = edn_read(s, 0);
    bool ok = (b.error == EDN_OK) && edn_value_equal(a.value, b.value);
    if (ok) {
        edn_value_t* ma = edn_value_meta(a.value);
        edn_value_t* mb = edn_value_meta(b.value);
        ok = ((ma == NULL && mb == NULL) || (ma != NULL && mb != NULL && edn_value_equal(ma, mb)));
    }
    free(s);
    edn_free(a.value);
    edn_free(b.value);
    return ok;
}

/* Short-form classification: each rule emits the minimal syntactic form. */

TEST(write_meta_tag_symbol_short) {
    char* s = write_with_metadata("^String foo");
    assert(s != NULL);
    assert_str_eq(s, "^String foo");
    free(s);
}

TEST(write_meta_param_tags_short) {
    char* s = write_with_metadata("^[java.lang.String long _] sym");
    assert(s != NULL);
    assert_str_eq(s, "^[java.lang.String long _] sym");
    free(s);
}

TEST(write_meta_true_keyword_short) {
    char* s = write_with_metadata("^:dynamic [1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "^:dynamic [1 2 3]");
    free(s);
}

TEST(write_meta_true_namespaced_keyword_short) {
    char* s = write_with_metadata("^:my.ns/dynamic [1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "^:my.ns/dynamic [1 2 3]");
    free(s);
}

/* Cases that must NOT take short form. */

TEST(write_meta_tag_string_full) {
    char* s = write_with_metadata("^\"java.lang.String\" sym");
    assert(s != NULL);
    assert_str_eq(s, "^{:tag \"java.lang.String\"} sym");
    free(s);
}

TEST(write_meta_multi_key_full) {
    char* s = write_with_metadata("^{:foo 1 :bar 2} [1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "^{:foo 1, :bar 2} [1 2 3]");
    free(s);
}

TEST(write_meta_keyword_non_true_full) {
    char* s = write_with_metadata("^{:dynamic false} [1]");
    assert(s != NULL);
    assert_str_eq(s, "^{:dynamic false} [1]");
    free(s);
}

TEST(write_meta_namespaced_tag_full) {
    char* s = write_with_metadata("^{:my.ns/tag String} [1]");
    assert(s != NULL);
    assert_str_eq(s, "^{:my.ns/tag String} [1]");
    free(s);
}

TEST(write_meta_empty_map) {
    char* s = write_with_metadata("^{} [1]");
    assert(s != NULL);
    assert_str_eq(s, "^{} [1]");
    free(s);
}

/* Option gates. */

TEST(write_meta_disabled_skips_meta) {
    char* s = write_roundtrip("^:dynamic [1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "[1 2 3]");
    free(s);
}

TEST(write_meta_no_meta_on_value) {
    char* s = write_with_metadata("[1 2 3]");
    assert(s != NULL);
    assert_str_eq(s, "[1 2 3]");
    free(s);
}

/* Recursive / interaction. */

TEST(write_meta_nested_in_value) {
    /* Metadata can only attach to collections, tagged literals, and symbols,
     * so the inner form is a vector rather than a bare scalar. */
    char* s = write_with_metadata("^:outer [^:inner [1] 2]");
    assert(s != NULL);
    assert_str_eq(s, "^:outer [^:inner [1] 2]");
    free(s);
}

TEST(write_meta_on_tagged) {
    char* s = write_with_metadata("^:tagged #inst \"2024-01-01\"");
    assert(s != NULL);
    assert_str_eq(s, "^:tagged #inst \"2024-01-01\"");
    free(s);
}

TEST(write_meta_with_sort_unordered) {
    edn_result_t r = edn_read("^{:b 2 :a 1} [1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;
    opts.sort_unordered = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s != NULL);
    assert_str_eq(s, "^{:a 1, :b 2} [1 2 3]");

    free(s);
    edn_free(r.value);
}

TEST(write_meta_roundtrip_structure) {
    static const char* const corpus[] = {
        "^Sym foo", "^[a b] sym", "^:k [1]", "^{:a 1} [1]", "^{} [1]", "^:o [^:i [1]]",
    };
    size_t n = sizeof(corpus) / sizeof(*corpus);
    for (size_t i = 0; i < n; i++) {
        if (!meta_roundtrip_ok(corpus[i])) {
            printf("Metadata round-trip failure for: %s\n", corpus[i]);
            assert_true(false);
        }
    }
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

/* --- writer registry scaffold --- */

TEST(write_registry_create_destroy) {
    edn_writer_registry_t* r = edn_writer_registry_create();
    assert(r != NULL);
    edn_writer_registry_destroy(r);
}

/* ============================================================
 * Streaming emitter (edn_emitter_*) tests
 * ============================================================ */

typedef struct {
    char buf[4096];
    size_t len;
} emit_capture_t;

static int emit_capture_cb(const char* data, size_t n, void* ctx) {
    emit_capture_t* c = ctx;
    if (c->len + n + 1 > sizeof(c->buf))
        return -EDN_ERROR_OUT_OF_MEMORY;
    memcpy(c->buf + c->len, data, n);
    c->len += n;
    c->buf[c->len] = '\0';
    return 0;
}

/* --- lifecycle / validation --- */

TEST(emitter_create_null_callback) {
    edn_emitter_t* em = edn_emitter_create(NULL, NULL, NULL);
    assert_ptr_eq(em, NULL);
}

TEST(emitter_create_bad_struct_size) {
    edn_write_options_t opts = {0};
    opts.struct_size = 4; /* nonzero but smaller than struct */
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert_ptr_eq(em, NULL);
}

TEST(emitter_create_sort_unordered_rejected) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.sort_unordered = true;
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert_ptr_eq(em, NULL);
}

TEST(emitter_create_writer_registry_rejected) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.writer_registry = edn_writer_registry_create();
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert_ptr_eq(em, NULL);
    edn_writer_registry_destroy(opts.writer_registry);
}

TEST(emitter_destroy_null_ok) {
    edn_emitter_destroy(NULL);
}

TEST(emitter_destroy_without_finish_ok) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* Begin a vector but never finish; destroy must not crash. */
    assert_int_eq(edn_emit_begin_vector(em), 0);
    assert_int_eq(edn_emit_int(em, 1), 0);
    edn_emitter_destroy(em);
}

TEST(emitter_finish_empty_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    int rc = edn_emitter_finish(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emitter_finish_then_emit_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_nil(em), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    int rc = edn_emit_nil(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emitter_finish_with_newline_at_end) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.newline_at_end = true;
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert(em != NULL);
    assert_int_eq(edn_emit_nil(em), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "nil\n");
    edn_emitter_destroy(em);
}

/* --- scalars --- */

/* Mini helper: run `body` in a fresh emitter and compare captured output. */
#define EMITTER_OUTPUT_EQ(body, expected)                               \
    do {                                                                \
        emit_capture_t c = {{0}, 0};                                    \
        edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, 0); \
        assert(em != NULL);                                             \
        body;                                                           \
        assert_int_eq(edn_emitter_finish(em), 0);                       \
        assert_str_eq(c.buf, expected);                                 \
        edn_emitter_destroy(em);                                        \
    } while (0)

TEST(emit_nil) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_nil(em), 0); }, "nil");
}

TEST(emit_bool_true) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_bool(em, true), 0); }, "true");
}

TEST(emit_bool_false) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_bool(em, false), 0); }, "false");
}

TEST(emit_int_zero) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_int(em, 0), 0); }, "0");
}

TEST(emit_int_min) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_int(em, INT64_MIN), 0); }, "-9223372036854775808");
}

TEST(emit_int_max) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_int(em, INT64_MAX), 0); }, "9223372036854775807");
}

TEST(emit_double_simple) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_double(em, 1.5), 0); }, "1.5");
}

TEST(emit_double_special) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_double(em, NAN), 0); }, "##NaN");
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_double(em, INFINITY), 0); }, "##Inf");
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_double(em, -INFINITY), 0); }, "##-Inf");
}

TEST(emit_string_simple) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_string(em, "hi", 2), 0); }, "\"hi\"");
}

TEST(emit_string_with_escapes) {
    /* Caller pre-encoded the EDN escapes into bytes. */
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_string(em, "a\\nb", 4), 0); }, "\"a\\nb\"");
}

TEST(emit_string_len_sentinel) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_string(em, "hi", (size_t) -1), 0); }, "\"hi\"");
}

TEST(emit_string_invalid_utf8_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* Bare 0xC3 with no continuation byte is invalid UTF-8. */
    int rc = edn_emit_string(em, "\xC3", 1);
    assert_int_eq(rc, -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_keyword_simple) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_keyword(em, "kw"), 0); }, ":kw");
}

TEST(emit_keyword_ns) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_keyword_ns(em, "my.ns", "kw"), 0); }, ":my.ns/kw");
}

TEST(emit_symbol_simple) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_symbol(em, "foo"), 0); }, "foo");
}

TEST(emit_symbol_ns) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_symbol_ns(em, "ns", "foo"), 0); }, "ns/foo");
}

TEST(emit_keyword_invalid_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* Embedded space. */
    assert_int_eq(edn_emit_keyword(em, "a b"), -EDN_ERROR_INVALID_ARGUMENT);
    /* Empty. */
    assert_int_eq(edn_emit_keyword(em, ""), -EDN_ERROR_INVALID_ARGUMENT);
    /* Leading ':'. */
    assert_int_eq(edn_emit_keyword(em, ":kw"), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_symbol_invalid_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* Leading digit. */
    assert_int_eq(edn_emit_symbol(em, "1abc"), -EDN_ERROR_INVALID_ARGUMENT);
    /* Embedded delimiter. */
    assert_int_eq(edn_emit_symbol(em, "a b"), -EDN_ERROR_INVALID_ARGUMENT);
    /* Empty. */
    assert_int_eq(edn_emit_symbol(em, ""), -EDN_ERROR_INVALID_ARGUMENT);
    /* +5 looks like a signed number. */
    assert_int_eq(edn_emit_symbol(em, "+5"), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_character_ascii) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_character(em, 'a'), 0); }, "\\a");
}

TEST(emit_character_named) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_character(em, '\n'), 0); }, "\\newline");
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_character(em, ' '), 0); }, "\\space");
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_character(em, '\t'), 0); }, "\\tab");
}

TEST(emit_character_unicode_bmp) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_character(em, 0x00E9), 0); }, "\\u00E9");
}

TEST(emit_character_invalid_codepoint_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_character(em, 0x110000), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_character_surrogate_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_character(em, 0xD800), -EDN_ERROR_INVALID_ARGUMENT);
    assert_int_eq(edn_emit_character(em, 0xDFFF), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_top_level_double_value_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_int(em, 1), 0);
    int rc = edn_emit_int(em, 2);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

/* --- collections --- */

TEST(emit_empty_vector) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_vector(em), 0);
            assert_int_eq(edn_emit_end_vector(em), 0);
        },
        "[]");
}

TEST(emit_vector_with_scalars) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_vector(em), 0);
            assert_int_eq(edn_emit_int(em, 1), 0);
            assert_int_eq(edn_emit_int(em, 2), 0);
            assert_int_eq(edn_emit_int(em, 3), 0);
            assert_int_eq(edn_emit_end_vector(em), 0);
        },
        "[1 2 3]");
}

TEST(emit_nested_vector) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_vector(em), 0);
            assert_int_eq(edn_emit_int(em, 1), 0);
            assert_int_eq(edn_emit_begin_vector(em), 0);
            assert_int_eq(edn_emit_int(em, 2), 0);
            assert_int_eq(edn_emit_end_vector(em), 0);
            assert_int_eq(edn_emit_end_vector(em), 0);
        },
        "[1 [2]]");
}

TEST(emit_empty_list) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_list(em), 0);
            assert_int_eq(edn_emit_end_list(em), 0);
        },
        "()");
}

TEST(emit_list_with_scalars) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_list(em), 0);
            assert_int_eq(edn_emit_keyword(em, "a"), 0);
            assert_int_eq(edn_emit_keyword(em, "b"), 0);
            assert_int_eq(edn_emit_end_list(em), 0);
        },
        "(:a :b)");
}

TEST(emit_empty_set) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_set(em), 0);
            assert_int_eq(edn_emit_end_set(em), 0);
        },
        "#{}");
}

TEST(emit_set_with_scalars) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_set(em), 0);
            assert_int_eq(edn_emit_int(em, 1), 0);
            assert_int_eq(edn_emit_int(em, 2), 0);
            assert_int_eq(edn_emit_end_set(em), 0);
        },
        "#{1 2}");
}

TEST(emit_empty_map) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_map(em), 0);
            assert_int_eq(edn_emit_end_map(em), 0);
        },
        "{}");
}

TEST(emit_map_with_pairs) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_begin_map(em), 0);
            assert_int_eq(edn_emit_keyword(em, "a"), 0);
            assert_int_eq(edn_emit_int(em, 1), 0);
            assert_int_eq(edn_emit_keyword(em, "b"), 0);
            assert_int_eq(edn_emit_int(em, 2), 0);
            assert_int_eq(edn_emit_end_map(em), 0);
        },
        "{:a 1, :b 2}");
}

TEST(emit_mismatched_end_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_begin_vector(em), 0);
    int rc = edn_emit_end_map(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_map_odd_count_at_end_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_begin_map(em), 0);
    assert_int_eq(edn_emit_keyword(em, "a"), 0);
    int rc = edn_emit_end_map(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_end_without_begin_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    int rc = edn_emit_end_vector(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_max_depth_exceeded) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* EDN_DEFAULT_MAX_DEPTH = 1024 -> opening 1024 vectors succeeds, the
     * 1025th fails. */
    for (size_t i = 0; i < EDN_DEFAULT_MAX_DEPTH; i++) {
        assert_int_eq(edn_emit_begin_vector(em), 0);
    }
    int rc = edn_emit_begin_vector(em);
    assert_int_eq(rc, -EDN_ERROR_MAX_DEPTH_EXCEEDED);
    edn_emitter_destroy(em);
}

/* --- indent interaction --- */

/* Compare streaming output against value-tree writer for the same source. */
static bool streaming_matches_value_tree(const char* edn_input, void (*build)(edn_emitter_t*),
                                         const edn_write_options_t* opts) {
    edn_result_t r = edn_read(edn_input, 0);
    if (r.error != EDN_OK) {
        edn_free(r.value);
        return false;
    }
    char* expected = edn_write_string(r.value, opts, NULL);
    edn_free(r.value);
    if (expected == NULL)
        return false;

    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, opts);
    if (em == NULL) {
        free(expected);
        return false;
    }
    build(em);
    int fr = edn_emitter_finish(em);
    edn_emitter_destroy(em);
    if (fr != 0) {
        free(expected);
        return false;
    }
    bool eq = strcmp(c.buf, expected) == 0;
    if (!eq) {
        printf("\n    streaming: \"%s\"\n", c.buf);
        printf("    expected:  \"%s\"\n", expected);
    }
    free(expected);
    return eq;
}

static void build_vec_123(edn_emitter_t* em) {
    edn_emit_begin_vector(em);
    edn_emit_int(em, 1);
    edn_emit_int(em, 2);
    edn_emit_int(em, 3);
    edn_emit_end_vector(em);
}

TEST(emit_vector_with_indent_matches_value_tree) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    assert_true(streaming_matches_value_tree("[1 2 3]", build_vec_123, &opts));
}

static void build_nested_map(edn_emitter_t* em) {
    edn_emit_begin_map(em);
    edn_emit_keyword(em, "a");
    edn_emit_begin_vector(em);
    edn_emit_int(em, 1);
    edn_emit_int(em, 2);
    edn_emit_end_vector(em);
    edn_emit_keyword(em, "b");
    edn_emit_int(em, 3);
    edn_emit_end_map(em);
}

TEST(emit_nested_map_with_indent_matches_value_tree) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    assert_true(streaming_matches_value_tree("{:a [1 2] :b 3}", build_nested_map, &opts));
}

TEST(emit_set_with_indent_two_space_step) {
    /* `#{` is two columns wide; pretty-printed children align two spaces in. */
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert(em != NULL);
    assert_int_eq(edn_emit_begin_set(em), 0);
    assert_int_eq(edn_emit_int(em, 1), 0);
    assert_int_eq(edn_emit_int(em, 2), 0);
    assert_int_eq(edn_emit_end_set(em), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "#{1\n  2}");
    edn_emitter_destroy(em);
}

/* --- tagged literals --- */

TEST(emit_tag_then_value) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_tag(em, "foo"), 0);
            assert_int_eq(edn_emit_int(em, 42), 0);
        },
        "#foo 42");
}

TEST(emit_tag_then_collection) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_tag(em, "foo"), 0);
            assert_int_eq(edn_emit_begin_vector(em), 0);
            assert_int_eq(edn_emit_int(em, 1), 0);
            assert_int_eq(edn_emit_int(em, 2), 0);
            assert_int_eq(edn_emit_end_vector(em), 0);
        },
        "#foo [1 2]");
}

TEST(emit_tag_invalid_identifier_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    int rc = edn_emit_tag(em, "a b");
    assert_int_eq(rc, -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_tag_namespaced) {
    EMITTER_OUTPUT_EQ(
        {
            assert_int_eq(edn_emit_tag(em, "my/tag"), 0);
            assert_int_eq(edn_emit_int(em, 42), 0);
        },
        "#my/tag 42");
}

TEST(emit_tag_invalid_namespace_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    int rc = edn_emit_tag(em, "/tag");
    assert_int_eq(rc, -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_tag_twice_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_tag(em, "foo"), 0);
    int rc = edn_emit_tag(em, "bar");
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_tag_then_finish_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_tag(em, "foo"), 0);
    int rc = edn_emitter_finish(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_tag_with_indent) {
    /* Tag prefix sits inline; the nested collection pretty-prints from the
     * column AFTER the trailing space. */
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 1;
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, &opts);
    assert(em != NULL);
    assert_int_eq(edn_emit_tag(em, "foo"), 0);
    assert_int_eq(edn_emit_begin_vector(em), 0);
    assert_int_eq(edn_emit_int(em, 1), 0);
    assert_int_eq(edn_emit_int(em, 2), 0);
    assert_int_eq(edn_emit_end_vector(em), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    /* `#foo [` puts `[` at column 5; items align at column 6. */
    assert_str_eq(c.buf, "#foo [1\n      2]");
    edn_emitter_destroy(em);
}

/* --- big numbers --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
TEST(emit_bigint_decimal) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_bigint(em, "12345", 10), 0); }, "12345N");
}

TEST(emit_bigint_hex) {
    EMITTER_OUTPUT_EQ({ assert_int_eq(edn_emit_bigint(em, "FF", 16), 0); }, "0xFFN");
}

TEST(emit_bigint_invalid_radix_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_bigint(em, "10", 1), -EDN_ERROR_INVALID_ARGUMENT);
    assert_int_eq(edn_emit_bigint(em, "10", 37), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_bigint_invalid_digits_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_bigint(em, "12X", 10), -EDN_ERROR_INVALID_ARGUMENT);
    assert_int_eq(edn_emit_bigint(em, "", 10), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_bigratio) {
    EMITTER_OUTPUT_EQ(
        { assert_int_eq(edn_emit_bigratio(em, "99999999999999999999", "3"), 0); },
        "99999999999999999999/3");
}

TEST(emit_bigratio_invalid_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    /* Denominator may not be signed. */
    assert_int_eq(edn_emit_bigratio(em, "1", "-3"), -EDN_ERROR_INVALID_ARGUMENT);
    /* Empty numerator. */
    assert_int_eq(edn_emit_bigratio(em, "", "3"), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

TEST(emit_bigdecimal) {
    EMITTER_OUTPUT_EQ(
        { assert_int_eq(edn_emit_bigdecimal(em, "3.14159265358979"), 0); }, "3.14159265358979M");
}

TEST(emit_bigdecimal_invalid_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_bigdecimal(em, "abc"), -EDN_ERROR_INVALID_ARGUMENT);
    assert_int_eq(edn_emit_bigdecimal(em, "1."), -EDN_ERROR_INVALID_ARGUMENT);
    assert_int_eq(edn_emit_bigdecimal(em, ""), -EDN_ERROR_INVALID_ARGUMENT);
    edn_emitter_destroy(em);
}

/* Streamed big-number output equals the value-tree path's output. */
static bool stream_bignum_matches(const char* input, void (*build)(edn_emitter_t*)) {
    edn_result_t r = edn_read(input, 0);
    if (r.error != EDN_OK) {
        edn_free(r.value);
        return false;
    }
    char* expected = edn_write(r.value);
    edn_free(r.value);
    if (!expected)
        return false;

    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    if (!em) {
        free(expected);
        return false;
    }
    build(em);
    int fr = edn_emitter_finish(em);
    edn_emitter_destroy(em);
    bool eq = (fr == 0) && strcmp(c.buf, expected) == 0;
    free(expected);
    return eq;
}

static void build_bigint_123(edn_emitter_t* em) {
    edn_emit_bigint(em, "123", 10);
}
static void build_bigratio_basic(edn_emitter_t* em) {
    edn_emit_bigratio(em, "99999999999999999999", "3");
}
static void build_bigdec_basic(edn_emitter_t* em) {
    edn_emit_bigdecimal(em, "3.1415926535897932384626433832795028841971693993751");
}

TEST(emit_bignum_matches_value_tree) {
    assert_true(stream_bignum_matches("123N", build_bigint_123));
    assert_true(stream_bignum_matches("99999999999999999999/3", build_bigratio_basic));
    assert_true(stream_bignum_matches("3.1415926535897932384626433832795028841971693993751M",
                                      build_bigdec_basic));
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

/* --- metadata (Clojure extension) --- */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
static edn_emitter_t* meta_emitter(emit_capture_t* c) {
    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;
    return edn_emitter_create(emit_capture_cb, c, &opts);
}

TEST(emit_meta_keyword_payload_then_value) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_keyword(em, "dynamic"), 0);
    assert_int_eq(edn_emit_symbol(em, "value"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "^:dynamic value");
    edn_emitter_destroy(em);
}

TEST(emit_meta_map_payload_streamed) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_begin_map(em), 0);
    assert_int_eq(edn_emit_keyword(em, "a"), 0);
    assert_int_eq(edn_emit_int(em, 1), 0);
    assert_int_eq(edn_emit_keyword(em, "b"), 0);
    assert_int_eq(edn_emit_int(em, 2), 0);
    assert_int_eq(edn_emit_end_map(em), 0);
    assert_int_eq(edn_emit_symbol(em, "value"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "^{:a 1, :b 2} value");
    edn_emitter_destroy(em);
}

TEST(emit_meta_map_payload_embedded) {
    edn_result_t r = edn_read("{:a 1}", 0);
    assert(r.error == EDN_OK);
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_value(em, r.value), 0);
    assert_int_eq(edn_emit_symbol(em, "value"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "^{:a 1} value");
    edn_emitter_destroy(em);
    edn_free(r.value);
}

TEST(emit_meta_vector_payload_embedded) {
    edn_result_t r = edn_read("[1 2]", 0);
    assert(r.error == EDN_OK);
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_value(em, r.value), 0);
    assert_int_eq(edn_emit_symbol(em, "value"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "^[1 2] value");
    edn_emitter_destroy(em);
    edn_free(r.value);
}

TEST(emit_meta_invalid_payload_type_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    /* Strings are not valid Clojure short-form meta payloads. */
    int rc = edn_emit_string(em, "x", 1);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_meta_disabled_in_options_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL); /* default = off */
    assert(em != NULL);
    int rc = edn_emit_meta(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}

TEST(emit_meta_stacked) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_keyword(em, "a"), 0);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_keyword(em, "b"), 0);
    assert_int_eq(edn_emit_symbol(em, "v"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "^:a ^:b v");
    edn_emitter_destroy(em);
}

TEST(emit_meta_with_tag_order_preserved) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_tag(em, "foo"), 0);
    assert_int_eq(edn_emit_meta(em), 0);
    assert_int_eq(edn_emit_keyword(em, "a"), 0);
    assert_int_eq(edn_emit_symbol(em, "v"), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "#foo ^:a v");
    edn_emitter_destroy(em);
}

TEST(emit_meta_then_finish_fails) {
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = meta_emitter(&c);
    assert(em != NULL);
    assert_int_eq(edn_emit_meta(em), 0);
    int rc = edn_emitter_finish(em);
    assert_int_eq(rc, -EDN_ERROR_INVALID_STATE);
    edn_emitter_destroy(em);
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

/* --- hybrid embed --- */

TEST(emit_value_embed_scalar) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_value(em, r.value), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "42");
    edn_emitter_destroy(em);
    edn_free(r.value);
}

TEST(emit_value_embed_collection_inside_open_vector) {
    edn_result_t r = edn_read("[1 2]", 0);
    assert(r.error == EDN_OK);
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_begin_vector(em), 0);
    assert_int_eq(edn_emit_keyword(em, "a"), 0);
    assert_int_eq(edn_emit_value(em, r.value), 0);
    assert_int_eq(edn_emit_end_vector(em), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "[:a [1 2]]");
    edn_emitter_destroy(em);
    edn_free(r.value);
}

TEST(emit_value_embed_with_pending_tag) {
    edn_result_t r = edn_read("\"2024-01-01\"", 0);
    assert(r.error == EDN_OK);
    emit_capture_t c = {{0}, 0};
    edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
    assert(em != NULL);
    assert_int_eq(edn_emit_tag(em, "inst"), 0);
    assert_int_eq(edn_emit_value(em, r.value), 0);
    assert_int_eq(edn_emitter_finish(em), 0);
    assert_str_eq(c.buf, "#inst \"2024-01-01\"");
    edn_emitter_destroy(em);
    edn_free(r.value);
}

/* --- round-trip corpus --- */

TEST(emitter_roundtrip_corpus) {
    static const char* const corpus[] = {
        "nil",       "true", "false",  "0",      "42",      "-17",        "1.5",
        "\"hello\"", ":kw",  ":ns/kw", "foo",    "ns/foo",  "\\a",        "[]",
        "[1 2 3]",   "{}",   "{:a 1}", "#{1 2}", "(1 2 3)", "[:a [1 2]]",
    };
    size_t n = sizeof(corpus) / sizeof(*corpus);
    for (size_t i = 0; i < n; i++) {
        edn_result_t a = edn_read(corpus[i], 0);
        assert(a.error == EDN_OK);

        emit_capture_t c = {{0}, 0};
        edn_emitter_t* em = edn_emitter_create(emit_capture_cb, &c, NULL);
        assert(em != NULL);
        assert_int_eq(edn_emit_value(em, a.value), 0);
        assert_int_eq(edn_emitter_finish(em), 0);
        edn_emitter_destroy(em);

        edn_result_t b = edn_read(c.buf, 0);
        if (b.error != EDN_OK || !edn_value_equal(a.value, b.value)) {
            printf("\n    round-trip mismatch for: %s\n", corpus[i]);
            printf("    re-emitted as:           %s\n", c.buf);
            assert_true(false);
        }
        edn_free(a.value);
        edn_free(b.value);
    }
}

int main(void) {
    printf("Running writer tests...\n");

    /* scalars */
    RUN_TEST(write_nil);
    RUN_TEST(write_bool_true);
    RUN_TEST(write_bool_false);
    RUN_TEST(write_int_positive);
    RUN_TEST(write_int_negative);
    RUN_TEST(write_int_zero);
    RUN_TEST(write_float_basic);
    RUN_TEST(write_float_negative);
    RUN_TEST(write_float_scientific);
    RUN_TEST(write_float_shortest_one);
    RUN_TEST(write_float_shortest_pi_approx);
    RUN_TEST(write_float_shortest_tenth);
    RUN_TEST(write_float_shortest_scientific_fixed);
    RUN_TEST(write_float_shortest_negative_zero);
    RUN_TEST(write_float_shortest_hundred);
    RUN_TEST(write_float_shortest_small);
    RUN_TEST(write_float_shortest_below_lower_bound);
    RUN_TEST(write_float_shortest_above_upper_bound);
    RUN_TEST(write_float_shortest_negative);
    RUN_TEST(write_float_shortest_positive_zero);
    RUN_TEST(write_float_nan);
    RUN_TEST(write_float_pos_inf);
    RUN_TEST(write_float_neg_inf);
    RUN_TEST(write_bigint);
    RUN_TEST(write_bigint_decimal_roundtrip);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    RUN_TEST(write_bigint_hex_roundtrip);
    RUN_TEST(write_bigint_octal_roundtrip);
    RUN_TEST(write_bigint_octal_canonicalizes_leading_zeros);
    RUN_TEST(write_bigint_octal_large);
    RUN_TEST(write_bigint_radix2_small);
    RUN_TEST(write_bigint_radix5_arbitrary);
    RUN_TEST(write_bigint_radix2_large);
    RUN_TEST(write_bigint_radix2_negative);
    RUN_TEST(write_bigratio_basic);
    RUN_TEST(write_bigratio_huge_denominator);
    RUN_TEST(write_bigratio_both_huge);
    RUN_TEST(write_bigratio_negative);
#endif
    RUN_TEST(write_bigdec);
    RUN_TEST(write_string_simple);
    RUN_TEST(write_string_with_escapes);
    RUN_TEST(write_character_letter);
    RUN_TEST(write_character_newline);
    RUN_TEST(write_character_unicode);
    RUN_TEST(write_character_bmp_max);
    RUN_TEST(write_character_control_codepoint);
#ifndef EDN_ENABLE_EXPERIMENTAL_EXTENSION
    RUN_TEST(write_character_supplementary_rejected_without_experimental);
#endif
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
    RUN_TEST(write_character_supplementary_emits_under_experimental);
#endif
    RUN_TEST(write_symbol_simple);
    RUN_TEST(write_symbol_namespaced);
    RUN_TEST(write_keyword_simple);
    RUN_TEST(write_keyword_namespaced);

    /* collections */
    RUN_TEST(write_list_empty);
    RUN_TEST(write_list_basic);
    RUN_TEST(write_vector_empty);
    RUN_TEST(write_vector_basic);
    RUN_TEST(write_set_basic);
    RUN_TEST(write_map_basic);
    RUN_TEST(write_map_nested);
    RUN_TEST(write_tagged);

    /* adapters */
    RUN_TEST(write_buffer_exact_size);
    RUN_TEST(write_buffer_truncated);
    RUN_TEST(write_buffer_zero_cap);
    RUN_TEST(write_file_tmpfile);
    RUN_TEST(write_stream_callback);
    RUN_TEST(write_stream_callback_abort);
    RUN_TEST(write_stream_callback_propagates_oom);
    RUN_TEST(write_stream_callback_propagates_io_failure);
    RUN_TEST(write_stream_null_cb_invalid_argument);
    RUN_TEST(write_file_null_fp_invalid_argument);
    RUN_TEST(write_buffer_null_buf_returns_sentinel);
    RUN_TEST(write_options_bogus_struct_size_invalid_argument);

    /* options */
    RUN_TEST(write_newline_at_end);
#ifndef EDN_ENABLE_CLOJURE_EXTENSION
    RUN_TEST(write_meta_rejects_when_extension_off);
#endif

    /* indent (pretty-print) */
    RUN_TEST(write_indent_empty_collections_stay_compact);
    RUN_TEST(write_indent_single_element_collections_stay_inline);
    RUN_TEST(write_indent_vector_multi);
    RUN_TEST(write_indent_list_multi);
    RUN_TEST(write_indent_set_multi_two_space_step);
    RUN_TEST(write_indent_map_multi);
    RUN_TEST(write_indent_nested_vector_in_vector);
    RUN_TEST(write_indent_nested_value_in_map);
    RUN_TEST(write_indent_nested_key_in_map);
    RUN_TEST(write_indent_set_with_nested_vector);
    RUN_TEST(write_indent_with_sort_unordered);
    RUN_TEST(write_indent_with_newline_at_end);
    RUN_TEST(write_indent_with_escape_unicode_in_collection);
    RUN_TEST(write_indent_tagged_with_collection);
    RUN_TEST(write_indent_roundtrip_via_pretty);
    RUN_TEST(write_indent_zero_matches_compact);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    RUN_TEST(write_indent_with_metadata_short_keyword);
    RUN_TEST(write_indent_with_metadata_full_map_stays_compact);
#endif

    /* escape_unicode */
    RUN_TEST(write_escape_unicode_ascii_passthrough);
    RUN_TEST(write_escape_unicode_bmp_2byte);
    RUN_TEST(write_escape_unicode_bmp_3byte);
    RUN_TEST(write_escape_unicode_supplementary_passthrough);
    RUN_TEST(write_escape_unicode_preserves_existing_escapes);
    RUN_TEST(write_escape_unicode_mixed);
    RUN_TEST(write_escape_unicode_off_preserves_raw_utf8);
    RUN_TEST(write_escape_unicode_empty_string);
    RUN_TEST(write_escape_unicode_inside_collection);
    RUN_TEST(write_escape_unicode_does_not_affect_keyword_with_utf8);
    RUN_TEST(write_escape_unicode_does_not_affect_character);
    RUN_TEST(write_escape_unicode_with_sort_unordered);

    /* sort_unordered */
    RUN_TEST(write_sort_unordered_basic);
    RUN_TEST(write_sort_unordered_mixed_types);
    RUN_TEST(write_sort_unordered_nested);
    RUN_TEST(write_sort_unordered_empty_and_singleton);
    RUN_TEST(write_sort_unordered_off_preserves_order);
    RUN_TEST(write_sort_unordered_set_basic);
    RUN_TEST(write_sort_unordered_set_mixed_types);
    RUN_TEST(write_sort_unordered_set_nested);
    RUN_TEST(write_sort_unordered_set_inside_map);
    RUN_TEST(write_sort_unordered_set_off_preserves_order);
    RUN_TEST(write_sort_unordered_idempotent_map);
    RUN_TEST(write_sort_unordered_idempotent_set);
    RUN_TEST(write_sort_unordered_idempotent_nested);
    RUN_TEST(write_sort_unordered_map_abort_cleans_up);
    RUN_TEST(write_sort_unordered_set_abort_cleans_up);

    /* roundtrip property corpus */
    RUN_TEST(write_roundtrip_corpus);

    /* emit_metadata (Clojure extension only) */
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    RUN_TEST(write_meta_tag_symbol_short);
    RUN_TEST(write_meta_param_tags_short);
    RUN_TEST(write_meta_true_keyword_short);
    RUN_TEST(write_meta_true_namespaced_keyword_short);
    RUN_TEST(write_meta_tag_string_full);
    RUN_TEST(write_meta_multi_key_full);
    RUN_TEST(write_meta_keyword_non_true_full);
    RUN_TEST(write_meta_namespaced_tag_full);
    RUN_TEST(write_meta_empty_map);
    RUN_TEST(write_meta_disabled_skips_meta);
    RUN_TEST(write_meta_no_meta_on_value);
    RUN_TEST(write_meta_nested_in_value);
    RUN_TEST(write_meta_on_tagged);
    RUN_TEST(write_meta_with_sort_unordered);
    RUN_TEST(write_meta_roundtrip_structure);
#endif

    /* registry */
    RUN_TEST(write_registry_create_destroy);

    /* streaming emitter: lifecycle + validation */
    RUN_TEST(emitter_create_null_callback);
    RUN_TEST(emitter_create_bad_struct_size);
    RUN_TEST(emitter_create_sort_unordered_rejected);
    RUN_TEST(emitter_create_writer_registry_rejected);
    RUN_TEST(emitter_destroy_null_ok);
    RUN_TEST(emitter_destroy_without_finish_ok);
    RUN_TEST(emitter_finish_empty_fails);
    RUN_TEST(emitter_finish_then_emit_fails);
    RUN_TEST(emitter_finish_with_newline_at_end);

    /* streaming emitter: scalars */
    RUN_TEST(emit_nil);
    RUN_TEST(emit_bool_true);
    RUN_TEST(emit_bool_false);
    RUN_TEST(emit_int_zero);
    RUN_TEST(emit_int_min);
    RUN_TEST(emit_int_max);
    RUN_TEST(emit_double_simple);
    RUN_TEST(emit_double_special);
    RUN_TEST(emit_string_simple);
    RUN_TEST(emit_string_with_escapes);
    RUN_TEST(emit_string_len_sentinel);
    RUN_TEST(emit_string_invalid_utf8_fails);
    RUN_TEST(emit_keyword_simple);
    RUN_TEST(emit_keyword_ns);
    RUN_TEST(emit_symbol_simple);
    RUN_TEST(emit_symbol_ns);
    RUN_TEST(emit_keyword_invalid_fails);
    RUN_TEST(emit_symbol_invalid_fails);
    RUN_TEST(emit_character_ascii);
    RUN_TEST(emit_character_named);
    RUN_TEST(emit_character_unicode_bmp);
    RUN_TEST(emit_character_invalid_codepoint_fails);
    RUN_TEST(emit_character_surrogate_fails);
    RUN_TEST(emit_top_level_double_value_fails);

    /* streaming emitter: collections */
    RUN_TEST(emit_empty_vector);
    RUN_TEST(emit_vector_with_scalars);
    RUN_TEST(emit_nested_vector);
    RUN_TEST(emit_empty_list);
    RUN_TEST(emit_list_with_scalars);
    RUN_TEST(emit_empty_set);
    RUN_TEST(emit_set_with_scalars);
    RUN_TEST(emit_empty_map);
    RUN_TEST(emit_map_with_pairs);
    RUN_TEST(emit_mismatched_end_fails);
    RUN_TEST(emit_map_odd_count_at_end_fails);
    RUN_TEST(emit_end_without_begin_fails);
    RUN_TEST(emit_max_depth_exceeded);

    /* streaming emitter: indent */
    RUN_TEST(emit_vector_with_indent_matches_value_tree);
    RUN_TEST(emit_nested_map_with_indent_matches_value_tree);
    RUN_TEST(emit_set_with_indent_two_space_step);

    /* streaming emitter: tagged */
    RUN_TEST(emit_tag_then_value);
    RUN_TEST(emit_tag_then_collection);
    RUN_TEST(emit_tag_invalid_identifier_fails);
    RUN_TEST(emit_tag_namespaced);
    RUN_TEST(emit_tag_invalid_namespace_fails);
    RUN_TEST(emit_tag_twice_fails);
    RUN_TEST(emit_tag_then_finish_fails);
    RUN_TEST(emit_tag_with_indent);

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    /* streaming emitter: big numbers */
    RUN_TEST(emit_bigint_decimal);
    RUN_TEST(emit_bigint_hex);
    RUN_TEST(emit_bigint_invalid_radix_fails);
    RUN_TEST(emit_bigint_invalid_digits_fails);
    RUN_TEST(emit_bigratio);
    RUN_TEST(emit_bigratio_invalid_fails);
    RUN_TEST(emit_bigdecimal);
    RUN_TEST(emit_bigdecimal_invalid_fails);
    RUN_TEST(emit_bignum_matches_value_tree);

    /* streaming emitter: metadata */
    RUN_TEST(emit_meta_keyword_payload_then_value);
    RUN_TEST(emit_meta_map_payload_streamed);
    RUN_TEST(emit_meta_map_payload_embedded);
    RUN_TEST(emit_meta_vector_payload_embedded);
    RUN_TEST(emit_meta_invalid_payload_type_fails);
    RUN_TEST(emit_meta_disabled_in_options_fails);
    RUN_TEST(emit_meta_stacked);
    RUN_TEST(emit_meta_with_tag_order_preserved);
    RUN_TEST(emit_meta_then_finish_fails);
#endif

    /* streaming emitter: hybrid embed + round-trip */
    RUN_TEST(emit_value_embed_scalar);
    RUN_TEST(emit_value_embed_collection_inside_open_vector);
    RUN_TEST(emit_value_embed_with_pending_tag);
    RUN_TEST(emitter_roundtrip_corpus);

    TEST_SUMMARY("writer");
}
