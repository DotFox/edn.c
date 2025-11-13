/**
 * Test reader registry operations
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Dummy reader functions for testing */
static edn_value_t* dummy_reader_1(edn_value_t* value, edn_arena_t* arena,
                                   const char** error_message) {
    (void) arena;
    (void) error_message;
    return value; /* Just return the value unchanged */
}

static edn_value_t* dummy_reader_2(edn_value_t* value, edn_arena_t* arena,
                                   const char** error_message) {
    (void) value;
    (void) arena;
    (void) error_message;
    return NULL; /* Always fail */
}

/* Test registry creation and destruction */
TEST(create_destroy_registry) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_registry_destroy(registry);
    /* No crash = success */
}

/* Test destroy with NULL */
TEST(destroy_null_registry) {
    edn_reader_registry_destroy(NULL);
    /* No crash = success */
}

/* Test registering a reader */
TEST(register_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    bool result = edn_reader_register(registry, "inst", dummy_reader_1);
    assert(result == true);

    edn_reader_registry_destroy(registry);
}

/* Test looking up registered reader */
TEST(lookup_registered_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inst", dummy_reader_1);

    edn_reader_fn found = edn_reader_lookup(registry, "inst");
    assert(found == dummy_reader_1);

    edn_reader_registry_destroy(registry);
}

/* Test looking up non-existent reader */
TEST(lookup_missing_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_fn found = edn_reader_lookup(registry, "unknown");
    assert(found == NULL);

    edn_reader_registry_destroy(registry);
}

/* Test looking up after registering different tag */
TEST(lookup_different_tag) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inst", dummy_reader_1);

    edn_reader_fn found = edn_reader_lookup(registry, "uuid");
    assert(found == NULL);

    edn_reader_registry_destroy(registry);
}

/* Test replacing existing reader */
TEST(replace_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inst", dummy_reader_1);
    edn_reader_register(registry, "inst", dummy_reader_2);

    edn_reader_fn found = edn_reader_lookup(registry, "inst");
    assert(found == dummy_reader_2); /* Should be replaced */

    edn_reader_registry_destroy(registry);
}

/* Test multiple readers */
TEST(multiple_readers) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inst", dummy_reader_1);
    edn_reader_register(registry, "uuid", dummy_reader_2);

    edn_reader_fn found1 = edn_reader_lookup(registry, "inst");
    edn_reader_fn found2 = edn_reader_lookup(registry, "uuid");

    assert(found1 == dummy_reader_1);
    assert(found2 == dummy_reader_2);

    edn_reader_registry_destroy(registry);
}

/* Test unregistering a reader */
TEST(unregister_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "inst", dummy_reader_1);

    edn_reader_fn found = edn_reader_lookup(registry, "inst");
    assert(found == dummy_reader_1);

    edn_reader_unregister(registry, "inst");

    found = edn_reader_lookup(registry, "inst");
    assert(found == NULL);

    edn_reader_registry_destroy(registry);
}

/* Test unregistering non-existent reader */
TEST(unregister_missing_reader) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_unregister(registry, "unknown");
    /* No crash = success */

    edn_reader_registry_destroy(registry);
}

/* Test namespaced tags */
TEST(namespaced_tags) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    edn_reader_register(registry, "myapp/custom", dummy_reader_1);

    edn_reader_fn found = edn_reader_lookup(registry, "myapp/custom");
    assert(found == dummy_reader_1);

    edn_reader_registry_destroy(registry);
}

/* Test long tag names */
TEST(long_tag_names) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    const char* long_tag = "myapp/very/long/namespaced/tag/name";

    edn_reader_register(registry, long_tag, dummy_reader_1);

    edn_reader_fn found = edn_reader_lookup(registry, long_tag);
    assert(found == dummy_reader_1);

    edn_reader_registry_destroy(registry);
}

/* Test many readers (hash table collision handling) */
TEST(many_readers) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    /* Register 20 readers */
    const char* tags[] = {"tag00", "tag01", "tag02", "tag03", "tag04", "tag05", "tag06",
                          "tag07", "tag08", "tag09", "tag10", "tag11", "tag12", "tag13",
                          "tag14", "tag15", "tag16", "tag17", "tag18", "tag19"};

    for (int i = 0; i < 20; i++) {
        bool result = edn_reader_register(registry, tags[i], dummy_reader_1);
        assert(result == true);
    }

    /* Verify all are present */
    for (int i = 0; i < 20; i++) {
        edn_reader_fn found = edn_reader_lookup(registry, tags[i]);
        assert(found == dummy_reader_1);
    }

    edn_reader_registry_destroy(registry);
}

/* Test NULL parameter handling */
TEST(null_parameter_handling) {
    edn_reader_registry_t* registry = edn_reader_registry_create();
    assert(registry != NULL);

    /* Register with NULL registry */
    bool result = edn_reader_register(NULL, "inst", dummy_reader_1);
    assert(result == false);

    /* Register with NULL tag */
    result = edn_reader_register(registry, NULL, dummy_reader_1);
    assert(result == false);

    /* Register with NULL reader */
    result = edn_reader_register(registry, "inst", NULL);
    assert(result == false);

    /* Lookup with NULL registry */
    edn_reader_fn found = edn_reader_lookup(NULL, "inst");
    assert(found == NULL);

    /* Lookup with NULL tag */
    found = edn_reader_lookup(registry, NULL);
    assert(found == NULL);

    /* Unregister with NULL registry */
    edn_reader_unregister(NULL, "inst");
    /* No crash = success */

    /* Unregister with NULL tag */
    edn_reader_unregister(registry, NULL);
    /* No crash = success */

    edn_reader_registry_destroy(registry);
}

int main(void) {
    printf("Running reader registry tests...\n");

    RUN_TEST(create_destroy_registry);
    RUN_TEST(destroy_null_registry);
    RUN_TEST(register_reader);
    RUN_TEST(lookup_registered_reader);
    RUN_TEST(lookup_missing_reader);
    RUN_TEST(lookup_different_tag);
    RUN_TEST(replace_reader);
    RUN_TEST(multiple_readers);
    RUN_TEST(unregister_reader);
    RUN_TEST(unregister_missing_reader);
    RUN_TEST(namespaced_tags);
    RUN_TEST(long_tag_names);
    RUN_TEST(many_readers);
    RUN_TEST(null_parameter_handling);

    TEST_SUMMARY("reader registry");
}
