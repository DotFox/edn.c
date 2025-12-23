/**
 * Tests for nil and boolean values
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

TEST(nil_source_position) {
    edn_result_t result = edn_read("nil", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_NIL);

    size_t start, end;
    assert(edn_source_position(result.value, &start, &end));
    assert_uint_eq(start, 0);
    assert_uint_eq(end, 3);

    edn_free(result.value);
}

TEST(true_source_position) {
    edn_result_t result = edn_read("true", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);

    bool val;
    assert(edn_bool_get(result.value, &val));
    assert(val == true);

    size_t start, end;
    assert(edn_source_position(result.value, &start, &end));
    assert_uint_eq(start, 0);
    assert_uint_eq(end, 4);

    edn_free(result.value);
}

TEST(false_source_position) {
    edn_result_t result = edn_read("false", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);

    bool val;
    assert(edn_bool_get(result.value, &val));
    assert(val == false);

    size_t start, end;
    assert(edn_source_position(result.value, &start, &end));
    assert_uint_eq(start, 0);
    assert_uint_eq(end, 5);

    edn_free(result.value);
}

TEST(true_false_different) {
    edn_result_t result_true = edn_read("true", 0);
    assert(result_true.error == EDN_OK);
    assert(result_true.value != NULL);

    edn_result_t result_false = edn_read("false", 0);
    assert(result_false.error == EDN_OK);
    assert(result_false.value != NULL);

    bool true_val, false_val;
    assert(edn_bool_get(result_true.value, &true_val));
    assert(edn_bool_get(result_false.value, &false_val));
    assert(true_val == true);
    assert(false_val == false);

    edn_free(result_true.value);
    edn_free(result_false.value);
}

TEST(singletons_in_vector) {
    edn_result_t result = edn_read("[nil true false]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    edn_value_t* nil_val = edn_vector_get(result.value, 0);
    edn_value_t* true_val = edn_vector_get(result.value, 1);
    edn_value_t* false_val = edn_vector_get(result.value, 2);

    assert(edn_type(nil_val) == EDN_TYPE_NIL);
    assert(edn_type(true_val) == EDN_TYPE_BOOL);
    assert(edn_type(false_val) == EDN_TYPE_BOOL);

    /* Check source positions: "[nil true false]"
     *                          0123456789...
     * nil:   1-4
     * true:  5-9
     * false: 10-15
     */
    size_t start, end;

    assert(edn_source_position(nil_val, &start, &end));
    assert_uint_eq(start, 1);
    assert_uint_eq(end, 4);

    assert(edn_source_position(true_val, &start, &end));
    assert_uint_eq(start, 5);
    assert_uint_eq(end, 9);

    assert(edn_source_position(false_val, &start, &end));
    assert_uint_eq(start, 10);
    assert_uint_eq(end, 15);

    edn_free(result.value);
}

/* Test that freeing nil/bool values works correctly */
TEST(free_singleton_safe) {
    edn_result_t result = edn_read("nil", 0);
    assert(result.error == EDN_OK);

    edn_free(result.value);

    /* Parse again to verify allocation still works */
    edn_result_t result2 = edn_read("nil", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);

    edn_free(result2.value);
}

/* Test nil/bool in map keys */
TEST(singletons_in_map) {
    edn_result_t result = edn_read("{nil 1 true 2 false 3 nil 4}", 0);
    /* Map with duplicate key should fail */
    assert(result.error != EDN_OK);
    edn_free(result.value);

    /* Valid map with nil/bool as keys */
    edn_result_t result2 = edn_read("{nil 1 true 2 false 3}", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);
    assert(edn_type(result2.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result2.value) == 3);

    edn_free(result2.value);
}

int main(void) {
    printf("Running singleton value tests...\n");

    RUN_TEST(nil_source_position);
    RUN_TEST(true_source_position);
    RUN_TEST(false_source_position);
    RUN_TEST(true_false_different);
    RUN_TEST(singletons_in_vector);
    RUN_TEST(free_singleton_safe);
    RUN_TEST(singletons_in_map);

    TEST_SUMMARY("singleton value");
}
