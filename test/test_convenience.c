/**
 * Test convenience and ergonomics functions
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* ========== Type Predicates Tests ========== */

TEST(is_string_true) {
    edn_result_t r = edn_read("\"hello\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_string(r.value) == true);
    edn_free(r.value);
}

TEST(is_string_false) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_string(r.value) == false);
    edn_free(r.value);
}

TEST(is_string_null) {
    assert(edn_is_string(NULL) == false);
}

TEST(is_number_int) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == true);
    edn_free(r.value);
}

TEST(is_number_bigint) {
    edn_result_t r = edn_read("999999999999999999999999999", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == true);
    edn_free(r.value);
}

TEST(is_number_float) {
    edn_result_t r = edn_read("3.14", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == true);
    edn_free(r.value);
}

TEST(is_number_bigdec) {
    edn_result_t r = edn_read("3.14M", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == true);
    edn_free(r.value);
}

#ifdef EDN_ENABLE_RATIO
TEST(is_number_ratio) {
    edn_result_t r = edn_read("22/7", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == true);
    edn_free(r.value);
}
#endif

TEST(is_number_false) {
    edn_result_t r = edn_read("\"not a number\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_number(r.value) == false);
    edn_free(r.value);
}

TEST(is_number_null) {
    assert(edn_is_number(NULL) == false);
}

TEST(is_integer_int) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_integer(r.value) == true);
    edn_free(r.value);
}

TEST(is_integer_bigint) {
    edn_result_t r = edn_read("42N", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_integer(r.value) == true);
    edn_free(r.value);
}

TEST(is_integer_false_float) {
    edn_result_t r = edn_read("3.14", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_integer(r.value) == false);
    edn_free(r.value);
}

TEST(is_integer_null) {
    assert(edn_is_integer(NULL) == false);
}

TEST(is_collection_list) {
    edn_result_t r = edn_read("(1 2 3)", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == true);
    edn_free(r.value);
}

TEST(is_collection_vector) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == true);
    edn_free(r.value);
}

TEST(is_collection_map) {
    edn_result_t r = edn_read("{:a 1}", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == true);
    edn_free(r.value);
}

TEST(is_collection_set) {
    edn_result_t r = edn_read("#{1 2 3}", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == true);
    edn_free(r.value);
}

TEST(is_collection_false) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == false);
    edn_free(r.value);
}

TEST(is_collection_null) {
    assert(edn_is_collection(NULL) == false);
}

/* ========== String Utilities Tests ========== */

