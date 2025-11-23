/**
 * Test suite for newline_finder module
 *
 * Tests SIMD-optimized newline detection across various scenarios.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/edn_internal.h"
#include "test_framework.h"

/* ========================================================================
 * TEST: Basic newline finding
 * ======================================================================== */

TEST(newline_find_all_basic) {
    const char* text = "Hello\nWorld\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 2);
    assert_uint_eq(positions->offsets[0], 5);
    assert_uint_eq(positions->offsets[1], 11);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Empty string
 * ======================================================================== */

TEST(newline_find_all_empty) {
    const char* text = "";
    size_t len = 0;

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 0);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: No newlines
 * ======================================================================== */

TEST(newline_find_all_no_newlines) {
    const char* text = "Hello World";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 0);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Only newlines
 * ======================================================================== */

TEST(newline_find_all_only_newlines) {
    const char* text = "\n\n\n\n\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 5);

    for (size_t i = 0; i < positions->count; i++) {
        assert_uint_eq(positions->offsets[i], i);
    }

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: SIMD boundary (16-byte chunks)
 * ======================================================================== */

TEST(newline_find_all_simd_boundary) {
    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    /* Exactly 16 bytes with newline at position 15 */
    {
        const char* text = "0123456789ABCDE\n";
        size_t len = 16;

        newline_positions_t* positions = newline_find_all(text, len, arena);

        assert(positions != NULL);
        assert_uint_eq(positions->count, 1);
        assert_uint_eq(positions->offsets[0], 15);
    }

    /* 17 bytes (crosses SIMD boundary) */
    {
        const char* text = "0123456789ABCDEF\n";
        size_t len = 17;

        newline_positions_t* positions = newline_find_all(text, len, arena);

        assert(positions != NULL);
        assert_uint_eq(positions->count, 1);
        assert_uint_eq(positions->offsets[0], 16);
    }

    /* 32 bytes (two SIMD chunks) with newlines in both */
    {
        const char* text = "0123456789ABCD\nF0123456789ABCD\n";
        size_t len = 32;

        newline_positions_t* positions = newline_find_all(text, len, arena);

        assert(positions != NULL);
        assert_uint_eq(positions->count, 2);
        assert_uint_eq(positions->offsets[0], 14);
        assert_uint_eq(positions->offsets[1], 30);
    }

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Large text with many newlines
 * ======================================================================== */

TEST(newline_find_all_large) {
    /* Create a large text with 1000 lines */
    const size_t num_lines = 1000;
    const char* line = "This is a line of text.\n";
    const size_t line_len = strlen(line);
    const size_t total_len = line_len * num_lines;

    char* text = (char*) malloc(total_len + 1);
    assert(text != NULL);

    for (size_t i = 0; i < num_lines; i++) {
        memcpy(text + (i * line_len), line, line_len);
    }
    text[total_len] = '\0';

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, total_len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, num_lines);

    /* Verify positions */
    for (size_t i = 0; i < positions->count; i++) {
        size_t expected_pos = (i + 1) * line_len - 1;
        assert_uint_eq(positions->offsets[i], expected_pos);
    }

    edn_arena_destroy(arena);
    free(text);
}

/* ========================================================================
 * TEST: UTF-8 strings with newlines
 * ======================================================================== */

TEST(newline_find_all_utf8) {
    /* UTF-8 text with newlines */
    const char* text = "Hello 世界\n你好\nСлава\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 3);

    /* Verify newlines are at correct byte positions */
    assert(text[positions->offsets[0]] == '\n');
    assert(text[positions->offsets[1]] == '\n');
    assert(text[positions->offsets[2]] == '\n');

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Dynamic growth
 * ======================================================================== */

TEST(newline_find_all_dynamic_growth) {
    /* Create text with more newlines than initial capacity */
    const size_t num_newlines = 200; /* More than INITIAL_CAPACITY (64) */
    char* text = (char*) malloc(num_newlines + 1);
    assert(text != NULL);

    memset(text, '\n', num_newlines);
    text[num_newlines] = '\0';

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, num_newlines, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, num_newlines);
    assert(positions->capacity >= num_newlines);

    edn_arena_destroy(arena);
    free(text);
}

TEST(newline_find_all_null_data) {
    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(NULL, 100, arena);

    assert_ptr_eq(positions, NULL);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Get position from offset
 * ======================================================================== */

TEST(newline_get_position_first_line) {
    const char* text = "Hello World\nSecond line\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Position 0: 'H' at line 1, column 1 */
    assert_true(newline_get_position(positions, 0, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 1);
    assert_uint_eq(pos.byte_offset, 0);

    /* Position 6: 'W' at line 1, column 7 */
    assert_true(newline_get_position(positions, 6, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 7);

    /* Position 11: '\n' at line 1, column 12 */
    assert_true(newline_get_position(positions, 11, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 12);

    edn_arena_destroy(arena);
}

TEST(newline_get_position_second_line) {
    const char* text = "First\nSecond\nThird\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Position 6: 'S' at line 2, column 1 */
    assert_true(newline_get_position(positions, 6, &pos));
    assert_uint_eq(pos.line, 2);
    assert_uint_eq(pos.column, 1);

    /* Position 8: 'c' at line 2, column 3 */
    assert_true(newline_get_position(positions, 8, &pos));
    assert_uint_eq(pos.line, 2);
    assert_uint_eq(pos.column, 3);

    edn_arena_destroy(arena);
}

TEST(newline_get_position_third_line) {
    const char* text = "First\nSecond\nThird\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Position 13: 'T' at line 3, column 1 */
    assert_true(newline_get_position(positions, 13, &pos));
    assert_uint_eq(pos.line, 3);
    assert_uint_eq(pos.column, 1);

    /* Position 17: 'd' at line 3, column 5 */
    assert_true(newline_get_position(positions, 17, &pos));
    assert_uint_eq(pos.line, 3);
    assert_uint_eq(pos.column, 5);

    edn_arena_destroy(arena);
}

TEST(newline_get_position_no_newlines) {
    const char* text = "Single line without newline";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Everything should be on line 1 */
    assert_true(newline_get_position(positions, 0, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 1);

    assert_true(newline_get_position(positions, 7, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 8);

    edn_arena_destroy(arena);
}

TEST(newline_get_position_empty) {
    const char* text = "";

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, 0, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Position 0 in empty document */
    assert_true(newline_get_position(positions, 0, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 1);

    edn_arena_destroy(arena);
}

TEST(newline_get_position_null_checks) {
    const char* text = "Hello\nWorld\n";

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, strlen(text), arena);
    document_position_t pos;

    /* NULL positions */
    assert_false(newline_get_position(NULL, 5, &pos));

    /* NULL out_position */
    assert_false(newline_get_position(positions, 5, NULL));

    edn_arena_destroy(arena);
}

TEST(newline_get_position_utf8) {
    /* UTF-8 text: "Hello 世界\n你好\n" 
     * '世' = 3 bytes, '界' = 3 bytes
     * '你' = 3 bytes, '好' = 3 bytes
     */
    const char* text = "Hello 世界\n你好\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all(text, len, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Position 0: 'H' at line 1, column 1 */
    assert_true(newline_get_position(positions, 0, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 1);

    /* First newline is after "Hello 世界" */
    /* "Hello " = 6 bytes, "世" = 3 bytes, "界" = 3 bytes, total = 12 bytes before \n */
    size_t first_newline = 12;

    /* Position after first newline should be line 2 */
    assert_true(newline_get_position(positions, first_newline + 1, &pos));
    assert_uint_eq(pos.line, 2);
    assert_uint_eq(pos.column, 1);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * TEST: Extended line terminator modes
 * ======================================================================== */

TEST(newline_find_all_ex_lf_mode) {
    const char* text = "Line 1\nLine 2\nLine 3\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_LF, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 3);
    assert_uint_eq(positions->offsets[0], 6);  /* After "Line 1" */
    assert_uint_eq(positions->offsets[1], 13); /* After "Line 2" */
    assert_uint_eq(positions->offsets[2], 20); /* After "Line 3" */

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_crlf_aware) {
    const char* text = "Windows\r\nLine\r\nEnding\r\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_CRLF_AWARE, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 3);

    /* Should record position of \n in each \r\n sequence */
    assert_uint_eq(positions->offsets[0], 8);  /* After "Windows\r" */
    assert_uint_eq(positions->offsets[1], 14); /* After "Line\r" */
    assert_uint_eq(positions->offsets[2], 22); /* After "Ending\r" */

    /* Verify these are the \n positions */
    assert(text[positions->offsets[0]] == '\n');
    assert(text[positions->offsets[1]] == '\n');
    assert(text[positions->offsets[2]] == '\n');

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_crlf_mixed) {
    /* Mixed line endings: CRLF and LF */
    const char* text = "Windows\r\nUnix\nMixed\r\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_CRLF_AWARE, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 3);

    /* All should be at \n positions */
    assert_uint_eq(positions->offsets[0], 8);  /* Windows\r\n */
    assert_uint_eq(positions->offsets[1], 13); /* Unix\n */
    assert_uint_eq(positions->offsets[2], 20); /* Mixed\r\n */

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_any_ascii) {
    /* CR and LF counted separately */
    const char* text = "Line\rOld\nMac\r\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_ANY_ASCII, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 4); /* \r, \n, \r, \n - all counted */

    assert_uint_eq(positions->offsets[0], 4);  /* \r after "Line" */
    assert_uint_eq(positions->offsets[1], 8);  /* \n after "Old" */
    assert_uint_eq(positions->offsets[2], 12); /* \r in "Mac\r\n" */
    assert_uint_eq(positions->offsets[3], 13); /* \n in "Mac\r\n" */

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_unicode) {
    /* Unicode line terminators */
    /* NEL = 0xC2 0x85, LS = 0xE2 0x80 0xA8, PS = 0xE2 0x80 0xA9 */
    const char* text = "Line1\nLine2\xC2\x85Line3\xE2\x80\xA8Line4\xE2\x80\xA9Line5\r\nEnd";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_UNICODE, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 5); /* \n, NEL, LS, PS, \r\n */

    /* Verify positions - let's calculate correctly:
     * "Line1\n"              = 6 bytes, \n at 5
     * "Line2"                = 5 bytes, ends at 10
     * NEL (0xC2 0x85)        = 2 bytes, starts at 11
     * "Line3"                = 5 bytes, ends at 17  
     * LS (0xE2 0x80 0xA8)    = 3 bytes, starts at 18
     * "Line4"                = 5 bytes, ends at 25
     * PS (0xE2 0x80 0xA9)    = 3 bytes, starts at 26
     * "Line5"                = 5 bytes, ends at 33
     * "\r\n"                 = 2 bytes, \n at 35
     */
    assert_uint_eq(positions->offsets[0], 5);  /* \n */
    assert_uint_eq(positions->offsets[1], 11); /* NEL (0xC2 0x85) */
    assert_uint_eq(positions->offsets[2], 18); /* LS (0xE2 0x80 0xA8) */
    assert_uint_eq(positions->offsets[3], 26); /* PS (0xE2 0x80 0xA9) */
    assert_uint_eq(positions->offsets[4], 35); /* \n in \r\n */

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_unicode_standalone_cr) {
    /* CR without following LF should be recognized in Unicode mode */
    const char* text = "Line1\rLine2\rLine3";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_UNICODE, arena);

    assert(positions != NULL);
    assert_uint_eq(positions->count, 2); /* Two standalone \r */

    assert_uint_eq(positions->offsets[0], 5);  /* \r */
    assert_uint_eq(positions->offsets[1], 11); /* \r */

    edn_arena_destroy(arena);
}

TEST(newline_find_all_ex_position_conversion_crlf) {
    /* Test position conversion with CRLF line endings */
    const char* text = "Line 1\r\nLine 2\r\nLine 3\r\n";
    size_t len = strlen(text);

    edn_arena_t* arena = edn_arena_create();
    assert(arena != NULL);

    newline_positions_t* positions = newline_find_all_ex(text, len, NEWLINE_MODE_CRLF_AWARE, arena);
    assert(positions != NULL);

    document_position_t pos;

    /* Offset 0: 'L' at line 1, column 1 */
    assert_true(newline_get_position(positions, 0, &pos));
    assert_uint_eq(pos.line, 1);
    assert_uint_eq(pos.column, 1);

    /* Line 1 = "Line 1\r\n" = 8 bytes, \n at position 7
     * Line 2 starts at position 8 */

    /* Offset 8: 'L' in "Line 2" at line 2, column 1 */
    assert_true(newline_get_position(positions, 8, &pos));
    assert_uint_eq(pos.line, 2);
    assert_uint_eq(pos.column, 1);

    /* Offset 16: 'L' in "Line 3" at line 3, column 1 */
    assert_true(newline_get_position(positions, 16, &pos));
    assert_uint_eq(pos.line, 3);
    assert_uint_eq(pos.column, 1);

    edn_arena_destroy(arena);
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    printf("Running newline_finder tests...\n\n");

    /* Basic tests */
    RUN_TEST(newline_find_all_basic);
    RUN_TEST(newline_find_all_empty);
    RUN_TEST(newline_find_all_no_newlines);
    RUN_TEST(newline_find_all_only_newlines);

    /* SIMD boundary tests */
    RUN_TEST(newline_find_all_simd_boundary);

    /* Large input tests */
    RUN_TEST(newline_find_all_large);

    /* UTF-8 tests */
    RUN_TEST(newline_find_all_utf8);

    /* Dynamic growth tests */
    RUN_TEST(newline_find_all_dynamic_growth);

    /* NULL handling tests */
    RUN_TEST(newline_find_all_null_data);

    /* Position tests */
    RUN_TEST(newline_get_position_first_line);
    RUN_TEST(newline_get_position_second_line);
    RUN_TEST(newline_get_position_third_line);
    RUN_TEST(newline_get_position_no_newlines);
    RUN_TEST(newline_get_position_empty);
    RUN_TEST(newline_get_position_null_checks);
    RUN_TEST(newline_get_position_utf8);

    /* Extended mode tests */
    RUN_TEST(newline_find_all_ex_lf_mode);
    RUN_TEST(newline_find_all_ex_crlf_aware);
    RUN_TEST(newline_find_all_ex_crlf_mixed);
    RUN_TEST(newline_find_all_ex_any_ascii);
    RUN_TEST(newline_find_all_ex_unicode);
    RUN_TEST(newline_find_all_ex_unicode_standalone_cr);
    RUN_TEST(newline_find_all_ex_position_conversion_crlf);

    TEST_SUMMARY("newline_finder");
}
