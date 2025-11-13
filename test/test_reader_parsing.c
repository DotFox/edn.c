/**
 * Test reader function invocation during parsing
 */

#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Reader that converts #double to actual double */
static edn_value_t* double_reader(edn_value_t* value, edn_arena_t* arena,
                                  const char** error_message) {
    /* Expect integer input */
    if (edn_type(value) != EDN_TYPE_INT) {
        *error_message = "#double requires integer value";
        return NULL;
    }

    int64_t int_val;
    edn_int64_get(value, &int_val);

    /* Create double value */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_FLOAT;
    result->as.floating = (double) int_val;
    result->arena = arena;

    return result;
}

/* Reader that always fails */
static edn_value_t* failing_reader(edn_value_t* value, edn_arena_t* arena,
                                   const char** error_message) {
    (void) value;
    (void) arena;
    *error_message = "This reader always fails";
    return NULL;
}

/* Reader that unwraps vector and returns first element */
static edn_value_t* first_reader(edn_value_t* value, edn_arena_t* arena,
                                 const char** error_message) {
    (void) arena;

    if (edn_type(value) != EDN_TYPE_VECTOR) {
        *error_message = "#first requires vector value";
        return NULL;
    }

    if (edn_vector_count(value) == 0) {
        *error_message = "#first requires non-empty vector";
        return NULL;
    }

    return edn_vector_get(value, 0);
}

/* Test parsing with registered reader */
TEST(parse_with_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#double 42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    double val;
    assert(edn_double_get(result.value, &val));
    assert(val == 42.0);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test parsing without reader (passthrough mode) */
TEST(parse_without_reader_passthrough) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#unknown 42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    size_t tag_len;
    edn_value_t* wrapped_value;

    assert(edn_tagged_get(result.value, &tag, &tag_len, &wrapped_value));
    assert(tag_len == 7);
    assert(memcmp(tag, "unknown", 7) == 0);

    assert(edn_type(wrapped_value) == EDN_TYPE_INT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test parsing without reader (unwrap mode) */
TEST(parse_without_reader_unwrap) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_UNWRAP};

    edn_result_t result = edn_parse_with_options("#unknown 42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(result.value, &val));
    assert(val == 42);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test parsing without reader (error mode) */
TEST(parse_without_reader_error) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_ERROR};

    edn_result_t result = edn_parse_with_options("#unknown 42", 0, &opts);

    assert(result.error == EDN_ERROR_UNKNOWN_TAG);
    assert(result.value == NULL);
    assert(result.error_message != NULL);

    edn_reader_registry_destroy(registry);
}

/* Test reader that fails */
TEST(parse_with_failing_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "fail", failing_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#fail 42", 0, &opts);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
    assert(strstr(result.error_message, "always fails") != NULL);

    edn_reader_registry_destroy(registry);
}

/* Test reader with wrong type */
TEST(parse_reader_wrong_type) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Reader expects int, but we give it a string */
    edn_result_t result = edn_parse_with_options("#double \"not an int\"", 0, &opts);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);

    edn_reader_registry_destroy(registry);
}

/* Test multiple readers in same parse */
TEST(parse_multiple_readers) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);
    edn_reader_register(registry, "first", first_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("[#double 10 #double 20]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    /* Both elements should be doubles */
    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    edn_value_t* elem1 = edn_vector_get(result.value, 1);

    assert(edn_type(elem0) == EDN_TYPE_FLOAT);
    assert(edn_type(elem1) == EDN_TYPE_FLOAT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test reader with nested collection */
TEST(parse_reader_with_collection) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "first", first_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#first [1 2 3]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(result.value, &val));
    assert(val == 1);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test nested tagged literals with readers */
TEST(parse_nested_tagged_with_readers) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Outer tag has no reader, inner tag has reader */
    edn_result_t result = edn_parse_with_options("#outer #double 42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* wrapped;

    assert(edn_tagged_get(result.value, &tag, NULL, &wrapped));
    assert(memcmp(tag, "outer", 5) == 0);

    /* Inner value should be a double (reader was applied) */
    assert(edn_type(wrapped) == EDN_TYPE_FLOAT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test parsing with NULL options (default behavior) */
TEST(parse_with_null_options) {
    edn_result_t result = edn_parse_with_options("#inst \"2024-01-01\"", 0, NULL);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Test that edn_parse still works (backward compatibility) */
TEST(parse_backward_compatible) {
    edn_result_t result = edn_parse("#inst \"2024-01-01\"", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    edn_free(result.value);
}

/* Test reader with namespaced tag */
TEST(parse_namespaced_tag_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "myapp/double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#myapp/double 42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test reader in map */
TEST(parse_reader_in_map) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("{:value #double 42}", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_value_t* value = edn_map_get_value(result.value, 0);
    assert(edn_type(value) == EDN_TYPE_FLOAT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test reader with whitespace */
TEST(parse_reader_with_whitespace) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "double", double_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#double   42", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_FLOAT);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

int main(void) {
    printf("Running reader parsing tests...\n");

    RUN_TEST(parse_with_reader);
    RUN_TEST(parse_without_reader_passthrough);
    RUN_TEST(parse_without_reader_unwrap);
    RUN_TEST(parse_without_reader_error);
    RUN_TEST(parse_with_failing_reader);
    RUN_TEST(parse_reader_wrong_type);
    RUN_TEST(parse_multiple_readers);
    RUN_TEST(parse_reader_with_collection);
    RUN_TEST(parse_nested_tagged_with_readers);
    RUN_TEST(parse_with_null_options);
    RUN_TEST(parse_backward_compatible);
    RUN_TEST(parse_namespaced_tag_reader);
    RUN_TEST(parse_reader_in_map);
    RUN_TEST(parse_reader_with_whitespace);

    TEST_SUMMARY("reader parsing");
}
