/**
 * Test suite for EDN error reporting with accurate line/column positions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* ========================================================================
 * TEST: Error positions on first line
 * ======================================================================== */

TEST(error_position_first_line) {
    /* Unmatched closing bracket */
    const char* input = "]";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    assert_uint_eq(result.error_start.line, 1);
}

/* ========================================================================
 * TEST: Error positions on second line (map with odd forms)
 * error_start points to the opening '{' on line 2
 * ======================================================================== */

TEST(error_position_second_line) {
    const char* input = "\n{\n:a}"; /* Map with odd number of forms on line 2-3 */

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    /* error_start is at the opening '{' on line 2 */
    assert_uint_eq(result.error_start.line, 2);
    /* error_end is at the closing '}' on line 3 */
    assert_uint_eq(result.error_end.line, 3);
}

/* ========================================================================
 * TEST: Error positions with multiple lines
 * error_start points to the opening '{' on line 1
 * ======================================================================== */

TEST(error_position_multiple_lines) {
    const char* input = "{:key1 \"value1\"\n"
                        " :key2 123\n"
                        " :key3}"; /* Missing value */

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    /* error_start is at the opening '{' on line 1 */
    assert_uint_eq(result.error_start.line, 1);
    /* error_end is at the closing '}' on line 3 */
    assert_uint_eq(result.error_end.line, 3);
}

/* ========================================================================
 * TEST: Unterminated string error position
 * ======================================================================== */

TEST(error_position_unterminated_string) {
    const char* input = "[42\n"
                        "\"unterminated]";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    assert_uint_eq(result.error_start.line, 2);
}

/* ========================================================================
 * TEST: Unmatched delimiter error position
 * error_start points to the opening '[' on line 1
 * ======================================================================== */

TEST(error_position_unmatched_delimiter) {
    const char* input = "[1 2 3\n"
                        " 4 5 6\n"
                        " 7 8 9}"; /* Wrong closing bracket */

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    /* error_start is at the opening '[' on line 1 */
    assert_uint_eq(result.error_start.line, 1);
    /* error_end is at the wrong '}' on line 3 */
    assert_uint_eq(result.error_end.line, 3);
}

/* ========================================================================
 * TEST: Error in nested structure
 * error_start points to the innermost problematic opening delimiter
 * ======================================================================== */

TEST(error_position_nested) {
    const char* input = "{:outer\n"
                        " {:inner\n"
                        "  {:deep]}}"; /* Wrong closing bracket */

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    /* error_start is at the innermost '{' on line 3 where the mismatch occurs */
    assert_uint_eq(result.error_start.line, 3);
}

/* ========================================================================
 * TEST: Success case - verify no error position
 * ======================================================================== */

TEST(success_no_error_position) {
    const char* input = "{:key1 \"value1\"\n"
                        " :key2 123\n"
                        " :key3 true}";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value != NULL);
    assert_uint_eq(result.error, EDN_OK);
    assert(result.error_message == NULL);

    edn_free(result.value);
}

/* ========================================================================
 * TEST: CRLF line endings (Windows)
 * ======================================================================== */

TEST(error_position_crlf_line_endings) {
    const char* input = "[42\r\n\"}";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    assert_uint_eq(result.error_start.line, 2);
}

/* ========================================================================
 * TEST: Large document with error
 * ======================================================================== */

TEST(error_position_large_document) {
    /* Create a document with many lines of comments to reach line 100 */
    const char* line = "; comment\n";
    const size_t line_len = strlen(line);
    const size_t num_lines = 99;
    const size_t total_len = line_len * num_lines + 20;

    char* input = (char*) malloc(total_len);
    assert(input != NULL);

    /* Fill with comment lines */
    for (size_t i = 0; i < num_lines; i++) {
        memcpy(input + (i * line_len), line, line_len);
    }

    /* Add error starting on line 100 - map with odd forms */
    strcpy(input + (num_lines * line_len), "{\n:a}");

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    /* error_start is at the opening '{' on line 100 */
    assert_uint_eq(result.error_start.line, 100);
    /* error_end is at the closing '}' on line 101 */
    assert_uint_eq(result.error_end.line, 101);

    free(input);
}

/* ========================================================================
 * TEST: Column position accuracy
 * error_start points to the opening delimiter
 * ======================================================================== */

TEST(error_column_position_accuracy) {
    /* Map with odd forms */
    const char* input = "{:a 1 :b}";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    assert_uint_eq(result.error_start.line, 1);
    /* error_start is at opening '{' (column 1) */
    assert_uint_eq(result.error_start.column, 1);
    /* error_end is at closing '}' (column 9) */
    assert_uint_eq(result.error_end.column, 9);
}

