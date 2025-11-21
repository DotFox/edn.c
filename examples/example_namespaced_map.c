/**
 * Example: Map Namespace Syntax
 * 
 * Demonstrates Clojure's map namespace syntax extension.
 * Compile with: make examples/example_namespaced_map
 * Run with: ./examples/example_namespaced_map
 */

#include <stdio.h>
#include <string.h>

#include "edn.h"

#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX

static void print_map(edn_value_t* map) {
    if (!map || edn_type(map) != EDN_TYPE_MAP) {
        printf("<not a map>\n");
        return;
    }

    printf("{\n");
    for (size_t i = 0; i < edn_map_count(map); i++) {
        edn_value_t* key = edn_map_get_key(map, i);
        edn_value_t* value = edn_map_get_value(map, i);

        printf("  ");

        // Print key
        if (edn_type(key) == EDN_TYPE_KEYWORD) {
            const char* ns;
            size_t ns_len;
            const char* name;
            size_t name_len;
            edn_keyword_get(key, &ns, &ns_len, &name, &name_len);

            if (ns != NULL) {
                printf(":%.*s/%.*s", (int) ns_len, ns, (int) name_len, name);
            } else {
                printf(":%.*s", (int) name_len, name);
            }
        } else if (edn_type(key) == EDN_TYPE_SYMBOL) {
            const char* ns;
            size_t ns_len;
            const char* name;
            size_t name_len;
            edn_symbol_get(key, &ns, &ns_len, &name, &name_len);

            if (ns != NULL) {
                printf("%.*s/%.*s", (int) ns_len, ns, (int) name_len, name);
            } else {
                printf("%.*s", (int) name_len, name);
            }
        } else if (edn_type(key) == EDN_TYPE_STRING) {
            size_t len;
            const char* str = edn_string_get(key, &len);
            printf("\"%.*s\"", (int) len, str);
        } else {
            printf("<key>");
        }

        printf(" ");

        // Print value
        if (edn_type(value) == EDN_TYPE_INT) {
            int64_t num;
            edn_int64_get(value, &num);
            printf("%lld", (long long) num);
        } else if (edn_type(value) == EDN_TYPE_STRING) {
            size_t len;
            const char* str = edn_string_get(value, &len);
            printf("\"%.*s\"", (int) len, str);
        } else {
            printf("<value>");
        }

        printf("\n");
    }
    printf("}\n");
}

static void example(const char* input, const char* description) {
    printf("\n%s\n", description);
    printf("Input:  %s\n", input);

    edn_result_t result = edn_read(input, 0);

    if (result.error != EDN_OK) {
        printf("Error:  %s\n", result.error_message);
        return;
    }

    printf("Output: ");
    print_map(result.value);

    edn_free(result.value);
}

int main(void) {
    printf("EDN.C Map Namespace Syntax Examples\n");
    printf("====================================\n");

    example("#:person{:name \"Alice\" :age 30}", "Basic map namespace syntax:");

    example("#:user{:id 123 :profile/photo \"pic.jpg\"}",
            "Mixed namespaces (some keys already namespaced):");

    example("#:db{:id 1 \"legacy\" \"value\"}", "Non-keyword keys are not transformed:");

    example("#:config{:host \"localhost\" :port 8080 :timeout 30}", "Configuration-style map:");

    example("#:foo{x 1 y 2}", "Symbol keys are also namespaced:");

    example("#:foo{x 1 :y 2 bar/z 3}", "Mixed key types (symbol, keyword, namespaced symbol):");

    example("#:api{}", "Empty namespaced map:");

    printf("\nNote: This feature is a Clojure extension, not part of official EDN.\n");
    printf("It can be disabled at compile time with MAP_NAMESPACE_SYNTAX=0\n");

    return 0;
}

#else

int main(void) {
    printf("Map namespace syntax is disabled.\n");
    printf("Rebuild with MAP_NAMESPACE_SYNTAX=1 to enable this feature.\n");
    return 0;
}

#endif
