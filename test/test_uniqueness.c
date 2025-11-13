/**
 * Test uniqueness checking functions
 */

#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Helper to create test arrays */
static edn_value_t* parse_helper(const char* input) {
    edn_result_t result = edn_parse(input, 0);
    if (result.error != EDN_OK) {
        return NULL;
    }
    return result.value;
}

/* Empty array - no duplicates */
TEST(no_duplicates_empty) {
    edn_value_t** elements = NULL;
    assert(!edn_has_duplicates(elements, 0));
}

/* Single element - no duplicates */
TEST(no_duplicates_single) {
    edn_value_t* val = parse_helper("42");
    assert(val != NULL);

    edn_value_t* elements[] = {val};
    assert(!edn_has_duplicates(elements, 1));

    edn_free(val);
}

/* All unique elements - small (linear scan) */
TEST(no_duplicates_small_unique) {
    edn_value_t* vals[5];
    vals[0] = parse_helper("1");
    vals[1] = parse_helper("2");
    vals[2] = parse_helper("3");
    vals[3] = parse_helper("4");
    vals[4] = parse_helper("5");

    for (int i = 0; i < 5; i++) {
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, 5));

    for (int i = 0; i < 5; i++) {
        edn_free(vals[i]);
    }
}

/* Duplicate at start */
TEST(duplicate_at_start) {
    edn_value_t* vals[5];
    vals[0] = parse_helper("1");
    vals[1] = parse_helper("1"); /* Duplicate */
    vals[2] = parse_helper("2");
    vals[3] = parse_helper("3");
    vals[4] = parse_helper("4");

    for (int i = 0; i < 5; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 5));

    for (int i = 0; i < 5; i++) {
        edn_free(vals[i]);
    }
}

/* Duplicate at middle */
TEST(duplicate_at_middle) {
    edn_value_t* vals[5];
    vals[0] = parse_helper("1");
    vals[1] = parse_helper("2");
    vals[2] = parse_helper("3");
    vals[3] = parse_helper("2"); /* Duplicate of vals[1] */
    vals[4] = parse_helper("4");

    for (int i = 0; i < 5; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 5));

    for (int i = 0; i < 5; i++) {
        edn_free(vals[i]);
    }
}

/* Duplicate at end */
TEST(duplicate_at_end) {
    edn_value_t* vals[5];
    vals[0] = parse_helper("1");
    vals[1] = parse_helper("2");
    vals[2] = parse_helper("3");
    vals[3] = parse_helper("4");
    vals[4] = parse_helper("1"); /* Duplicate of vals[0] */

    for (int i = 0; i < 5; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 5));

    for (int i = 0; i < 5; i++) {
        edn_free(vals[i]);
    }
}

/* All identical elements */
TEST(all_identical) {
    edn_value_t* vals[4];
    vals[0] = parse_helper("42");
    vals[1] = parse_helper("42");
    vals[2] = parse_helper("42");
    vals[3] = parse_helper("42");

    for (int i = 0; i < 4; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 4));

    for (int i = 0; i < 4; i++) {
        edn_free(vals[i]);
    }
}

/* String duplicates */
TEST(duplicate_strings) {
    edn_value_t* vals[4];
    vals[0] = parse_helper("\"hello\"");
    vals[1] = parse_helper("\"world\"");
    vals[2] = parse_helper("\"hello\""); /* Duplicate */
    vals[3] = parse_helper("\"foo\"");

    for (int i = 0; i < 4; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 4));

    for (int i = 0; i < 4; i++) {
        edn_free(vals[i]);
    }
}

/* Keyword duplicates */
TEST(duplicate_keywords) {
    edn_value_t* vals[4];
    vals[0] = parse_helper(":foo");
    vals[1] = parse_helper(":bar");
    vals[2] = parse_helper(":baz");
    vals[3] = parse_helper(":foo"); /* Duplicate */

    for (int i = 0; i < 4; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 4));

    for (int i = 0; i < 4; i++) {
        edn_free(vals[i]);
    }
}

/* Symbol duplicates */
TEST(duplicate_symbols) {
    edn_value_t* vals[3];
    vals[0] = parse_helper("foo");
    vals[1] = parse_helper("bar");
    vals[2] = parse_helper("foo"); /* Duplicate */

    for (int i = 0; i < 3; i++) {
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, 3));

    for (int i = 0; i < 3; i++) {
        edn_free(vals[i]);
    }
}

