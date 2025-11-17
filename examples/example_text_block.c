/**
 * Example: Text Block parsing with automatic indentation stripping
 * 
 * Demonstrates Java-style text blocks in EDN with automatic indentation handling.
 * Compile with: -DEDN_ENABLE_TEXT_BLOCKS
 */

#include <stdio.h>
#include <string.h>

#include "../include/edn.h"

#ifdef EDN_ENABLE_TEXT_BLOCKS

int main(void) {
    printf("=== Text Block with Indentation Stripping ===\n\n");

    /* Example 1: SQL query with automatic indentation stripping */
    const char* example1 = 
        "{:query \"\"\"\n"
        "         SELECT users.name, orders.total\n"
        "         FROM users\n"
        "         JOIN orders ON users.id = orders.user_id\n"
        "         WHERE orders.total > 100\n"
        "         ORDER BY orders.total DESC\n"
        "         \"\"\"}";

    printf("Example 1: SQL query in EDN map\n");
    printf("Input (with indentation):\n%s\n\n", example1);

    edn_result_t result1 = edn_parse(example1, 0);
    if (result1.error == EDN_OK) {
        edn_result_t key = edn_parse(":query", 0);
        edn_value_t* query_val = edn_map_lookup(result1.value, key.value);
        
        if (query_val) {
            size_t len;
            const char* query = edn_string_get(query_val, &len);
            printf("Parsed query (indentation stripped):\n%s\n", query);
        }
        
        edn_free(key.value);
        edn_free(result1.value);
    }
    printf("\n");

    /* Example 2: Closing delimiter on same line (no trailing newline) */
    const char* example2 = 
        "{:greeting \"\"\"\n"
        "            Hello,\n"
        "            World!\"\"\"}";

    printf("Example 2: Closing delimiter on same line\n");
    printf("Input:\n%s\n\n", example2);

    edn_result_t result2 = edn_parse(example2, 0);
    if (result2.error == EDN_OK) {
        edn_result_t key = edn_parse(":greeting", 0);
        edn_value_t* val = edn_map_lookup(result2.value, key.value);
        
        if (val) {
            size_t len;
            const char* str = edn_string_get(val, &len);
            printf("Result: \"%s\" (length: %zu)\n", str, len);
            printf("Note: No trailing newline (closing \"\"\" on same line as content)\n");
        }
        
        edn_free(key.value);
        edn_free(result2.value);
    }
    printf("\n");

    /* Example 3: Closing delimiter on own line (adds trailing newline) */
    const char* example3 = 
        "{:greeting \"\"\"\n"
        "            Hello,\n"
        "            World!\n"
        "            \"\"\"}";

    printf("Example 3: Closing delimiter on own line\n");
    printf("Input:\n%s\n\n", example3);

    edn_result_t result3 = edn_parse(example3, 0);
    if (result3.error == EDN_OK) {
        edn_result_t key = edn_parse(":greeting", 0);
        edn_value_t* val = edn_map_lookup(result3.value, key.value);
        
        if (val) {
            size_t len;
            const char* str = edn_string_get(val, &len);
            printf("Result: \"%s\" (length: %zu)\n", str, len);
            printf("Note: Trailing newline added (closing \"\"\" on own line)\n");
        }
        
        edn_free(key.value);
        edn_free(result3.value);
    }
    printf("\n");

    /* Example 4: Indentation determined by closing delimiter */
    const char* example4 = 
        "{:message \"\"\"\n"
        "              Level 1\n"
        "             Level 2\n"
        "            Level 3\n"
        "            \"\"\"}";

    printf("Example 4: Closing delimiter determines base indentation\n");
    printf("Input:\n%s\n\n", example4);

    edn_result_t result4 = edn_parse(example4, 0);
    if (result4.error == EDN_OK) {
        edn_result_t key = edn_parse(":message", 0);
        edn_value_t* val = edn_map_lookup(result4.value, key.value);
        
        if (val) {
            size_t len;
            const char* str = edn_string_get(val, &len);
            printf("Result (showing spaces as _):\n");
            for (size_t i = 0; i < len; i++) {
                if (str[i] == ' ') printf("_");
                else if (str[i] == '\n') printf("\\n\n");
                else printf("%c", str[i]);
            }
            printf("\n");
            printf("Note: Closing \"\"\" at column 12, so 12 spaces stripped from each line\n");
        }
        
        edn_free(key.value);
        edn_free(result4.value);
    }
    printf("\n");

    /* Example 5: Configuration with sections */
    const char* example5 = 
        "{:database \"\"\"\n"
        "            host=localhost\n"
        "            port=5432\n"
        "            name=myapp\n"
        "            \"\"\"\n"
        " :cache \"\"\"\n"
        "         enabled=true\n"
        "         ttl=3600\n"
        "         \"\"\"}";

    printf("Example 5: Multiple text blocks in one map\n");
    printf("Input:\n%s\n\n", example5);

    edn_result_t result5 = edn_parse(example5, 0);
    if (result5.error == EDN_OK) {
        printf("Database config:\n");
        edn_result_t db_key = edn_parse(":database", 0);
        edn_value_t* db_val = edn_map_lookup(result5.value, db_key.value);
        if (db_val) {
            const char* db_str = edn_string_get(db_val, NULL);
            printf("%s\n", db_str);
        }
        
        printf("Cache config:\n");
        edn_result_t cache_key = edn_parse(":cache", 0);
        edn_value_t* cache_val = edn_map_lookup(result5.value, cache_key.value);
        if (cache_val) {
            const char* cache_str = edn_string_get(cache_val, NULL);
            printf("%s\n", cache_str);
        }
        
        edn_free(db_key.value);
        edn_free(cache_key.value);
        edn_free(result5.value);
    }
    printf("\n");

    /* Example 6: Preserving extra indentation */
    const char* example6 = 
        "\"\"\"\n"
        "    def hello():\n"
        "        print('Hello')\n"
        "        print('World')\n"
        "    \"\"\"";

    printf("Example 6: Preserving code indentation\n");
    printf("Input:\n%s\n\n", example6);

    edn_result_t result6 = edn_parse(example6, 0);
    if (result6.error == EDN_OK) {
        size_t len;
        const char* code = edn_string_get(result6.value, &len);
        printf("Result (showing spaces as _):\n");
        for (size_t i = 0; i < len; i++) {
            if (code[i] == ' ') printf("_");
            else if (code[i] == '\n') printf("\\n\n");
            else printf("%c", code[i]);
        }
        printf("\n");
        printf("Note: 4 spaces stripped (from closing \"\"\"), but relative indentation preserved\n");
        
        edn_free(result6.value);
    }
    printf("\n");

    /* Example 7: Trailing whitespace removal */
    const char* example7 = 
        "\"\"\"\n"
        "    line with trailing spaces   \n"
        "    another line  \n"
        "    \"\"\"";

    printf("Example 7: Automatic trailing whitespace removal\n");

    edn_result_t result7 = edn_parse(example7, 0);
    if (result7.error == EDN_OK) {
        size_t len;
        const char* str = edn_string_get(result7.value, &len);
        printf("Result length: %zu (trailing spaces removed)\n", len);
        printf("Result: \"%s\"\n", str);
        
        edn_free(result7.value);
    }

    printf("\n=== End of examples ===\n");
    return 0;
}

#else

int main(void) {
    printf("Text block feature is not enabled.\n");
    printf("Compile with -DEDN_ENABLE_TEXT_BLOCKS to enable.\n");
    return 0;
}

#endif
