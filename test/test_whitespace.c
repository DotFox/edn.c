#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test whitespace skipping including line comments */

TEST(skip_spaces) {
    const char* input = "    abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 4);
    assert(*result == 'a');
}

TEST(skip_tabs) {
    const char* input = "\t\t\tabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 3);
    assert(*result == 'a');
}

TEST(skip_newlines) {
    const char* input = "\n\n\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 3);
    assert(*result == 'a');
}

TEST(skip_commas) {
    const char* input = ",,,abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 3);
    assert(*result == 'a');
}

TEST(skip_formfeeds) {
    const char* input = "\f\f\fabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 3);
    assert(*result == 'a');
}

TEST(skip_mixed_whitespace) {
    const char* input = " \t\n\r\f, abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 7);
    assert(*result == 'a');
}

TEST(skip_line_comment) {
    const char* input = "; this is a comment\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 20); /* After the newline */
    assert(*result == 'a');
}

TEST(skip_line_comment_eof) {
    const char* input = "; comment without newline";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + strlen(input)); /* At EOF */
}

TEST(skip_multiple_comments) {
    const char* input = "; comment 1\n; comment 2\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 24); /* After both comments */
    assert(*result == 'a');
}

TEST(skip_comment_with_whitespace) {
    const char* input = "  ; comment\n  abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 14); /* After whitespace following comment (at 'a') */
    assert(*result == 'a');
}

TEST(skip_empty_comment) {
    const char* input = ";\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 2); /* After semicolon and newline */
    assert(*result == 'a');
}

TEST(no_whitespace) {
    const char* input = "abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input); /* No movement */
    assert(*result == 'a');
}

TEST(all_whitespace) {
    const char* input = "   \t\n  ";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + strlen(input)); /* At EOF */
}

TEST(large_whitespace_block) {
    /* Test SIMD path with 16+ bytes of whitespace */
    const char* input = "                    abc"; /* 20 spaces */
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 20);
    assert(*result == 'a');
}

TEST(large_formfeed_block) {
    /* Test SIMD path with 16+ formfeeds to test SIMD processing */
    const char* input = "\f\f\f\f\f\f\f\f\f\f\f\f\f\f\f\f\f\f\f\fabc"; /* 20 formfeeds */
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 20);
    assert(*result == 'a');
}

TEST(long_comment) {
    /* Test SIMD path for finding newline in long comment (50+ chars) */
    const char* input = "; This is a very long comment with more than 16 characters to test SIMD "
                        "newline detection\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    /* Should skip past the entire comment and newline */
    assert(*result == 'a');
}

TEST(very_long_comment) {
    /* Test SIMD path with 100+ character comment */
    const char* input = "; "
                        "01234567890123456789012345678901234567890123456789012345678901234567890123"
                        "45678901234567890123456789\nabc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(*result == 'a');
}

TEST(formfeed_in_vector) {
    /* Test formfeed as delimiter in parsed EDN */
    const char* input = "[1\f2\f3]";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);
    edn_free(result.value);
}

TEST(formfeed_as_delimiter) {
    /* Test formfeed stops identifier scanning */
    const char* input = "[:a\f:b]";
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    edn_value_t* first = edn_vector_get(result.value, 0);
    edn_value_t* second = edn_vector_get(result.value, 1);
    assert(edn_type(first) == EDN_TYPE_KEYWORD);
    assert(edn_type(second) == EDN_TYPE_KEYWORD);

    edn_free(result.value);
}

TEST(vertical_tab_as_whitespace) {
    /* Vertical tab (0x0B) should act as whitespace */
    const char input[] = {'[', '1', 0x0B, '2', ']', 0};
    edn_result_t result = edn_read(input, strlen(input));
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);
    edn_free(result.value);
}

TEST(separator_chars_as_whitespace) {
    /* File/Group/Record/Unit separators (0x1C-0x1F) should act as whitespace */
    const char input_fs[] = {'[', '1', 0x1C, '2', ']', 0}; /* File separator */
    const char input_gs[] = {'[', '3', 0x1D, '4', ']', 0}; /* Group separator */
    const char input_rs[] = {'[', '5', 0x1E, '6', ']', 0}; /* Record separator */
    const char input_us[] = {'[', '7', 0x1F, '8', ']', 0}; /* Unit separator */

    edn_result_t result;

    result = edn_read(input_fs, strlen(input_fs));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 2);
    edn_free(result.value);

    result = edn_read(input_gs, strlen(input_gs));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 2);
    edn_free(result.value);

    result = edn_read(input_rs, strlen(input_rs));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 2);
    edn_free(result.value);

    result = edn_read(input_us, strlen(input_us));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 2);
    edn_free(result.value);
}

TEST(control_chars_in_identifiers) {
    /* Control chars 0x01-0x08, 0x0E-0x1B should be valid in identifiers */
    const char input1[] = {'[', 'f', 'o', 'o', 0x01, 'b', 'a', 'r', ']', 0}; /* SOH */
    const char input2[] = {'[', 'f', 'o', 'o', 0x08, 'b', 'a', 'r', ']', 0}; /* Backspace */
    const char input3[] = {'[', 'f', 'o', 'o', 0x0E, 'b', 'a', 'r', ']', 0}; /* Shift Out */
    const char input4[] = {'[', 'f', 'o', 'o', 0x1B, 'b', 'a', 'r', ']', 0}; /* ESC */

    edn_result_t result;

    result = edn_read(input1, strlen(input1));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 1);
    edn_free(result.value);

    result = edn_read(input2, strlen(input2));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 1);
    edn_free(result.value);

    result = edn_read(input3, strlen(input3));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 1);
    edn_free(result.value);

    result = edn_read(input4, strlen(input4));
    assert(result.error == EDN_OK);
    assert(edn_vector_count(result.value) == 1);
    edn_free(result.value);
}

int main(void) {
    printf("Running whitespace skipping tests...\n");

    run_test_skip_spaces();
    run_test_skip_tabs();
    run_test_skip_newlines();
    run_test_skip_commas();
    run_test_skip_formfeeds();
    run_test_skip_mixed_whitespace();
    run_test_skip_line_comment();
    run_test_skip_line_comment_eof();
    run_test_skip_multiple_comments();
    run_test_skip_comment_with_whitespace();
    run_test_skip_empty_comment();
    run_test_no_whitespace();
    run_test_all_whitespace();
    run_test_large_whitespace_block();
    run_test_large_formfeed_block();
    run_test_long_comment();
    run_test_very_long_comment();
    run_test_formfeed_in_vector();
    run_test_formfeed_as_delimiter();
    run_test_vertical_tab_as_whitespace();
    run_test_separator_chars_as_whitespace();
    run_test_control_chars_in_identifiers();

    TEST_SUMMARY("whitespace skipping");
}
