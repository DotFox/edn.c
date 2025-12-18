/**
 * Test value equality and comparison functions
 */

#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Helper function to create values for testing */
static edn_value_t* parse_helper(const char* input) {
    edn_result_t result = edn_read(input, 0);
    if (result.error != EDN_OK) {
        return NULL;
    }
    return result.value;
}

/* Nil equality */
TEST(equal_nil) {
    edn_value_t* a = parse_helper("nil");
    edn_value_t* b = parse_helper("nil");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));
    assert(edn_value_equal(a, a)); /* Self-equality */

    edn_free(a);
    edn_free(b);
}

/* Boolean equality */
TEST(equal_bool_true) {
    edn_value_t* a = parse_helper("true");
    edn_value_t* b = parse_helper("true");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_bool_false) {
    edn_value_t* a = parse_helper("false");
    edn_value_t* b = parse_helper("false");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_bool) {
    edn_value_t* a = parse_helper("true");
    edn_value_t* b = parse_helper("false");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Integer equality */
TEST(equal_int) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("42");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_int) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("43");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_negative_int) {
    edn_value_t* a = parse_helper("-123");
    edn_value_t* b = parse_helper("-123");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Float equality */
TEST(equal_float) {
    edn_value_t* a = parse_helper("3.14");
    edn_value_t* b = parse_helper("3.14");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_float) {
    edn_value_t* a = parse_helper("3.14");
    edn_value_t* b = parse_helper("3.15");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_nan) {
    edn_value_t* a = parse_helper("##NaN");
    edn_value_t* b = parse_helper("##NaN");

    assert(a != NULL);
    assert(b != NULL);
    /* NaN == NaN in EDN semantics */
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_infinity) {
    edn_value_t* a = parse_helper("##Inf");
    edn_value_t* b = parse_helper("##Inf");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_inf_neg_inf) {
    edn_value_t* a = parse_helper("##Inf");
    edn_value_t* b = parse_helper("##-Inf");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Character equality */
TEST(equal_character) {
    edn_value_t* a = parse_helper("\\a");
    edn_value_t* b = parse_helper("\\a");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_character) {
    edn_value_t* a = parse_helper("\\a");
    edn_value_t* b = parse_helper("\\b");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_unicode_character) {
    edn_value_t* a = parse_helper("\\u03B1");
    edn_value_t* b = parse_helper("\\u03B1");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* String equality */
TEST(equal_string) {
    edn_value_t* a = parse_helper("\"hello\"");
    edn_value_t* b = parse_helper("\"hello\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_string) {
    edn_value_t* a = parse_helper("\"hello\"");
    edn_value_t* b = parse_helper("\"world\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_string_with_escapes) {
    edn_value_t* a = parse_helper("\"hello\\nworld\"");
    edn_value_t* b = parse_helper("\"hello\\nworld\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_empty_string) {
    edn_value_t* a = parse_helper("\"\"");
    edn_value_t* b = parse_helper("\"\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Symbol equality */
TEST(equal_symbol) {
    edn_value_t* a = parse_helper("foo");
    edn_value_t* b = parse_helper("foo");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_symbol) {
    edn_value_t* a = parse_helper("foo");
    edn_value_t* b = parse_helper("bar");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_symbol_with_namespace) {
    edn_value_t* a = parse_helper("ns/foo");
    edn_value_t* b = parse_helper("ns/foo");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_symbol_different_namespace) {
    edn_value_t* a = parse_helper("ns1/foo");
    edn_value_t* b = parse_helper("ns2/foo");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Keyword equality */
TEST(equal_keyword) {
    edn_value_t* a = parse_helper(":foo");
    edn_value_t* b = parse_helper(":foo");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_keyword) {
    edn_value_t* a = parse_helper(":foo");
    edn_value_t* b = parse_helper(":bar");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_keyword_with_namespace) {
    edn_value_t* a = parse_helper(":ns/foo");
    edn_value_t* b = parse_helper(":ns/foo");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Type mismatch */
TEST(not_equal_different_types) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("\"42\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_int_float) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("42.0");

    assert(a != NULL);
    assert(b != NULL);
    /* Different types, not equal even if numerically same */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* NULL checks */
TEST(equal_null_null) {
    assert(edn_value_equal(NULL, NULL));
}

TEST(not_equal_value_null) {
    edn_value_t* a = parse_helper("42");

    assert(a != NULL);
    assert(!edn_value_equal(a, NULL));
    assert(!edn_value_equal(NULL, a));

    edn_free(a);
}

/* Comparison function tests */
TEST(compare_same_values) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("42");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_compare(&a, &b) == 0);

    edn_free(a);
    edn_free(b);
}

TEST(compare_different_ints) {
    edn_value_t* a = parse_helper("10");
    edn_value_t* b = parse_helper("20");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_compare(&a, &b) < 0);
    assert(edn_value_compare(&b, &a) > 0);

    edn_free(a);
    edn_free(b);
}

TEST(compare_strings) {
    edn_value_t* a = parse_helper("\"apple\"");
    edn_value_t* b = parse_helper("\"banana\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_compare(&a, &b) < 0);
    assert(edn_value_compare(&b, &a) > 0);

    edn_free(a);
    edn_free(b);
}

/* Hash function tests */
TEST(hash_equal_values_same_hash) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("42");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    assert(hash_a == hash_b);

    edn_free(a);
    edn_free(b);
}

TEST(hash_different_values_different_hash) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("43");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    /* Different values should (usually) have different hashes */
    /* Note: hash collisions are possible, but very unlikely for these values */
    assert(hash_a != hash_b);

    edn_free(a);
    edn_free(b);
}

TEST(hash_nan_deterministic) {
    edn_value_t* a = parse_helper("##NaN");
    edn_value_t* b = parse_helper("##NaN");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    /* NaN values should have same hash */
    assert(hash_a == hash_b);

    edn_free(a);
    edn_free(b);
}

TEST(hash_null) {
    uint64_t hash = edn_value_hash(NULL);
    /* Should not crash and return some value */
    (void) hash;
}

/* Set order-independence tests */
TEST(equal_set_same_order) {
    edn_value_t* a = parse_helper("#{1 2 3}");
    edn_value_t* b = parse_helper("#{1 2 3}");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_set_different_order) {
    edn_value_t* a = parse_helper("#{1 2 3}");
    edn_value_t* b = parse_helper("#{3 2 1}");

    assert(a != NULL);
    assert(b != NULL);
    /* Sets are order-independent: #{1 2 3} == #{3 2 1} */
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_set_different_elements) {
    edn_value_t* a = parse_helper("#{1 2 3}");
    edn_value_t* b = parse_helper("#{1 2 4}");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(hash_set_order_independent) {
    edn_value_t* a = parse_helper("#{1 2 3}");
    edn_value_t* b = parse_helper("#{3 2 1}");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    /* Order-independent sets should have same hash */
    assert(hash_a == hash_b);

    edn_free(a);
    edn_free(b);
}

/* List equality - order matters */
TEST(equal_list_same_order) {
    edn_value_t* a = parse_helper("(1 2 3)");
    edn_value_t* b = parse_helper("(1 2 3)");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_list_different_order) {
    edn_value_t* a = parse_helper("(1 2 3)");
    edn_value_t* b = parse_helper("(3 2 1)");

    assert(a != NULL);
    assert(b != NULL);
    /* Lists are order-dependent: (1 2 3) != (3 2 1) */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_list_different_length) {
    edn_value_t* a = parse_helper("(1 2 3)");
    edn_value_t* b = parse_helper("(1 2)");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_empty_lists) {
    edn_value_t* a = parse_helper("()");
    edn_value_t* b = parse_helper("()");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Vector equality - order matters */
TEST(equal_vector_same_order) {
    edn_value_t* a = parse_helper("[1 2 3]");
    edn_value_t* b = parse_helper("[1 2 3]");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_vector_different_order) {
    edn_value_t* a = parse_helper("[1 2 3]");
    edn_value_t* b = parse_helper("[3 2 1]");

    assert(a != NULL);
    assert(b != NULL);
    /* Vectors are order-dependent: [1 2 3] != [3 2 1] */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_empty_vectors) {
    edn_value_t* a = parse_helper("[]");
    edn_value_t* b = parse_helper("[]");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_list_vs_vector) {
    edn_value_t* a = parse_helper("(1 2 3)");
    edn_value_t* b = parse_helper("[1 2 3]");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_list_vs_vector) {
    edn_value_t* a = parse_helper("(3 2 1)");
    edn_value_t* b = parse_helper("[1 2 3]");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_empty_list_vs_vector) {
    edn_value_t* a = parse_helper("()");
    edn_value_t* b = parse_helper("[]");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Map equality - order independent */
TEST(equal_map_same_order) {
    edn_value_t* a = parse_helper("{:a 1 :b 2 :c 3}");
    edn_value_t* b = parse_helper("{:a 1 :b 2 :c 3}");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_map_different_order) {
    edn_value_t* a = parse_helper("{:a 1 :b 2}");
    edn_value_t* b = parse_helper("{:b 2 :a 1}");

    assert(a != NULL);
    assert(b != NULL);
    /* Maps are order-independent */
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_map_different_keys) {
    edn_value_t* a = parse_helper("{:a 1 :b 2}");
    edn_value_t* b = parse_helper("{:a 1 :c 2}");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_map_different_values) {
    edn_value_t* a = parse_helper("{:a 1 :b 2}");
    edn_value_t* b = parse_helper("{:a 1 :b 3}");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_empty_maps) {
    edn_value_t* a = parse_helper("{}");
    edn_value_t* b = parse_helper("{}");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(hash_map_order_independent) {
    edn_value_t* a = parse_helper("{:a 1 :b 2}");
    edn_value_t* b = parse_helper("{:b 2 :a 1}");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    /* Order-independent maps should have same hash */
    assert(hash_a == hash_b);

    edn_free(a);
    edn_free(b);
}

/* Nested collection equality */
TEST(equal_nested_collections) {
    edn_value_t* a = parse_helper("{:list (1 2) :vec [3 4] :set #{5 6}}");
    edn_value_t* b = parse_helper("{:vec [3 4] :set #{6 5} :list (1 2)}");

    assert(a != NULL);
    assert(b != NULL);
    /* Maps are order-independent, sets are order-independent */
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_nested_different_element) {
    edn_value_t* a = parse_helper("[[1 2] [3 4]]");
    edn_value_t* b = parse_helper("[[1 2] [3 5]]");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Tagged value equality */
TEST(equal_tagged_same_tag_and_value) {
    edn_value_t* a = parse_helper("#inst \"2024-01-01\"");
    edn_value_t* b = parse_helper("#inst \"2024-01-01\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_tagged_different_value) {
    edn_value_t* a = parse_helper("#inst \"2024-01-01\"");
    edn_value_t* b = parse_helper("#inst \"2024-01-02\"");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_tagged_different_tag) {
    edn_value_t* a = parse_helper("#tag1 42");
    edn_value_t* b = parse_helper("#tag2 42");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_tagged_nested_value) {
    edn_value_t* a = parse_helper("#tag [1 2 3]");
    edn_value_t* b = parse_helper("#tag [1 2 3]");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* BigInt equality */
TEST(equal_bigint) {
    edn_value_t* a = parse_helper("99999999999999999999999999999");
    edn_value_t* b = parse_helper("99999999999999999999999999999");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_type(a) == EDN_TYPE_BIGINT);
    assert(edn_type(b) == EDN_TYPE_BIGINT);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_bigint) {
    edn_value_t* a = parse_helper("99999999999999999999999999999");
    edn_value_t* b = parse_helper("99999999999999999999999999998");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_bigint_vs_int) {
    edn_value_t* a = parse_helper("42");
    edn_value_t* b = parse_helper("99999999999999999999999999999");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_type(a) == EDN_TYPE_INT);
    assert(edn_type(b) == EDN_TYPE_BIGINT);
    /* Different types, not equal */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

/* Ratio equality */
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
TEST(equal_ratio) {
    edn_value_t* a = parse_helper("22/7");
    edn_value_t* b = parse_helper("22/7");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_type(a) == EDN_TYPE_RATIO);
    assert(edn_type(b) == EDN_TYPE_RATIO);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_ratio_different_numerator) {
    edn_value_t* a = parse_helper("22/7");
    edn_value_t* b = parse_helper("21/7");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_ratio_different_denominator) {
    edn_value_t* a = parse_helper("22/7");
    edn_value_t* b = parse_helper("22/8");

    assert(a != NULL);
    assert(b != NULL);
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_ratio_vs_int) {
    /* 10/5 reduces to 2/1 which becomes integer 2, so this test now checks */
    /* that a ratio that doesn't reduce to an integer is not equal to an int */
    edn_value_t* a = parse_helper("5/2"); /* 5/2 stays as ratio */
    edn_value_t* b = parse_helper("2");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_type(a) == EDN_TYPE_RATIO);
    assert(edn_type(b) == EDN_TYPE_INT);
    /* Different types, not equal even if mathematically similar */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(not_equal_ratio_vs_float) {
    edn_value_t* a = parse_helper("1/2");
    edn_value_t* b = parse_helper("0.5");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_type(a) == EDN_TYPE_RATIO);
    assert(edn_type(b) == EDN_TYPE_FLOAT);
    /* Different types, not equal even if mathematically equivalent */
    assert(!edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(equal_ratio_negative) {
    edn_value_t* a = parse_helper("-3/4");
    edn_value_t* b = parse_helper("-3/4");

    assert(a != NULL);
    assert(b != NULL);
    assert(edn_value_equal(a, b));

    edn_free(a);
    edn_free(b);
}

TEST(hash_ratio_same_value) {
    edn_value_t* a = parse_helper("22/7");
    edn_value_t* b = parse_helper("22/7");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    assert(hash_a == hash_b);

    edn_free(a);
    edn_free(b);
}

TEST(hash_ratio_different_value) {
    edn_value_t* a = parse_helper("22/7");
    edn_value_t* b = parse_helper("1/3");

    assert(a != NULL);
    assert(b != NULL);

    uint64_t hash_a = edn_value_hash(a);
    uint64_t hash_b = edn_value_hash(b);

    /* Different values should (usually) have different hashes */
    assert(hash_a != hash_b);

    edn_free(a);
    edn_free(b);
}
#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

int main(void) {
    printf("Running equality tests...\n");

    /* Nil */
    RUN_TEST(equal_nil);

    /* Boolean */
    RUN_TEST(equal_bool_true);
    RUN_TEST(equal_bool_false);
    RUN_TEST(not_equal_bool);

    /* Integer */
    RUN_TEST(equal_int);
    RUN_TEST(not_equal_int);
    RUN_TEST(equal_negative_int);

    /* Float */
    RUN_TEST(equal_float);
    RUN_TEST(not_equal_float);
    RUN_TEST(equal_nan);
    RUN_TEST(equal_infinity);
    RUN_TEST(not_equal_inf_neg_inf);

    /* Character */
    RUN_TEST(equal_character);
    RUN_TEST(not_equal_character);
    RUN_TEST(equal_unicode_character);

    /* String */
    RUN_TEST(equal_string);
    RUN_TEST(not_equal_string);
    RUN_TEST(equal_string_with_escapes);
    RUN_TEST(equal_empty_string);

    /* Symbol */
    RUN_TEST(equal_symbol);
    RUN_TEST(not_equal_symbol);
    RUN_TEST(equal_symbol_with_namespace);
    RUN_TEST(not_equal_symbol_different_namespace);

    /* Keyword */
    RUN_TEST(equal_keyword);
    RUN_TEST(not_equal_keyword);
    RUN_TEST(equal_keyword_with_namespace);

    /* Type mismatch */
    RUN_TEST(not_equal_different_types);
    RUN_TEST(not_equal_int_float);

    /* NULL checks */
    RUN_TEST(equal_null_null);
    RUN_TEST(not_equal_value_null);

    /* Comparison */
    RUN_TEST(compare_same_values);
    RUN_TEST(compare_different_ints);
    RUN_TEST(compare_strings);

    /* Hash */
    RUN_TEST(hash_equal_values_same_hash);
    RUN_TEST(hash_different_values_different_hash);
    RUN_TEST(hash_nan_deterministic);
    RUN_TEST(hash_null);

    /* Set order-independence */
    RUN_TEST(equal_set_same_order);
    RUN_TEST(equal_set_different_order);
    RUN_TEST(not_equal_set_different_elements);
    RUN_TEST(hash_set_order_independent);

    /* List equality */
    RUN_TEST(equal_list_same_order);
    RUN_TEST(not_equal_list_different_order);
    RUN_TEST(not_equal_list_different_length);
    RUN_TEST(equal_empty_lists);

    /* Vector equality */
    RUN_TEST(equal_vector_same_order);
    RUN_TEST(not_equal_vector_different_order);
    RUN_TEST(equal_empty_vectors);

    /* List vs Vector */
    RUN_TEST(equal_list_vs_vector);
    RUN_TEST(equal_empty_list_vs_vector);
    RUN_TEST(not_equal_list_vs_vector);

    /* Map equality */
    RUN_TEST(equal_map_same_order);
    RUN_TEST(equal_map_different_order);
    RUN_TEST(not_equal_map_different_keys);
    RUN_TEST(not_equal_map_different_values);
    RUN_TEST(equal_empty_maps);
    RUN_TEST(hash_map_order_independent);

    /* Nested collections */
    RUN_TEST(equal_nested_collections);
    RUN_TEST(not_equal_nested_different_element);

    /* Tagged values */
    RUN_TEST(equal_tagged_same_tag_and_value);
    RUN_TEST(not_equal_tagged_different_value);
    RUN_TEST(not_equal_tagged_different_tag);
    RUN_TEST(equal_tagged_nested_value);

    /* BigInt */
    RUN_TEST(equal_bigint);
    RUN_TEST(not_equal_bigint);
    RUN_TEST(not_equal_bigint_vs_int);

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    /* Ratio */
    RUN_TEST(equal_ratio);
    RUN_TEST(not_equal_ratio_different_numerator);
    RUN_TEST(not_equal_ratio_different_denominator);
    RUN_TEST(not_equal_ratio_vs_int);
    RUN_TEST(not_equal_ratio_vs_float);
    RUN_TEST(equal_ratio_negative);
    RUN_TEST(hash_ratio_same_value);
    RUN_TEST(hash_ratio_different_value);
#endif

    TEST_SUMMARY("equality");
}
