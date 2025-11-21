/**
 * EDN.C - Reader example
 * 
 * Demonstrates how to use custom reader functions for tagged literals.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"

/* Example: Reader that converts #timestamp integers to formatted strings */
static edn_value_t* timestamp_reader(edn_value_t* value, edn_arena_t* arena,
                                     const char** error_message) {
    /* Expect integer input */
    if (edn_type(value) != EDN_TYPE_INT) {
        *error_message = "#timestamp requires integer value (Unix timestamp)";
        return NULL;
    }

    int64_t timestamp;
    edn_int64_get(value, &timestamp);

    /* Convert to time_t and format */
    time_t t = (time_t) timestamp;
    struct tm* tm_info = gmtime(&t);
    if (tm_info == NULL) {
        *error_message = "Invalid timestamp value";
        return NULL;
    }

    /* Format as ISO8601 string: YYYY-MM-DDTHH:MM:SSZ */
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    /* Allocate string in arena */
    size_t len = strlen(buffer);
    char* str_data = (char*) edn_arena_alloc(arena, len + 1);
    if (str_data == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    memcpy(str_data, buffer, len + 1);

    /* Create EDN string value */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_STRING;
    result->as.string.data = str_data;
    result->as.string.length_and_flags = len;
    result->as.string.decoded = NULL;
    result->arena = arena;

    return result;
}

/* Example: Reader that extracts first element from vector */
static edn_value_t* first_reader(edn_value_t* value, edn_arena_t* arena,
                                 const char** error_message) {
    (void) arena;

    if (edn_type(value) != EDN_TYPE_VECTOR) {
        *error_message = "#first requires vector value";
        return NULL;
    }

    size_t count = edn_vector_count(value);
    if (count == 0) {
        *error_message = "#first requires non-empty vector";
        return NULL;
    }

    return edn_vector_get(value, 0);
}

/* Example: Reader that converts to uppercase keyword */
static edn_value_t* upper_reader(edn_value_t* value, edn_arena_t* arena,
                                 const char** error_message) {
    if (edn_type(value) != EDN_TYPE_KEYWORD) {
        *error_message = "#upper requires keyword value";
        return NULL;
    }

    const char* ns;
    size_t ns_len;
    const char* name;
    size_t name_len;

    edn_keyword_get(value, &ns, &ns_len, &name, &name_len);

    /* Allocate uppercase name */
    char* upper_name = (char*) edn_arena_alloc(arena, name_len + 1);
    if (upper_name == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    for (size_t i = 0; i < name_len; i++) {
        char c = name[i];
        upper_name[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    upper_name[name_len] = '\0';

    /* Create new keyword value */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_KEYWORD;
    result->as.keyword.namespace = ns;
    result->as.keyword.ns_length = ns_len;
    result->as.keyword.name = upper_name;
    result->as.keyword.name_length = name_len;
    result->arena = arena;

    return result;
}

/* Example: Reader that appends "!" to string (namespaced tag #foo/bar) */
static edn_value_t* exclaim_reader(edn_value_t* value, edn_arena_t* arena,
                                   const char** error_message) {
    /* Expect string input */
    if (edn_type(value) != EDN_TYPE_STRING) {
        *error_message = "#foo/bar requires string value";
        return NULL;
    }

    size_t orig_len;
    const char* orig_str = edn_string_get(value, &orig_len);
    if (orig_str == NULL) {
        *error_message = "Failed to get string value";
        return NULL;
    }

    /* Allocate new string with "!" appended */
    size_t new_len = orig_len + 1;
    char* new_str = (char*) edn_arena_alloc(arena, new_len + 1);
    if (new_str == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    memcpy(new_str, orig_str, orig_len);
    new_str[orig_len] = '!';
    new_str[new_len] = '\0';

    /* Create new string value */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_STRING;
    result->as.string.data = new_str;
    result->as.string.length_and_flags = new_len; /* length with no flags set */
    result->as.string.decoded = NULL;
    result->arena = arena;

    return result;
}

int main(void) {
    printf("EDN.C Reader Examples\n");
    printf("=====================\n\n");

    /* Create reader registry */
    edn_reader_registry_t* registry = edn_reader_registry_create();
    if (registry == NULL) {
        fprintf(stderr, "Failed to create reader registry\n");
        return 1;
    }

    /* Register custom readers */
    edn_reader_register(registry, "timestamp", timestamp_reader);
    edn_reader_register(registry, "first", first_reader);
    edn_reader_register(registry, "upper", upper_reader);
    edn_reader_register(registry, "foo/bar", exclaim_reader);

    /* Example 1: Timestamp reader */
    printf("Example 1: #timestamp reader\n");
    printf("Input:  #timestamp 1704067200\n");

    edn_parse_options_t opts = {.reader_registry = registry,
                                .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH};

    edn_result_t result1 = edn_read_with_options("#timestamp 1704067200", 0, &opts);
    if (result1.error == EDN_OK) {
        size_t len;
        const char* str = edn_string_get(result1.value, &len);
        printf("Output: \"%.*s\"\n", (int) len, str);
    }
    printf("\n");
    edn_free(result1.value);

    /* Example 2: First reader */
    printf("Example 2: #first reader\n");
    printf("Input:  #first [1 2 3 4 5]\n");

    edn_result_t result2 = edn_read_with_options("#first [1 2 3 4 5]", 0, &opts);
    if (result2.error == EDN_OK) {
        int64_t val;
        edn_int64_get(result2.value, &val);
        printf("Output: %lld\n", (long long) val);
    }
    printf("\n");
    edn_free(result2.value);

    /* Example 3: Upper reader */
    printf("Example 3: #upper reader\n");
    printf("Input:  #upper :hello\n");

    edn_result_t result3 = edn_read_with_options("#upper :hello", 0, &opts);
    if (result3.error == EDN_OK) {
        const char* name;
        size_t name_len;
        edn_keyword_get(result3.value, NULL, NULL, &name, &name_len);
        printf("Output: :%.*s\n", (int) name_len, name);
    }
    printf("\n");
    edn_free(result3.value);

    /* Example 4: foo/bar reader (namespaced tag, adds "!") */
    printf("Example 4: #foo/bar reader (adds \"!\" to strings)\n");
    printf("Input:  #foo/bar \"Hello World\"\n");

    edn_result_t result4 = edn_read_with_options("#foo/bar \"Hello World\"", 0, &opts);
    if (result4.error == EDN_OK) {
        size_t len;
        const char* str = edn_string_get(result4.value, &len);
        printf("Output: \"%.*s\"\n", (int) len, str);
    }
    printf("\n");
    edn_free(result4.value);

    /* Example 5: Multiple readers in one parse */
    printf("Example 5: Multiple readers\n");
    printf("Input:  [#timestamp 1704067200 #first [10 20 30] #upper :world #foo/bar \"test\"]\n");

    edn_result_t result5 = edn_read_with_options(
        "[#timestamp 1704067200 #first [10 20 30] #upper :world #foo/bar \"test\"]", 0, &opts);
    if (result5.error == EDN_OK) {
        printf("Output: [\n");

        /* First element - timestamp string */
        edn_value_t* elem0 = edn_vector_get(result5.value, 0);
        size_t len0;
        const char* str0 = edn_string_get(elem0, &len0);
        printf("  \"%.*s\"\n", (int) len0, str0);

        /* Second element - first of vector */
        edn_value_t* elem1 = edn_vector_get(result5.value, 1);
        int64_t val1;
        edn_int64_get(elem1, &val1);
        printf("  %lld\n", (long long) val1);

        /* Third element - uppercase keyword */
        edn_value_t* elem2 = edn_vector_get(result5.value, 2);
        const char* name2;
        size_t name_len2;
        edn_keyword_get(elem2, NULL, NULL, &name2, &name_len2);
        printf("  :%.*s\n", (int) name_len2, name2);

        /* Fourth element - exclaimed string */
        edn_value_t* elem3 = edn_vector_get(result5.value, 3);
        size_t len3;
        const char* str3 = edn_string_get(elem3, &len3);
        printf("  \"%.*s\"\n", (int) len3, str3);

        printf("]\n");
    }
    printf("\n");
    edn_free(result5.value);

    /* Example 6: Default fallback modes */
    printf("Example 6: Default fallback - PASSTHROUGH\n");
    printf("Input:  #unknown 42\n");

    opts.default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH;
    edn_result_t result6 = edn_read_with_options("#unknown 42", 0, &opts);
    if (result6.error == EDN_OK) {
        printf("Output: Tagged literal (EDN_TYPE_TAGGED)\n");
        const char* tag;
        size_t tag_len;
        edn_value_t* wrapped;
        edn_tagged_get(result6.value, &tag, &tag_len, &wrapped);
        printf("  Tag:   %.*s\n", (int) tag_len, tag);
        int64_t val;
        edn_int64_get(wrapped, &val);
        printf("  Value: %lld\n", (long long) val);
    }
    printf("\n");
    edn_free(result6.value);

    printf("Example 7: Default fallback - UNWRAP\n");
    printf("Input:  #unknown 42\n");

    opts.default_reader_mode = EDN_DEFAULT_READER_UNWRAP;
    edn_result_t result7 = edn_read_with_options("#unknown 42", 0, &opts);
    if (result7.error == EDN_OK) {
        int64_t val;
        edn_int64_get(result7.value, &val);
        printf("Output: %lld (tag discarded)\n", (long long) val);
    }
    printf("\n");
    edn_free(result7.value);

    printf("Example 8: Default fallback - ERROR\n");
    printf("Input:  #unknown 42\n");

    opts.default_reader_mode = EDN_DEFAULT_READER_ERROR;
    edn_result_t result8 = edn_read_with_options("#unknown 42", 0, &opts);
    if (result8.error != EDN_OK) {
        printf("Output: Error - %s\n", result8.error_message);
    }
    printf("\n");

    /* Cleanup */
    edn_reader_registry_destroy(registry);

    printf("All examples completed successfully!\n");
    return 0;
}
