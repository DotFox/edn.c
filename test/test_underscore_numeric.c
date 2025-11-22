#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test underscore in numeric literals (EDN_ENABLE_UNDERSCORE_IN_NUMERIC) */

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC

#include <math.h>

#include "../src/edn_internal.h"

/* Test basic integer parsing with underscores */
TEST(underscore_integer_simple) {
    edn_result_t r = edn_read("1_000", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 1000);

    edn_free(r.value);
}

TEST(underscore_integer_multiple) {
    edn_result_t r = edn_read("1_000_000", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 1000000);

    edn_free(r.value);
}

TEST(underscore_integer_many) {
    edn_result_t r = edn_read("4____2", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 42);

    edn_free(r.value);
}

TEST(underscore_integer_negative) {
    edn_result_t r = edn_read("-1_234", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -1234);

    edn_free(r.value);
}

TEST(underscore_integer_large) {
    edn_result_t r = edn_read("9_876_543_210", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 9876543210LL);

    edn_free(r.value);
}

/* Test floating point with underscores */
TEST(underscore_float_integer_part) {
    edn_result_t r = edn_read("1_000.5", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1000.5) < 0.0001);

    edn_free(r.value);
}

TEST(underscore_float_fractional_part) {
    edn_result_t r = edn_read("3.14_15_92", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 3.141592) < 0.000001);

    edn_free(r.value);
}

TEST(underscore_float_both_parts) {
    edn_result_t r = edn_read("1_234.56_78", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1234.5678) < 0.0001);

    edn_free(r.value);
}

/* Test scientific notation with underscores */
TEST(underscore_scientific_mantissa) {
    edn_result_t r = edn_read("1_500e10", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1500e10) < 1e12);

    edn_free(r.value);
}

TEST(underscore_scientific_exponent) {
    edn_result_t r = edn_read("1.5e1_0", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1.5e10) < 1e6);

    edn_free(r.value);
}

TEST(underscore_scientific_both) {
    edn_result_t r = edn_read("1_5.2_5e1_0", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 15.25e10) < 1e8);

    edn_free(r.value);
}

TEST(underscore_scientific_negative_exp) {
    edn_result_t r = edn_read("3e-1_2", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 3e-12) < 1e-15);

    edn_free(r.value);
}

/* Test BigInt with underscores */
TEST(underscore_bigint) {
    edn_result_t r = edn_read("1_234_567_890_123_456_789N", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGINT);

    size_t length;
    bool negative;
    uint8_t radix;
    const char* digits = edn_bigint_get(r.value, &length, &negative, &radix);

    assert(digits != NULL);
    assert(negative == false);
    assert(radix == 10);

    edn_free(r.value);
}

TEST(underscore_bigint_negative) {
    edn_result_t r = edn_read("-9_999_999_999_999_999_999N", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGINT);

    size_t length;
    bool negative;
    uint8_t radix;
    const char* digits = edn_bigint_get(r.value, &length, &negative, &radix);

    assert(digits != NULL);
    assert(negative == true);
    assert(radix == 10);

    edn_free(r.value);
}

