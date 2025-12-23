/* test_errors.c - Tests for EDN parsing error cases
 */

#include "../include/edn.h"
#include "test_framework.h"

/* ========================================================================
 * Unterminated Collections
 * ======================================================================== */

/* Unterminated List */

TEST(unterminated_list_empty) {
    /* Input: "(" - error_start at '(' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("(", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_list_with_elements) {
    /* Input: "(1 2 3" - error_start at '(' (offset 0), error_end at EOF (offset 6) */
    edn_result_t result = edn_read("(1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(unterminated_list_nested) {
    /* Input: "(1 (2 3)" - outer list unterminated, error_start at outer '(' (offset 0) */
    edn_result_t result = edn_read("(1 (2 3)", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(unterminated_list_with_whitespace) {
    /* Input: "(   " - error_start at '(' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("(   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Vector */

TEST(unterminated_vector_empty) {
    /* Input: "[" - error_start at '[' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("[", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_vector_with_elements) {
    /* Input: "[1 2 3" - error_start at '[' (offset 0), error_end at EOF (offset 6) */
    edn_result_t result = edn_read("[1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(unterminated_vector_nested) {
    /* Input: "[1 [2 3]" - outer vector unterminated, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[1 [2 3]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(unterminated_vector_with_whitespace) {
    /* Input: "[   " - error_start at '[' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("[   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Map */

TEST(unterminated_map_empty) {
    /* Input: "{" - error_start at '{' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_map_with_key) {
    /* Input: "{:key" - error_start at '{' (offset 0), error_end at EOF (offset 5) */
    edn_result_t result = edn_read("{:key", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 5);
}

TEST(unterminated_map_with_pairs) {
    /* Input: "{:a 1 :b 2" - error_start at '{' (offset 0), error_end at EOF (offset 10) */
    edn_result_t result = edn_read("{:a 1 :b 2", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_map_nested) {
    /* Input: "{:a {:b 1}" - outer map unterminated, error_start at outer '{' (offset 0) */
    edn_result_t result = edn_read("{:a {:b 1}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_map_with_whitespace) {
    /* Input: "{   " - error_start at '{' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("{   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Set */

TEST(unterminated_set_empty) {
    /* Input: "#{" - error_start at '#' (offset 0), error_end at EOF (offset 2) */
    edn_result_t result = edn_read("#{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(unterminated_set_with_elements) {
    /* Input: "#{1 2 3" - error_start at '#' (offset 0), error_end at EOF (offset 7) */
    edn_result_t result = edn_read("#{1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(unterminated_set_nested) {
    /* Input: "#{1 #{2 3}" - outer set unterminated, error_start at outer '#' (offset 0) */
    edn_result_t result = edn_read("#{1 #{2 3}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_set_with_whitespace) {
    /* Input: "#{   " - error_start at '#' (offset 0), error_end at EOF (offset 5) */
    edn_result_t result = edn_read("#{   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 5);
}

/* Deeply Nested Unterminated Collections */

TEST(unterminated_deeply_nested_list) {
    edn_result_t result = edn_read("(((", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
}

TEST(unterminated_deeply_nested_vector) {
    edn_result_t result = edn_read("[[[", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
}

TEST(unterminated_deeply_nested_map) {
    edn_result_t result = edn_read("{:a {:b {:c", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
}

TEST(unterminated_deeply_nested_set) {
    edn_result_t result = edn_read("#{#{#{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
}

/* Mixed Nested Unterminated Collections */
/* Note: Error message reports the innermost unterminated collection */

TEST(unterminated_mixed_list_in_vector) {
    edn_result_t result = edn_read("[(", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
}

TEST(unterminated_mixed_vector_in_map) {
    edn_result_t result = edn_read("{:key [", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
}

TEST(unterminated_mixed_map_in_set) {
    edn_result_t result = edn_read("#{1 {", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
}

TEST(unterminated_mixed_set_in_list) {
    edn_result_t result = edn_read("(1 #{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
}

/* ========================================================================
 * Mismatched Delimiters
 * 
 * For mismatched delimiters:
 * - error_start = opening delimiter
 * - error_end = wrong closing delimiter + 1
 * ======================================================================== */

TEST(mismatched_vector_with_brace) {
    /* Input: "[1 2 }" - error_start at '[' (offset 0), error_end after '}' (offset 6) */
    edn_result_t result = edn_read("[1 2 }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_list_with_bracket) {
    /* Input: "(1 2 ]" - error_start at '(' (offset 0), error_end after ']' (offset 6) */
    edn_result_t result = edn_read("(1 2 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in list");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_list_with_brace) {
    /* Input: "(1 2 }" - error_start at '(' (offset 0), error_end after '}' (offset 6) */
    edn_result_t result = edn_read("(1 2 }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in list");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_map_with_paren) {
    /* Input: "{:a 1 )" - error_start at '{' (offset 0), error_end after ')' (offset 7) */
    edn_result_t result = edn_read("{:a 1 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in map");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_map_with_bracket) {
    /* Input: "{:a 1 ]" - error_start at '{' (offset 0), error_end after ']' (offset 7) */
    edn_result_t result = edn_read("{:a 1 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in map");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_set_with_bracket) {
    /* Input: "#{1 2 ]" - error_start at '#' (offset 0), error_end after ']' (offset 7) */
    edn_result_t result = edn_read("#{1 2 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in set");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_set_with_paren) {
    /* Input: "#{1 2 )" - error_start at '#' (offset 0), error_end after ')' (offset 7) */
    edn_result_t result = edn_read("#{1 2 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in set");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_vector_with_paren) {
    /* Input: "[1 2 )" - error_start at '[' (offset 0), error_end after ')' (offset 6) */
    edn_result_t result = edn_read("[1 2 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_nested_inner) {
    /* Input: "[[1 2] }" - outer vector mismatched, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[[1 2] }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(mismatched_nested_outer) {
    /* Input: "[(1 2) }" - outer vector mismatched, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[(1 2) }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

/* ========================================================================
 * Character Literal Errors
 * ======================================================================== */

TEST(character_unexpected_eof) {
    /* Input: "\" - just backslash, unexpected end of input */
    edn_result_t result = edn_read("\\", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unexpected end of input in character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(character_invalid_unicode_too_short) {
    /* Input: "\u12" - only 2 hex digits, need at least 4 */
    edn_result_t result = edn_read("\\u12", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Unicode escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_invalid_unicode_bad_hex) {
    /* Input: "\u12GH" - invalid hex digits */
    edn_result_t result = edn_read("\\u12GH", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Unicode escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_invalid_unicode_out_of_range) {
    /* Input: "\uFFFFFF" - codepoint > 0x10FFFF (only with experimental extension) */
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
    edn_result_t result = edn_read("\\uFFFFFF", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unicode codepoint out of valid range");
    assert_uint_eq(result.error_start.offset, 0);
#endif
}

TEST(character_unsupported_whitespace_space) {
    /* Input: "\ " - space is not a valid single character literal */
    edn_result_t result = edn_read("\\ ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_unsupported_whitespace_tab) {
    /* Input: "\<tab>" - tab is not a valid single character literal */
    edn_result_t result = edn_read("\\\t", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_unsupported_whitespace_newline) {
    /* Input: "\<newline>" - newline is not a valid single character literal */
    edn_result_t result = edn_read("\\\n", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_unsupported_whitespace_return) {
    /* Input: "\<return>" - carriage return is not a valid single character literal */
    edn_result_t result = edn_read("\\\r", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_missing_delimiter_after) {
    /* Input: "\abc" - character literal must be followed by delimiter */
    edn_result_t result = edn_read("\\abc", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message,
                  "Unsupported character - expected delimiter after character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_invalid_named_partial) {
    /* Input: "\new" - partial match of "newline", not valid */
    edn_result_t result = edn_read("\\new", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message,
                  "Unsupported character - expected delimiter after character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_in_vector_invalid) {
    /* Input: "[\u12]" - invalid unicode in vector context */
    edn_result_t result = edn_read("[\\u12]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Unicode escape sequence in character literal");
}

TEST(character_in_map_key_invalid) {
    /* Input: "{\u12 :val}" - invalid unicode as map key */
    edn_result_t result = edn_read("{\\u12 :val}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Unicode escape sequence in character literal");
}

/* ========================================================================
 * Character Literal Errors - Clojure Extension
 * ======================================================================== */

#ifdef EDN_ENABLE_CLOJURE_EXTENSION

TEST(character_octal_invalid_digit_8) {
    /* Input: "\o8" - 8 is not a valid octal digit */
    edn_result_t result = edn_read("\\o8", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_octal_invalid_digit_9) {
    /* Input: "\o9" - 9 is not a valid octal digit */
    edn_result_t result = edn_read("\\o9", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_octal_overflow) {
    /* Input: "\o400" - octal 400 = 256 decimal, exceeds 0377 (255) */
    edn_result_t result = edn_read("\\o400", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_octal_trailing_invalid_digit) {
    /* Input: "\o128" - starts valid but has 8 following valid octal digits */
    edn_result_t result = edn_read("\\o128", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
    assert_uint_eq(result.error_start.offset, 0);
}

TEST(character_unsupported_whitespace_formfeed) {
    /* Input: "\<formfeed>" - formfeed byte is not a valid single character literal */
    edn_result_t result = edn_read("\\\f", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_unsupported_whitespace_backspace) {
    /* Input: "\<backspace>" - backspace byte is not a valid single character literal */
    edn_result_t result = edn_read("\\\b", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Unsupported character literal");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(character_octal_in_vector_invalid) {
    /* Input: "[\o8]" - invalid octal in vector context */
    edn_result_t result = edn_read("[\\o8]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
}

TEST(character_octal_in_map_key_invalid) {
    /* Input: "{\o400 :val}" - invalid octal overflow as map key */
    edn_result_t result = edn_read("{\\o400 :val}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_CHARACTER);
    assert_str_eq(result.error_message, "Invalid Octal escape sequence in character literal");
}

#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

/* ========================================================================
 * Discard Reader Errors
 * ======================================================================== */

TEST(discard_missing_value_eof) {
    /* Input: "#_" - discard at end of input with no value to discard */
    edn_result_t result = edn_read("#_", 0);
    assert(result.value == NULL);
    /* At top level, this results in unexpected EOF */
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
}

TEST(discard_missing_value_in_vector) {
    /* Input: "[1 #_]" - discard at end of vector with no value */
    edn_result_t result = edn_read("[1 #_]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 3);
    assert_uint_eq(result.error_end.offset, 5);
}

TEST(discard_missing_value_in_list) {
    /* Input: "(1 #_)" - discard at end of list with no value */
    edn_result_t result = edn_read("(1 #_)", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 3);
    assert_uint_eq(result.error_end.offset, 5);
}

TEST(discard_missing_value_in_map) {
    /* Input: "{:a #_}" - discard at end of map with no value */
    edn_result_t result = edn_read("{:a #_}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 4);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(discard_missing_value_in_set) {
    /* Input: "#{1 #_}" - discard at end of set with no value */
    edn_result_t result = edn_read("#{1 #_}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 4);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(discard_with_only_whitespace) {
    /* Input: "#_   " - discard followed by only whitespace */
    edn_result_t result = edn_read("#_   ", 0);
    assert(result.value == NULL);
    /* At top level with only whitespace, this results in unexpected EOF */
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
}

TEST(discard_with_comment_only) {
    /* Input: "#_ ; comment\n" - discard followed by only comment */
    edn_result_t result = edn_read("#_ ; comment\n", 0);
    assert(result.value == NULL);
    /* At top level with only comment, this results in unexpected EOF */
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
}

TEST(discard_propagates_nested_error) {
    /* Input: "#_[1 2" - discard with unterminated vector */
    edn_result_t result = edn_read("#_[1 2", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
}

TEST(discard_propagates_string_error) {
    /* Input: "#_\"unterminated" - discard with unterminated string */
    edn_result_t result = edn_read("#_\"unterminated", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_STRING);
}

TEST(discard_nested_missing_value) {
    /* Input: "[1 #_#_]" - nested discard with no values */
    /* Error should point to the last (inner) #_ at offset 5-7 */
    edn_result_t result = edn_read("[1 #_#_]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 5);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(discard_nested_partial_missing_value) {
    /* Input: "[1 #_#_2]" - nested discard, outer discard missing value */
    /* Error should point to the first (outer) #_ at offset 3-5 */
    edn_result_t result = edn_read("[1 #_#_2]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.offset, 3);
    assert_uint_eq(result.error_end.offset, 5);
}

TEST(discard_creates_odd_map_elements) {
    /* Input: "{:a 1 :b #_2}" - discarding last value leaves :a 1 :b = odd elements */
    edn_result_t result = edn_read("{:a 1 :b #_2}", 0);
    assert(result.value == NULL);
    /* Map with odd number of elements results in invalid syntax error */
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert_str_eq(result.error_message, "Map has odd number of elements (key without value)");
}

TEST(discard_multiline_error_position) {
    /* Input: "[\n#_\n]" - discard on line 2 with no value */
    edn_result_t result = edn_read("[\n#_\n]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_DISCARD);
    assert_str_eq(result.error_message, "Discard macro missing value");
    assert_uint_eq(result.error_start.line, 2);
}

/* ========================================================================
 * Multi-line Error Positions
 * ======================================================================== */

TEST(mismatched_multiline) {
    /* Input: "[1\n2\n}" - mismatched, error_start at '[' on line 1, error_end at '}' on line 3 */
    edn_result_t result = edn_read("[1\n2\n}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_end.offset, 6);
    assert_uint_eq(result.error_end.line, 3);
}

TEST(unterminated_multiline) {
    /* Input: "[\n1\n2" - error_start at '[' on line 1, error_end at EOF on line 3 */
    edn_result_t result = edn_read("[\n1\n2", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_end.offset, 5);
    assert_uint_eq(result.error_end.line, 3);
}

int main(void) {
    printf("Running error tests...\n");

    /* Unterminated List */
    RUN_TEST(unterminated_list_empty);
    RUN_TEST(unterminated_list_with_elements);
    RUN_TEST(unterminated_list_nested);
    RUN_TEST(unterminated_list_with_whitespace);

    /* Unterminated Vector */
    RUN_TEST(unterminated_vector_empty);
    RUN_TEST(unterminated_vector_with_elements);
    RUN_TEST(unterminated_vector_nested);
    RUN_TEST(unterminated_vector_with_whitespace);

    /* Unterminated Map */
    RUN_TEST(unterminated_map_empty);
    RUN_TEST(unterminated_map_with_key);
    RUN_TEST(unterminated_map_with_pairs);
    RUN_TEST(unterminated_map_nested);
    RUN_TEST(unterminated_map_with_whitespace);

    /* Unterminated Set */
    RUN_TEST(unterminated_set_empty);
    RUN_TEST(unterminated_set_with_elements);
    RUN_TEST(unterminated_set_nested);
    RUN_TEST(unterminated_set_with_whitespace);

    /* Deeply Nested Unterminated Collections */
    RUN_TEST(unterminated_deeply_nested_list);
    RUN_TEST(unterminated_deeply_nested_vector);
    RUN_TEST(unterminated_deeply_nested_map);
    RUN_TEST(unterminated_deeply_nested_set);

    /* Mixed Nested Unterminated Collections */
    RUN_TEST(unterminated_mixed_list_in_vector);
    RUN_TEST(unterminated_mixed_vector_in_map);
    RUN_TEST(unterminated_mixed_map_in_set);
    RUN_TEST(unterminated_mixed_set_in_list);

    /* Mismatched Delimiters */
    RUN_TEST(mismatched_vector_with_brace);
    RUN_TEST(mismatched_list_with_bracket);
    RUN_TEST(mismatched_list_with_brace);
    RUN_TEST(mismatched_map_with_paren);
    RUN_TEST(mismatched_map_with_bracket);
    RUN_TEST(mismatched_set_with_bracket);
    RUN_TEST(mismatched_set_with_paren);
    RUN_TEST(mismatched_vector_with_paren);
    RUN_TEST(mismatched_nested_inner);
    RUN_TEST(mismatched_nested_outer);

    /* Character Literal Errors */
    RUN_TEST(character_unexpected_eof);
    RUN_TEST(character_invalid_unicode_too_short);
    RUN_TEST(character_invalid_unicode_bad_hex);
    RUN_TEST(character_invalid_unicode_out_of_range);
    RUN_TEST(character_unsupported_whitespace_space);
    RUN_TEST(character_unsupported_whitespace_tab);
    RUN_TEST(character_unsupported_whitespace_newline);
    RUN_TEST(character_unsupported_whitespace_return);
    RUN_TEST(character_missing_delimiter_after);
    RUN_TEST(character_invalid_named_partial);
    RUN_TEST(character_in_vector_invalid);
    RUN_TEST(character_in_map_key_invalid);

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    /* Character Literal Errors - Clojure Extension */
    RUN_TEST(character_octal_invalid_digit_8);
    RUN_TEST(character_octal_invalid_digit_9);
    RUN_TEST(character_octal_overflow);
    RUN_TEST(character_octal_trailing_invalid_digit);
    RUN_TEST(character_unsupported_whitespace_formfeed);
    RUN_TEST(character_unsupported_whitespace_backspace);
    RUN_TEST(character_octal_in_vector_invalid);
    RUN_TEST(character_octal_in_map_key_invalid);
#endif

    /* Discard Reader Errors */
    RUN_TEST(discard_missing_value_eof);
    RUN_TEST(discard_missing_value_in_vector);
    RUN_TEST(discard_missing_value_in_list);
    RUN_TEST(discard_missing_value_in_map);
    RUN_TEST(discard_missing_value_in_set);
    RUN_TEST(discard_with_only_whitespace);
    RUN_TEST(discard_with_comment_only);
    RUN_TEST(discard_propagates_nested_error);
    RUN_TEST(discard_propagates_string_error);
    RUN_TEST(discard_nested_missing_value);
    RUN_TEST(discard_nested_partial_missing_value);
    RUN_TEST(discard_creates_odd_map_elements);
    RUN_TEST(discard_multiline_error_position);

    /* Multi-line Error Positions */
    RUN_TEST(mismatched_multiline);
    RUN_TEST(unterminated_multiline);

    TEST_SUMMARY("errors");
}
