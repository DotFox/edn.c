/**
 * Test parser depth tracking
 */

#include <stdlib.h>
#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Helper macro to initialize parser for testing */
#define INIT_PARSER(parser, input_str)                                 \
    do {                                                               \
        (parser).input = (input_str);                                  \
        (parser).current = (input_str);                                \
        (parser).end = (input_str) + strlen(input_str);                \
        (parser).depth = 0;                                            \
        (parser).max_depth = EDN_DEFAULT_MAX_DEPTH;                    \
        (parser).arena = edn_arena_create();                           \
        (parser).error = EDN_OK;                                       \
        (parser).error_message = NULL;                                 \
        (parser).reader_registry = NULL;                               \
        (parser).default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH; \
        (parser).discard_mode = false;                                 \
    } while (0)

/* Test: depth is 0 at initialization */
TEST(depth_initial) {
    edn_parser_t parser;
    const char* input = "[1 2 3]";
    INIT_PARSER(parser, input);

    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing simple values */
TEST(depth_simple_values) {
    edn_parser_t parser;
    const char* input = "42";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing a single-level collection */
TEST(depth_single_level) {
    edn_parser_t parser;
    const char* input = "[1 2 3]";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_VECTOR);
    assert(parser.depth == 0); /* Should return to 0 after complete parse */

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing nested collections */
TEST(depth_nested) {
    edn_parser_t parser;
    const char* input = "[1 [2 [3 [4]]]]";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_VECTOR);
    assert(parser.depth == 0); /* Should return to 0 after complete parse */

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing list */
TEST(depth_list) {
    edn_parser_t parser;
    const char* input = "(1 2 3)";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_LIST);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing map */
TEST(depth_map) {
    edn_parser_t parser;
    const char* input = "{:a 1 :b 2}";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_MAP);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing set */
TEST(depth_set) {
    edn_parser_t parser;
    const char* input = "#{1 2 3}";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_SET);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing mixed nested collections */
TEST(depth_mixed_nested) {
    edn_parser_t parser;
    const char* input = "{:list (1 2 3) :vector [4 5 6] :set #{7 8 9}}";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_MAP);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored to 0 even on parse errors */
TEST(depth_error_restoration) {
    edn_parser_t parser;
    const char* input = "[1 2"; /* Unterminated vector */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert(parser.depth == 0); /* Depth should be restored even on error */

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored on nested error */
TEST(depth_nested_error_restoration) {
    edn_parser_t parser;
    const char* input = "[[1 [2"; /* Deeply unterminated */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert(parser.depth == 0); /* Depth should be restored even on error */

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored on map error */
TEST(depth_map_error_restoration) {
    edn_parser_t parser;
    const char* input = "{:a 1 :b"; /* Map with missing value */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert(parser.depth == 0); /* Depth should be restored even on error */

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing tagged literal */
TEST(depth_tagged) {
    edn_parser_t parser;
    const char* input = "#inst \"2024-01-01\"";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_TAGGED);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing nested tagged literal */
TEST(depth_nested_tagged) {
    edn_parser_t parser;
    const char* input = "#outer #inner [1 2 3]";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_TAGGED);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth is 0 after parsing tagged with collection value */
TEST(depth_tagged_with_collection) {
    edn_parser_t parser;
    const char* input = "#myapp/custom {:data [1 2 3]}";
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value != NULL);
    assert(value->type == EDN_TYPE_TAGGED);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored on tagged literal error (missing tag) */
TEST(depth_tagged_error_missing_tag) {
    edn_parser_t parser;
    const char* input = "#"; /* Just # without tag */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored on tagged literal error (missing value) */
TEST(depth_tagged_error_missing_value) {
    edn_parser_t parser;
    const char* input = "#inst"; /* Tag without value */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: depth restored on tagged literal error (invalid tag) */
TEST(depth_tagged_error_invalid_tag) {
    edn_parser_t parser;
    const char* input = "#:keyword \"value\""; /* Keyword instead of symbol */
    INIT_PARSER(parser, input);

    edn_value_t* value = edn_read_value(&parser);
    assert(value == NULL); /* Should fail */
    assert(parser.error == EDN_ERROR_INVALID_SYNTAX);
    assert(parser.depth == 0);

    edn_arena_destroy(parser.arena);
}

/* Test: deeply nested input is rejected with EDN_ERROR_MAX_DEPTH_EXCEEDED
 * instead of overflowing the C stack. */
TEST(depth_cap_enforced) {
    enum { N = 2000 };
    char* input = malloc(2 * N + 2);
    assert(input != NULL);
    for (int i = 0; i < N; i++) {
        input[i] = '[';
    }
    for (int i = 0; i < N; i++) {
        input[N + i] = ']';
    }
    input[2 * N] = '\0';

    edn_result_t result = edn_read(input, 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_MAX_DEPTH_EXCEEDED);
    free(input);
}

/* Test: chained discard cannot blow the stack. */
TEST(depth_cap_discard) {
    enum { N = 4000 };
    char* input = malloc(2 * N + 2);
    assert(input != NULL);
    for (int i = 0; i < N; i++) {
        input[2 * i] = '#';
        input[2 * i + 1] = '_';
    }
    input[2 * N] = '1';
    input[2 * N + 1] = '\0';

    edn_result_t result = edn_read(input, 0);
    /* Should fail with depth-exceeded error (or in-flight error), never crash. */
    assert(result.error == EDN_ERROR_MAX_DEPTH_EXCEEDED);
    free(input);
}

int main(void) {
    printf("Running parser depth tracking tests...\n");

    RUN_TEST(depth_initial);
    RUN_TEST(depth_simple_values);
    RUN_TEST(depth_single_level);
    RUN_TEST(depth_nested);
    RUN_TEST(depth_list);
    RUN_TEST(depth_map);
    RUN_TEST(depth_set);
    RUN_TEST(depth_mixed_nested);
    RUN_TEST(depth_error_restoration);
    RUN_TEST(depth_nested_error_restoration);
    RUN_TEST(depth_map_error_restoration);
    RUN_TEST(depth_tagged);
    RUN_TEST(depth_nested_tagged);
    RUN_TEST(depth_tagged_with_collection);
    RUN_TEST(depth_tagged_error_missing_tag);
    RUN_TEST(depth_tagged_error_missing_value);
    RUN_TEST(depth_tagged_error_invalid_tag);
    RUN_TEST(depth_cap_enforced);
    RUN_TEST(depth_cap_discard);

    TEST_SUMMARY("parser depth tracking");
}
