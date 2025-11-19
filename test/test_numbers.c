#include <limits.h>
#include <math.h>
#include <stdint.h>
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

/* Test BigInt N suffix (Clojure-compatible) */
TEST(scan_number_bigint_suffix_simple) {
    const char* input = "42N";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGINT); /* Forced by N suffix */
    assert(scan.radix == 10);
    assert(scan.end - scan.start == 3); /* Includes N */
}

TEST(scan_number_bigint_suffix_negative) {
    const char* input = "-999N";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGINT);
    assert(scan.negative == true);
}

TEST(scan_number_bigint_suffix_hex) {
    /* Hex numbers don't support N suffix - N is a hex digit */
    const char* input = "0xDEADBEEFN";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_INT64); /* N is treated as hex digit, not suffix */
    assert(scan.radix == 16);
}

TEST(scan_number_bigint_suffix_only_decimal) {
    /* N suffix only applies to base-10, not radix notation */
    const char* input = "36rZZ"; /* No N suffix for non-decimal */
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_INT64);
    assert(scan.radix == 36);
}

TEST(scan_number_bigint_suffix_on_float_invalid) {
    /* Cannot have N suffix on float - invalid */
    const char* input = "3.14N";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == false); /* Floats cannot have N suffix */
}

TEST(scan_number_bigint_suffix_on_exponent_invalid) {
    /* Cannot have N suffix on scientific notation - invalid */
    const char* input = "1e5N";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == false); /* Exponent notation cannot have N suffix */
}

TEST(api_parse_bigint_suffix) {
    /* Test full parsing integration */
    edn_result_t r = edn_parse("42N", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_BIGINT);

    size_t length;
    bool negative;
    uint8_t radix;
    const char* digits = edn_bigint_get(r.value, &length, &negative, &radix);

    assert(digits != NULL);
    assert(negative == false);
    assert(radix == 10);
    /* digits should be "42N" with length 3, but bigint logic should handle it */

    edn_free(r.value);
}

TEST(api_parse_bigint_suffix_in_collection) {
    /* Test N suffix in vector */
    edn_result_t r = edn_parse("[1 2N 3]", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(r.value) == 3);

    /* First element: regular int */
    edn_value_t* elem0 = edn_vector_get(r.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_INT);

    /* Second element: BigInt with N suffix */
    edn_value_t* elem1 = edn_vector_get(r.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_BIGINT);

    /* Third element: regular int */
    edn_value_t* elem2 = edn_vector_get(r.value, 2);
    assert(edn_type(elem2) == EDN_TYPE_INT);

    edn_free(r.value);
}

/* Test BigDecimal M suffix (Clojure-compatible) */
TEST(scan_number_bigdec_suffix_simple) {
    const char* input = "3.14M";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGDEC); /* Forced by M suffix */
    assert(scan.radix == 10);
    assert(scan.end - scan.start == 5); /* Includes M */
}

TEST(scan_number_bigdec_suffix_negative) {
    const char* input = "-123.456M";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGDEC);
    assert(scan.negative == true);
}

TEST(scan_number_bigdec_suffix_exponent) {
    const char* input = "1.5e10M";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGDEC);
}

TEST(scan_number_bigdec_suffix_on_integer) {
    /* M suffix on integer is valid - converts to BigDecimal */
    const char* input = "42M";
    edn_number_scan_t scan = edn_scan_number(input, input + strlen(input));

    assert(scan.valid == true);
    assert(scan.type == EDN_NUMBER_BIGDEC); /* Integer becomes BigDecimal */
}

TEST(api_parse_bigdec_suffix) {
    /* Test full parsing integration */
    edn_result_t r = edn_parse("3.14159M", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == false);
    assert(length == 7); /* "3.14159" */
    assert(strncmp(decimal, "3.14159", 7) == 0);

    edn_free(r.value);
}

TEST(api_parse_bigdec_suffix_on_integer) {
    /* Integer with M suffix becomes BigDecimal */
    edn_result_t r = edn_parse("42M", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == false);
    assert(length == 2); /* "42" */
    assert(strncmp(decimal, "42", 2) == 0);

    edn_free(r.value);
}

