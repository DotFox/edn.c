/**
 * Test: Metadata parsing (^{...} form)
 *
 * This is a Clojure extension to EDN, not part of the official spec.
 * Requires EDN_ENABLE_METADATA compilation flag.
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

#ifdef EDN_ENABLE_METADATA

TEST(basic_metadata_map) {
    /* ^{:test true} [1 2 3] */
    edn_result_t result = edn_parse("^{:test true} [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    edn_result_t key_result = edn_parse(":test", 0);
    edn_value_t* value = edn_map_lookup(meta, key_result.value);
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_BOOL);

    edn_free(key_result.value);
    edn_free(result.value);
}

TEST(keyword_metadata_shorthand) {
    /* ^:test [1 2 3] should expand to ^{:test true} [1 2 3] */
    edn_result_t result = edn_parse("^:test [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    edn_result_t key_result = edn_parse(":test", 0);
    edn_value_t* value = edn_map_lookup(meta, key_result.value);
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_BOOL);

    edn_free(key_result.value);
    edn_free(result.value);
}

TEST(string_metadata_tag) {
    /* ^"String" [1 2 3] should expand to ^{:tag "String"} [1 2 3] */
    edn_result_t result = edn_parse("^\"String\" [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    edn_result_t key_result = edn_parse(":tag", 0);
    edn_value_t* value = edn_map_lookup(meta, key_result.value);
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(value, &len);
    assert_uint_eq(len, 6);
    assert(strncmp(str, "String", 6) == 0);

    edn_free(key_result.value);
    edn_free(result.value);
}

TEST(symbol_metadata_tag) {
    /* ^String [1 2 3] should expand to ^{:tag String} [1 2 3] */
    edn_result_t result = edn_parse("^String [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    edn_result_t key_result = edn_parse(":tag", 0);
    edn_value_t* value = edn_map_lookup(meta, key_result.value);
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_SYMBOL);

    const char* name;
    size_t name_len;
    assert_true(edn_symbol_get(value, NULL, NULL, &name, &name_len));
    assert_uint_eq(name_len, 6);
    assert(strncmp(name, "String", 6) == 0);

    edn_free(key_result.value);
    edn_free(result.value);
}

TEST(chained_metadata) {
    /* ^:test ^:foo [1 2 3] should merge metadata */
    edn_result_t result = edn_parse("^:test ^:foo [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 2);

    edn_result_t key1 = edn_parse(":test", 0);
    edn_result_t key2 = edn_parse(":foo", 0);

    edn_value_t* val1 = edn_map_lookup(meta, key1.value);
    edn_value_t* val2 = edn_map_lookup(meta, key2.value);

    assert(val1 != NULL);
    assert(val2 != NULL);
    assert(edn_type(val1) == EDN_TYPE_BOOL);
    assert(edn_type(val2) == EDN_TYPE_BOOL);

    edn_free(key1.value);
    edn_free(key2.value);
    edn_free(result.value);
}

TEST(mixed_chained_metadata) {
    /* ^{:a 1} ^:b [1 2 3] should merge */
    edn_result_t result = edn_parse("^{:a 1} ^:b [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 2);

    edn_result_t key_a = edn_parse(":a", 0);
    edn_value_t* val_a = edn_map_lookup(meta, key_a.value);
    assert(val_a != NULL);
    assert(edn_type(val_a) == EDN_TYPE_INT);
    int64_t num;
    assert_true(edn_int64_get(val_a, &num));
    assert_int_eq(num, 1);

    edn_result_t key_b = edn_parse(":b", 0);
    edn_value_t* val_b = edn_map_lookup(meta, key_b.value);
    assert(val_b != NULL);
    assert(edn_type(val_b) == EDN_TYPE_BOOL);

    edn_free(key_a.value);
    edn_free(key_b.value);
    edn_free(result.value);
}

TEST(overlapping_chained_metadata) {
    /* ^{:a "outer"} ^{:a "inner"} [1 2 3] should merge */
    edn_result_t result = edn_parse("^{:a \"outer\"} ^{:a \"inner\"} [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    edn_result_t key_a = edn_parse(":a", 0);
    edn_value_t* val_a = edn_map_lookup(meta, key_a.value);
    assert(val_a != NULL);
    assert(edn_type(val_a) == EDN_TYPE_STRING);
    size_t len;
    const char* str = edn_string_get(val_a, &len);
    assert_str_eq(str, "outer");

    edn_free(key_a.value);
    edn_free(result.value);
}

TEST(metadata_on_map) {
    /* ^:test {:a 1 :b 2} */
    edn_result_t result = edn_parse("^:test {:a 1 :b 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);

    edn_free(result.value);
}

TEST(metadata_on_list) {
    /* ^:test (1 2 3) */
    edn_result_t result = edn_parse("^:test (1 2 3)", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);

    edn_free(result.value);
}

TEST(metadata_on_set) {
    /* ^:test #{1 2 3} */
    edn_result_t result = edn_parse("^:test #{1 2 3}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);

    edn_free(result.value);
}

TEST(metadata_on_symbol) {
    /* ^:test foo */
    edn_result_t result = edn_parse("^:test foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);

    edn_free(result.value);
}

TEST(no_metadata_by_default) {
    /* [1 2 3] without metadata */
    edn_result_t result = edn_parse("[1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    assert_false(edn_value_has_meta(result.value));
    assert(edn_value_meta(result.value) == NULL);

    edn_free(result.value);
}

TEST(metadata_eof_after_marker) {
    /* ^<EOF> should error */
    edn_result_t result = edn_parse("^", 0);
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.error_message != NULL);
}

TEST(metadata_eof_after_value) {
    /* ^:test<EOF> should error */
    edn_result_t result = edn_parse("^:test", 0);
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.error_message != NULL);
}

TEST(metadata_invalid_type) {
    /* ^123 [1 2 3] should error (numbers not allowed as metadata) */
    edn_result_t result = edn_parse("^123 [1 2 3]", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_with_whitespace) {
    edn_result_t result = edn_parse("^  :test  [  1  2  3  ]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert_true(edn_value_has_meta(result.value));

    edn_free(result.value);
}

TEST(metadata_complex_map) {
    edn_result_t result = edn_parse("^{:doc \"A vector\" :test true :tag Vector} [1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    /* Check metadata */
    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 3);

    /* Check :doc */
    edn_result_t key_doc = edn_parse(":doc", 0);
    edn_value_t* val_doc = edn_map_lookup(meta, key_doc.value);
    assert(val_doc != NULL);
    assert(edn_type(val_doc) == EDN_TYPE_STRING);
    size_t len;
    const char* str = edn_string_get(val_doc, &len);
    assert_uint_eq(len, 8);
    assert(strncmp(str, "A vector", 8) == 0);

    /* Check :test */
    edn_result_t key_test = edn_parse(":test", 0);
    edn_value_t* val_test = edn_map_lookup(meta, key_test.value);
    assert(val_test != NULL);
    assert(edn_type(val_test) == EDN_TYPE_BOOL);

    /* Check :tag */
    edn_result_t key_tag = edn_parse(":tag", 0);
    edn_value_t* val_tag = edn_map_lookup(meta, key_tag.value);
    assert(val_tag != NULL);
    assert(edn_type(val_tag) == EDN_TYPE_SYMBOL);

    edn_free(key_doc.value);
    edn_free(key_test.value);
    edn_free(key_tag.value);
    edn_free(result.value);
}

TEST(metadata_invalid_value_type_number) {
    /* ^:test 123 should error - cannot attach metadata to numbers */
    edn_result_t result = edn_parse("^:test 123", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_invalid_value_type_string) {
    /* ^:test "hello" should error - cannot attach metadata to strings */
    edn_result_t result = edn_parse("^:test \"hello\"", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_invalid_value_type_keyword) {
    /* ^:test :foo should error - cannot attach metadata to keywords */
    edn_result_t result = edn_parse("^:test :foo", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_invalid_value_type_nil) {
    /* ^:test nil should error - cannot attach metadata to nil */
    edn_result_t result = edn_parse("^:test nil", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_invalid_value_type_bool) {
    /* ^:test true should error - cannot attach metadata to booleans */
    edn_result_t result = edn_parse("^:test true", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(metadata_on_tagged_literal) {
    /* ^:test #inst "2024-01-01" should work - tagged literals can have metadata */
    edn_result_t result = edn_parse("^:test #inst \"2024-01-01\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    /* Check metadata exists */
    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);

    edn_free(result.value);
}

TEST(metadata_vector_param_tags) {
    /* ^[java.lang.String long _] form should expand to ^{:param-tags [java.lang.String long _]} form */
    edn_result_t result = edn_parse("^[String long _] foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    /* Check metadata exists */
    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 1);

    /* Check :param-tags key */
    edn_result_t key = edn_parse(":param-tags", 0);
    edn_value_t* value = edn_map_lookup(meta, key.value);
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_VECTOR);
    assert_uint_eq(edn_vector_count(value), 3);

    edn_free(key.value);
    edn_free(result.value);
}

TEST(metadata_vector_chained) {
    /* ^:test ^[String] foo should merge both metadata */
    edn_result_t result = edn_parse("^:test ^[String] foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    /* Check metadata exists with both keys */
    assert_true(edn_value_has_meta(result.value));
    edn_value_t* meta = edn_value_meta(result.value);
    assert(meta != NULL);
    assert(edn_type(meta) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(meta), 2);

    /* Check :param-tags key */
    edn_result_t key1 = edn_parse(":param-tags", 0);
    edn_value_t* val1 = edn_map_lookup(meta, key1.value);
    assert(val1 != NULL);
    assert(edn_type(val1) == EDN_TYPE_VECTOR);

    /* Check :test key */
    edn_result_t key2 = edn_parse(":test", 0);
    edn_value_t* val2 = edn_map_lookup(meta, key2.value);
    assert(val2 != NULL);
    assert(edn_type(val2) == EDN_TYPE_BOOL);

    edn_free(key1.value);
    edn_free(key2.value);
    edn_free(result.value);
}

#else

TEST(metadata_disabled) {
    /* When metadata is disabled, ^ should be treated as a symbol character */
    edn_result_t result = edn_parse("^test", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    const char* name;
    size_t len;
    assert_true(edn_symbol_get(result.value, NULL, NULL, &name, &len));
    assert_uint_eq(len, 5);
    assert(strncmp(name, "^test", 5) == 0);

    edn_free(result.value);
}

#endif

int main(void) {
#ifdef EDN_ENABLE_METADATA
    RUN_TEST(basic_metadata_map);
    RUN_TEST(keyword_metadata_shorthand);
    RUN_TEST(string_metadata_tag);
    RUN_TEST(symbol_metadata_tag);
    RUN_TEST(chained_metadata);
    RUN_TEST(mixed_chained_metadata);
    RUN_TEST(overlapping_chained_metadata);
    RUN_TEST(metadata_on_map);
    RUN_TEST(metadata_on_list);
    RUN_TEST(metadata_on_set);
    RUN_TEST(metadata_on_symbol);
    RUN_TEST(no_metadata_by_default);
    RUN_TEST(metadata_eof_after_marker);
    RUN_TEST(metadata_eof_after_value);
    RUN_TEST(metadata_invalid_type);
    RUN_TEST(metadata_with_whitespace);
    RUN_TEST(metadata_complex_map);
    RUN_TEST(metadata_invalid_value_type_number);
    RUN_TEST(metadata_invalid_value_type_string);
    RUN_TEST(metadata_invalid_value_type_keyword);
    RUN_TEST(metadata_invalid_value_type_nil);
    RUN_TEST(metadata_invalid_value_type_bool);
    RUN_TEST(metadata_on_tagged_literal);
    RUN_TEST(metadata_vector_param_tags);
    RUN_TEST(metadata_vector_chained);
#else
    RUN_TEST(metadata_disabled);
#endif

    printf("\nAll metadata tests passed!\n");
    return 0;
}
