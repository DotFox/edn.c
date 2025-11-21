#include <math.h>
#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test fast double parsing with Clinger's algorithm */

TEST(test_fast_double_simple) {
    /* Simple decimal: 3.14 */
    const char* input = "3.14";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - 3.14) < 0.0001);

    edn_free(result.value);
}

TEST(test_fast_double_negative) {
    /* Negative: -2.5 */
    const char* input = "-2.5";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - (-2.5)) < 0.0001);

    edn_free(result.value);
}

TEST(test_fast_double_integer_part_only) {
    /* Integer as double: 42.0 */
    const char* input = "42.0";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - 42.0) < 0.0001);

    edn_free(result.value);
}

TEST(test_fast_double_small_exponent) {
    /* Scientific with small exponent: 1.5e2 = 150 */
    const char* input = "1.5e2";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - 150.0) < 0.0001);

    edn_free(result.value);
}

TEST(test_fast_double_negative_exponent) {
    /* Scientific with negative exponent: 1.5e-2 = 0.015 */
    const char* input = "1.5e-2";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - 0.015) < 0.0001);

    edn_free(result.value);
}

TEST(test_fast_double_boundary_exponent) {
    /* Exponent at boundary: 1.0e22 (still in fast path) */
    const char* input = "1.0e22";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(d == 1.0e22);

    edn_free(result.value);
}

TEST(test_slow_double_large_exponent) {
    /* Large exponent (fallback to strtod): 1.5e100 */
    const char* input = "1.5e100";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(d == 1.5e100);

    edn_free(result.value);
}

TEST(test_fast_double_many_decimals) {
    /* Many decimal places: 0.123456789 */
    const char* input = "0.123456789";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(fabs(d - 0.123456789) < 0.000000001);

    edn_free(result.value);
}

TEST(test_fast_double_zero) {
    /* Zero: 0.0 */
    const char* input = "0.0";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(d == 0.0);

    edn_free(result.value);
}

TEST(test_fast_double_very_small) {
    /* Very small: 1.0e-20 */
    const char* input = "1.0e-20";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double d;
    assert(edn_double_get(result.value, &d));
    assert(d == 1.0e-20);

    edn_free(result.value);
}

TEST(test_double_in_vector) {
    /* Test fast doubles in a vector */
    const char* input = "[3.14 -2.5 1.5e2 0.123]";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    edn_value_t* elem;
    double d;

    elem = edn_vector_get(result.value, 0);
    assert(elem != NULL);
    assert(edn_double_get(elem, &d));
    assert(fabs(d - 3.14) < 0.0001);

    elem = edn_vector_get(result.value, 1);
    assert(elem != NULL);
    assert(edn_double_get(elem, &d));
    assert(fabs(d - (-2.5)) < 0.0001);

    elem = edn_vector_get(result.value, 2);
    assert(elem != NULL);
    assert(edn_double_get(elem, &d));
    assert(fabs(d - 150.0) < 0.0001);

    elem = edn_vector_get(result.value, 3);
    assert(elem != NULL);
    assert(edn_double_get(elem, &d));
    assert(fabs(d - 0.123) < 0.0001);

    edn_free(result.value);
}

TEST(test_double_common_values) {
    /* Test common double values that should hit fast path */
    const char* inputs[] = {"0.5", "1.0", "2.0", "10.0", "100.0", "0.1", "0.01", "0.001"};
    double expected[] = {0.5, 1.0, 2.0, 10.0, 100.0, 0.1, 0.01, 0.001};

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        edn_result_t result = edn_read(inputs[i], strlen(inputs[i]));
        assert(result.value != NULL);
        assert(edn_type(result.value) == EDN_TYPE_FLOAT);

        double d;
        assert(edn_double_get(result.value, &d));
        assert(fabs(d - expected[i]) < 0.0001);

        edn_free(result.value);
    }
}

int main(void) {
    printf("Running fast double parsing tests...\n\n");

    RUN_TEST(test_fast_double_simple);
    RUN_TEST(test_fast_double_negative);
    RUN_TEST(test_fast_double_integer_part_only);
    RUN_TEST(test_fast_double_small_exponent);
    RUN_TEST(test_fast_double_negative_exponent);
    RUN_TEST(test_fast_double_boundary_exponent);
    RUN_TEST(test_slow_double_large_exponent);
    RUN_TEST(test_fast_double_many_decimals);
    RUN_TEST(test_fast_double_zero);
    RUN_TEST(test_fast_double_very_small);
    RUN_TEST(test_double_in_vector);
    RUN_TEST(test_double_common_values);

    TEST_SUMMARY("fast double parsing");
}
