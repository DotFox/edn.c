/**
 * Test identifier parsing (symbols, keywords, reserved words)
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test reserved keywords */
TEST(reserved_nil) {
    edn_result_t result = edn_parse("nil", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_NIL);
    edn_free(result.value);
}

TEST(reserved_true) {
    edn_result_t result = edn_parse("true", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);
    edn_free(result.value);
}

TEST(reserved_false) {
    edn_result_t result = edn_parse("false", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_BOOL);
    edn_free(result.value);
}

TEST(symbol_simple) {
    edn_result_t result = edn_parse("foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    const char *ns, *name;
    assert(edn_symbol_get(result.value, &ns, NULL, &name, NULL));
    assert(ns == NULL);
    assert(strcmp(name, "foo") == 0);

    edn_free(result.value);
}

TEST(symbol_simple_with_unicode) {
    edn_result_t result = edn_parse("föö", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_with_dash) {
    edn_result_t result = edn_parse("foo-bar", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    const char *ns, *name;
    assert(edn_symbol_get(result.value, &ns, NULL, &name, NULL));
    assert(ns == NULL);
    assert(strcmp(name, "foo-bar") == 0);

    edn_free(result.value);
}

TEST(symbol_nil_prefix) {
    edn_result_t result = edn_parse("nilo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_true_prefix) {
    edn_result_t result = edn_parse("truee", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_false_suffix) {
    edn_result_t result = edn_parse("falsee", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_plus) {
    edn_result_t result = edn_parse("+", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_slash_alone) {
    edn_result_t result = edn_parse("/", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_namespaced_simple) {
    edn_result_t result = edn_parse("foo/bar", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    const char *ns, *name;
    size_t ns_len, name_len;
    assert(edn_symbol_get(result.value, &ns, &ns_len, &name, &name_len));
    assert(ns != NULL);
    assert(ns_len == 3);
    assert(memcmp(ns, "foo", 3) == 0);
    assert(name_len == 3);
    assert(memcmp(name, "bar", 3) == 0);

    edn_free(result.value);
}

TEST(symbol_namespaced_multiple_slashes) {
    edn_result_t result = edn_parse("foo/bar/baz", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);

    const char *ns, *name;
    size_t ns_len, name_len;
    assert(edn_symbol_get(result.value, &ns, &ns_len, &name, &name_len));
    assert(ns_len == 3);
    assert(memcmp(ns, "foo", 3) == 0);
    assert(name_len == 7);
    assert(memcmp(name, "bar/baz", 7) == 0);

    edn_free(result.value);
}

TEST(symbol_namespaced_reserved_in_namespace) {
    edn_result_t result = edn_parse("nil/foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(symbol_namespaced_reserved_in_name) {
    edn_result_t result = edn_parse("foo/nil", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(keyword_simple) {
    edn_result_t result = edn_parse(":foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);

    const char *ns, *name;
    assert(edn_keyword_get(result.value, &ns, NULL, &name, NULL));
    assert(ns == NULL);
    assert(strcmp(name, "foo") == 0);

    edn_free(result.value);
}

TEST(keyword_with_dash) {
    edn_result_t result = edn_parse(":foo-bar", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);
    edn_free(result.value);
}

TEST(keyword_double_colon_name) {
    edn_result_t result = edn_parse("::name", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(keyword_double_colon_namespace) {
    edn_result_t result = edn_parse("::ns/name", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(keyword_double_colon_namespaced_name) {
    edn_result_t result = edn_parse("::a/b/c", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(keyword_plus) {
    edn_result_t result = edn_parse(":+", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);
    edn_free(result.value);
}

TEST(keyword_namespaced_simple) {
    edn_result_t result = edn_parse(":foo/bar", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);

    const char *ns, *name;
    size_t ns_len, name_len;
    assert(edn_keyword_get(result.value, &ns, &ns_len, &name, &name_len));
    assert(ns_len == 3);
    assert(memcmp(ns, "foo", 3) == 0);
    assert(name_len == 3);
    assert(memcmp(name, "bar", 3) == 0);

    edn_free(result.value);
}

TEST(keyword_namespaced_short) {
    edn_result_t result = edn_parse(":a/b", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);
    edn_free(result.value);
}

TEST(keyword_namespaced_long) {
    edn_result_t result = edn_parse(":foo.bar.baz/qux", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);
    edn_free(result.value);
}

TEST(invalid_empty) {
    edn_result_t result = edn_parse("", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_colon_alone) {
    edn_result_t result = edn_parse(":", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_slash_at_start) {
    edn_result_t result = edn_parse("/foo", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_slash_at_end) {
    edn_result_t result = edn_parse("foo/", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_colon_slash) {
    edn_result_t result = edn_parse(":/", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_colon_slash_name) {
    edn_result_t result = edn_parse(":/foo", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(invalid_keyword_slash_at_end) {
    edn_result_t result = edn_parse(":foo/", 0);
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

int main(void) {
    printf("Running identifier parsing tests...\n\n");

    RUN_TEST(reserved_nil);
    RUN_TEST(reserved_true);
    RUN_TEST(reserved_false);
    RUN_TEST(symbol_simple);
    RUN_TEST(symbol_simple_with_unicode);
    RUN_TEST(symbol_with_dash);
    RUN_TEST(symbol_nil_prefix);
    RUN_TEST(symbol_true_prefix);
    RUN_TEST(symbol_false_suffix);
    RUN_TEST(symbol_plus);
    RUN_TEST(symbol_slash_alone);
    RUN_TEST(symbol_namespaced_simple);
    RUN_TEST(symbol_namespaced_multiple_slashes);
    RUN_TEST(symbol_namespaced_reserved_in_namespace);
    RUN_TEST(symbol_namespaced_reserved_in_name);
    RUN_TEST(keyword_simple);
    RUN_TEST(keyword_with_dash);
    RUN_TEST(keyword_double_colon_name);
    RUN_TEST(keyword_double_colon_namespace);
    RUN_TEST(keyword_double_colon_namespaced_name);
    RUN_TEST(keyword_plus);
    RUN_TEST(keyword_namespaced_simple);
    RUN_TEST(keyword_namespaced_short);
    RUN_TEST(keyword_namespaced_long);
    RUN_TEST(invalid_empty);
    RUN_TEST(invalid_colon_alone);
    RUN_TEST(invalid_slash_at_start);
    RUN_TEST(invalid_slash_at_end);
    RUN_TEST(invalid_colon_slash);
    RUN_TEST(invalid_colon_slash_name);
    RUN_TEST(invalid_keyword_slash_at_end);

    TEST_SUMMARY("identifier parsing");
}
