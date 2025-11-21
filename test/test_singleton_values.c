/**
 * Tests for singleton values (nil, true, false)
 *
 * Verifies that immutable values are reused across multiple parses.
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test that multiple nil values are the same instance */
TEST(nil_singleton) {
    edn_result_t result1 = edn_read("nil", 0);
    assert(result1.error == EDN_OK);
    assert(result1.value != NULL);
    assert(edn_type(result1.value) == EDN_TYPE_NIL);

    edn_result_t result2 = edn_read("nil", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);
    assert(edn_type(result2.value) == EDN_TYPE_NIL);

    /* Both nil values should be the same instance */
    assert(result1.value == result2.value);

    edn_free(result1.value);
    edn_free(result2.value);
}

/* Test that multiple true values are the same instance */
TEST(true_singleton) {
    edn_result_t result1 = edn_read("true", 0);
    assert(result1.error == EDN_OK);
    assert(result1.value != NULL);
    assert(edn_type(result1.value) == EDN_TYPE_BOOL);

    edn_result_t result2 = edn_read("true", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);
    assert(edn_type(result2.value) == EDN_TYPE_BOOL);

    /* Both true values should be the same instance */
    assert(result1.value == result2.value);

    edn_free(result1.value);
    edn_free(result2.value);
}

/* Test that multiple false values are the same instance */
TEST(false_singleton) {
    edn_result_t result1 = edn_read("false", 0);
    assert(result1.error == EDN_OK);
    assert(result1.value != NULL);
    assert(edn_type(result1.value) == EDN_TYPE_BOOL);

    edn_result_t result2 = edn_read("false", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);
    assert(edn_type(result2.value) == EDN_TYPE_BOOL);

    /* Both false values should be the same instance */
    assert(result1.value == result2.value);

    edn_free(result1.value);
    edn_free(result2.value);
}

/* Test that true and false are different instances */
TEST(true_false_different) {
    edn_result_t result_true = edn_read("true", 0);
    assert(result_true.error == EDN_OK);
    assert(result_true.value != NULL);

    edn_result_t result_false = edn_read("false", 0);
    assert(result_false.error == EDN_OK);
    assert(result_false.value != NULL);

    /* True and false should be different instances */
    assert(result_true.value != result_false.value);

    edn_free(result_true.value);
    edn_free(result_false.value);
}

/* Test singletons in a vector */
TEST(singletons_in_vector) {
    edn_result_t result = edn_read("[nil true false nil true false]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 6);

    edn_value_t* nil1 = edn_vector_get(result.value, 0);
    edn_value_t* true1 = edn_vector_get(result.value, 1);
    edn_value_t* false1 = edn_vector_get(result.value, 2);
    edn_value_t* nil2 = edn_vector_get(result.value, 3);
    edn_value_t* true2 = edn_vector_get(result.value, 4);
    edn_value_t* false2 = edn_vector_get(result.value, 5);

    /* All nils should be the same instance */
    assert(nil1 == nil2);

    /* All trues should be the same instance */
    assert(true1 == true2);

    /* All falses should be the same instance */
    assert(false1 == false2);

    /* But they should all be different from each other */
    assert(nil1 != true1);
    assert(nil1 != false1);
    assert(true1 != false1);

    edn_free(result.value);
}

/* Test that freeing singletons is safe (no-op) */
TEST(free_singleton_safe) {
    edn_result_t result = edn_read("nil", 0);
    assert(result.error == EDN_OK);

    /* Freeing singleton should be safe (no-op) */
    edn_free(result.value);

    /* Parse again to verify singleton still works */
    edn_result_t result2 = edn_read("nil", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value == result.value); /* Should be the same instance */

    edn_free(result2.value);
}

/* Test singletons in map keys */
TEST(singletons_in_map) {
    edn_result_t result = edn_read("{nil 1 true 2 false 3 nil 4}", 0);
    /* Map with duplicate key should fail */
    assert(result.error != EDN_OK);
    edn_free(result.value);

    /* Valid map with singletons as keys */
    edn_result_t result2 = edn_read("{nil 1 true 2 false 3}", 0);
    assert(result2.error == EDN_OK);
    assert(result2.value != NULL);
    assert(edn_type(result2.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result2.value) == 3);

    edn_free(result2.value);
}

int main(void) {
    printf("Running singleton value tests...\n");

    RUN_TEST(nil_singleton);
    RUN_TEST(true_singleton);
    RUN_TEST(false_singleton);
    RUN_TEST(true_false_different);
    RUN_TEST(singletons_in_vector);
    RUN_TEST(free_singleton_safe);
    RUN_TEST(singletons_in_map);

    TEST_SUMMARY("singleton value");
}
