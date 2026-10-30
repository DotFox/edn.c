#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Simple test to verify build system and API */

TEST(parse_null_input) {
    edn_result_t result = edn_read(NULL, 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(parse_empty_string) {
    edn_result_t result = edn_read("", 0);
    assert(result.value == NULL);
    assert(result.error != EDN_OK);
}

TEST(edn_free_null) {
    /* Should not crash */
    edn_free(NULL);
}

TEST(edn_type_null) {
    edn_type_t type = edn_type(NULL);
    assert(type == EDN_TYPE_NIL);
}

TEST(parse_eof_with_eof_value) {
    /* First, parse a simple value to use as eof_value */
    edn_result_t eof_result = edn_read(":eof", 0);
    assert(eof_result.error == EDN_OK);
    assert(eof_result.value != NULL);
    assert(edn_type(eof_result.value) == EDN_TYPE_KEYWORD);

    /* Now test that EOF error returns eof_value */
    edn_parse_options_t options = {0};
    options.eof_value = eof_result.value;
    options.reader_registry = NULL;
    options.default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH;

    edn_result_t result = edn_read_with_options("   ", 3, &options);

    /* Should return EDN_OK with the eof_value */
    assert(result.error == EDN_OK);
    assert(result.value == eof_result.value);
    assert(result.error_message == NULL);

    /* Verify it's the expected keyword */
    const char* name;
    bool got_keyword = edn_keyword_get(result.value, NULL, NULL, &name, NULL);
    assert(got_keyword);
    assert(strncmp(name, "eof", 3) == 0);

    /* Clean up */
    edn_free(eof_result.value);
}

TEST(parse_eof_without_eof_value) {
    /* Test that EOF error is returned when eof_value is not set */
    edn_parse_options_t options = {0};
    options.eof_value = NULL;
    options.reader_registry = NULL;
    options.default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH;

    edn_result_t result = edn_read_with_options("   ", 3, &options);

    /* Should return EOF error */
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

TEST(eof_value_after_complete_input) {
    const char* inputs[] = {"",          " \n,\t",    "; comment", "#_ 1", "#_ [1] ; comment",
                            "#_ 1 #_ 2", "#_ #_ 1 2", "#_ #tag 1"};
    edn_result_t sentinel = edn_read("nil", 0);
    edn_parse_options_t options = {0};
    options.struct_size = sizeof(options);
    options.eof_value = sentinel.value;

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        edn_result_t baseline = edn_read(inputs[i], 0);
        assert_int_eq(baseline.error, EDN_ERROR_UNEXPECTED_EOF);
        assert(baseline.value == NULL);

        edn_result_t result = edn_read_with_options(inputs[i], 0, &options);
        assert_int_eq(result.error, EDN_OK);
        assert_ptr_eq(result.value, sentinel.value);
        assert(result.error_message == NULL);
    }
    edn_free(sentinel.value);
}

TEST(eof_value_preserves_incomplete_form_errors) {
    const char* inputs[] = {
        "#",
        "#tag",
        "#tag \n; comment",
        "#_",
        "#_ ; comment",
        "#_ #_ 1",
        "#_ #tag",
        "#_ 1 #tag",
        "#tag #_ 1",
        "[",
        "(",
        "{",
        "{:a",
        "#{",
        "[1 #_ 2",
        "\"unfinished",
        "\\",
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        "^",
        "^:private",
        "^:private #_ 1",
        "#_ ^:private",
#endif
    };
    edn_result_t sentinel = edn_read("nil", 0);
    edn_parse_options_t options = {0};
    options.struct_size = sizeof(options);
    options.eof_value = sentinel.value;

    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        edn_result_t baseline = edn_read(inputs[i], 0);
        assert(baseline.error != EDN_OK);
        assert(baseline.value == NULL);

        edn_result_t result = edn_read_with_options(inputs[i], 0, &options);
        assert_int_eq(result.error, baseline.error);
        assert(result.value == NULL);
        assert_str_eq(result.error_message, baseline.error_message);
        assert_uint_eq(result.error_start.offset, baseline.error_start.offset);
        assert_uint_eq(result.error_start.line, baseline.error_start.line);
        assert_uint_eq(result.error_start.column, baseline.error_start.column);
        assert_uint_eq(result.error_end.offset, baseline.error_end.offset);
        assert_uint_eq(result.error_end.line, baseline.error_end.line);
        assert_uint_eq(result.error_end.column, baseline.error_end.column);
    }
    edn_free(sentinel.value);
}

int main(void) {
    printf("Running basic API tests...\n");

    run_test_parse_null_input();
    run_test_parse_empty_string();
    run_test_edn_free_null();
    run_test_edn_type_null();
    run_test_parse_eof_with_eof_value();
    run_test_parse_eof_without_eof_value();
    RUN_TEST(eof_value_after_complete_input);
    RUN_TEST(eof_value_preserves_incomplete_form_errors);

    TEST_SUMMARY("basic API");
}
