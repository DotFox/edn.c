/**
 * Example: Extended Character Literals
 * 
 * Demonstrates the extended character literal features available when
 * compiled with EDN_ENABLE_EXTENDED_CHARACTERS flag.
 * 
 * Build:
 *   make EXTENDED_CHARACTERS=1 examples/example_extended_characters
 *   
 * Or with CMake:
 *   cmake -DEDN_ENABLE_EXTENDED_CHARACTERS=ON ..
 *   make example_extended_characters
 */

#include <stdio.h>
#include <stdlib.h>

#include "../include/edn.h"

#ifdef EDN_ENABLE_EXTENDED_CHARACTERS

static void print_character(const char* description, const char* input) {
    printf("\n%s\n", description);
    printf("  Input:  %s\n", input);

    edn_result_t result = edn_read(input, 0);

    if (result.error != EDN_OK) {
        printf("  Error:  %s\n", result.error_message);
        return;
    }

    if (edn_type(result.value) == EDN_TYPE_CHARACTER) {
        uint32_t codepoint;
        if (edn_character_get(result.value, &codepoint)) {
            printf("  Result: U+%04X (decimal: %u)\n", codepoint, codepoint);
            if (codepoint >= 32 && codepoint < 127) {
                printf("  ASCII:  '%c'\n", (char) codepoint);
            }
        }
    } else if (edn_type(result.value) == EDN_TYPE_VECTOR) {
        size_t count = edn_vector_count(result.value);
        printf("  Vector with %zu characters:\n", count);
        for (size_t i = 0; i < count; i++) {
            edn_value_t* elem = edn_vector_get(result.value, i);
            uint32_t codepoint;
            if (edn_character_get(elem, &codepoint)) {
                if (codepoint >= 32 && codepoint < 127) {
                    printf("    [%zu] U+%04X '%c'\n", i, codepoint, (char) codepoint);
                } else {
                    printf("    [%zu] U+%04X\n", i, codepoint);
                }
            }
        }
    }

    edn_free(result.value);
}

int main(void) {
    printf("EDN.C Extended Character Literals Example\n");
    printf("==========================================\n");

    printf("\n--- Extended Named Characters ---\n");
    print_character("Formfeed character:", "\\formfeed");
    print_character("Backspace character:", "\\backspace");

    printf("\n--- Octal Escape Sequences ---\n");
    print_character("Single digit octal (\\o7 - bell):", "\\o7");
    print_character("Two digit octal (\\o12 - line feed):", "\\o12");
    print_character("Three digit octal (\\o101 - 'A'):", "\\o101");
    print_character("Three digit octal (\\o141 - 'a'):", "\\o141");
    print_character("Maximum octal (\\o377 - 255):", "\\o377");
    print_character("Octal zero (\\o0 - null):", "\\o0");

    printf("\n--- In Collections ---\n");
    print_character("Octal characters in vector:", "[\\o101 \\o102 \\o103]");
    print_character("Mixed character types:", "[\\a \\o101 \\u0041 \\formfeed]");

    printf("\n--- Practical Examples ---\n");
    print_character("Control characters for terminal:", "[\\o33 \\o133 \\o61 \\o155]");
    print_character("ASCII digits via octal:", "[\\o60 \\o61 \\o62 \\o63 \\o64]");

    return 0;
}

#else

int main(void) {
    printf("Error: This example requires EDN_ENABLE_EXTENDED_CHARACTERS flag.\n");
    printf("\nBuild with:\n");
    printf("  make EXTENDED_CHARACTERS=1 examples/example_extended_characters\n");
    printf("\nOr with CMake:\n");
    printf("  cmake -DEDN_ENABLE_EXTENDED_CHARACTERS=ON ..\n");
    printf("  make example_extended_characters\n");
    return 1;
}

#endif
