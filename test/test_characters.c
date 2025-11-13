/**
 * Test character parsing
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test named characters */
TEST(named_newline) {
    edn_result_t result = edn_parse("\\newline", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0A);

    edn_free(result.value);
}

TEST(named_space) {
    edn_result_t result = edn_parse("\\space", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x20);

    edn_free(result.value);
}

TEST(named_tab) {
    edn_result_t result = edn_parse("\\tab", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x09);

    edn_free(result.value);
}

TEST(named_return) {
    edn_result_t result = edn_parse("\\return", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0D);

    edn_free(result.value);
}

TEST(single_lowercase) {
    edn_result_t result = edn_parse("\\a", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'a');

    edn_free(result.value);
}

TEST(single_uppercase) {
    edn_result_t result = edn_parse("\\Z", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'Z');

    edn_free(result.value);
}

TEST(single_digit) {
    edn_result_t result = edn_parse("\\5", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '5');

    edn_free(result.value);
}

TEST(special_backslash) {
    edn_result_t result = edn_parse("\\\\", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '\\');

    edn_free(result.value);
}

TEST(special_quote) {
    edn_result_t result = edn_parse("\\\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '"');

    edn_free(result.value);
}

TEST(special_parens) {
    edn_result_t result = edn_parse("\\(", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '(');
    edn_free(result.value);

    result = edn_parse("\\)", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ')');
    edn_free(result.value);
}

TEST(special_brackets) {
    edn_result_t result = edn_parse("\\[", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '[');
    edn_free(result.value);

    result = edn_parse("\\]", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ']');
    edn_free(result.value);
}

TEST(special_braces) {
    edn_result_t result = edn_parse("\\{", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '{');
    edn_free(result.value);

    result = edn_parse("\\}", 0);
    assert(result.error == EDN_OK);
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '}');
    edn_free(result.value);
}

TEST(special_semicolon) {
    edn_result_t result = edn_parse("\\;", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ';');

    edn_free(result.value);
}

TEST(special_at_sign) {
    edn_result_t result = edn_parse("\\@", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '@');

    edn_free(result.value);
}

TEST(special_hash) {
    edn_result_t result = edn_parse("\\#", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '#');

    edn_free(result.value);
}

TEST(special_comma) {
    edn_result_t result = edn_parse("\\,", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == ',');

    edn_free(result.value);
}

TEST(unicode_basic_latin) {
    edn_result_t result = edn_parse("\\u0041", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x0041);

    edn_free(result.value);
}

TEST(unicode_greek) {
    edn_result_t result = edn_parse("\\u03B1", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x03B1);

    edn_free(result.value);
}

TEST(unicode_cjk) {
    edn_result_t result = edn_parse("\\u4E2D", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x4E2D);

    edn_free(result.value);
}

TEST(unicode_lowercase_hex) {
    edn_result_t result = edn_parse("\\u00e9", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 0x00E9);

    edn_free(result.value);
}

TEST(invalid_no_backslash) {
    edn_result_t result = edn_parse("a", 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(invalid_backslash_only) {
    edn_result_t result = edn_parse("\\", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unknown_name) {
    edn_result_t result = edn_parse("\\unknown", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unicode_short) {
    edn_result_t result = edn_parse("\\u12", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_unicode_bad_hex) {
    edn_result_t result = edn_parse("\\uXYZW", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_whitespace_space) {
    edn_result_t result = edn_parse("\\ ", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_whitespace_tab) {
    edn_result_t result = edn_parse("\\\t", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(api_wrong_type) {
    edn_result_t result = edn_parse("42", 0);
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
    edn_result_t result = edn_parse("\\a", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    assert(!edn_character_get(result.value, NULL));

    edn_free(result.value);
}

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
    RUN_TEST(invalid_unknown_name);
    RUN_TEST(invalid_unicode_short);
    RUN_TEST(invalid_unicode_bad_hex);
    RUN_TEST(invalid_whitespace_space);
    RUN_TEST(invalid_whitespace_tab);
    RUN_TEST(api_wrong_type);
    RUN_TEST(api_null_value);
    RUN_TEST(api_null_out);

    TEST_SUMMARY("character parsing");
}
