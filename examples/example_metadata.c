/**
 * Example: Metadata parsing
 * 
 * Demonstrates Clojure-style metadata syntax.
 * Compile with: make METADATA=1 examples/example_metadata
 */

#include <stdio.h>
#include <string.h>

#include "../include/edn.h"

#ifdef EDN_ENABLE_METADATA

static void print_metadata(edn_value_t* meta) {
    if (meta == NULL) {
        printf("  (no metadata)\n");
        return;
    }

    if (edn_type(meta) != EDN_TYPE_MAP) {
        printf("  (invalid metadata - not a map)\n");
        return;
    }

    size_t count = edn_map_count(meta);
    printf("  Metadata (%zu entries):\n", count);

    for (size_t i = 0; i < count; i++) {
        edn_value_t* key = edn_map_get_key(meta, i);
        edn_value_t* val = edn_map_get_value(meta, i);

        if (edn_type(key) == EDN_TYPE_KEYWORD) {
            const char* name;
            size_t len;
            edn_keyword_get(key, NULL, NULL, &name, &len);
            printf("    :%.*s => ", (int) len, name);

            switch (edn_type(val)) {
                case EDN_TYPE_BOOL:
                    printf("true\n");
                    break;
                case EDN_TYPE_STRING: {
                    size_t str_len;
                    const char* str = edn_string_get(val, &str_len);
                    printf("\"%.*s\"\n", (int) str_len, str);
                    break;
                }
                case EDN_TYPE_SYMBOL: {
                    const char* sym_name;
                    size_t sym_len;
                    edn_symbol_get(val, NULL, NULL, &sym_name, &sym_len);
                    printf("%.*s\n", (int) sym_len, sym_name);
                    break;
                }
                case EDN_TYPE_INT: {
                    int64_t num;
                    edn_int64_get(val, &num);
                    printf("%lld\n", (long long) num);
                    break;
                }
                default:
                    printf("(other type)\n");
            }
        }
    }
}

int main(void) {
    printf("EDN Metadata Parsing Examples\n");
    printf("==============================\n\n");

    /* Example 1: Keyword shorthand */
    {
        printf("Example 1: ^:test [1 2 3]\n");
        edn_result_t result = edn_read("^:test [1 2 3]", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 2: Map metadata */
    {
        printf("Example 2: ^{:doc \"A vector\" :test true} [1 2 3]\n");
        edn_result_t result = edn_read("^{:doc \"A vector\" :test true} [1 2 3]", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 3: String tag */
    {
        printf("Example 3: ^\"String\" [1 2 3]\n");
        edn_result_t result = edn_read("^\"String\" [1 2 3]", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 4: Symbol tag */
    {
        printf("Example 4: ^Vector [1 2 3]\n");
        edn_result_t result = edn_read("^Vector [1 2 3]", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 5: Chained metadata */
    {
        printf("Example 5: ^:private ^:dynamic my-var\n");
        edn_result_t result = edn_read("^:private ^:dynamic my-var", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            printf("Type: ");
            if (edn_type(result.value) == EDN_TYPE_SYMBOL) {
                const char* name;
                size_t len;
                edn_symbol_get(result.value, NULL, NULL, &name, &len);
                printf("Symbol '%.*s'\n", (int) len, name);
            }

            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 6: Complex metadata */
    {
        printf("Example 6: ^{:doc \"A function\" :tag Fn :arglists ([x y])} my-fn\n");
        edn_result_t result = edn_read("^{:doc \"A function\" :tag Fn :arglists ([x y])} my-fn", 0);

        if (result.error == EDN_OK) {
            printf("Parsed successfully!\n");
            if (edn_value_has_meta(result.value)) {
                edn_value_t* meta = edn_value_meta(result.value);
                print_metadata(meta);
            }
        } else {
            printf("Parse error: %s\n", result.error_message);
        }

        edn_free(result.value);
        printf("\n");
    }

    /* Example 7: Metadata on different types */
    {
        printf("Example 7: Metadata on different collection types\n");

        const char* examples[] = {"^:test (1 2 3)", "^:test [1 2 3]", "^:test {:a 1}",
                                  "^:test #{1 2 3}"};

        const char* type_names[] = {"list", "vector", "map", "set"};

        for (size_t i = 0; i < 4; i++) {
            printf("  %s: ", examples[i]);
            edn_result_t result = edn_read(examples[i], 0);

            if (result.error == EDN_OK && edn_value_has_meta(result.value)) {
                printf("✓ (metadata attached to %s)\n", type_names[i]);
            } else {
                printf("✗\n");
            }

            edn_free(result.value);
        }
        printf("\n");
    }

    printf("All examples completed!\n");
    return 0;
}

#else

int main(void) {
    printf("This example requires metadata support.\n");
    printf("Compile with: make METADATA=1 examples/example_metadata\n");
    return 1;
}

#endif
