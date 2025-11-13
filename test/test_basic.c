#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Simple test to verify build system and API */

TEST(parse_null_input) {
    edn_result_t result = edn_parse(NULL, 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.error_message != NULL);
}

TEST(parse_empty_string) {
    edn_result_t result = edn_parse("", 0);
    /* Parser not implemented yet, should return error */
    assert(result.value == NULL);
    assert(result.error != EDN_OK);
}

TEST(edn_free_null) {
    /* Should not crash */
    edn_free(NULL);
}

TEST(edn_type_null) {
    edn_type_t type = edn_type(NULL);
    assert(type == EDN_TYPE_NIL);
}

int main(void) {
    printf("Running basic API tests...\n");

    run_test_parse_null_input();
    run_test_parse_empty_string();
    run_test_edn_free_null();
    run_test_edn_type_null();

    TEST_SUMMARY("basic API");
}
