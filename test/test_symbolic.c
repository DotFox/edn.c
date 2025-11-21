#include <math.h>
#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test symbolic value parsing: ##Inf, ##-Inf, ##NaN */

/* Test ##Inf */
TEST(parse_inf) {
    edn_result_t result = edn_read("##Inf", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    assert(isinf(result.value->as.floating));
    assert(result.value->as.floating > 0);
    edn_free(result.value);
}

/* Test ##-Inf */
TEST(parse_neg_inf) {
    edn_result_t result = edn_read("##-Inf", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    assert(isinf(result.value->as.floating));
    assert(result.value->as.floating < 0);
    edn_free(result.value);
}

/* Test ##NaN */
TEST(parse_nan) {
    edn_result_t result = edn_read("##NaN", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    assert(isnan(result.value->as.floating));
    edn_free(result.value);
}

/* Test with whitespace */
TEST(parse_inf_with_whitespace) {
    edn_result_t result = edn_read("  ##Inf  ", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    assert(isinf(result.value->as.floating));
    assert(result.value->as.floating > 0);
    edn_free(result.value);
}

TEST(parse_nan_with_comment) {
    edn_result_t result = edn_read("; comment\n##NaN", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    assert(isnan(result.value->as.floating));
    edn_free(result.value);
}

/* Test API functions */
TEST(api_double_get_inf) {
    edn_result_t result = edn_read("##Inf", 0);
    assert(result.error == EDN_OK);

    double value;
    bool success = edn_double_get(result.value, &value);
    assert(success == true);
    assert(isinf(value) && value > 0);

    edn_free(result.value);
}

TEST(api_double_get_neg_inf) {
    edn_result_t result = edn_read("##-Inf", 0);
    assert(result.error == EDN_OK);

    double value;
    bool success = edn_double_get(result.value, &value);
    assert(success == true);
    assert(isinf(value) && value < 0);

    edn_free(result.value);
}

TEST(api_double_get_nan) {
    edn_result_t result = edn_read("##NaN", 0);
    assert(result.error == EDN_OK);

    double value;
    bool success = edn_double_get(result.value, &value);
    assert(success == true);
    assert(isnan(value));

    edn_free(result.value);
}

/* Test invalid symbolic values */
TEST(invalid_only_hash_hash) {
    edn_result_t result = edn_read("##", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

TEST(invalid_unknown_symbolic) {
    edn_result_t result = edn_read("##Foo", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

TEST(invalid_incomplete_inf) {
    edn_result_t result = edn_read("##In", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

TEST(invalid_incomplete_nan) {
    edn_result_t result = edn_read("##Na", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

/* Test case sensitivity */
TEST(case_sensitive_inf) {
    edn_result_t result = edn_read("##inf", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

TEST(case_sensitive_nan) {
    edn_result_t result = edn_read("##nan", 0);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

int main(void) {
    printf("Running symbolic value tests...\n");

    /* Valid symbolic values */
    RUN_TEST(parse_inf);
    RUN_TEST(parse_neg_inf);
    RUN_TEST(parse_nan);

    /* With whitespace/comments */
    RUN_TEST(parse_inf_with_whitespace);
    RUN_TEST(parse_nan_with_comment);

    /* API functions */
    RUN_TEST(api_double_get_inf);
    RUN_TEST(api_double_get_neg_inf);
    RUN_TEST(api_double_get_nan);

    /* Invalid cases */
    RUN_TEST(invalid_only_hash_hash);
    RUN_TEST(invalid_unknown_symbolic);
    RUN_TEST(invalid_incomplete_inf);
    RUN_TEST(invalid_incomplete_nan);
    RUN_TEST(case_sensitive_inf);
    RUN_TEST(case_sensitive_nan);

    TEST_SUMMARY("symbolic value");
}
