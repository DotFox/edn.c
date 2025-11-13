/* test_framework.h - Unified test framework for EDN.C */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static bool current_test_failed = false;

/* Custom assert that doesn't abort the program */
#ifdef assert
#undef assert
#endif

#define assert(expr)                                                           \
    do {                                                                       \
        if (!(expr)) {                                                         \
            printf("\n    ASSERTION FAILED: %s (line %d)\n", #expr, __LINE__); \
            current_test_failed = true;                                        \
            return;                                                            \
        }                                                                      \
    } while (0)

/* Type-specific assertions that print actual values */
#define assert_int_eq(actual, expected)                                                           \
    do {                                                                                          \
        int64_t _actual = (int64_t) (actual);                                                     \
        int64_t _expected = (int64_t) (expected);                                                 \
        if (_actual != _expected) {                                                               \
            printf("\n    ASSERTION FAILED: %s == %s (line %d)\n", #actual, #expected, __LINE__); \
            printf("      Expected: %lld\n", (long long) _expected);                              \
            printf("      Actual:   %lld\n", (long long) _actual);                                \
            current_test_failed = true;                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define assert_uint_eq(actual, expected)                                                          \
    do {                                                                                          \
        uint64_t _actual = (uint64_t) (actual);                                                   \
        uint64_t _expected = (uint64_t) (expected);                                               \
        if (_actual != _expected) {                                                               \
            printf("\n    ASSERTION FAILED: %s == %s (line %d)\n", #actual, #expected, __LINE__); \
            printf("      Expected: %llu\n", (unsigned long long) _expected);                     \
            printf("      Actual:   %llu\n", (unsigned long long) _actual);                       \
            current_test_failed = true;                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define assert_double_eq(actual, expected)                                                        \
    do {                                                                                          \
        double _actual = (double) (actual);                                                       \
        double _expected = (double) (expected);                                                   \
        if (_actual != _expected) {                                                               \
            printf("\n    ASSERTION FAILED: %s == %s (line %d)\n", #actual, #expected, __LINE__); \
            printf("      Expected: %.17g\n", _expected);                                         \
            printf("      Actual:   %.17g\n", _actual);                                           \
            current_test_failed = true;                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define assert_str_eq(actual, expected)                                                           \
    do {                                                                                          \
        const char* _actual = (actual);                                                           \
        const char* _expected = (expected);                                                       \
        if (_actual == NULL || _expected == NULL || strcmp(_actual, _expected) != 0) {            \
            printf("\n    ASSERTION FAILED: %s == %s (line %d)\n", #actual, #expected, __LINE__); \
            printf("      Expected: \"%s\"\n", _expected ? _expected : "(null)");                 \
            printf("      Actual:   \"%s\"\n", _actual ? _actual : "(null)");                     \
            current_test_failed = true;                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define assert_ptr_eq(actual, expected)                                                           \
    do {                                                                                          \
        const void* _actual = (const void*) (actual);                                             \
        const void* _expected = (const void*) (expected);                                         \
        if (_actual != _expected) {                                                               \
            printf("\n    ASSERTION FAILED: %s == %s (line %d)\n", #actual, #expected, __LINE__); \
            printf("      Expected: %p\n", _expected);                                            \
            printf("      Actual:   %p\n", _actual);                                              \
            current_test_failed = true;                                                           \
            return;                                                                               \
        }                                                                                         \
    } while (0)

#define assert_true(expr)                                                                     \
    do {                                                                                      \
        if (!(expr)) {                                                                        \
            printf("\n    ASSERTION FAILED: %s should be true (line %d)\n", #expr, __LINE__); \
            current_test_failed = true;                                                       \
            return;                                                                           \
        }                                                                                     \
    } while (0)

#define assert_false(expr)                                                                     \
    do {                                                                                       \
        if (expr) {                                                                            \
            printf("\n    ASSERTION FAILED: %s should be false (line %d)\n", #expr, __LINE__); \
            current_test_failed = true;                                                        \
            return;                                                                            \
        }                                                                                      \
    } while (0)

/* Test definition macro - creates both the test function and its runner */
#define TEST(name)                             \
    static void test_##name(void);             \
    static void run_test_##name(void) {        \
        tests_run++;                           \
        current_test_failed = false;           \
        printf("  Running test_%s...", #name); \
        test_##name();                         \
        if (current_test_failed) {             \
            tests_failed++;                    \
            printf(" FAIL\n");                 \
        } else {                               \
            tests_passed++;                    \
            printf(" PASS\n");                 \
        }                                      \
    }                                          \
    static void test_##name(void)

/* Run a test */
#define RUN_TEST(name) run_test_##name()

/* Print test summary and return exit code */
#define TEST_SUMMARY(suite_name)                               \
    do {                                                       \
        printf("\n");                                          \
        tests_failed = tests_run - tests_passed;               \
        printf("Tests run: %d\n", tests_run);                  \
        printf("Tests passed: %d\n", tests_passed);            \
        printf("Tests failed: %d\n", tests_failed);            \
        if (tests_failed == 0) {                               \
            printf("\n✓ All " suite_name " tests passed!\n");  \
            return 0;                                          \
        } else {                                               \
            printf("\n✗ Some " suite_name " tests failed!\n"); \
            return 1;                                          \
        }                                                      \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
