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

TEST(skip_mixed_whitespace) {
    const char* input = " \t\n\r, abc";
    const char* result = edn_simd_skip_whitespace(input, input + strlen(input));
    assert(result == input + 6);
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

int main(void) {
    printf("Running whitespace skipping tests...\n");

    run_test_skip_spaces();
    run_test_skip_tabs();
    run_test_skip_newlines();
    run_test_skip_commas();
    run_test_skip_mixed_whitespace();
    run_test_skip_line_comment();
    run_test_skip_line_comment_eof();
    run_test_skip_multiple_comments();
    run_test_skip_comment_with_whitespace();
    run_test_skip_empty_comment();
    run_test_no_whitespace();
    run_test_all_whitespace();
    run_test_large_whitespace_block();
    run_test_long_comment();
    run_test_very_long_comment();

    TEST_SUMMARY("whitespace skipping");
}