TEST(string_equals_true) {
    edn_result_t r = edn_read("\"hello\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "hello") == true);
    edn_free(r.value);
}

TEST(string_equals_false) {
    edn_result_t r = edn_read("\"hello\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "world") == false);
    edn_free(r.value);
}

TEST(string_equals_empty) {
    edn_result_t r = edn_read("\"\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "") == true);
    assert(edn_string_equals(r.value, "x") == false);
    edn_free(r.value);
}

TEST(string_equals_different_length) {
    edn_result_t r = edn_read("\"hi\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "hello") == false);
    edn_free(r.value);
}

TEST(string_equals_with_escapes) {
    edn_result_t r = edn_read("\"hello\\nworld\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "hello\nworld") == true);
    edn_free(r.value);
}

TEST(string_equals_wrong_type) {
    edn_result_t r = edn_read("42", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, "42") == false);
    edn_free(r.value);
}

TEST(string_equals_null_value) {
    assert(edn_string_equals(NULL, "test") == false);
}

TEST(string_equals_null_str) {
    edn_result_t r = edn_read("\"hello\"", 0);
    assert(r.error == EDN_OK);
    assert(edn_string_equals(r.value, NULL) == false);
    edn_free(r.value);
}

/* ========== Map Convenience Functions Tests ========== */

TEST(map_get_keyword_found) {
    edn_result_t r = edn_read("{:name \"Alice\" :age 30}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* name = edn_map_get_keyword(r.value, "name");
    assert(name != NULL);
    assert(edn_is_string(name) == true);
    assert(edn_string_equals(name, "Alice") == true);

    edn_free(r.value);
}

TEST(map_get_keyword_not_found) {
    edn_result_t r = edn_read("{:name \"Alice\"}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* missing = edn_map_get_keyword(r.value, "age");
    assert(missing == NULL);

    edn_free(r.value);
}

TEST(map_get_keyword_empty_map) {
    edn_result_t r = edn_read("{}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val = edn_map_get_keyword(r.value, "any");
    assert(val == NULL);

    edn_free(r.value);
}

TEST(map_get_keyword_multiple_values) {
    edn_result_t r = edn_read("{:a 1 :b 2 :c 3}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val_a = edn_map_get_keyword(r.value, "a");
    edn_value_t* val_b = edn_map_get_keyword(r.value, "b");
    edn_value_t* val_c = edn_map_get_keyword(r.value, "c");

    assert(val_a != NULL);
    assert(val_b != NULL);
    assert(val_c != NULL);

    int64_t num;
    assert(edn_int64_get(val_a, &num) && num == 1);
    assert(edn_int64_get(val_b, &num) && num == 2);
    assert(edn_int64_get(val_c, &num) && num == 3);

    edn_free(r.value);
}

TEST(map_get_keyword_null_map) {
    edn_value_t* val = edn_map_get_keyword(NULL, "key");
    assert(val == NULL);
}

TEST(map_get_keyword_null_keyword) {
    edn_result_t r = edn_read("{:a 1}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val = edn_map_get_keyword(r.value, NULL);
    assert(val == NULL);

    edn_free(r.value);
}

TEST(map_get_keyword_wrong_type) {
    edn_result_t r = edn_read("[1 2 3]", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val = edn_map_get_keyword(r.value, "key");
    assert(val == NULL);

    edn_free(r.value);
}

TEST(map_get_string_key_found) {
    edn_result_t r = edn_read("{\"name\" \"Alice\" \"age\" 30}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* name = edn_map_get_string_key(r.value, "name");
    assert(name != NULL);
    assert(edn_string_equals(name, "Alice") == true);

    edn_free(r.value);
}

TEST(map_get_string_key_not_found) {
    edn_result_t r = edn_read("{\"name\" \"Alice\"}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* missing = edn_map_get_string_key(r.value, "age");
    assert(missing == NULL);

    edn_free(r.value);
}

TEST(map_get_string_key_empty_string) {
    edn_result_t r = edn_read("{\"\" 42}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val = edn_map_get_string_key(r.value, "");
    assert(val != NULL);

    int64_t num;
    assert(edn_int64_get(val, &num) && num == 42);

    edn_free(r.value);
}

TEST(map_get_string_key_null_map) {
    edn_value_t* val = edn_map_get_string_key(NULL, "key");
    assert(val == NULL);
}

TEST(map_get_string_key_null_key) {
    edn_result_t r = edn_read("{\"a\" 1}", 0);
    assert(r.error == EDN_OK);

    edn_value_t* val = edn_map_get_string_key(r.value, NULL);
    assert(val == NULL);

    edn_free(r.value);
}

/* ========== Combined/Integration Tests ========== */

TEST(combined_type_checks) {
    edn_result_t r = edn_read("[42 \"hello\" [1 2] {:a 1}]", 0);
    assert(r.error == EDN_OK);
    assert(edn_is_collection(r.value) == true);

    edn_value_t* elem0 = edn_vector_get(r.value, 0);
    assert(edn_is_number(elem0) == true);
    assert(edn_is_integer(elem0) == true);
    assert(edn_is_string(elem0) == false);

    edn_value_t* elem1 = edn_vector_get(r.value, 1);
    assert(edn_is_string(elem1) == true);
    assert(edn_is_number(elem1) == false);

    edn_value_t* elem2 = edn_vector_get(r.value, 2);
    assert(edn_is_collection(elem2) == true);

    edn_value_t* elem3 = edn_vector_get(r.value, 3);
    assert(edn_is_collection(elem3) == true);

    edn_free(r.value);
}

TEST(combined_map_lookup_workflow) {
    edn_result_t r = edn_read("{:name \"Alice\" :age 30 :active true}", 0);
    assert(r.error == EDN_OK);

    // Get name
    edn_value_t* name = edn_map_get_keyword(r.value, "name");
    assert(name != NULL && edn_is_string(name));
    assert(edn_string_equals(name, "Alice"));

    // Get age
    edn_value_t* age = edn_map_get_keyword(r.value, "age");
    assert(age != NULL && edn_is_number(age));
    int64_t age_val;
    assert(edn_int64_get(age, &age_val) && age_val == 30);

    // Get active flag
    edn_value_t* active = edn_map_get_keyword(r.value, "active");
    assert(active != NULL);
    bool active_val;
    assert(edn_bool_get(active, &active_val) && active_val == true);

    // Check missing key
    assert(edn_map_get_keyword(r.value, "missing") == NULL);

    edn_free(r.value);
}

int main(void) {
    printf("Running convenience function tests...\n");

    /* Type predicates */
    printf("\nType Predicates:\n");
    RUN_TEST(is_string_true);
    RUN_TEST(is_string_false);
    RUN_TEST(is_string_null);

    RUN_TEST(is_number_int);
    RUN_TEST(is_number_bigint);
    RUN_TEST(is_number_float);
    RUN_TEST(is_number_bigdec);
#ifdef EDN_ENABLE_RATIO
    RUN_TEST(is_number_ratio);
#endif
    RUN_TEST(is_number_false);
    RUN_TEST(is_number_null);

    RUN_TEST(is_integer_int);
    RUN_TEST(is_integer_bigint);
    RUN_TEST(is_integer_false_float);
    RUN_TEST(is_integer_null);

    RUN_TEST(is_collection_list);
    RUN_TEST(is_collection_vector);
    RUN_TEST(is_collection_map);
    RUN_TEST(is_collection_set);
    RUN_TEST(is_collection_false);
    RUN_TEST(is_collection_null);

    /* String utilities */
    printf("\nString Utilities:\n");
    RUN_TEST(string_equals_true);
    RUN_TEST(string_equals_false);
    RUN_TEST(string_equals_empty);
    RUN_TEST(string_equals_different_length);
    RUN_TEST(string_equals_with_escapes);
    RUN_TEST(string_equals_wrong_type);
    RUN_TEST(string_equals_null_value);
    RUN_TEST(string_equals_null_str);

    /* Map convenience */
    printf("\nMap Convenience Functions:\n");
    RUN_TEST(map_get_keyword_found);
    RUN_TEST(map_get_keyword_not_found);
    RUN_TEST(map_get_keyword_empty_map);
    RUN_TEST(map_get_keyword_multiple_values);
    RUN_TEST(map_get_keyword_null_map);
    RUN_TEST(map_get_keyword_null_keyword);
    RUN_TEST(map_get_keyword_wrong_type);

    RUN_TEST(map_get_string_key_found);
    RUN_TEST(map_get_string_key_not_found);
    RUN_TEST(map_get_string_key_empty_string);
    RUN_TEST(map_get_string_key_null_map);
    RUN_TEST(map_get_string_key_null_key);

    /* Integration */
    printf("\nIntegration Tests:\n");
    RUN_TEST(combined_type_checks);
    RUN_TEST(combined_map_lookup_workflow);

    TEST_SUMMARY("convenience functions");
}
