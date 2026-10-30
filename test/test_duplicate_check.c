/**
 * Test that duplicates in sets and maps are rejected (EDN spec requirement)
 */

#include "../include/edn.h"
#include "test_framework.h"

/* Test: Set with duplicate elements should be rejected */
TEST(set_duplicate_elements_rejected) {
    edn_result_t result = edn_read("#{1 2 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Set without duplicates should work */
TEST(set_no_duplicates_allowed) {
    edn_result_t result = edn_read("#{1 2 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 3);

    edn_free(result.value);
}

/* Test: Map with duplicate keys should be rejected */
TEST(map_duplicate_keys_rejected) {
    edn_result_t result = edn_read("{:a 1 :b 2 :a 3}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Map without duplicate keys should work */
TEST(map_no_duplicates_allowed) {
    edn_result_t result = edn_read("{:a 1 :b 2 :c 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 3);

    edn_free(result.value);
}

/* Test: Set with duplicate strings */
TEST(set_duplicate_strings_rejected) {
    edn_result_t result = edn_read("#{\"foo\" \"bar\" \"foo\"}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Map with duplicate complex keys (vectors) */
TEST(map_duplicate_vector_keys_rejected) {
    edn_result_t result = edn_read("{[1 2] :a [1 2] :b}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Set with duplicate keywords */
TEST(set_duplicate_keywords_rejected) {
    edn_result_t result = edn_read("#{:foo :bar :foo}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Map with nested duplicate check */
TEST(map_nested_duplicate_keys_rejected) {
    edn_result_t result = edn_read("{{:x 1} :a {:x 1} :b}", 0);

    /* Maps as keys, structurally equal */
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Empty set and map (no duplicates possible) */
TEST(empty_collections_no_error) {
    edn_result_t r1 = edn_read("#{}", 0);
    assert(r1.error == EDN_OK);
    assert(edn_set_count(r1.value) == 0);
    edn_free(r1.value);

    edn_result_t r2 = edn_read("{}", 0);
    assert(r2.error == EDN_OK);
    assert(edn_map_count(r2.value) == 0);
    edn_free(r2.value);
}

/* Test: Single element set and map (no duplicates possible) */
TEST(single_element_no_error) {
    edn_result_t r1 = edn_read("#{42}", 0);
    assert(r1.error == EDN_OK);
    assert(edn_set_count(r1.value) == 1);
    edn_free(r1.value);

    edn_result_t r2 = edn_read("{:a 1}", 0);
    assert(r2.error == EDN_OK);
    assert(edn_map_count(r2.value) == 1);
    edn_free(r2.value);
}

/* Test: Large set without duplicates (tests sorted algorithm) */
TEST(large_set_no_duplicates) {
    edn_result_t result = edn_read("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 20);

    edn_free(result.value);
}

/* Test: Large set with duplicate at end (tests sorted algorithm) */
TEST(large_set_duplicate_at_end) {
    edn_result_t result = edn_read("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Set with mixed types - no duplicates */
TEST(set_mixed_types_no_duplicates) {
    edn_result_t result = edn_read("#{1 \"1\" :foo foo true 1.0}", 0);

    /* All different types, no duplicates */
    assert(result.error == EDN_OK);
    assert(edn_set_count(result.value) == 6);

    edn_free(result.value);
}

/* Test: Map with different value for same key still duplicate */
TEST(map_same_key_different_value_rejected) {
    edn_result_t result = edn_read("{1 :a 2 :b 1 :c}", 0);

    /* Key 1 appears twice, even with different values */
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

TEST(collection_duplicates_at_sort_thresholds) {
    const size_t counts[] = {16, 17, 1000};
    const struct {
        const char* format;
        const char* duplicate;
    } forms[] = {
        {"[%zu]", "[0]"},
        {"(%zu)", "(0)"},
        {"[%zu]", "(0)"},
        {"#{%zu -1}", "#{-1 0}"},
        {"{:a %zu :b -1}", "{:b -1 :a 0}"},
        {"#tag [%zu]", "#tag [0]"},
    };
    char input[32768];

    for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
        for (size_t f = 0; f < sizeof(forms) / sizeof(forms[0]); f++) {
            for (int is_map = 0; is_map <= 1; is_map++) {
                for (int duplicate = 0; duplicate <= 1; duplicate++) {
                    size_t used =
                        (size_t) snprintf(input, sizeof(input), "%s", is_map ? "{" : "#{");
                    for (size_t i = 0; i < counts[c]; i++) {
                        int written;
                        if (duplicate && i == counts[c] - 1) {
                            written = snprintf(input + used, sizeof(input) - used, "%s",
                                               forms[f].duplicate);
                        } else {
                            written =
                                snprintf(input + used, sizeof(input) - used, forms[f].format, i);
                        }
                        assert(written >= 0 && (size_t) written < sizeof(input) - used);
                        used += (size_t) written;
                        written = snprintf(input + used, sizeof(input) - used, "%s",
                                           is_map ? " nil " : " ");
                        assert(written >= 0 && (size_t) written < sizeof(input) - used);
                        used += (size_t) written;
                    }
                    assert(used + 1 < sizeof(input));
                    input[used++] = '}';
                    input[used] = '\0';

                    edn_result_t result = edn_read(input, used);
                    size_t count =
                        is_map ? edn_map_count(result.value) : edn_set_count(result.value);
                    bool has_value = result.value != NULL;
                    edn_free(result.value);
                    assert_int_eq(result.error, duplicate ? (is_map ? EDN_ERROR_DUPLICATE_KEY
                                                                    : EDN_ERROR_DUPLICATE_ELEMENT)
                                                          : EDN_OK);
                    assert(has_value == !duplicate);
                    if (!duplicate) {
                        assert_uint_eq(count, counts[c]);
                    }
                }
            }
        }
    }
}

int main(void) {
    printf("Running duplicate detection tests...\n");

    RUN_TEST(set_duplicate_elements_rejected);
    RUN_TEST(set_no_duplicates_allowed);
    RUN_TEST(map_duplicate_keys_rejected);
    RUN_TEST(map_no_duplicates_allowed);
    RUN_TEST(set_duplicate_strings_rejected);
    RUN_TEST(map_duplicate_vector_keys_rejected);
    RUN_TEST(set_duplicate_keywords_rejected);
    RUN_TEST(map_nested_duplicate_keys_rejected);
    RUN_TEST(empty_collections_no_error);
    RUN_TEST(single_element_no_error);
    RUN_TEST(large_set_no_duplicates);
    RUN_TEST(large_set_duplicate_at_end);
    RUN_TEST(set_mixed_types_no_duplicates);
    RUN_TEST(map_same_key_different_value_rejected);
    RUN_TEST(collection_duplicates_at_sort_thresholds);

    TEST_SUMMARY("duplicate detection");
}