/* Test BigDecimal with underscores */
TEST(underscore_bigdec) {
    edn_result_t r = edn_read("1_234.56_78M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == false);

    edn_free(r.value);
}

TEST(underscore_bigdec_exponent) {
    edn_result_t r = edn_read("1_5.2_5e1_0M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    edn_free(r.value);
}

#ifdef EDN_ENABLE_EXTENDED_INTEGERS

/* Test hex with underscores */
TEST(underscore_hex) {
    edn_result_t r = edn_read("0xDE_AD_BE_EF", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 0xDEADBEEF);

    edn_free(r.value);
}

TEST(underscore_hex_uppercase) {
    edn_result_t r = edn_read("0xFF_FF", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 0xFFFF);

    edn_free(r.value);
}

/* Test octal with underscores */
TEST(underscore_octal) {
    edn_result_t r = edn_read("07_77", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 0777);

    edn_free(r.value);
}

/* Test binary with underscores */
TEST(underscore_binary) {
    edn_result_t r = edn_read("2r1010_1010", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 170); // 0b10101010 = 170

    edn_free(r.value);
}

/* Test radix notation with underscores */
TEST(underscore_radix_36) {
    edn_result_t r = edn_read("36rZ_Z", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 1295); /* 35*36 + 35 */

    edn_free(r.value);
}

#endif /* EDN_ENABLE_EXTENDED_INTEGERS */

/* Test invalid underscore positions */
TEST(underscore_invalid_at_start) {
    edn_result_t r = edn_read("_123", 0);

    /* Should not parse as a number - underscores at start are invalid */
    /* This should parse as a symbol instead */
    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_SYMBOL);

    edn_free(r.value);
}

TEST(underscore_invalid_at_end) {
    edn_result_t r = edn_read("123_", 0);

    /* Should fail - underscore at end is invalid */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_before_dot) {
    edn_result_t r = edn_read("123_.5", 0);

    /* Should fail - underscore adjacent to decimal point */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_after_dot) {
    edn_result_t r = edn_read("123._5", 0);

    /* Should fail - underscore adjacent to decimal point */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_before_exponent) {
    edn_result_t r = edn_read("123_e10", 0);

    /* Should fail - underscore before exponent marker */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_after_exponent) {
    edn_result_t r = edn_read("123e_10", 0);

    /* Should fail - underscore after exponent marker */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_before_N_suffix) {
    edn_result_t r = edn_read("123_N", 0);

    /* Should fail - underscore before N suffix */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

TEST(underscore_invalid_before_M_suffix) {
    edn_result_t r = edn_read("123.45_M", 0);

    /* Should fail - underscore before M suffix */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

/* Test underscores in collections */
TEST(underscore_in_vector) {
    edn_result_t r = edn_read("[1_000 2_000 3_000]", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(r.value) == 3);

    edn_value_t* elem0 = edn_vector_get(r.value, 0);
    int64_t val0;
    edn_int64_get(elem0, &val0);
    assert(val0 == 1000);

    edn_value_t* elem1 = edn_vector_get(r.value, 1);
    int64_t val1;
    edn_int64_get(elem1, &val1);
    assert(val1 == 2000);

    edn_value_t* elem2 = edn_vector_get(r.value, 2);
    int64_t val2;
    edn_int64_get(elem2, &val2);
    assert(val2 == 3000);

    edn_free(r.value);
}

TEST(underscore_in_map) {
    edn_result_t r = edn_read("{:count 1_000 :total 5_000}", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_MAP);
    assert(edn_map_count(r.value) == 2);

    edn_value_t* val0 = edn_map_get_value(r.value, 0);
    int64_t num0;
    edn_int64_get(val0, &num0);
    assert(num0 == 1000);

    edn_value_t* val1 = edn_map_get_value(r.value, 1);
    int64_t num1;
    edn_int64_get(val1, &num1);
    assert(num1 == 5000);

    edn_free(r.value);
}

/* Test number scanner with underscores */
TEST(scan_number_underscore_simple) {
    const char* input = "1_000";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_INT64);
    assert(scan.radix == 10);
}

TEST(scan_number_underscore_float) {
    const char* input = "3.14_15";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_DOUBLE);
}

TEST(scan_number_underscore_invalid_end) {
    /* "123_" - scanner will parse "123" and stop at the underscore */
    /* The trailing underscore will be caught as an invalid delimiter by the full parser */
    const char* input = "123_";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    /* Scanner should stop before the trailing underscore */
    assert(scan.end - input == 3); /* Scanned "123", stopped at "_" */
}

TEST(scan_number_underscore_invalid_dot) {
    /* "123_.5" - scanner will parse "123" and stop at the underscore before dot */
    const char* input = "123_.5";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    /* Scanner should stop before the underscore */
    assert(scan.end - input == 3); /* Scanned "123", stopped at "_" */
}

/* Test parse_int64 with underscores */
TEST(parse_int64_underscore) {
    const char* input = "1_000_000";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == 1000000);
}

