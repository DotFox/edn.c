#include <math.h>
#include <string.h>

#include "edn_internal.h"

edn_value_t* edn_parse_symbolic_value(edn_parser_t* parser) {
    const char* ptr = parser->current;

    ptr += 2;

    double value;
    size_t len = parser->end - ptr;

    if (len >= 3 && strncmp(ptr, "Inf", 3) == 0) {
        value = INFINITY;
        ptr += 3;
    } else if (len >= 4 && strncmp(ptr, "-Inf", 4) == 0) {
        value = -INFINITY;
        ptr += 4;
    } else if (len >= 3 && strncmp(ptr, "NaN", 3) == 0) {
        value = NAN;
        ptr += 3;
    } else {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Invalid symbolic value (expected ##Inf, ##-Inf, or ##NaN)";
        return NULL;
    }

    edn_value_t* result = edn_arena_alloc_value(parser->arena);
    if (!result) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_FLOAT;
    result->as.floating = value;
    result->arena = parser->arena;

    parser->current = ptr;

    return result;
}
