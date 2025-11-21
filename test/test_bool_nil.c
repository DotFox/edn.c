/**
 * Test boolean and nil accessor functions
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test edn_is_nil() */
TEST(is_nil_true) {
    edn_result_t result = edn_read("nil", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_NIL);
    assert(edn_is_nil(result.value) == true);

    edn_free(result.value);
}

TEST(is_nil_false_with_bool) {
    edn_result_t result = edn_read("true", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_is_nil(result.value) == false);

    edn_free(result.value);
}

TEST(is_nil_false_with_number) {
    edn_result_t result = edn_read("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_is_nil(result.value) == false);

    edn_free(result.value);
}

TEST(is_nil_false_with_string) {
    edn_result_t result = edn_read("\"hello\"", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_is_nil(result.value) == false);

    edn_free(result.value);
}

TEST(is_nil_null_input) {
    assert(edn_is_nil(NULL) == false);
}

/* Test edn_bool_get() with true */
TEST(bool_get_true) {
    edn_result_t result = edn_read("true", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);

    bool value;
    assert(edn_bool_get(result.value, &value) == true);
    assert(value == true);

    edn_free(result.value);
}

/* Test edn_bool_get() with false */
TEST(bool_get_false) {
    edn_result_t result = edn_read("false", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);

    bool value;
    assert(edn_bool_get(result.value, &value) == true);
    assert(value == false);

    edn_free(result.value);
}

/* Test edn_bool_get() with wrong type */
TEST(bool_get_wrong_type_nil) {
    edn_result_t result = edn_read("nil", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    bool value = true; // Initialize to non-false value
    assert(edn_bool_get(result.value, &value) == false);
    // value should remain unchanged when function returns false
    assert(value == true);

    edn_free(result.value);
}

TEST(bool_get_wrong_type_number) {
    edn_result_t result = edn_read("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    bool value;
    assert(edn_bool_get(result.value, &value) == false);

    edn_free(result.value);
}

TEST(bool_get_wrong_type_string) {
    edn_result_t result = edn_read("\"true\"", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    bool value;
    assert(edn_bool_get(result.value, &value) == false);

    edn_free(result.value);
}

/* Test edn_bool_get() with NULL input */
TEST(bool_get_null_value) {
    bool value;
    assert(edn_bool_get(NULL, &value) == false);
}

TEST(bool_get_null_output) {
    edn_result_t result = edn_read("true", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    assert(edn_bool_get(result.value, NULL) == false);

    edn_free(result.value);
}

/* Test boolean in collections */
TEST(bool_in_vector) {
    edn_result_t result = edn_read("[true false nil]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    // First element: true
    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(elem0 != NULL);
    bool val0;
    assert(edn_bool_get(elem0, &val0) == true);
    assert(val0 == true);

    // Second element: false
    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(elem1 != NULL);
    bool val1;
    assert(edn_bool_get(elem1, &val1) == true);
    assert(val1 == false);

    // Third element: nil
    edn_value_t* elem2 = edn_vector_get(result.value, 2);
    assert(elem2 != NULL);
    assert(edn_is_nil(elem2) == true);
    bool val2;
    assert(edn_bool_get(elem2, &val2) == false); // nil is not a bool

    edn_free(result.value);
}

/* Test boolean in map */
TEST(bool_in_map) {
    edn_result_t result = edn_read("{:active true :deleted false}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    // Look up :active
    edn_result_t key_result = edn_read(":active", 0);
    edn_value_t* active_val = edn_map_lookup(result.value, key_result.value);
    assert(active_val != NULL);
    bool active;
    assert(edn_bool_get(active_val, &active) == true);
    assert(active == true);
    edn_free(key_result.value);

    // Look up :deleted
    key_result = edn_read(":deleted", 0);
    edn_value_t* deleted_val = edn_map_lookup(result.value, key_result.value);
    assert(deleted_val != NULL);
    bool deleted;
    assert(edn_bool_get(deleted_val, &deleted) == true);
    assert(deleted == false);
    edn_free(key_result.value);

    edn_free(result.value);
}

/* Test nil in collections */
TEST(nil_in_vector) {
    edn_result_t result = edn_read("[1 nil \"foo\"]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_vector_count(result.value) == 3);

    edn_value_t* elem = edn_vector_get(result.value, 1);
    assert(elem != NULL);
    assert(edn_is_nil(elem) == true);

    edn_free(result.value);
}

TEST(nil_in_map_value) {
    edn_result_t result = edn_read("{:key nil}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    edn_result_t key_result = edn_read(":key", 0);
    edn_value_t* val = edn_map_lookup(result.value, key_result.value);
    assert(val != NULL);
    assert(edn_is_nil(val) == true);

    edn_free(key_result.value);
    edn_free(result.value);
}

int main(void) {
    printf("Running boolean and nil tests...\n");

    /* edn_is_nil() tests */
    RUN_TEST(is_nil_true);
    RUN_TEST(is_nil_false_with_bool);
    RUN_TEST(is_nil_false_with_number);
    RUN_TEST(is_nil_false_with_string);
    RUN_TEST(is_nil_null_input);

    /* edn_bool_get() tests */
    RUN_TEST(bool_get_true);
    RUN_TEST(bool_get_false);
    RUN_TEST(bool_get_wrong_type_nil);
    RUN_TEST(bool_get_wrong_type_number);
    RUN_TEST(bool_get_wrong_type_string);
    RUN_TEST(bool_get_null_value);
    RUN_TEST(bool_get_null_output);

    /* Boolean in collections */
    RUN_TEST(bool_in_vector);
    RUN_TEST(bool_in_map);

    /* Nil in collections */
    RUN_TEST(nil_in_vector);
    RUN_TEST(nil_in_map_value);

    TEST_SUMMARY("boolean and nil");
}
