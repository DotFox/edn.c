/**
 * Test character parsing
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test named characters */
TEST(named_newline) {
    edn_result_t result = edn_read("\\newline", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0A);

    edn_free(result.value);
}

TEST(named_space) {
    edn_result_t result = edn_read("\\space", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x20);

    edn_free(result.value);
}

TEST(named_tab) {
    edn_result_t result = edn_read("\\tab", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x09);

    edn_free(result.value);
}

TEST(named_return) {
    edn_result_t result = edn_read("\\return", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0D);

    edn_free(result.value);
}

TEST(single_lowercase) {
    edn_result_t result = edn_read("\\a", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'a');

    edn_free(result.value);
}

TEST(single_uppercase) {
    edn_result_t result = edn_read("\\Z", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'Z');

    edn_free(result.value);
}

TEST(single_digit) {
    edn_result_t result = edn_read("\\5", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '5');

    edn_free(result.value);
}

TEST(special_backslash) {
    edn_result_t result = edn_read("\\\\", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '\\');

    edn_free(result.value);
}

TEST(special_quote) {
    edn_result_t result = edn_read("\\\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '"');

    edn_free(result.value);
}

TEST(special_parens) {
    edn_result_t result = edn_read("\\(", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '(');
    edn_free(result.value);

    result = edn_read("\\)", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ')');
    edn_free(result.value);
}

TEST(special_brackets) {
    edn_result_t result = edn_read("\\[", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '[');
    edn_free(result.value);

    result = edn_read("\\]", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ']');
    edn_free(result.value);
}

TEST(special_braces) {
    edn_result_t result = edn_read("\\{", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '{');
    edn_free(result.value);

    result = edn_read("\\}", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '}');
    edn_free(result.value);
}

TEST(special_semicolon) {
    edn_result_t result = edn_read("\\;", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ';');

    edn_free(result.value);
}

TEST(special_at_sign) {
    edn_result_t result = edn_read("\\@", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '@');

    edn_free(result.value);
}

TEST(special_hash) {
    edn_result_t result = edn_read("\\#", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '#');

    edn_free(result.value);
}

TEST(special_comma) {
    edn_result_t result = edn_read("\\,", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ',');

    edn_free(result.value);
}

TEST(unicode_basic_latin) {
    edn_result_t result = edn_read("\\u0041", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0041);

    edn_free(result.value);
}

TEST(unicode_greek) {
    edn_result_t result = edn_read("\\u03B1", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x03B1);

    edn_free(result.value);
}

TEST(unicode_cjk) {
    edn_result_t result = edn_read("\\u4E2D", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x4E2D);

    edn_free(result.value);
}

TEST(unicode_lowercase_hex) {
    edn_result_t result = edn_read("\\u00e9", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x00E9);

    edn_free(result.value);
}

TEST(invalid_no_backslash) {
    edn_result_t result = edn_read("a", 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(invalid_backslash_only) {
    edn_result_t result = edn_read("\\", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_character_u_with_text) {
    edn_result_t result = edn_read("\\unknown", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_character_u_in_vector) {
    edn_result_t result = edn_read("[\\unknown]", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unicode_long) {
    edn_result_t result = edn_read("\\uffffff", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unicode_short) {
    edn_result_t result = edn_read("\\u12", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unicode_bad_hex) {
    edn_result_t result = edn_read("\\uXYZW", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(single_u_no_hex) {
    edn_result_t result = edn_read("\\u", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'u');

    edn_free(result.value);
}

TEST(single_u_not_hex) {
    edn_result_t result = edn_read("[\\u x]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_CHARACTER);
    uint32_t codepoint;
    assert(edn_character_get(elem0, &codepoint));
    assert(codepoint == 'u');

    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_SYMBOL);
    const char* name;
    size_t len;
    assert(edn_symbol_get(elem1, NULL, NULL, &name, &len));
    assert(len == 1);
    assert(memcmp(name, "x", 1) == 0);

    edn_free(result.value);
}

TEST(invalid_whitespace_space) {
    edn_result_t result = edn_read("\\ ", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_whitespace_tab) {
    edn_result_t result = edn_read("\\\t", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(api_wrong_type) {
    edn_result_t result = edn_read("42", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    uint32_t codepoint;
    assert(!edn_character_get(result.value, &codepoint));

    edn_free(result.value);
}

TEST(api_null_value) {
    uint32_t codepoint;
    assert(!edn_character_get(NULL, &codepoint));
}

TEST(api_null_out) {
    edn_result_t result = edn_read("\\a", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    assert(!edn_character_get(result.value, NULL));

    edn_free(result.value);
}

#ifdef EDN_ENABLE_EXTENDED_CHARACTERS

TEST(extended_formfeed) {
    edn_result_t result = edn_read("\\formfeed", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0C);

    edn_free(result.value);
}

TEST(extended_backspace) {
    edn_result_t result = edn_read("\\backspace", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x08);

    edn_free(result.value);
}

TEST(octal_single_digit) {
    edn_result_t result = edn_read("\\o7", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 7);

    edn_free(result.value);
}

TEST(octal_two_digits) {
    edn_result_t result = edn_read("\\o12", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 10);

    edn_free(result.value);
}

TEST(octal_three_digits) {
    edn_result_t result = edn_read("\\o101", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 65); /* 'A' */

    edn_free(result.value);
}

TEST(octal_max_value) {
    edn_result_t result = edn_read("\\o377", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 255); /* 0377 octal = 255 decimal */

    edn_free(result.value);
}

TEST(octal_above_max) {
    edn_result_t result = edn_read("\\o400", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(octal_777_invalid) {
    edn_result_t result = edn_read("\\o777", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(octal_zero) {
    edn_result_t result = edn_read("\\o0", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0);

    edn_free(result.value);
}

TEST(single_o_no_digits) {
    edn_result_t result = edn_read("\\o", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'o');

    edn_free(result.value);
}

TEST(invalid_octal_digit_8) {
    edn_result_t result = edn_read("\\o8", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_octal_digit_9) {
    edn_result_t result = edn_read("\\o9", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_octal_78) {
    edn_result_t result = edn_read("\\o78", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(extended_in_vector) {
    edn_result_t result = edn_read("[\\formfeed \\backspace]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    uint32_t codepoint;
    edn_value_t* elem = edn_vector_get(result.value, 0);
    assert(edn_character_get(elem, &codepoint));
    assert(codepoint == 0x0C);

    elem = edn_vector_get(result.value, 1);
    assert(edn_character_get(elem, &codepoint));
    assert(codepoint == 0x08);

    edn_free(result.value);
}

TEST(octal_in_vector) {
    edn_result_t result = edn_read("[\\o101 \\o102 \\o103]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    uint32_t codepoint;
    edn_value_t* elem = edn_vector_get(result.value, 0);
    assert(edn_character_get(elem, &codepoint));
    assert(codepoint == 65); /* 'A' */

    elem = edn_vector_get(result.value, 1);
    assert(edn_character_get(elem, &codepoint));
    assert(codepoint == 66); /* 'B' */

    elem = edn_vector_get(result.value, 2);
    assert(edn_character_get(elem, &codepoint));
    assert(codepoint == 67); /* 'C' */

    edn_free(result.value);
}

#else /* !EDN_ENABLE_EXTENDED_CHARACTERS */

TEST(fallback_formfeed) {
    edn_result_t result = edn_read("[\\f ormfeed]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_CHARACTER);
    uint32_t codepoint;
    assert(edn_character_get(elem0, &codepoint));
    assert(codepoint == 'f');

    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_SYMBOL);
    const char* name;
    size_t len;
    assert(edn_symbol_get(elem1, NULL, NULL, &name, &len));
    assert(len == 7);
    assert(memcmp(name, "ormfeed", 7) == 0);

    edn_free(result.value);
}

TEST(fallback_backspace) {
    edn_result_t result = edn_read("[\\b ackspace]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_CHARACTER);
    uint32_t codepoint;
    assert(edn_character_get(elem0, &codepoint));
    assert(codepoint == 'b');

    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_SYMBOL);
    const char* name;
    size_t len;
    assert(edn_symbol_get(elem1, NULL, NULL, &name, &len));
    assert(len == 8);
    assert(memcmp(name, "ackspace", 8) == 0);

    edn_free(result.value);
}

TEST(fallback_octal) {
    edn_result_t result = edn_read("[\\o 123]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(edn_type(elem0) == EDN_TYPE_CHARACTER);
    uint32_t codepoint;
    assert(edn_character_get(elem0, &codepoint));
    assert(codepoint == 'o');

    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(edn_type(elem1) == EDN_TYPE_INT);
    int64_t num;
    assert(edn_int64_get(elem1, &num));
    assert(num == 123);

    edn_free(result.value);
}

TEST(fallback_single_f) {
    edn_result_t result = edn_read("\\f", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'f');

    edn_free(result.value);
}

TEST(fallback_single_b) {
    edn_result_t result = edn_read("\\b", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'b');

    edn_free(result.value);
}

TEST(fallback_single_o) {
    edn_result_t result = edn_read("\\o", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'o');

    edn_free(result.value);
}

#endif /* EDN_ENABLE_EXTENDED_CHARACTERS */

int main(void) {
    printf("Running character parsing tests...\n\n");

    RUN_TEST(named_newline);
    RUN_TEST(named_space);
    RUN_TEST(named_tab);
    RUN_TEST(named_return);
    RUN_TEST(single_lowercase);
    RUN_TEST(single_uppercase);
    RUN_TEST(single_digit);
    RUN_TEST(special_backslash);
    RUN_TEST(special_quote);
    RUN_TEST(special_parens);
    RUN_TEST(special_brackets);
    RUN_TEST(special_braces);
    RUN_TEST(special_semicolon);
    RUN_TEST(special_at_sign);
    RUN_TEST(special_hash);
    RUN_TEST(special_comma);
    RUN_TEST(unicode_basic_latin);
    RUN_TEST(unicode_greek);
    RUN_TEST(unicode_cjk);
    RUN_TEST(unicode_lowercase_hex);
    RUN_TEST(invalid_no_backslash);
    RUN_TEST(invalid_backslash_only);
    RUN_TEST(invalid_character_u_with_text);
    RUN_TEST(invalid_character_u_in_vector);
    RUN_TEST(invalid_unicode_long);
    RUN_TEST(invalid_unicode_short);
    RUN_TEST(invalid_unicode_bad_hex);
    RUN_TEST(single_u_no_hex);
    RUN_TEST(single_u_not_hex);
    RUN_TEST(invalid_whitespace_space);
    RUN_TEST(invalid_whitespace_tab);
    RUN_TEST(api_wrong_type);
    RUN_TEST(api_null_value);
    RUN_TEST(api_null_out);

#ifdef EDN_ENABLE_EXTENDED_CHARACTERS
    RUN_TEST(extended_formfeed);
    RUN_TEST(extended_backspace);
    RUN_TEST(octal_single_digit);
    RUN_TEST(octal_two_digits);
    RUN_TEST(octal_three_digits);
    RUN_TEST(octal_max_value);
    RUN_TEST(octal_above_max);
    RUN_TEST(octal_777_invalid);
    RUN_TEST(octal_zero);
    RUN_TEST(single_o_no_digits);
    RUN_TEST(invalid_octal_digit_8);
    RUN_TEST(invalid_octal_digit_9);
    RUN_TEST(invalid_octal_78);
    RUN_TEST(extended_in_vector);
    RUN_TEST(octal_in_vector);
#else
    RUN_TEST(fallback_formfeed);
    RUN_TEST(fallback_backspace);
    RUN_TEST(fallback_octal);
    RUN_TEST(fallback_single_f);
    RUN_TEST(fallback_single_b);
    RUN_TEST(fallback_single_o);
#endif

    TEST_SUMMARY("character parsing");
}
