/**
 * Test reader memory safety patterns
 */

#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Simple reader for testing */
static edn_value_t* test_reader(edn_value_t* value, edn_arena_t* arena,
                                const char** error_message) {
    (void) error_message;

    /* Allocate new value from arena */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    /* Copy input value type */
    result->type = value->type;
    result->as = value->as;
    result->arena = arena;

    return result;
}

/* Test: Registry can be destroyed before values are freed */
TEST(registry_destroyed_before_values) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", test_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#test 42", 0, &opts);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Destroy registry immediately after parsing */
    edn_reader_registry_destroy(registry);

    /* Values should still be usable after registry is destroyed */
    assert(edn_type(result.value) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(result.value, &val));
    assert(val == 42);

    /* Free arena last */
    edn_free(result.value);
}

/* Test: Multiple parses with same registry, destroy after all */
TEST(multiple_parses_one_registry) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", test_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* First parse */
    edn_result_t result1 = edn_parse_with_options("#test 10", 0, &opts);
    assert(result1.error == EDN_OK);

    /* Second parse (reusing registry) */
    edn_result_t result2 = edn_parse_with_options("#test 20", 0, &opts);
    assert(result2.error == EDN_OK);

    /* Third parse (reusing registry) */
    edn_result_t result3 = edn_parse_with_options("#test 30", 0, &opts);
    assert(result3.error == EDN_OK);

    /* Destroy registry after all parses */
    edn_reader_registry_destroy(registry);

    /* All values still usable */
    int64_t val1, val2, val3;
    assert(edn_int64_get(result1.value, &val1));
    assert(edn_int64_get(result2.value, &val2));
    assert(edn_int64_get(result3.value, &val3));

    assert(val1 == 10);
    assert(val2 == 20);
    assert(val3 == 30);

    /* Free arenas */
    edn_free(result1.value);
    edn_free(result2.value);
    edn_free(result3.value);
}

/* Test: Registry destroyed, then new registry created */
TEST(registry_recreated) {
    /* First registry */
    edn_reader_registry_t* registry1 = edn_reader_registry_create();
    assert(registry1 != NULL);

    edn_reader_register(registry1, "test", test_reader);

    edn_parse_options_t opts1 = {.reader_registry = registry1,
                                 .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result1 = edn_parse_with_options("#test 100", 0, &opts1);
    assert(result1.error == EDN_OK);

    /* Destroy first registry */
    edn_reader_registry_destroy(registry1);

    /* Create second registry (different address) */
    edn_reader_registry_t* registry2 = edn_reader_registry_create();
    assert(registry2 != NULL);

    edn_reader_register(registry2, "test", test_reader);

    edn_parse_options_t opts2 = {.reader_registry = registry2,
                                 .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result2 = edn_parse_with_options("#test 200", 0, &opts2);
    assert(result2.error == EDN_OK);

    /* Destroy second registry */
    edn_reader_registry_destroy(registry2);

    /* Both values still usable */
    int64_t val1, val2;
    assert(edn_int64_get(result1.value, &val1));
    assert(edn_int64_get(result2.value, &val2));

    assert(val1 == 100);
    assert(val2 == 200);

    /* Free arenas */
    edn_free(result1.value);
    edn_free(result2.value);
}

/* Reader that allocates a string in arena */
static edn_value_t* string_reader(edn_value_t* value, edn_arena_t* arena,
                                  const char** error_message) {
    (void) value;

    /* Allocate string from arena */
    const char* str = "test string";
    size_t len = strlen(str);
    char* arena_str = (char*) edn_arena_alloc(arena, len + 1);
    if (arena_str == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }
    memcpy(arena_str, str, len + 1);

    /* Allocate value from arena */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_STRING;
    result->as.string.data = arena_str;
    edn_string_set_length(result, len);
    edn_string_set_has_escapes(result, false);
    result->as.string.decoded = NULL;
    result->arena = arena;

    return result;
}

/* Test: Registry with string allocation from arena */
TEST(reader_string_allocation_safety) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "str", string_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("#str 42", 0, &opts);
    assert(result.error == EDN_OK);

    /* Destroy registry */
    edn_reader_registry_destroy(registry);

    /* String still usable (allocated from arena) */
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert(strcmp(str, "test string") == 0);

    /* Free arena */
    edn_free(result.value);
}

/* Test: Nested tagged literals with reader */
TEST(nested_tagged_reader_safety) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inner", test_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Outer tag has no reader, inner tag has reader */
    edn_result_t result = edn_parse_with_options("#outer #inner 42", 0, &opts);
    assert(result.error == EDN_OK);

    /* Destroy registry */
    edn_reader_registry_destroy(registry);

    /* Outer tagged literal is preserved */
    assert(edn_type(result.value) == EDN_TYPE_TAGGED);

    const char* tag;
    edn_value_t* wrapped;
    assert(edn_tagged_get(result.value, &tag, NULL, &wrapped));

    /* Inner value was transformed by reader */
    assert(edn_type(wrapped) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(wrapped, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* Test: Collection containing reader-transformed values */
TEST(collection_with_readers_safety) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", test_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result = edn_parse_with_options("[#test 1 #test 2 #test 3]", 0, &opts);
    assert(result.error == EDN_OK);

    /* Destroy registry */
    edn_reader_registry_destroy(registry);

    /* Collection and all elements still usable */
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    for (size_t i = 0; i < 3; i++) {
        edn_value_t* elem = edn_vector_get(result.value, i);
        assert(edn_type(elem) == EDN_TYPE_INT);

        int64_t val;
        assert(edn_int64_get(elem, &val));
        assert(val == (int64_t) (i + 1));
    }

    edn_free(result.value);
}

int main(void) {
    printf("Running reader memory safety tests...\n");

    RUN_TEST(registry_destroyed_before_values);
    RUN_TEST(multiple_parses_one_registry);
    RUN_TEST(registry_recreated);
    RUN_TEST(reader_string_allocation_safety);
    RUN_TEST(nested_tagged_reader_safety);
    RUN_TEST(collection_with_readers_safety);

    TEST_SUMMARY("reader memory safety");
}
