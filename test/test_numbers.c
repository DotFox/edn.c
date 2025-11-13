#include <limits.h>
#include <math.h>
#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test number parsing with three-tier strategy */

/* Test SIMD digit scanning */
TEST(simd_scan_digits_simple) {
    const char* input = "12345abc";
    const char* result = edn_simd_scan_digits(input, input + strlen(input));
    assert(result == input + 5);
    assert(*result == 'a');
}

TEST(simd_scan_digits_long) {
    const char* input = "12345678901234567890xyz";
    const char* result = edn_simd_scan_digits(input, input + strlen(input));
    assert(result == input + 20);
    assert(*result == 'x');
}

TEST(simd_scan_digits_no_digits) {
    const char* input = "abc";
    const char* result = edn_simd_scan_digits(input, input + strlen(input));
    assert(result == input);
}

/* Test number scanning */
TEST(scan_number_simple_int) {
    const char* input = "42";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_INT64);
    assert(scan.negative == false);
    assert(scan.radix == 10);
    assert(scan.end - scan.start == 2);
}

TEST(scan_number_negative_int) {
    const char* input = "-123";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_INT64);
    assert(scan.negative == true);
}

TEST(scan_number_double) {
    const char* input = "3.14";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_DOUBLE);
}

TEST(scan_number_scientific) {
    const char* input = "1.5e10";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_DOUBLE);
}

TEST(scan_number_hex) {
    const char* input = "0x2A";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.radix == 16);
}

TEST(scan_number_binary) {
    const char* input = "2r1010";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.radix == 2);
}

/* Test int64_t parsing */
TEST(parse_int64_simple) {
    const char* input = "42";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == 42);
}

TEST(parse_int64_negative) {
    const char* input = "-123";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == -123);
}

TEST(parse_int64_zero) {
    const char* input = "0";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == 0);
}

TEST(parse_int64_max) {
    const char* input = "9223372036854775807"; /* INT64_MAX */
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == INT64_MAX);
}

TEST(parse_int64_min) {
    const char* input = "-9223372036854775808"; /* INT64_MIN */
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == true);
    assert(result == INT64_MIN);
}

TEST(parse_int64_overflow) {
    const char* input = "9223372036854775808"; /* INT64_MAX + 1 */
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 10);

    assert(success == false); /* Should overflow and return false */
}

TEST(parse_int64_hex) {
    const char* input = "2A";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 16);

    assert(success == true);
    assert(result == 42);
}

TEST(parse_int64_binary) {
    const char* input = "1010";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 2);

    assert(success == true);
    assert(result == 10);
}

TEST(parse_int64_octal) {
    const char* input = "777";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 8);

    assert(success == true);
    assert(result == 511); /* 7*64 + 7*8 + 7 = 511 */
}

TEST(parse_int64_octal_zero_prefix) {
    const char* input = "0777";
    int64_t result;
    bool success = edn_parse_int64(input, input + strlen(input), &result, 8);

    assert(success == true);
    assert(result == 511);
}

TEST(scan_number_octal) {
    const char* input = "0777";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.radix == 8);
    assert(scan.type == EDN_NUMBER_INT64);
}

TEST(scan_number_octal_edge_08) {
    /* 08 is INVALID in EDN (leading zero with non-octal digit) */
    const char* input = "08";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == false); /* Invalid number */
}

TEST(scan_number_octal_edge_09) {
    /* 09 is INVALID in EDN (leading zero with non-octal digit) */
    const char* input = "09";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == false); /* Invalid number */
}

TEST(scan_number_zero) {
    const char* input = "0";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.radix == 10); /* Just zero, decimal */
}

TEST(scan_number_zero_float) {
    const char* input = "0.5";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.radix == 10);
    assert(scan.type == EDN_NUMBER_DOUBLE);
}

/* Test double parsing */
TEST(parse_double_simple) {
    const char* input = "3.14";
    double result = edn_parse_double(input, input + strlen(input));

    assert(fabs(result - 3.14) < 0.0001);
}

TEST(parse_double_scientific) {
    const char* input = "1.5e10";
    double result = edn_parse_double(input, input + strlen(input));

    assert(fabs(result - 1.5e10) < 1e6);
}

/* Test API functions */
TEST(api_int64_get) {
    edn_value_t value;
    value.type = EDN_TYPE_INT;
    value.as.integer = 42;

    int64_t result;
    bool success = edn_int64_get(&value, &result);

    assert(success == true);
    assert(result == 42);
}

TEST(api_bigint_get) {
    edn_value_t value;
    value.type = EDN_TYPE_BIGINT;
    value.as.bigint.digits = "12345678901234567890";
    value.as.bigint.length = 20;
    value.as.bigint.negative = false;
    value.as.bigint.radix = 10;

    size_t length;
    bool negative;
    uint8_t radix;
    const char* digits = edn_bigint_get(&value, &length, &negative, &radix);

    assert(digits != NULL);
    assert(length == 20);
    assert(negative == false);
    assert(radix == 10);
    assert(strncmp(digits, "12345678901234567890", 20) == 0);
}

TEST(api_double_get) {
    edn_value_t value;
    value.type = EDN_TYPE_FLOAT;
    value.as.floating = 3.14;

    double result;
    bool success = edn_double_get(&value, &result);

    assert(success == true);
    assert(fabs(result - 3.14) < 0.0001);
}

TEST(api_number_as_double_int) {
    edn_value_t value;
    value.type = EDN_TYPE_INT;
    value.as.integer = 42;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == true);
    assert(result == 42.0);
}

TEST(api_number_as_double_bigint) {
    edn_value_t value;
    value.type = EDN_TYPE_BIGINT;
    value.as.bigint.digits = "12345";
    value.as.bigint.length = 5;
    value.as.bigint.negative = false;
    value.as.bigint.radix = 10;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == true);
    assert(result == 12345.0);
}

TEST(api_number_as_double_float) {
    edn_value_t value;
    value.type = EDN_TYPE_FLOAT;
    value.as.floating = 3.14;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == true);
    assert(fabs(result - 3.14) < 0.0001);
}

int main(void) {
    printf("Running number parsing tests...\n");

    /* SIMD tests */
    run_test_simd_scan_digits_simple();
    run_test_simd_scan_digits_long();
    run_test_simd_scan_digits_no_digits();

    /* Scanning tests */
    run_test_scan_number_simple_int();
    run_test_scan_number_negative_int();
    run_test_scan_number_double();
    run_test_scan_number_scientific();
    run_test_scan_number_hex();
    run_test_scan_number_binary();

    /* int64_t parsing tests */
    run_test_parse_int64_simple();
    run_test_parse_int64_negative();
    run_test_parse_int64_zero();
    run_test_parse_int64_max();
    run_test_parse_int64_min();
    run_test_parse_int64_overflow();
    run_test_parse_int64_hex();
    run_test_parse_int64_binary();
    run_test_parse_int64_octal();
    run_test_parse_int64_octal_zero_prefix();

    /* Scanning tests for octal */
    run_test_scan_number_octal();
    run_test_scan_number_octal_edge_08();
    run_test_scan_number_octal_edge_09();
    run_test_scan_number_zero();
    run_test_scan_number_zero_float();

    /* Double parsing tests */
    run_test_parse_double_simple();
    run_test_parse_double_scientific();

    /* API tests */
    run_test_api_int64_get();
    run_test_api_bigint_get();
    run_test_api_double_get();
    run_test_api_number_as_double_int();
    run_test_api_number_as_double_bigint();
    run_test_api_number_as_double_float();

    TEST_SUMMARY("number parsing");
}
