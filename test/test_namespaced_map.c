/**
 * Test: Namespaced map syntax (#:ns{...})
 * 
 * This is a Clojure extension to EDN, not part of the official spec.
 * Requires EDN_ENABLE_MAP_NAMESPACE_SYNTAX compilation flag.
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX

TEST(basic_namespaced_map) {
    /* #:foo{:x 1 :y 2} should be equivalent to {:foo/x 1 :foo/y 2} */
    edn_result_t result = edn_read("#:foo{:x 1 :y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Check first key has namespace */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    assert(edn_type(key1) == EDN_TYPE_KEYWORD);

    const char* ns1;
    size_t ns1_len;
    const char* name1;
    size_t name1_len;
    assert_true(edn_keyword_get(key1, &ns1, &ns1_len, &name1, &name1_len));
    assert(ns1 != NULL);
    assert_uint_eq(ns1_len, 3);
    assert(strncmp(ns1, "foo", 3) == 0);
    assert_uint_eq(name1_len, 1);
    assert(strncmp(name1, "x", 1) == 0);

    /* Check second key has namespace */
    edn_value_t* key2 = edn_map_get_key(result.value, 1);
    assert(edn_type(key2) == EDN_TYPE_KEYWORD);

    const char* ns2;
    size_t ns2_len;
    const char* name2;
    size_t name2_len;
    assert_true(edn_keyword_get(key2, &ns2, &ns2_len, &name2, &name2_len));
    assert(ns2 != NULL);
    assert_uint_eq(ns2_len, 3);
    assert(strncmp(ns2, "foo", 3) == 0);
    assert_uint_eq(name2_len, 1);
    assert(strncmp(name2, "y", 1) == 0);

    /* Check values */
    edn_value_t* val1 = edn_map_get_value(result.value, 0);
    int64_t num1;
    assert_true(edn_int64_get(val1, &num1));
    assert_int_eq(num1, 1);

    edn_value_t* val2 = edn_map_get_value(result.value, 1);
    int64_t num2;
    assert_true(edn_int64_get(val2, &num2));
    assert_int_eq(num2, 2);

    edn_free(result.value);
}

