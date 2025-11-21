/**
 * test_discard.c - Tests for discard reader macro (#_)
 */

#include <stdio.h>
#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test simple discard of atomic values */
TEST(discard_integer) {
    edn_result_t result = edn_read("[1 #_2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(first != NULL);
    int64_t val;
    assert(edn_int64_get(first, &val));
    assert_int_eq(val, 1);

    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(second != NULL);
    assert(edn_int64_get(second, &val));
    assert_int_eq(val, 3);

    edn_free(result.value);
}

TEST(discard_string) {
    edn_result_t result = edn_read("[\"foo\" #_\"bar\" \"baz\"]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(first != NULL);
    size_t len;
    const char* str = edn_string_get(first, &len);
    assert_str_eq(str, "foo");

    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(second != NULL);
    str = edn_string_get(second, &len);
    assert_str_eq(str, "baz");

    edn_free(result.value);
}

TEST(discard_keyword) {
    edn_result_t result = edn_read("[#_:discarded :kept]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 1);

    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(first != NULL);
    assert(edn_type(first) == EDN_TYPE_KEYWORD);

    edn_free(result.value);
}

/* Test discard of nested structures */
TEST(discard_vector) {
    edn_result_t result = edn_read("[1 #_[2 3 4] 5]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    int64_t val;
    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(edn_int64_get(first, &val));
    assert_int_eq(val, 1);

    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(edn_int64_get(second, &val));
    assert_int_eq(val, 5);

    edn_free(result.value);
}

TEST(discard_list) {
    edn_result_t result = edn_read("(foo #_(bar baz) qux)", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);

    size_t count = edn_list_count(result.value);
    assert_uint_eq(count, 2);

    edn_free(result.value);
}

TEST(discard_map) {
    edn_result_t result = edn_read("[:a #_{:b 2 :c 3} :d]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    edn_free(result.value);
}

/* Test multiple consecutive discards */
TEST(discard_multiple) {
    edn_result_t result = edn_read("[1 #_2 #_3 #_4 5]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    int64_t val;
    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(edn_int64_get(first, &val));
    assert_int_eq(val, 1);

    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(edn_int64_get(second, &val));
    assert_int_eq(val, 5);

    edn_free(result.value);
}

/* Test discard in maps */
TEST(discard_map_key) {
    edn_result_t result = edn_read("{:a 1 #_:b #_2 :c 3}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    size_t count = edn_map_count(result.value);
    assert_uint_eq(count, 2);

    edn_free(result.value);
}

TEST(discard_map_value) {
    /* Discarding a value in a map leaves a key without a value */
    /* This should result in an error (odd number of elements) */
    edn_result_t result = edn_read("{:a 1 :b #_discarded :c 3}", 0);
    /* After parsing: :a, 1, :b, :c, 3 = 5 elements (odd) */
    /* This should be an error */
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

/* Test discard with whitespace */
TEST(discard_with_whitespace) {
    edn_result_t result = edn_read("[1 #_  2  3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    edn_free(result.value);
}

TEST(discard_with_newlines) {
    edn_result_t result = edn_read("[1 #_\n2\n3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    edn_free(result.value);
}

/* Test nested discard */
TEST(discard_nested) {
    edn_result_t result = edn_read("[1 #_#_2 3 4]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* #_#_2 3 means: discard (discard 2), which discards 2, then discard 3 */
    /* Result should be [1 4] */
    size_t count = edn_vector_count(result.value);
    assert_uint_eq(count, 2);

    int64_t val;
    edn_value_t* first = edn_vector_get(result.value, 0);
    assert(edn_int64_get(first, &val));
    assert_int_eq(val, 1);

    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(edn_int64_get(second, &val));
    assert_int_eq(val, 4);

    edn_free(result.value);
}

/* Test error cases */
TEST(discard_missing_value) {
    edn_result_t result = edn_read("[1 #_]", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(discard_eof) {
    edn_result_t result = edn_read("#_", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(discard_at_end_of_collection) {
    edn_result_t result = edn_read("[1 2 #_]", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

/* Test top-level discard */
TEST(discard_top_level) {
    /* Discarding at top level should result in no value parsed */
    edn_result_t result = edn_read("#_42", 0);
    /* This might return NULL with no error, or return an error */
    /* The behavior depends on implementation - should clarify spec */
    /* For now, we'll just check it doesn't crash */
    edn_free(result.value);
}

int main(void) {
    RUN_TEST(discard_integer);
    RUN_TEST(discard_string);
    RUN_TEST(discard_keyword);
    RUN_TEST(discard_vector);
    RUN_TEST(discard_list);
    RUN_TEST(discard_map);
    RUN_TEST(discard_multiple);
    RUN_TEST(discard_map_key);
    RUN_TEST(discard_map_value);
    RUN_TEST(discard_with_whitespace);
    RUN_TEST(discard_with_newlines);
    RUN_TEST(discard_nested);
    RUN_TEST(discard_missing_value);
    RUN_TEST(discard_eof);
    RUN_TEST(discard_at_end_of_collection);
    RUN_TEST(discard_top_level);

    TEST_SUMMARY("discard");
}
