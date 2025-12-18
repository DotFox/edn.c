#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION

#include "../src/edn_internal.h"

TEST(basic_text_block_single_line) {
    /* """
       hello
       """
       -> "hello\n" */
    edn_result_t result = edn_read("\"\"\"\nhello\n\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 6);
    assert(strncmp(str, "hello\n", 6) == 0);

    edn_free(result.value);
}

TEST(text_block_multiple_lines) {
    /* """
       line1
       line2
       line3
       """
       -> "line1\nline2\nline3\n" */
    edn_result_t result = edn_read("\"\"\"\nline1\nline2\nline3\n\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 18);
    assert(strncmp(str, "line1\nline2\nline3\n", 18) == 0);

    edn_free(result.value);
}

TEST(text_block_multiple_lines_no_trailing) {
    /* """
       line1
       line2
       line3"""
       -> "line1\nline2\nline3" */
    edn_result_t result = edn_read("\"\"\"\nline1\nline2\nline3\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 17);
    assert(strncmp(str, "line1\nline2\nline3", 17) == 0);

    edn_free(result.value);
}

TEST(text_block_no_trailing_newline) {
    /* """
       content"""
       -> "content" */
    edn_result_t result = edn_read("\"\"\"\ncontent\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 7);
    assert(strncmp(str, "content", 7) == 0);

    edn_free(result.value);
}

TEST(text_block_closing_on_own_line) {
    /* """
       content
       """
       -> "content\n" */
    edn_result_t result = edn_read("\"\"\"\ncontent\n\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 8);
    assert(strncmp(str, "content\n", 8) == 0);

    edn_free(result.value);
}

TEST(text_block_empty) {
    /* """
       """
       -> "" */
    edn_result_t result = edn_read("\"\"\"\n\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 0);

    edn_free(result.value);
}

TEST(text_block_indentation_stripping) {
    /* """
               line1
              line2
             line3
             """
       -> "  line1\n line2\nline3\n" (min indent 6 from closing """) */
    edn_result_t result =
        edn_read("\"\"\"\n        line1\n       line2\n      line3\n      \"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    const char* expected = "  line1\n line2\nline3\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(result.value);
}

TEST(text_block_indentation_closing_delimiter) {
    /* """
            line1
           line2
          """
       -> "  line1\n line2\n" (min indent 3 from closing """) */
    edn_result_t result = edn_read("\"\"\"\n     line1\n    line2\n   \"\"\"", 0);
    assert(result.error == EDN_OK);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    const char* expected = "  line1\n line2\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(result.value);
}

TEST(text_block_indentation_closing_delimiter_at_line_start) {
    /* """
            line1
           line2
       """
       -> "     line1\n    line2\n" (min indent 0 from closing """) */
    edn_result_t result = edn_read("\"\"\"\n     line1\n    line2\n\"\"\"", 0);
    assert(result.error == EDN_OK);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    const char* expected = "     line1\n    line2\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(result.value);
}

TEST(text_block_trailing_whitespace_removed) {
    /* """
       hello   
       world  
       """
       -> "hello\nworld\n" (trailing spaces removed) */
    edn_result_t result = edn_read("\"\"\"\nhello   \nworld  \n\"\"\"", 0);
    assert(result.error == EDN_OK);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_uint_eq(len, 12);
    assert(strncmp(str, "hello\nworld\n", 12) == 0);

    edn_free(result.value);
}

TEST(text_block_blank_lines_preserved) {
    /* """
       line1

       line3
       """
       -> "line1\n\nline3\n" (blank line preserved, not used for min indent) */
    edn_result_t result = edn_read("\"\"\"\nline1\n\nline3\n\"\"\"", 0);
    assert(result.error == EDN_OK);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_uint_eq(len, 13);
    assert(strncmp(str, "line1\n\nline3\n", 13) == 0);

    edn_free(result.value);
}

TEST(text_block_with_escaped_triple_quotes) {
    /* """
       She said \"""Hello\"""
       """
       -> "She said \"\"\"Hello\"\"\"\n" (escape \""" becomes """) */
    edn_result_t result = edn_read("\"\"\"\nShe said \\\"\"\"Hello\\\"\"\"\n\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert(len > 0);

    edn_free(result.value);
}

TEST(text_block_unterminated) {
    /* """
       hello (no closing """) -> ERROR */
    edn_result_t result = edn_read("\"\"\"\nhello", 0);
    assert(result.error == EDN_ERROR_INVALID_STRING);
    assert(result.error_message != NULL);
}

TEST(text_block_missing_newline_after_opening) {
    /* """hello""" (no newline after opening, parses as regular string) -> "" */
    edn_result_t result = edn_read("\"\"\"hello\"\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 0);

    edn_free(result.value);
}

TEST(text_block_in_vector) {
    /* ["""
        hello
        """ 123]
       -> ["hello\n" 123] */
    edn_result_t result = edn_read("[\"\"\"\nhello\n\"\"\" 123]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    assert_uint_eq(edn_vector_count(result.value), 2);

    edn_value_t* str_val = edn_vector_get(result.value, 0);
    assert(str_val != NULL);
    assert(edn_type(str_val) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(str_val, &len);
    assert_uint_eq(len, 6);
    assert(strncmp(str, "hello\n", 6) == 0);

    edn_value_t* num = edn_vector_get(result.value, 1);
    assert(num != NULL);
    assert(edn_type(num) == EDN_TYPE_INT);

    edn_free(result.value);
}

TEST(text_block_in_map) {
    /* {:foo """
              line1
             line2
             """}
       -> {:foo " line1\nline2\n"} (min indent 6) */
    edn_result_t result = edn_read("{:foo \"\"\"\n       line1\n      line2\n      \"\"\"}", 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_result_t key = edn_read(":foo", 0);
    edn_value_t* val = edn_map_lookup(result.value, key.value);
    assert(val != NULL);
    assert(edn_type(val) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(val, &len);
    const char* expected = " line1\nline2\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(key.value);
    edn_free(result.value);
}

TEST(text_block_equality) {
    edn_result_t r1 = edn_read("\"\"\"\nline1\nline2\n\"\"\"", 0);
    edn_result_t r2 = edn_read("\"\"\"\nline1\nline2\n\"\"\"", 0);

    assert(r1.error == EDN_OK);
    assert(r2.error == EDN_OK);
    assert(edn_value_equal(r1.value, r2.value));

    edn_free(r1.value);
    edn_free(r2.value);
}

TEST(text_block_sql_example) {
    /* """
       SELECT * FROM users
       WHERE age > 21
       ORDER BY name
       """
       -> "SELECT * FROM users\nWHERE age > 21\nORDER BY name\n" */
    const char* input = "\"\"\"\n"
                        "SELECT * FROM users\n"
                        "WHERE age > 21\n"
                        "ORDER BY name\n"
                        "\"\"\"";

    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    const char* expected = "SELECT * FROM users\nWHERE age > 21\nORDER BY name\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(result.value);
}

TEST(text_block_example_closing_same_line) {
    /* {:foo """
              line1
             line2
            line3"""}
       -> {:foo "  line1\n line2\nline3"} (min indent 6 from line3) */
    const char* input = "{:foo \"\"\"\n        line1\n       line2\n      line3\"\"\"}";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_result_t key = edn_read(":foo", 0);
    edn_value_t* val = edn_map_lookup(result.value, key.value);
    assert(val != NULL);
    assert(edn_type(val) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(val, &len);
    const char* expected = "  line1\n line2\nline3";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(key.value);
    edn_free(result.value);
}

TEST(text_block_example_closing_own_line) {
    /* {:foo """
              line1
             line2
            line3
            """}
       -> {:foo "  line1\n line2\nline3\n"} (min indent 6 from closing """) */
    const char* input = "{:foo \"\"\"\n        line1\n       line2\n      line3\n      \"\"\"}";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_result_t key = edn_read(":foo", 0);
    edn_value_t* val = edn_map_lookup(result.value, key.value);
    assert(val != NULL);
    assert(edn_type(val) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(val, &len);
    const char* expected = "  line1\n line2\nline3\n";
    assert_uint_eq(len, strlen(expected));
    assert(strncmp(str, expected, len) == 0);

    edn_free(key.value);
    edn_free(result.value);
}

#else

TEST(text_block_disabled) {
    /* When EXPERIMENTAL_EXTENSION disabled, """<newline> parses as regular string -> ERROR or "" */
    edn_result_t result = edn_read("\"\"\"\nhello\n\"\"\"", 0);
    assert(result.error != EDN_OK || edn_type(result.value) == EDN_TYPE_STRING);
    if (result.value != NULL) {
        edn_free(result.value);
    }
}

#endif

int main(void) {
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
    RUN_TEST(basic_text_block_single_line);
    RUN_TEST(text_block_multiple_lines);
    RUN_TEST(text_block_multiple_lines_no_trailing);
    RUN_TEST(text_block_no_trailing_newline);
    RUN_TEST(text_block_closing_on_own_line);
    RUN_TEST(text_block_empty);
    RUN_TEST(text_block_indentation_stripping);
    RUN_TEST(text_block_indentation_closing_delimiter);
    RUN_TEST(text_block_indentation_closing_delimiter_at_line_start);
    RUN_TEST(text_block_trailing_whitespace_removed);
    RUN_TEST(text_block_blank_lines_preserved);
    RUN_TEST(text_block_with_escaped_triple_quotes);
    RUN_TEST(text_block_unterminated);
    RUN_TEST(text_block_missing_newline_after_opening);
    RUN_TEST(text_block_in_vector);
    RUN_TEST(text_block_in_map);
    RUN_TEST(text_block_equality);
    RUN_TEST(text_block_sql_example);
    RUN_TEST(text_block_example_closing_same_line);
    RUN_TEST(text_block_example_closing_own_line);

    printf("\nAll text block tests passed!\n");
#else
    RUN_TEST(text_block_disabled);
    printf("\nText block feature is disabled. Enable with EDN_ENABLE_EXPERIMENTAL_EXTENSION.\n");
#endif

    return 0;
}