/* ========================================================================
 * TEST: Error after comments
 * ======================================================================== */

TEST(error_position_after_comment) {
    const char* input = "; This is a comment\n"
                        "[42\n"
                        "; Another comment\n"
                        "\"}";

    edn_result_t result = edn_read(input, strlen(input));

    assert(result.value == NULL);
    assert(result.error != EDN_OK);
    assert_uint_eq(result.error_start.line, 4);
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

TEST(error_positions_match_newline_index) {
    const char* forms[] = {"",        "]",   "[1\n 2}", "{\n:a}", "#tag", "#_",
                           "\"abc\n", ")\n", "\n\n",    "\r",     "\r\n"};
    const size_t prefix_lengths[] = {0, 1, 15, 16, 31, 32, 63, 64, 127, 128, 4096};
    const char* patterns[] = {" ", "\n", "\r\n", " \t\n\n\r"};
    char input[8192];

    for (size_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
        for (size_t n = 0; n < sizeof(prefix_lengths) / sizeof(prefix_lengths[0]); n++) {
            size_t prefix_length = prefix_lengths[n];
            for (size_t i = 0; i < prefix_length; i++) {
                input[i] = patterns[p][i % strlen(patterns[p])];
            }
            for (size_t f = 0; f < sizeof(forms) / sizeof(forms[0]); f++) {
                size_t length = prefix_length + strlen(forms[f]);
                memcpy(input + prefix_length, forms[f], strlen(forms[f]));
                input[length] = '\0';
                edn_result_t result = edn_read(input, length);
                assert(result.error != EDN_OK);
                assert(result.value == NULL);
                assert(result.error_start.offset <= length);
                assert(result.error_end.offset <= length);

                edn_arena_t* arena = edn_arena_create();
                assert(arena != NULL);
                newline_positions_t* positions =
                    newline_find_all_ex(input, length, NEWLINE_MODE_LF, arena);
                document_position_t start = {0}, end = {0};
                bool start_ok = newline_get_position(positions, result.error_start.offset, &start);
                bool end_ok = newline_get_position(positions, result.error_end.offset, &end);
                edn_arena_destroy(arena);
                assert(start_ok && end_ok);
                assert_uint_eq(result.error_start.line, start.line);
                assert_uint_eq(result.error_start.column, start.column);
                assert_uint_eq(result.error_end.line, end.line);
                assert_uint_eq(result.error_end.column, end.column);
            }
        }
    }
}

TEST(error_positions_with_truncated_character) {
    char* input = malloc(4);
    assert(input != NULL);
    memcpy(input, "\\u12", 4);
    edn_result_t result = edn_read(input, 4);
    free(input);

    assert_int_eq(result.error, EDN_ERROR_INVALID_CHARACTER);
    assert(result.value == NULL);
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_start.column, 1);
    assert_uint_eq(result.error_end.offset, 6);
    assert_uint_eq(result.error_end.line, 1);
    assert_uint_eq(result.error_end.column, 7);
}

TEST(error_positions_with_large_unread_tail) {
    size_t length = 1024 * 1024;
    char* input = malloc(length);
    assert(input != NULL);
    memset(input, '\n', length);
    memcpy(input, "[1\n 2}", 6);
    edn_result_t result = edn_read(input, length);
    free(input);

    assert_int_eq(result.error, EDN_ERROR_UNMATCHED_DELIMITER);
    assert(result.value == NULL);
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_start.column, 1);
    assert_uint_eq(result.error_end.offset, 6);
    assert_uint_eq(result.error_end.line, 2);
    assert_uint_eq(result.error_end.column, 4);
}

int main(void) {
    printf("Running EDN error position tests...\n\n");

    RUN_TEST(error_position_first_line);
    RUN_TEST(error_position_second_line);
    RUN_TEST(error_position_multiple_lines);
    RUN_TEST(error_position_unterminated_string);
    RUN_TEST(error_position_unmatched_delimiter);
    RUN_TEST(error_position_nested);
    RUN_TEST(success_no_error_position);
    RUN_TEST(error_position_crlf_line_endings);
    RUN_TEST(error_position_large_document);
    RUN_TEST(error_column_position_accuracy);
    RUN_TEST(error_position_after_comment);
    RUN_TEST(error_positions_match_newline_index);
    RUN_TEST(error_positions_with_truncated_character);
    RUN_TEST(error_positions_with_large_unread_tail);

    TEST_SUMMARY("error_positions");
}
