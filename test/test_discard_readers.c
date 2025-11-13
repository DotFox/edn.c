/**
 * Test that readers are not invoked during discard (EDN spec requirement)
 */

#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Global flag to track if reader was called */
static bool reader_was_called = false;

/* Test reader that sets a flag when invoked */
static edn_value_t* tracking_reader(edn_value_t* value, edn_arena_t* arena,
                                    const char** error_message) {
    (void) arena;
    (void) error_message;
    reader_was_called = true;
    return value;
}

/* Test: Reader should NOT be called when discarded */
TEST(reader_not_called_in_discard) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Parse with discard - reader should NOT be called */
    edn_result_t result = edn_parse_with_options("[1 #_#test 42 3]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    /* Verify reader was NOT called */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Reader SHOULD be called when NOT discarded */
TEST(reader_called_normally) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Parse without discard - reader SHOULD be called */
    edn_result_t result = edn_parse_with_options("[1 #test 42 3]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    /* Verify reader WAS called */
    assert(reader_was_called == true);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Nested discard with tagged literals */
TEST(nested_discard_with_tagged) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "tag1", tracking_reader);
    edn_reader_register(registry, "tag2", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Parse with nested discard */
    edn_result_t result = edn_parse_with_options("[#_[#tag1 1 #tag2 2] 3]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_vector_count(result.value) == 1);

    /* Verify no readers were called */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Multiple discards with tagged literals */
TEST(multiple_discards_with_tagged) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Multiple discarded tagged literals */
    edn_result_t result = edn_parse_with_options("[1 #_#test 2 #_#test 3 4]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_vector_count(result.value) == 2);

    /* Verify reader was NOT called for any */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Discard nested tagged inside discard */
TEST(discard_nested_tagged) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "outer", tracking_reader);
    edn_reader_register(registry, "inner", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* #_#outer #inner 42 */
    edn_result_t result = edn_parse_with_options("[#_#outer #inner 42]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_vector_count(result.value) == 0);

    /* Verify no readers were called */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Tagged literal in map key position with discard */
TEST(discard_tagged_in_map) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "test", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Discard tagged literal in map - #test takes a value, so #_#test 42 discards "#test 42" */
    edn_result_t result = edn_parse_with_options("{:a 1 #_#test 42 :b 2}", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 2);

    /* Verify reader was NOT called */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

/* Test: Complex nested discard with multiple tagged literals in maps */
TEST(discard_complex_nested_map_with_tagged) {
    reader_was_called = false;

    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "tag1", tracking_reader);
    edn_reader_register(registry, "tag2", tracking_reader);
    edn_reader_register(registry, "tag3", tracking_reader);

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    /* Discard entire map with multiple tagged literals: #_ {:foo #tag1 0 :bar #_ {:baz #tag2 0} #tag3 0} */
    edn_result_t result = edn_parse_with_options(
        "[#_ {:foo #tag1 0 :bar #_ {:baz #tag2 0} #tag3 0} :result]", 0, &opts);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 1);

    /* Verify NO readers were called (tag1, tag2, tag3 all inside discarded forms) */
    assert(reader_was_called == false);

    edn_free(result.value);
    edn_reader_registry_destroy(registry);
}

int main(void) {
    printf("Running discard with readers tests...\n");

    RUN_TEST(reader_not_called_in_discard);
    RUN_TEST(reader_called_normally);
    RUN_TEST(nested_discard_with_tagged);
    RUN_TEST(multiple_discards_with_tagged);
    RUN_TEST(discard_nested_tagged);
    RUN_TEST(discard_tagged_in_map);
    RUN_TEST(discard_complex_nested_map_with_tagged);

    TEST_SUMMARY("discard with readers");
}