/* Mixed types - all unique */
TEST(no_duplicates_mixed_types) {
    edn_value_t* vals[6];
    vals[0] = parse_helper("42");     /* int */
    vals[1] = parse_helper("\"42\""); /* string */
    vals[2] = parse_helper(":foo");   /* keyword */
    vals[3] = parse_helper("foo");    /* symbol */
    vals[4] = parse_helper("true");   /* boolean */
    vals[5] = parse_helper("3.14");   /* float */

    for (int i = 0; i < 6; i++) {
        assert(vals[i] != NULL);
    }

    /* All different types, no duplicates */
    assert(!edn_has_duplicates(vals, 6));

    for (int i = 0; i < 6; i++) {
        edn_free(vals[i]);
    }
}

/* Test medium size (triggers sorted algorithm) */
TEST(no_duplicates_medium_size) {
#define MEDIUM_SIZE 20
    edn_value_t* vals[MEDIUM_SIZE];

    /* Create unique values */
    for (int i = 0; i < MEDIUM_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, MEDIUM_SIZE));

    for (int i = 0; i < MEDIUM_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef MEDIUM_SIZE
}

TEST(duplicate_medium_size) {
#define MEDIUM_SIZE 20
    edn_value_t* vals[MEDIUM_SIZE];

    /* Create values with one duplicate */
    for (int i = 0; i < MEDIUM_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    /* Add duplicate of first element at the end */
    edn_free(vals[MEDIUM_SIZE - 1]);
    vals[MEDIUM_SIZE - 1] = parse_helper("0");
    assert(vals[MEDIUM_SIZE - 1] != NULL);

    assert(edn_has_duplicates(vals, MEDIUM_SIZE));

    for (int i = 0; i < MEDIUM_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef MEDIUM_SIZE
}

/* Test boundary between linear and sorted (16 elements) */
TEST(boundary_linear_sorted_unique) {
#define BOUNDARY_SIZE 16
    edn_value_t* vals[BOUNDARY_SIZE];

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, BOUNDARY_SIZE));

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef BOUNDARY_SIZE
}

TEST(boundary_linear_sorted_duplicate) {
#define BOUNDARY_SIZE 16
    edn_value_t* vals[BOUNDARY_SIZE];

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    /* Add duplicate */
    edn_free(vals[BOUNDARY_SIZE - 1]);
    vals[BOUNDARY_SIZE - 1] = parse_helper("0");
    assert(vals[BOUNDARY_SIZE - 1] != NULL);

    assert(edn_has_duplicates(vals, BOUNDARY_SIZE));

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef BOUNDARY_SIZE
}

/* Test large size (triggers hash algorithm) */
TEST(no_duplicates_large_size) {
#define LARGE_SIZE 2000
    edn_value_t* vals[LARGE_SIZE];

    /* Create unique values */
    for (int i = 0; i < LARGE_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, LARGE_SIZE));

    for (int i = 0; i < LARGE_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef LARGE_SIZE
}