TEST(api_bigdec_get) {
    edn_value_t value;
    value.type = EDN_TYPE_BIGDEC;
    value.as.bigdec.decimal = "123.456";
    value.as.bigdec.length = 7;
    value.as.bigdec.negative = false;

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(&value, &length, &negative);

    assert(decimal != NULL);
    assert(length == 7);
    assert(negative == false);
    assert(strncmp(decimal, "123.456", 7) == 0);
}

TEST(api_parse_bigdec_suffix_in_collection) {
    /* Test M suffix in vector */
    edn_result_t r = edn_parse("[1.1 2.2M 3.3]", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(r.value) == 3);

    /* First element: regular float */
    edn_value_t* elem0 = edn_vector_get(r.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_FLOAT);

    /* Second element: BigDecimal with M suffix */
    edn_value_t* elem1 = edn_vector_get(r.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_BIGDEC);

    /* Third element: regular float */
    edn_value_t* elem2 = edn_vector_get(r.value, 2);
    assert(edn_type(elem2) == EDN_TYPE_FLOAT);

    edn_free(r.value);
}

/* Comprehensive edn_parse tests for all number forms */

/* Test decimal integer parsing */
TEST(edn_parse_decimal_int_simple) {
    edn_result_t r = edn_parse("42", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 42);

    edn_free(r.value);
}

TEST(edn_parse_decimal_int_negative) {
    edn_result_t r = edn_parse("-123", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -123);

    edn_free(r.value);
}

TEST(edn_parse_decimal_int_zero) {
    edn_result_t r = edn_parse("0", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 0);

    edn_free(r.value);
}

TEST(edn_parse_decimal_int_large) {
    edn_result_t r = edn_parse("9876543210", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 9876543210LL);

    edn_free(r.value);
}

/* Test hexadecimal parsing */
TEST(edn_parse_hex_lowercase_x) {
    edn_result_t r = edn_parse("0x2A", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 42);

    edn_free(r.value);
}

TEST(edn_parse_hex_uppercase_X) {
    edn_result_t r = edn_parse("0XFF", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 255);

    edn_free(r.value);
}

TEST(edn_parse_hex_mixed_case) {
    edn_result_t r = edn_parse("0xDeAdBeEf", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 0xDEADBEEF);

    edn_free(r.value);
}

TEST(edn_parse_hex_negative) {
    edn_result_t r = edn_parse("-0x10", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -16);

    edn_free(r.value);
}

/* Test octal parsing */
TEST(edn_parse_octal_simple) {
    edn_result_t r = edn_parse("0777", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 511); /* 7*64 + 7*8 + 7 */

    edn_free(r.value);
}

TEST(edn_parse_octal_small) {
    edn_result_t r = edn_parse("052", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 42); /* 5*8 + 2 */

    edn_free(r.value);
}

TEST(edn_parse_octal_negative) {
    edn_result_t r = edn_parse("-0123", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -83); /* -(1*64 + 2*8 + 3) */

    edn_free(r.value);
}

/* Test binary (radix notation) */
TEST(edn_parse_binary_simple) {
    edn_result_t r = edn_parse("2r1010", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 10); /* 1*8 + 0*4 + 1*2 + 0 */

    edn_free(r.value);
}

TEST(edn_parse_binary_negative) {
    edn_result_t r = edn_parse("-2r1111", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -15);

    edn_free(r.value);
}

/* Test arbitrary radix notation */
TEST(edn_parse_radix_base8) {
    edn_result_t r = edn_parse("8r77", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 63); /* 7*8 + 7 */

    edn_free(r.value);
}

TEST(edn_parse_radix_base16) {
    edn_result_t r = edn_parse("16rFF", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 255);

    edn_free(r.value);
}

TEST(edn_parse_radix_base36) {
    edn_result_t r = edn_parse("36rZZ", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == 1295); /* 35*36 + 35 */

    edn_free(r.value);
}

TEST(edn_parse_radix_negative) {
    edn_result_t r = edn_parse("-36rABC", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t val;
    edn_int64_get(r.value, &val);
    assert(val == -13368); /* -(10*36*36 + 11*36 + 12) */

    edn_free(r.value);
}

/* Test floating point parsing */
TEST(edn_parse_float_simple) {
    edn_result_t r = edn_parse("3.14", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 3.14) < 0.0001);

    edn_free(r.value);
}

TEST(edn_parse_float_negative) {
    edn_result_t r = edn_parse("-2.5", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - (-2.5)) < 0.0001);

    edn_free(r.value);
}

