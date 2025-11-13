#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test SWAR 8-digit parsing specifically */

TEST(test_swar_eight_digits) {
    /* Test parsing exactly 8 digits */
    const char* input = "12345678";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 12345678);

    edn_free(result.value);
}

TEST(test_swar_sixteen_digits) {
    /* Test parsing 16 digits (two 8-digit chunks) */
    const char* input = "1234567890123456";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 1234567890123456LL);

    edn_free(result.value);
}

TEST(test_swar_long_number) {
    /* Test parsing very long number (multiple 8-digit chunks) */
    const char* input = "123456789012345678"; /* 18 digits */
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 123456789012345678LL);

    edn_free(result.value);
}

TEST(test_swar_with_remainder) {
    /* Test parsing number where digits don't align to 8 */
    const char* input = "1234567890"; /* 10 digits: 8 + 2 */
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 1234567890LL);

    edn_free(result.value);
}

TEST(test_swar_negative_long) {
    /* Test negative number with SWAR */
    const char* input = "-12345678901234";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == -12345678901234LL);

    edn_free(result.value);
}

TEST(test_swar_all_nines) {
    /* Test SWAR with all nines */
    const char* input = "99999999";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 99999999);

    edn_free(result.value);
}

TEST(test_swar_max_int64) {
    /* Test INT64_MAX (19 digits) */
    const char* input = "9223372036854775807";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(result.value, &num));
    assert(num == 9223372036854775807LL);

    edn_free(result.value);
}

TEST(test_swar_overflow_detection) {
    /* Test that overflow is detected correctly with SWAR */
    const char* input = "9223372036854775808"; /* INT64_MAX + 1 */
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BIGINT); /* Should overflow to BigInt */

    edn_free(result.value);
}

TEST(test_swar_in_vector) {
    /* Test SWAR parsing in a vector context */
    const char* input = "[12345678 87654321 11111111]";
    edn_result_t result = edn_parse(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    edn_value_t* elem;
    int64_t num;

    elem = edn_vector_get(result.value, 0);
    assert(elem != NULL);
    assert(edn_int64_get(elem, &num));
    assert(num == 12345678);

    elem = edn_vector_get(result.value, 1);
    assert(elem != NULL);
    assert(edn_int64_get(elem, &num));
    assert(num == 87654321);

    elem = edn_vector_get(result.value, 2);
    assert(elem != NULL);
    assert(edn_int64_get(elem, &num));
    assert(num == 11111111);

    edn_free(result.value);
}

TEST(test_swar_short_numbers_still_work) {
    /* Ensure short numbers (< 8 digits) still work correctly */
    const char* inputs[] = {"1", "12", "123", "1234", "12345", "123456", "1234567"};
    int64_t expected[] = {1, 12, 123, 1234, 12345, 123456, 1234567};

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        edn_result_t result = edn_parse(inputs[i], strlen(inputs[i]));
        assert(result.value != NULL);
        assert(edn_type(result.value) == EDN_TYPE_INT);

        int64_t num;
        assert(edn_int64_get(result.value, &num));
        assert(num == expected[i]);

        edn_free(result.value);
    }
}

int main(void) {
    printf("Running SWAR number parsing tests...\n\n");

    RUN_TEST(test_swar_eight_digits);
    RUN_TEST(test_swar_sixteen_digits);
    RUN_TEST(test_swar_long_number);
    RUN_TEST(test_swar_with_remainder);
    RUN_TEST(test_swar_negative_long);
    RUN_TEST(test_swar_all_nines);
    RUN_TEST(test_swar_max_int64);
    RUN_TEST(test_swar_overflow_detection);
    RUN_TEST(test_swar_in_vector);
    RUN_TEST(test_swar_short_numbers_still_work);

    TEST_SUMMARY("SWAR number parsing");
}