TEST(parse_int64_underscore_negative) {
    const char* input = "-1_234_567";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == -1234567);
}

#ifdef EDN_ENABLE_EXTENDED_INTEGERS
TEST(parse_int64_underscore_hex) {
    const char* input = "DE_AD";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 16);

    assert(success == true);
    assert(result == 0xDEAD);
}

TEST(parse_int64_underscore_binary) {
    const char* input = "1010_1010";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 2);

    assert(success == true);
    assert(result == 170); // 0b10101010 = 170
}
#endif

/* Test parse_double with underscores */
TEST(parse_double_underscore) {
    const char* input = "1_234.56_78";
    double result = edn_parse_double(input, input + strlen(input));

    assert(fabs(result - 1234.5678) < 0.0001);
}

TEST(parse_double_underscore_scientific) {
    const char* input = "1_5.2_5e1_0";
    double result = edn_parse_double(input, input + strlen(input));

    assert(fabs(result - 15.25e10) < 1e8);
}

#else /* EDN_ENABLE_UNDERSCORE_IN_NUMERIC not defined */

/* Test that underscores fail when feature is disabled */
TEST(underscore_disabled) {
    edn_result_t r = edn_read("1_000", 0);

    /* Should fail or parse differently when feature is disabled */
    /* The number scanner should stop at the underscore */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);

    edn_free(r.value);
}

#endif /* EDN_ENABLE_UNDERSCORE_IN_NUMERIC */

int main(void) {
    printf("Running underscore in numeric tests...\n");

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* Integer tests */
    run_test_underscore_integer_simple();
    run_test_underscore_integer_multiple();
    run_test_underscore_integer_many();
    run_test_underscore_integer_negative();
    run_test_underscore_integer_large();

    /* Float tests */
    run_test_underscore_float_integer_part();
    run_test_underscore_float_fractional_part();
    run_test_underscore_float_both_parts();

    /* Scientific notation tests */
    run_test_underscore_scientific_mantissa();
    run_test_underscore_scientific_exponent();
    run_test_underscore_scientific_both();
    run_test_underscore_scientific_negative_exp();

    /* BigInt tests */
    run_test_underscore_bigint();
    run_test_underscore_bigint_negative();

    /* BigDecimal tests */
    run_test_underscore_bigdec();
    run_test_underscore_bigdec_exponent();

#ifdef EDN_ENABLE_EXTENDED_INTEGERS
    /* Extended integer format tests */
    run_test_underscore_hex();
    run_test_underscore_hex_uppercase();
    run_test_underscore_octal();
    run_test_underscore_binary();
    run_test_underscore_radix_36();
#endif

    /* Invalid position tests */
    run_test_underscore_invalid_at_start();
    run_test_underscore_invalid_at_end();
    run_test_underscore_invalid_before_dot();
    run_test_underscore_invalid_after_dot();
    run_test_underscore_invalid_before_exponent();
    run_test_underscore_invalid_after_exponent();
    run_test_underscore_invalid_before_N_suffix();
    run_test_underscore_invalid_before_M_suffix();

    /* Collection tests */
    run_test_underscore_in_vector();
    run_test_underscore_in_map();

    /* Scanner tests */
    run_test_scan_number_underscore_simple();
    run_test_scan_number_underscore_float();
    run_test_scan_number_underscore_invalid_end();
    run_test_scan_number_underscore_invalid_dot();

    /* Parser tests */
    run_test_parse_int64_underscore();
    run_test_parse_int64_underscore_negative();
#ifdef EDN_ENABLE_EXTENDED_INTEGERS
    run_test_parse_int64_underscore_hex();
    run_test_parse_int64_underscore_binary();
#endif
    run_test_parse_double_underscore();
    run_test_parse_double_underscore_scientific();
#else
    /* Feature disabled test */
    run_test_underscore_disabled();
#endif

    TEST_SUMMARY("underscore in numeric");
}