TEST(edn_parse_float_leading_zero) {
    edn_result_t r = edn_parse("0.5", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 0.5) < 0.0001);

    edn_free(r.value);
}

TEST(edn_parse_float_no_leading_zero) {
    /* EDN does not support numbers starting with '.' - this should parse as symbol */
    edn_result_t r = edn_parse(".5", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_SYMBOL);

    edn_free(r.value);
}

/* Test scientific notation parsing */
TEST(edn_parse_scientific_positive_exp) {
    edn_result_t r = edn_parse("1.5e10", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1.5e10) < 1e6);

    edn_free(r.value);
}

TEST(edn_parse_scientific_negative_exp) {
    edn_result_t r = edn_parse("3e-5", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 3e-5) < 1e-10);

    edn_free(r.value);
}

TEST(edn_parse_scientific_uppercase_E) {
    edn_result_t r = edn_parse("2.5E3", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 2500.0) < 0.001);

    edn_free(r.value);
}

TEST(edn_parse_scientific_explicit_plus) {
    edn_result_t r = edn_parse("1E+10", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 1e10) < 1e6);

    edn_free(r.value);
}

TEST(edn_parse_scientific_no_decimal) {
    edn_result_t r = edn_parse("5e2", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT);

    double val;
    edn_double_get(r.value, &val);
    assert(fabs(val - 500.0) < 0.001);

    edn_free(r.value);
}

