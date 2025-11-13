#include <stdbool.h>
#include <string.h>

#include "edn_internal.h"

static bool parse_unicode_escape(const char* ptr, const char* end, uint32_t* codepoint) {
    if (ptr + 4 > end) {
        return false;
    }

    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        char h = ptr[i];
        uint32_t digit;

        if (h >= '0' && h <= '9') {
            digit = h - '0';
        } else if (h >= 'a' && h <= 'f') {
            digit = 10 + (h - 'a');
        } else if (h >= 'A' && h <= 'F') {
            digit = 10 + (h - 'A');
        } else {
            return false;
        }

        result = (result << 4) | digit;
    }

    *codepoint = result;
    return true;
}

static bool is_valid_single_char(char c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return false;
    }
    return true;
}

static bool match_string(const char* ptr, const char* end, const char* str, size_t len) {
    if (ptr + len > end) {
        return false;
    }
    return memcmp(ptr, str, len) == 0;
}

edn_value_t* edn_parse_character(edn_parser_t* parser) {
    const char* ptr = parser->current;
    const char* end = parser->end;

    if (ptr >= end || *ptr != '\\') {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Expected backslash at start of character literal";
        return NULL;
    }

    ptr++;

    if (ptr >= end) {
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unexpected end of input in character literal";
        return NULL;
    }

    uint32_t codepoint;

    if (match_string(ptr, end, "newline", 7)) {
        codepoint = 0x0A;
        ptr += 7;
    } else if (match_string(ptr, end, "return", 6)) {
        codepoint = 0x0D;
        ptr += 6;
    } else if (match_string(ptr, end, "space", 5)) {
        codepoint = 0x20;
        ptr += 5;
    } else if (match_string(ptr, end, "tab", 3)) {
        codepoint = 0x09;
        ptr += 3;
    } else if (*ptr == 'u') {
        ptr++;
        if (!parse_unicode_escape(ptr, end, &codepoint)) {
            parser->error = EDN_ERROR_INVALID_ESCAPE;
            parser->error_message = "Invalid Unicode escape sequence in character literal";
            return NULL;
        }
        ptr += 4;
    } else {
        if (!is_valid_single_char(*ptr)) {
            parser->error = EDN_ERROR_INVALID_SYNTAX;
            parser->error_message = "Invalid character literal";
            return NULL;
        }
        codepoint = (uint32_t) (unsigned char) *ptr;
        ptr++;
    }

    if (codepoint > 0x10FFFF) {
        parser->error = EDN_ERROR_INVALID_ESCAPE;
        parser->error_message = "Unicode codepoint out of valid range";
        return NULL;
    }

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_CHARACTER;
    value->as.character = codepoint;
    value->arena = parser->arena;

    parser->current = ptr;

    return value;
}
