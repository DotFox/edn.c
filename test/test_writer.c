/* test_writer.c - Tests for EDN writer (PR1: compact-only). */

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

TEST(write_reject_indent) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.indent = 2;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s == NULL);

    edn_free(r.value);
}

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

TEST(writer_sort_unordered_basic) {
    char* s = write_sorted("{:b 1 :a 2 :c 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:a 2, :b 1, :c 3}");
    free(s);
}

TEST(writer_sort_unordered_mixed_types) {
    /* Byte-wise lex order over serialized keys:
     *   "\"a\"" starts with 0x22, "1" starts with 0x31, ":b" starts with 0x3A
     * Expected order: "a", 1, :b */
    char* s = write_sorted("{:b 1 \"a\" 2 1 3}");
    assert(s != NULL);
    assert_str_eq(s, "{\"a\" 2, 1 3, :b 1}");
    free(s);
}

TEST(writer_sort_unordered_nested) {
    char* s = write_sorted("{:b {:y 1 :x 2} :a 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:a 3, :b {:x 2, :y 1}}");
    free(s);
}

TEST(writer_sort_unordered_empty_and_singleton) {
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

TEST(writer_sort_unordered_off_preserves_order) {
    char* s = write_roundtrip("{:b 1, :a 2, :c 3}");
    assert(s != NULL);
    assert_str_eq(s, "{:b 1, :a 2, :c 3}");
    free(s);
}

TEST(writer_sort_unordered_set_basic) {
    char* s = write_sorted("#{:c :a :b}");
    assert(s != NULL);
    assert_str_eq(s, "#{:a :b :c}");
    free(s);
}

TEST(writer_sort_unordered_set_mixed_types) {
    /* Same byte-wise rationale as writer_sort_unordered_mixed_types:
     *   "\"a\"" -> 0x22, "1" -> 0x31, ":b" -> 0x3A. */
    char* s = write_sorted("#{:b \"a\" 1}");
    assert(s != NULL);
    assert_str_eq(s, "#{\"a\" 1 :b}");
    free(s);
}

TEST(writer_sort_unordered_set_nested) {
    /* Inner set sorts to "#{:x :y}" (starts with '#' = 0x23), which is less
     * than ":a" (starts with ':' = 0x3A). */
    char* s = write_sorted("#{#{:y :x} :a}");
    assert(s != NULL);
    assert_str_eq(s, "#{#{:x :y} :a}");
    free(s);
}

TEST(writer_sort_unordered_set_inside_map) {
    char* s = write_sorted("{:s #{:c :a :b}}");
    assert(s != NULL);
    assert_str_eq(s, "{:s #{:a :b :c}}");
    free(s);
}

TEST(writer_sort_unordered_set_off_preserves_order) {
    char* s = write_roundtrip("#{:c :a :b}");
    assert(s != NULL);
    assert_str_eq(s, "#{:c :a :b}");
    free(s);
}

TEST(write_reject_emit_metadata) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.emit_metadata = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s == NULL);

    edn_free(r.value);
}

TEST(write_reject_escape_unicode) {
    edn_result_t r = edn_read("\"x\"", 0);
    assert(r.error == EDN_OK);

    edn_write_options_t opts = {0};
    opts.struct_size = sizeof(opts);
    opts.escape_unicode = true;

    char* s = edn_write_string(r.value, &opts, NULL);
    assert(s == NULL);

    edn_free(r.value);
}

/* --- writer registry scaffold --- */

TEST(writer_registry_create_destroy) {
    edn_writer_registry_t* r = edn_writer_registry_create();
    assert(r != NULL);
    edn_writer_registry_destroy(r);
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
#endif
    RUN_TEST(write_bigdec);
    RUN_TEST(write_string_simple);
    RUN_TEST(write_string_with_escapes);
    RUN_TEST(write_character_letter);
    RUN_TEST(write_character_newline);
    RUN_TEST(write_character_unicode);
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
    RUN_TEST(write_reject_indent);
    RUN_TEST(write_reject_emit_metadata);
    RUN_TEST(write_reject_escape_unicode);

    /* sort_unordered */
    RUN_TEST(writer_sort_unordered_basic);
    RUN_TEST(writer_sort_unordered_mixed_types);
    RUN_TEST(writer_sort_unordered_nested);
    RUN_TEST(writer_sort_unordered_empty_and_singleton);
    RUN_TEST(writer_sort_unordered_off_preserves_order);
    RUN_TEST(writer_sort_unordered_set_basic);
    RUN_TEST(writer_sort_unordered_set_mixed_types);
    RUN_TEST(writer_sort_unordered_set_nested);
    RUN_TEST(writer_sort_unordered_set_inside_map);
    RUN_TEST(writer_sort_unordered_set_off_preserves_order);

    /* registry */
    RUN_TEST(writer_registry_create_destroy);

    TEST_SUMMARY("writer");
}