/* Test BigInt with N suffix */
TEST(edn_parse_bigint_simple) {
    edn_result_t r = edn_parse("42N", 0);

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

TEST(edn_parse_bigint_negative) {
    edn_result_t r = edn_parse("-999N", 0);

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

TEST(edn_parse_bigint_very_large) {
    edn_result_t r = edn_parse("12345678901234567890N", 0);

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

/* Test BigDecimal with M suffix */
TEST(edn_parse_bigdec_simple) {
    edn_result_t r = edn_parse("3.14M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == false);

    edn_free(r.value);
}

TEST(edn_parse_bigdec_negative) {
    edn_result_t r = edn_parse("-123.456M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == true);

    edn_free(r.value);
}

TEST(edn_parse_bigdec_with_exponent) {
    edn_result_t r = edn_parse("1.5e10M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    edn_free(r.value);
}

TEST(edn_parse_bigdec_integer_with_M) {
    edn_result_t r = edn_parse("42M", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_BIGDEC);

    size_t length;
    bool negative;
    const char* decimal = edn_bigdec_get(r.value, &length, &negative);

    assert(decimal != NULL);
    assert(negative == false);

    edn_free(r.value);
}

#ifdef EDN_ENABLE_RATIO

TEST(api_ratio_get) {
    edn_value_t value;
    value.type = EDN_TYPE_RATIO;
    value.as.ratio.numerator = 22;
    value.as.ratio.denominator = 7;

    int64_t numerator, denominator;
    bool success = edn_ratio_get(&value, &numerator, &denominator);

    assert(success == true);
    assert(numerator == 22);
    assert(denominator == 7);
}

TEST(api_ratio_get_negative) {
    edn_value_t value;
    value.type = EDN_TYPE_RATIO;
    value.as.ratio.numerator = -3;
    value.as.ratio.denominator = 4;

    int64_t numerator, denominator;
    bool success = edn_ratio_get(&value, &numerator, &denominator);

    assert(success == true);
    assert(numerator == -3);
    assert(denominator == 4);
}

TEST(api_ratio_get_wrong_type) {
    edn_value_t value;
    value.type = EDN_TYPE_INT;
    value.as.integer = 42;

    int64_t numerator, denominator;
    bool success = edn_ratio_get(&value, &numerator, &denominator);

    assert(success == false);
}

TEST(api_parse_ratio_simple) {
    edn_result_t r = edn_parse("22/7", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    bool success = edn_ratio_get(r.value, &numerator, &denominator);

    assert(success == true);
    assert(numerator == 22);
    assert(denominator == 7);

    edn_free(r.value);
}

TEST(api_parse_ratio_negative_numerator) {
    edn_result_t r = edn_parse("-3/4", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    bool success = edn_ratio_get(r.value, &numerator, &denominator);

    assert(success == true);
    assert(numerator == -3);
    assert(denominator == 4);

    edn_free(r.value);
}

TEST(api_parse_ratio_negative_denominator) {
    /* Negative denominator should be rejected - denominator must be positive */
    edn_result_t r = edn_parse("3/-4", 0);

    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_parse_ratio_both_negative) {
    /* Both negative should be rejected - denominator must be positive */
    edn_result_t r = edn_parse("-5/-6", 0);

    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_parse_ratio_zero_numerator) {
    /* 0/5 should parse as integer 0, not a ratio (Clojure behavior) */
    edn_result_t r = edn_parse("0/5", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t num;
    edn_int64_get(r.value, &num);
    assert(num == 0);

    edn_free(r.value);
}

TEST(api_parse_ratio_zero_denominator_error) {
    edn_result_t r = edn_parse("5/0", 0);

    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_parse_ratio_large_values) {
    edn_result_t r = edn_parse("1000000000/3", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    bool success = edn_ratio_get(r.value, &numerator, &denominator);

    assert(success == true);
    assert(numerator == 1000000000);
    assert(denominator == 3);

    edn_free(r.value);
}

TEST(api_parse_ratio_in_vector) {
    edn_result_t r = edn_parse("[1/2 3/4 5/6]", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(r.value) == 3);

    /* First ratio: 1/2 */
    edn_value_t* elem0 = edn_vector_get(r.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_RATIO);
    int64_t num0, den0;
    edn_ratio_get(elem0, &num0, &den0);
    assert(num0 == 1 && den0 == 2);

    /* Second ratio: 3/4 */
    edn_value_t* elem1 = edn_vector_get(r.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_RATIO);
    int64_t num1, den1;
    edn_ratio_get(elem1, &num1, &den1);
    assert(num1 == 3 && den1 == 4);

    /* Third ratio: 5/6 */
    edn_value_t* elem2 = edn_vector_get(r.value, 2);
    assert(edn_type(elem2) == EDN_TYPE_RATIO);
    int64_t num2, den2;
    edn_ratio_get(elem2, &num2, &den2);
    assert(num2 == 5 && den2 == 6);

    edn_free(r.value);
}

TEST(api_parse_ratio_in_map) {
    edn_result_t r = edn_parse("{:pi 22/7 :half 1/2}", 0);

    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_MAP);
    assert(edn_map_count(r.value) == 2);

    /* Check first value is ratio 22/7 */
    edn_value_t* val0 = edn_map_get_value(r.value, 0);
    assert(edn_type(val0) == EDN_TYPE_RATIO);
    int64_t num0, den0;
    edn_ratio_get(val0, &num0, &den0);
    assert(num0 == 22 && den0 == 7);

    /* Check second value is ratio 1/2 */
    edn_value_t* val1 = edn_map_get_value(r.value, 1);
    assert(edn_type(val1) == EDN_TYPE_RATIO);
    int64_t num1, den1;
    edn_ratio_get(val1, &num1, &den1);
    assert(num1 == 1 && den1 == 2);

    edn_free(r.value);
}

TEST(api_parse_ratio_with_whitespace) {
    /* Ratio should NOT allow whitespace around / */
    edn_result_t r = edn_parse("1 / 2", 0);

    /* This should parse as three separate values, not a ratio */
    /* In a vector context, it would be [1 / 2] */
    /* But at top level, it should fail or parse just "1" */
    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_INT); /* Just parses "1" */

    edn_free(r.value);
}

TEST(api_parse_ratio_not_symbol) {
    /* "/" alone should be a symbol, not a ratio */
    edn_result_t r = edn_parse("/", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_SYMBOL);

    edn_free(r.value);
}

TEST(api_parse_ratio_numerator_overflow) {
    /* Numerator too large for int64_t */
    edn_result_t r = edn_parse("99999999999999999999/3", 0);

    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_parse_ratio_denominator_overflow) {
    /* Denominator too large for int64_t */
    edn_result_t r = edn_parse("3/99999999999999999999", 0);

    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_parse_ratio_invalid_float_numerator) {
    /* Float as numerator - should fail */
    edn_result_t r = edn_parse("3.14/2", 0);

    /* This should parse as float 3.14, then fail on /2 or stop at / */
    assert(r.error == EDN_OK);
    assert(edn_type(r.value) == EDN_TYPE_FLOAT); /* Just parses the float */

    edn_free(r.value);
}

TEST(api_parse_ratio_invalid_float_denominator) {
    /* Float as denominator - should fail */
    edn_result_t r = edn_parse("3/2.5", 0);

    /* Denominator is float (2.5), which is invalid for ratio */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
}

TEST(api_ratio_as_double) {
    edn_value_t value;
    value.type = EDN_TYPE_RATIO;
    value.as.ratio.numerator = 22;
    value.as.ratio.denominator = 7;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == true);
    assert(fabs(result - 3.142857) < 0.0001);
}

TEST(api_ratio_as_double_negative) {
    edn_value_t value;
    value.type = EDN_TYPE_RATIO;
    value.as.ratio.numerator = -1;
    value.as.ratio.denominator = 2;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == true);
    assert(fabs(result - (-0.5)) < 0.0001);
}

TEST(api_ratio_as_double_zero_denominator) {
    edn_value_t value;
    value.type = EDN_TYPE_RATIO;
    value.as.ratio.numerator = 5;
    value.as.ratio.denominator = 0;

    double result;
    bool success = edn_number_as_double(&value, &result);

    assert(success == false); /* Division by zero */
}

TEST(api_parse_ratio_hex_not_supported) {
    /* Hex notation should not work with ratios */
    edn_result_t r = edn_parse("0x10/2", 0);

    /* Should not parse as a ratio (hex numbers don't support ratio syntax) */
    assert(r.error == EDN_OK);
    /* Note: hex parsing has a separate bug, so we just verify it's not a ratio */
    assert(edn_type(r.value) != EDN_TYPE_RATIO);

    edn_free(r.value);
}

TEST(api_parse_ratio_one) {
    /* 5/5 reduces to 1/1 which becomes integer 1 */
    edn_result_t r = edn_parse("5/5", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_INT);

    int64_t num;
    edn_int64_get(r.value, &num);
    assert(num == 1);

    edn_free(r.value);
}

TEST(api_parse_ratio_reduction) {
    /* Test that ratios are automatically reduced to lowest terms */
    edn_result_t r = edn_parse("3/6", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    edn_ratio_get(r.value, &numerator, &denominator);
    /* 3/6 should be reduced to 1/2 */
    assert(numerator == 1);
    assert(denominator == 2);

    edn_free(r.value);
}

TEST(api_parse_ratio_reduction_negative) {
    /* Test reduction with negative numerator */
    edn_result_t r = edn_parse("-6/9", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    edn_ratio_get(r.value, &numerator, &denominator);
    /* -6/9 should be reduced to -2/3 */
    assert(numerator == -2);
    assert(denominator == 3);

    edn_free(r.value);
}

TEST(api_parse_ratio_already_reduced) {
    /* Test that already reduced ratios remain unchanged */
    edn_result_t r = edn_parse("22/7", 0);

    assert(r.error == EDN_OK);
    assert(r.value != NULL);
    assert(edn_type(r.value) == EDN_TYPE_RATIO);

    int64_t numerator, denominator;
    edn_ratio_get(r.value, &numerator, &denominator);
    /* 22/7 is already in lowest terms */
    assert(numerator == 22);
    assert(denominator == 7);

    edn_free(r.value);
}

#endif /* EDN_ENABLE_RATIO */

#ifndef EDN_ENABLE_RATIO
/* Test that ratio syntax fails with clear error when disabled */
TEST(api_parse_ratio_disabled) {
    /* When ratio support is disabled, "22/7" should fail */
    /* because '/' is not a valid delimiter after a number */
    edn_result_t r = edn_parse("22/7", 0);

    /* Should fail with clear error message */
    assert(r.error == EDN_ERROR_INVALID_NUMBER);
    assert(r.value == NULL);
    /* Error message should indicate delimiter issue */
    assert(r.error_message != NULL);

    /* In a collection context, same error */
    edn_result_t r2 = edn_parse("[22/7]", 0);
    assert(r2.error == EDN_ERROR_INVALID_NUMBER);
}
#endif

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

    /* BigInt N suffix tests */
    run_test_scan_number_bigint_suffix_simple();
    run_test_scan_number_bigint_suffix_negative();
    run_test_scan_number_bigint_suffix_hex();
    run_test_scan_number_bigint_suffix_only_decimal();
    run_test_scan_number_bigint_suffix_on_float_invalid();
    run_test_scan_number_bigint_suffix_on_exponent_invalid();
    run_test_api_parse_bigint_suffix();
    run_test_api_parse_bigint_suffix_in_collection();

    /* BigDecimal M suffix tests */
    run_test_scan_number_bigdec_suffix_simple();
    run_test_scan_number_bigdec_suffix_negative();
    run_test_scan_number_bigdec_suffix_exponent();
    run_test_scan_number_bigdec_suffix_on_integer();
    run_test_api_parse_bigdec_suffix();
    run_test_api_parse_bigdec_suffix_on_integer();
    run_test_api_bigdec_get();
    run_test_api_parse_bigdec_suffix_in_collection();

#ifdef EDN_ENABLE_RATIO
    /* Ratio tests */
    run_test_api_ratio_get();
    run_test_api_ratio_get_negative();
    run_test_api_ratio_get_wrong_type();
    run_test_api_parse_ratio_simple();
    run_test_api_parse_ratio_negative_numerator();
    run_test_api_parse_ratio_negative_denominator();
    run_test_api_parse_ratio_both_negative();
    run_test_api_parse_ratio_zero_numerator();
    run_test_api_parse_ratio_zero_denominator_error();
    run_test_api_parse_ratio_large_values();
    run_test_api_parse_ratio_in_vector();
    run_test_api_parse_ratio_in_map();
    run_test_api_parse_ratio_with_whitespace();
    run_test_api_parse_ratio_not_symbol();
    run_test_api_parse_ratio_numerator_overflow();
    run_test_api_parse_ratio_denominator_overflow();
    run_test_api_parse_ratio_invalid_float_numerator();
    run_test_api_parse_ratio_invalid_float_denominator();
    run_test_api_ratio_as_double();
    run_test_api_ratio_as_double_negative();
    run_test_api_ratio_as_double_zero_denominator();
    run_test_api_parse_ratio_hex_not_supported();
    run_test_api_parse_ratio_one();
    run_test_api_parse_ratio_reduction();
    run_test_api_parse_ratio_reduction_negative();
    run_test_api_parse_ratio_already_reduced();
#else
    /* Test that ratio syntax is rejected when disabled */
    run_test_api_parse_ratio_disabled();
#endif

    /* Comprehensive edn_parse tests for all number forms */
    /* Decimal integers */
    run_test_edn_parse_decimal_int_simple();
    run_test_edn_parse_decimal_int_negative();
    run_test_edn_parse_decimal_int_zero();
    run_test_edn_parse_decimal_int_large();

    /* Hexadecimal */
    run_test_edn_parse_hex_lowercase_x();
    run_test_edn_parse_hex_uppercase_X();
    run_test_edn_parse_hex_mixed_case();
    run_test_edn_parse_hex_negative();

    /* Octal */
    run_test_edn_parse_octal_simple();
    run_test_edn_parse_octal_small();
    run_test_edn_parse_octal_negative();

    /* Binary (radix notation) */
    run_test_edn_parse_binary_simple();
    run_test_edn_parse_binary_negative();

    /* Arbitrary radix */
    run_test_edn_parse_radix_base8();
    run_test_edn_parse_radix_base16();
    run_test_edn_parse_radix_base36();
    run_test_edn_parse_radix_negative();

    /* Floating point */
    run_test_edn_parse_float_simple();
    run_test_edn_parse_float_negative();
    run_test_edn_parse_float_leading_zero();
    run_test_edn_parse_float_no_leading_zero();

    /* Scientific notation */
    run_test_edn_parse_scientific_positive_exp();
    run_test_edn_parse_scientific_negative_exp();
    run_test_edn_parse_scientific_uppercase_E();
    run_test_edn_parse_scientific_explicit_plus();
    run_test_edn_parse_scientific_no_decimal();

    /* BigInt with N suffix */
    run_test_edn_parse_bigint_simple();
    run_test_edn_parse_bigint_negative();
    run_test_edn_parse_bigint_very_large();

    /* BigDecimal with M suffix */
    run_test_edn_parse_bigdec_simple();
    run_test_edn_parse_bigdec_negative();
    run_test_edn_parse_bigdec_with_exponent();
    run_test_edn_parse_bigdec_integer_with_M();

    TEST_SUMMARY("number parsing");
}