TEST(namespaced_map_with_already_namespaced_keys) {
    /* #:foo{:x 1 :bar/y 2} should be {:foo/x 1 :bar/y 2} */
    /* Keys with existing namespace should not be transformed */
    edn_result_t result = edn_read("#:foo{:x 1 :bar/y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Find key :foo/x */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    const char* ns1;
    size_t ns1_len;
    const char* name1;
    size_t name1_len;
    assert_true(edn_keyword_get(key1, &ns1, &ns1_len, &name1, &name1_len));

    if (name1_len == 1 && strncmp(name1, "x", 1) == 0) {
        /* This is :foo/x */
        assert(ns1 != NULL);
        assert_uint_eq(ns1_len, 3);
        assert(strncmp(ns1, "foo", 3) == 0);

        /* Check the other key is :bar/y */
        edn_value_t* key2 = edn_map_get_key(result.value, 1);
        const char* ns2;
        size_t ns2_len;
        const char* name2;
        size_t name2_len;
        assert_true(edn_keyword_get(key2, &ns2, &ns2_len, &name2, &name2_len));
        assert(ns2 != NULL);
        assert_uint_eq(ns2_len, 3);
        assert(strncmp(ns2, "bar", 3) == 0);
        assert_uint_eq(name2_len, 1);
        assert(strncmp(name2, "y", 1) == 0);
    } else {
        /* This is :bar/y */
        assert(ns1 != NULL);
        assert_uint_eq(ns1_len, 3);
        assert(strncmp(ns1, "bar", 3) == 0);
        assert_uint_eq(name1_len, 1);
        assert(strncmp(name1, "y", 1) == 0);

        /* Check the other key is :foo/x */
        edn_value_t* key2 = edn_map_get_key(result.value, 1);
        const char* ns2;
        size_t ns2_len;
        const char* name2;
        size_t name2_len;
        assert_true(edn_keyword_get(key2, &ns2, &ns2_len, &name2, &name2_len));
        assert(ns2 != NULL);
        assert_uint_eq(ns2_len, 3);
        assert(strncmp(ns2, "foo", 3) == 0);
        assert_uint_eq(name2_len, 1);
        assert(strncmp(name2, "x", 1) == 0);
    }

    edn_free(result.value);
}

TEST(namespaced_map_with_non_keyword_keys) {
    /* #:foo{"x" 1 :y 2} should be {"x" 1 :foo/y 2} */
    /* Non-keyword keys should not be transformed */
    edn_result_t result = edn_read("#:foo{\"x\" 1 :y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Find string key */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    if (edn_type(key1) == EDN_TYPE_STRING) {
        size_t len;
        const char* str = edn_string_get(key1, &len);
        assert(str != NULL);
        assert_uint_eq(len, 1);
        assert(strncmp(str, "x", 1) == 0);

        /* Check the keyword key was transformed */
        edn_value_t* key2 = edn_map_get_key(result.value, 1);
        assert(edn_type(key2) == EDN_TYPE_KEYWORD);
        const char* ns2;
        size_t ns2_len;
        const char* name2;
        size_t name2_len;
        assert_true(edn_keyword_get(key2, &ns2, &ns2_len, &name2, &name2_len));
        assert(ns2 != NULL);
        assert_uint_eq(ns2_len, 3);
        assert(strncmp(ns2, "foo", 3) == 0);
        assert_uint_eq(name2_len, 1);
        assert(strncmp(name2, "y", 1) == 0);
    } else {
        /* This is the keyword */
        assert(edn_type(key1) == EDN_TYPE_KEYWORD);
        const char* ns1;
        size_t ns1_len;
        const char* name1;
        size_t name1_len;
        assert_true(edn_keyword_get(key1, &ns1, &ns1_len, &name1, &name1_len));
        assert(ns1 != NULL);
        assert_uint_eq(ns1_len, 3);
        assert(strncmp(ns1, "foo", 3) == 0);
        assert_uint_eq(name1_len, 1);
        assert(strncmp(name1, "y", 1) == 0);

        /* Check the string key was not transformed */
        edn_value_t* key2 = edn_map_get_key(result.value, 1);
        assert(edn_type(key2) == EDN_TYPE_STRING);
        size_t len;
        const char* str = edn_string_get(key2, &len);
        assert(str != NULL);
        assert_uint_eq(len, 1);
        assert(strncmp(str, "x", 1) == 0);
    }

    edn_free(result.value);
}

TEST(namespaced_map_empty) {
    edn_result_t result = edn_read("#:foo{}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 0);

    edn_free(result.value);
}

TEST(namespaced_map_whitespace) {
    edn_result_t result = edn_read("#:foo  { :x  1  :y  2 }", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    edn_free(result.value);
}

TEST(namespaced_map_error_namespace_not_keyword) {
    edn_result_t result = edn_read("#:\"foo\"{:x 1}", 0);
    assert(result.error != EDN_OK);
}

TEST(namespaced_map_error_namespace_has_namespace) {
    edn_result_t result = edn_read("#:foo/bar{:x 1}", 0);
    assert(result.error != EDN_OK);
}

TEST(namespaced_map_error_not_followed_by_map) {
    edn_result_t result = edn_read("#:foo[:x 1]", 0);
    assert(result.error != EDN_OK);
}

TEST(namespaced_map_duplicate_keys) {
    edn_result_t result = edn_read("#:foo{:x 1 :x 2}", 0);
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
}

TEST(namespaced_map_duplicate_keys_extra) {
    /* After transformation, these become duplicates: :foo/x and :foo/x */
    edn_result_t result = edn_read("#:foo{:x 1 :foo/x 2}", 0);
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
}

TEST(namespaced_map_with_symbol_keys) {
    /* #:foo{x 1 y 2} should be {foo/x 1 foo/y 2} */
    /* Symbol keys without namespace should be transformed */
    edn_result_t result = edn_read("#:foo{x 1 y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Check first key has namespace */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    assert(edn_type(key1) == EDN_TYPE_SYMBOL);

    const char* ns1;
    size_t ns1_len;
    const char* name1;
    size_t name1_len;
    assert_true(edn_symbol_get(key1, &ns1, &ns1_len, &name1, &name1_len));
    assert(ns1 != NULL);
    assert_uint_eq(ns1_len, 3);
    assert(strncmp(ns1, "foo", 3) == 0);
    assert_uint_eq(name1_len, 1);
    assert(strncmp(name1, "x", 1) == 0 || strncmp(name1, "y", 1) == 0);

    /* Check second key has namespace */
    edn_value_t* key2 = edn_map_get_key(result.value, 1);
    assert(edn_type(key2) == EDN_TYPE_SYMBOL);

    const char* ns2;
    size_t ns2_len;
    const char* name2;
    size_t name2_len;
    assert_true(edn_symbol_get(key2, &ns2, &ns2_len, &name2, &name2_len));
    assert(ns2 != NULL);
    assert_uint_eq(ns2_len, 3);
    assert(strncmp(ns2, "foo", 3) == 0);
    assert_uint_eq(name2_len, 1);
    assert(strncmp(name2, "x", 1) == 0 || strncmp(name2, "y", 1) == 0);

    /* Ensure keys are different */
    assert(strncmp(name1, name2, 1) != 0);

    /* Check values */
    edn_value_t* val1 = edn_map_get_value(result.value, 0);
    int64_t num1;
    assert_true(edn_int64_get(val1, &num1));
    assert(num1 == 1 || num1 == 2);

    edn_value_t* val2 = edn_map_get_value(result.value, 1);
    int64_t num2;
    assert_true(edn_int64_get(val2, &num2));
    assert(num2 == 1 || num2 == 2);

    edn_free(result.value);
}

TEST(namespaced_map_with_mixed_symbol_keyword_keys) {
    /* #:foo{x 1 :y 2} should be {foo/x 1 :foo/y 2} */
    /* Both symbols and keywords should be namespaced */
    edn_result_t result = edn_read("#:foo{x 1 :y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Check we have one symbol and one keyword */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    edn_value_t* key2 = edn_map_get_key(result.value, 1);

    bool has_symbol = false;
    bool has_keyword = false;

    if (edn_type(key1) == EDN_TYPE_SYMBOL) {
        has_symbol = true;
        const char* ns;
        size_t ns_len;
        const char* name;
        size_t name_len;
        assert_true(edn_symbol_get(key1, &ns, &ns_len, &name, &name_len));
        assert(ns != NULL);
        assert_uint_eq(ns_len, 3);
        assert(strncmp(ns, "foo", 3) == 0);
        assert_uint_eq(name_len, 1);
        assert(strncmp(name, "x", 1) == 0);
    } else if (edn_type(key1) == EDN_TYPE_KEYWORD) {
        has_keyword = true;
        const char* ns;
        size_t ns_len;
        const char* name;
        size_t name_len;
        assert_true(edn_keyword_get(key1, &ns, &ns_len, &name, &name_len));
        assert(ns != NULL);
        assert_uint_eq(ns_len, 3);
        assert(strncmp(ns, "foo", 3) == 0);
        assert_uint_eq(name_len, 1);
        assert(strncmp(name, "y", 1) == 0);
    }

    if (edn_type(key2) == EDN_TYPE_SYMBOL) {
        has_symbol = true;
        const char* ns;
        size_t ns_len;
        const char* name;
        size_t name_len;
        assert_true(edn_symbol_get(key2, &ns, &ns_len, &name, &name_len));
        assert(ns != NULL);
        assert_uint_eq(ns_len, 3);
        assert(strncmp(ns, "foo", 3) == 0);
        assert_uint_eq(name_len, 1);
        assert(strncmp(name, "x", 1) == 0);
    } else if (edn_type(key2) == EDN_TYPE_KEYWORD) {
        has_keyword = true;
        const char* ns;
        size_t ns_len;
        const char* name;
        size_t name_len;
        assert_true(edn_keyword_get(key2, &ns, &ns_len, &name, &name_len));
        assert(ns != NULL);
        assert_uint_eq(ns_len, 3);
        assert(strncmp(ns, "foo", 3) == 0);
        assert_uint_eq(name_len, 1);
        assert(strncmp(name, "y", 1) == 0);
    }

    assert_true(has_symbol && has_keyword);

    edn_free(result.value);
}

TEST(namespaced_map_with_already_namespaced_symbol_keys) {
    /* #:foo{x 1 bar/y 2} should be {foo/x 1 bar/y 2} */
    /* Symbols with existing namespace should not be transformed */
    edn_result_t result = edn_read("#:foo{x 1 bar/y 2}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert_uint_eq(edn_map_count(result.value), 2);

    /* Find keys */
    edn_value_t* key1 = edn_map_get_key(result.value, 0);
    edn_value_t* key2 = edn_map_get_key(result.value, 1);

    assert(edn_type(key1) == EDN_TYPE_SYMBOL);
    assert(edn_type(key2) == EDN_TYPE_SYMBOL);

    const char* ns1;
    size_t ns1_len;
    const char* name1;
    size_t name1_len;
    assert_true(edn_symbol_get(key1, &ns1, &ns1_len, &name1, &name1_len));

    const char* ns2;
    size_t ns2_len;
    const char* name2;
    size_t name2_len;
    assert_true(edn_symbol_get(key2, &ns2, &ns2_len, &name2, &name2_len));

    /* One should be foo/x and the other bar/y */
    bool found_foo_x = false;
    bool found_bar_y = false;

    if (name1_len == 1 && strncmp(name1, "x", 1) == 0) {
        assert(ns1 != NULL);
        assert_uint_eq(ns1_len, 3);
        assert(strncmp(ns1, "foo", 3) == 0);
        found_foo_x = true;
    } else if (name1_len == 1 && strncmp(name1, "y", 1) == 0) {
        assert(ns1 != NULL);
        assert_uint_eq(ns1_len, 3);
        assert(strncmp(ns1, "bar", 3) == 0);
        found_bar_y = true;
    }

    if (name2_len == 1 && strncmp(name2, "x", 1) == 0) {
        assert(ns2 != NULL);
        assert_uint_eq(ns2_len, 3);
        assert(strncmp(ns2, "foo", 3) == 0);
        found_foo_x = true;
    } else if (name2_len == 1 && strncmp(name2, "y", 1) == 0) {
        assert(ns2 != NULL);
        assert_uint_eq(ns2_len, 3);
        assert(strncmp(ns2, "bar", 3) == 0);
        found_bar_y = true;
    }

    assert_true(found_foo_x && found_bar_y);

    edn_free(result.value);
}

#else /* EDN_ENABLE_MAP_NAMESPACE_SYNTAX */

/* Test that namespaced map syntax fails when disabled */
TEST(namespaced_map_syntax_disabled) {
    edn_result_t result = edn_read("#:foo{:x 1}", 0);
    assert(result.error != EDN_OK);
    assert_ptr_eq(result.value, NULL);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(strstr(result.error_message, "symbol") != NULL);
}

#endif /* !EDN_ENABLE_MAP_NAMESPACE_SYNTAX */

int main(void) {
#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX
    printf("Running namespaced map tests:\n");

    RUN_TEST(basic_namespaced_map);
    RUN_TEST(namespaced_map_with_already_namespaced_keys);
    RUN_TEST(namespaced_map_with_non_keyword_keys);
    RUN_TEST(namespaced_map_empty);
    RUN_TEST(namespaced_map_whitespace);
    RUN_TEST(namespaced_map_error_namespace_not_keyword);
    RUN_TEST(namespaced_map_error_namespace_has_namespace);
    RUN_TEST(namespaced_map_error_not_followed_by_map);
    RUN_TEST(namespaced_map_duplicate_keys);
    RUN_TEST(namespaced_map_duplicate_keys_extra);
    RUN_TEST(namespaced_map_with_symbol_keys);
    RUN_TEST(namespaced_map_with_mixed_symbol_keyword_keys);
    RUN_TEST(namespaced_map_with_already_namespaced_symbol_keys);

    TEST_SUMMARY("namespaced map");
#else
    printf("Namespaced map syntax is disabled (EDN_ENABLE_MAP_NAMESPACE_SYNTAX not defined)\n");
    printf("Running disabled feature tests:\n");

    RUN_TEST(namespaced_map_syntax_disabled);

    TEST_SUMMARY("namespaced map (disabled)");
#endif
}