TEST(duplicate_large_size) {
#define LARGE_SIZE 2000
    edn_value_t* vals[LARGE_SIZE];

    /* Create values with one duplicate */
    for (int i = 0; i < LARGE_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    /* Add duplicate of first element at the end */
    edn_free(vals[LARGE_SIZE - 1]);
    vals[LARGE_SIZE - 1] = parse_helper("0");
    assert(vals[LARGE_SIZE - 1] != NULL);

    assert(edn_has_duplicates(vals, LARGE_SIZE));

    for (int i = 0; i < LARGE_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef LARGE_SIZE
}

TEST(duplicate_large_size_middle) {
#define LARGE_SIZE 2000
    edn_value_t* vals[LARGE_SIZE];

    /* Create values with duplicate in middle */
    for (int i = 0; i < LARGE_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    /* Add duplicate in middle */
    edn_free(vals[LARGE_SIZE / 2]);
    vals[LARGE_SIZE / 2] = parse_helper("0");
    assert(vals[LARGE_SIZE / 2] != NULL);

    assert(edn_has_duplicates(vals, LARGE_SIZE));

    for (int i = 0; i < LARGE_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef LARGE_SIZE
}

/* Test boundary between sorted and hash (1000 elements) */
TEST(boundary_sorted_hash_unique) {
#define BOUNDARY_SIZE 1000
    edn_value_t* vals[BOUNDARY_SIZE];

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, BOUNDARY_SIZE));

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef BOUNDARY_SIZE
}

TEST(boundary_sorted_hash_duplicate) {
#define BOUNDARY_SIZE 1000
    edn_value_t* vals[BOUNDARY_SIZE];

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", i);
        vals[i] = parse_helper(buf);
        assert(vals[i] != NULL);
    }

    /* Add duplicate */
    edn_free(vals[BOUNDARY_SIZE - 1]);
    vals[BOUNDARY_SIZE - 1] = parse_helper("0");
    assert(vals[BOUNDARY_SIZE - 1] != NULL);

    assert(edn_has_duplicates(vals, BOUNDARY_SIZE));

    for (int i = 0; i < BOUNDARY_SIZE; i++) {
        edn_free(vals[i]);
    }
#undef BOUNDARY_SIZE
}

/* Test hash table with complex types */
TEST(hash_large_strings) {
#define LARGE_SIZE 1500
    /* Build vector with all strings in one parse/arena */
    char input[65536];
    int pos = 0;
    pos += snprintf(input + pos, sizeof(input) - pos, "[");
    for (int i = 0; i < LARGE_SIZE; i++) {
        if (i > 0) {
            pos += snprintf(input + pos, sizeof(input) - pos, " ");
        }
        pos += snprintf(input + pos, sizeof(input) - pos, "\"string_number_%d\"", i);
    }
    pos += snprintf(input + pos, sizeof(input) - pos, "]");

    edn_result_t result = edn_parse(input, 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    size_t count = edn_vector_count(result.value);
    assert(count == LARGE_SIZE);

    edn_value_t* vals[LARGE_SIZE];
    for (int i = 0; i < LARGE_SIZE; i++) {
        vals[i] = edn_vector_get(result.value, i);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, LARGE_SIZE));

    edn_free(result.value);
#undef LARGE_SIZE
}

TEST(hash_large_keywords) {
#define LARGE_SIZE 1500
    /* Build vector with all keywords in one parse/arena */
    char input[65536];
    int pos = 0;
    pos += snprintf(input + pos, sizeof(input) - pos, "[");
    for (int i = 0; i < LARGE_SIZE; i++) {
        if (i > 0) {
            pos += snprintf(input + pos, sizeof(input) - pos, " ");
        }
        pos += snprintf(input + pos, sizeof(input) - pos, ":keyword%d", i);
    }
    pos += snprintf(input + pos, sizeof(input) - pos, "]");

    edn_result_t result = edn_parse(input, 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    size_t count = edn_vector_count(result.value);
    assert(count == LARGE_SIZE);

    edn_value_t* vals[LARGE_SIZE];
    for (int i = 0; i < LARGE_SIZE; i++) {
        vals[i] = edn_vector_get(result.value, i);
        assert(vals[i] != NULL);
    }

    assert(!edn_has_duplicates(vals, LARGE_SIZE));

    edn_free(result.value);
#undef LARGE_SIZE
}

TEST(hash_large_keywords_with_duplicate) {
#define LARGE_SIZE 1500
    /* Build vector with all keywords in one parse/arena */
    char input[65536];
    int pos = 0;
    pos += snprintf(input + pos, sizeof(input) - pos, "[");
    for (int i = 0; i < LARGE_SIZE; i++) {
        if (i > 0) {
            pos += snprintf(input + pos, sizeof(input) - pos, " ");
        }
        pos += snprintf(input + pos, sizeof(input) - pos, ":keyword%d", i);
    }
    /* Add duplicate of first keyword */
    pos += snprintf(input + pos, sizeof(input) - pos, " :keyword0");
    pos += snprintf(input + pos, sizeof(input) - pos, "]");

    edn_result_t result = edn_parse(input, 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    size_t count = edn_vector_count(result.value);
    assert(count == LARGE_SIZE + 1);

    edn_value_t* vals[LARGE_SIZE + 1];
    for (int i = 0; i <= LARGE_SIZE; i++) {
        vals[i] = edn_vector_get(result.value, i);
        assert(vals[i] != NULL);
    }

    assert(edn_has_duplicates(vals, LARGE_SIZE + 1));

    edn_free(result.value);
#undef LARGE_SIZE
}

int main(void) {
    printf("Running uniqueness tests...\n");

    RUN_TEST(no_duplicates_empty);
    RUN_TEST(no_duplicates_single);
    RUN_TEST(no_duplicates_small_unique);
    RUN_TEST(duplicate_at_start);
    RUN_TEST(duplicate_at_middle);
    RUN_TEST(duplicate_at_end);
    RUN_TEST(all_identical);
    RUN_TEST(duplicate_strings);
    RUN_TEST(duplicate_keywords);
    RUN_TEST(duplicate_symbols);
    RUN_TEST(no_duplicates_mixed_types);
    RUN_TEST(no_duplicates_medium_size);
    RUN_TEST(duplicate_medium_size);
    RUN_TEST(boundary_linear_sorted_unique);
    RUN_TEST(boundary_linear_sorted_duplicate);
    RUN_TEST(no_duplicates_large_size);
    RUN_TEST(duplicate_large_size);
    RUN_TEST(duplicate_large_size_middle);
    RUN_TEST(boundary_sorted_hash_unique);
    RUN_TEST(boundary_sorted_hash_duplicate);
    RUN_TEST(hash_large_strings);
    RUN_TEST(hash_large_keywords);
    RUN_TEST(hash_large_keywords_with_duplicate);

    TEST_SUMMARY("uniqueness");
}
