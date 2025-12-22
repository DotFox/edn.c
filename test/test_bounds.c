#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test bounds checking - ensure no buffer overruns */

/* Test single character inputs that might trigger lookahead */
TEST(bounds_check_single_hash) {
    edn_result_t result = edn_read("#", 1);
    /* Should error (invalid), but not crash */
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(bounds_check_single_plus) {
    edn_result_t result = edn_read("+", 1);
    /* Should parse as symbol */
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(bounds_check_single_minus) {
    edn_result_t result = edn_read("-", 1);
    /* Should parse as symbol */
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(bounds_check_single_zero) {
    edn_result_t result = edn_read("0", 1);
    /* Should parse as number */
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 0);
    edn_free(result.value);
}

TEST(bounds_check_hash_hash) {
    edn_result_t result = edn_read("##", 2);
    /* Should error (symbolic values not implemented), but not crash */
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

TEST(bounds_check_hash_brace) {
    edn_result_t result = edn_read("#{", 2);
    /* Should error (unterminated set), but not crash */
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert(result.value == NULL);
}

TEST(bounds_check_plus_digit) {
    edn_result_t result = edn_read("+1", 2);
    /* Should parse as number */
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 1);
    edn_free(result.value);
}

TEST(bounds_check_minus_digit) {
    edn_result_t result = edn_read("-1", 2);
    /* Should parse as number */
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == -1);
    edn_free(result.value);
}

TEST(bounds_check_zero_x) {
    edn_result_t result = edn_read("0x", 2);
    /* Should error (incomplete hex number), but not crash */
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(bounds_check_zero_digit) {
    edn_result_t result = edn_read("07", 2);
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 7);
    edn_free(result.value);
#else
    assert(result.error != EDN_OK);
    assert(result.value == NULL);
#endif
}

TEST(bounds_check_empty_after_whitespace) {
    edn_result_t result = edn_read(" ", 1);
    /* Should error (EOF after whitespace) */
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

int main(void) {
    printf("Running bounds checking tests...\n");

    RUN_TEST(bounds_check_single_hash);
    RUN_TEST(bounds_check_single_plus);
    RUN_TEST(bounds_check_single_minus);
    RUN_TEST(bounds_check_single_zero);
    RUN_TEST(bounds_check_hash_hash);
    RUN_TEST(bounds_check_hash_brace);
    RUN_TEST(bounds_check_plus_digit);
    RUN_TEST(bounds_check_minus_digit);
    RUN_TEST(bounds_check_zero_x);
    RUN_TEST(bounds_check_zero_digit);
    RUN_TEST(bounds_check_empty_after_whitespace);

    TEST_SUMMARY("bounds checking");
}
