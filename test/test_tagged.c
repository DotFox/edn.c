/**
 * Test tagged literal parser
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Basic tagged literal with string value */
TEST(parse_tagged_inst) {
    edn_result_t result = edn_parse("#inst \"2024-01-01T00:00:00Z\"", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    size_t tag_len;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, &tag_len, &value));
    assert(tag != NULL);
    assert(tag_len == 4);
    assert(memcmp(tag, "inst", 4) == 0);

    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_STRING);

    edn_free(result.value);
}

/* Tagged literal with UUID string */
TEST(parse_tagged_uuid) {
    edn_result_t result = edn_parse("#uuid \"550e8400-e29b-41d4-a716-446655440000\"", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    size_t tag_len;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, &tag_len, &value));
    assert(tag_len == 4);
    assert(memcmp(tag, "uuid", 4) == 0);

    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_STRING);

    edn_free(result.value);
}

/* Tagged literal with namespaced tag */
TEST(parse_tagged_namespaced) {
    edn_result_t result = edn_parse("#myapp/custom 42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    size_t tag_len;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, &tag_len, &value));
    assert(tag_len == 12); /* "myapp/custom" */
    assert(memcmp(tag, "myapp/custom", 12) == 0);

    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(value, &num));
    assert(num == 42);

    edn_free(result.value);
}

/* Tagged literal with integer value */
TEST(parse_tagged_int_value) {
    edn_result_t result = edn_parse("#timestamp 1234567890", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, NULL, &value));
    assert(memcmp(tag, "timestamp", 9) == 0);

    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_INT);

    edn_free(result.value);
}

/* Tagged literal with map value */
TEST(parse_tagged_map_value) {
    edn_result_t result = edn_parse("#myapp/data {:foo 1 :bar 2}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, NULL, &value));
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_MAP);
    assert(edn_map_count(value) == 2);

    edn_free(result.value);
}

/* Tagged literal with vector value */
TEST(parse_tagged_vector_value) {
    edn_result_t result = edn_parse("#coords [1.0 2.0 3.0]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, NULL, &value));
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(value) == 3);

    edn_free(result.value);
}

/* Tagged literal with list value */
TEST(parse_tagged_list_value) {
    edn_result_t result = edn_parse("#point (1 2)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, NULL, &value));
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_LIST);
    assert(edn_list_count(value) == 2);

    edn_free(result.value);
}

/* Tagged literal with set value */
TEST(parse_tagged_set_value) {
    edn_result_t result = edn_parse("#flags #{:read :write}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* value;

    assert(edn_tagged_get(result.value, &tag, NULL, &value));
    assert(value != NULL);
    assert(edn_type(value) == EDN_TYPE_SET);
    assert(edn_set_count(value) == 2);

    edn_free(result.value);
}

/* Nested tagged literals */
TEST(parse_nested_tagged) {
    edn_result_t result = edn_parse("#outer #inner 42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* outer_tag;
    edn_value_t* outer_value;

    assert(edn_tagged_get(result.value, &outer_tag, NULL, &outer_value));
    assert(memcmp(outer_tag, "outer", 5) == 0);

    /* Inner value should also be tagged */
    assert(outer_value != NULL);
    assert(edn_type(outer_value) == EDN_TYPE_TAGGED);

    const char* inner_tag;
    edn_value_t* inner_value;

    assert(edn_tagged_get(outer_value, &inner_tag, NULL, &inner_value));
    assert(memcmp(inner_tag, "inner", 5) == 0);

    /* Innermost value is the integer */
    assert(inner_value != NULL);
    assert(edn_type(inner_value) == EDN_TYPE_INT);

    edn_free(result.value);
}

/* Tagged literal in collection */
TEST(parse_tagged_in_vector) {
    edn_result_t result = edn_parse("[1 #tag 2 3]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    /* Second element should be tagged */
    edn_value_t* elem = edn_vector_get(result.value, 1);
    assert(elem != NULL);
    assert(edn_type(elem) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Tagged literal with whitespace */
TEST(parse_tagged_with_whitespace) {
    edn_result_t result = edn_parse("#tag   42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Tagged literal with newlines */
TEST(parse_tagged_with_newlines) {
    edn_result_t result = edn_parse("#tag\n\n42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Tagged literal with comment */
TEST(parse_tagged_with_comment) {
    edn_result_t result = edn_parse("#tag ; comment\n 42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Error: keyword as tag */
TEST(error_keyword_tag) {
    edn_result_t result = edn_parse("#:keyword 42", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: nil as tag */
TEST(error_nil_tag) {
    edn_result_t result = edn_parse("#nil 42", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: true as tag */
TEST(error_true_tag) {
    edn_result_t result = edn_parse("#true 42", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: false as tag */
TEST(error_false_tag) {
    edn_result_t result = edn_parse("#false 42", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: missing tag */
TEST(error_missing_tag) {
    edn_result_t result = edn_parse("#", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* Error: missing value */
TEST(error_missing_value) {
    edn_result_t result = edn_parse("#tag", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* Error: whitespace after hash */
TEST(error_hash_with_whitespace) {
    edn_result_t result = edn_parse("# tag 42", 0);

    /* Whitespace after # is not allowed */
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* API with wrong type */
TEST(tagged_api_wrong_type) {
    edn_result_t result = edn_parse("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    const char* tag;
    edn_value_t* value;

    /* Should return false for non-tagged */
    assert(!edn_tagged_get(result.value, &tag, NULL, &value));

    edn_free(result.value);
}

/* API with NULL */
TEST(tagged_api_null) {
    const char* tag;
    edn_value_t* value;

    assert(!edn_tagged_get(NULL, &tag, NULL, &value));
}

int main(void) {
    printf("Running tagged literal tests...\n");

    RUN_TEST(parse_tagged_inst);
    RUN_TEST(parse_tagged_uuid);
    RUN_TEST(parse_tagged_namespaced);
    RUN_TEST(parse_tagged_int_value);
    RUN_TEST(parse_tagged_map_value);
    RUN_TEST(parse_tagged_vector_value);
    RUN_TEST(parse_tagged_list_value);
    RUN_TEST(parse_tagged_set_value);
    RUN_TEST(parse_nested_tagged);
    RUN_TEST(parse_tagged_in_vector);
    RUN_TEST(parse_tagged_with_whitespace);
    RUN_TEST(parse_tagged_with_newlines);
    RUN_TEST(parse_tagged_with_comment);
    RUN_TEST(error_keyword_tag);
    RUN_TEST(error_nil_tag);
    RUN_TEST(error_true_tag);
    RUN_TEST(error_false_tag);
    RUN_TEST(error_missing_tag);
    RUN_TEST(error_missing_value);
    RUN_TEST(error_hash_with_whitespace);
    RUN_TEST(tagged_api_wrong_type);
    RUN_TEST(tagged_api_null);

    TEST_SUMMARY("tagged literal");
}
