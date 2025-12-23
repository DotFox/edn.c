#include <stdbool.h>
#include <string.h>

#include "edn_internal.h"

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
static bool parse_unicode_escape(const char* ptr, const char* end, uint32_t* codepoint,
                                 int* digits_consumed) {
    if (ptr + 4 > end) {
        return false;
    }

    uint32_t result = 0;
    int i = 0;

    /* Parse up to 6 hex digits, minimum 4 required */
    for (; i < 6 && ptr + i < end; i++) {
        char h = ptr[i];
        uint32_t digit;

        if (h >= '0' && h <= '9') {
            digit = h - '0';
        } else if (h >= 'a' && h <= 'f') {
            digit = 10 + (h - 'a');
        } else if (h >= 'A' && h <= 'F') {
            digit = 10 + (h - 'A');
        } else {
            break;
        }

        result = (result << 4) | digit;
    }

    if (i < 4) {
        return false;
    }

    *codepoint = result;
    *digits_consumed = i;
    return true;
}
#else
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
#endif

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
static bool parse_octal_escape(const char* ptr, const char* end, uint32_t* codepoint,
                               const char** next_ptr) {
    if (ptr >= end) {
        return false;
    }

    uint32_t result = 0;
    int digits = 0;

    while (digits < 3 && ptr < end && *ptr >= '0' && *ptr <= '7') {
        result = (result << 3) | (*ptr - '0');
        ptr++;
        digits++;
    }

    if (digits == 0) {
        return false;
    }

    if (ptr < end && (*ptr == '8' || *ptr == '9')) {
        return false;
    }

    if (result > 0377) {
        return false;
    }

    *codepoint = result;
    *next_ptr = ptr;
    return true;
}
#endif

static bool is_valid_single_char(char c) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r'
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        || c == '\f' || c == '\b'
#endif
    ) {
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

edn_value_t* edn_read_character(edn_parser_t* parser) {
    const char* start = parser->current;
    const char* ptr = parser->current;
    const char* end = parser->end;

    ptr++;

    if (ptr >= end) {
        parser->error = EDN_ERROR_INVALID_CHARACTER;
        parser->error_message = "Unexpected end of input in character literal";
        parser->error_start = start;
        parser->error_end = ptr;
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
    }
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    else if (match_string(ptr, end, "formfeed", 8)) {
        codepoint = 0x0C;
        ptr += 8;
    } else if (match_string(ptr, end, "backspace", 9)) {
        codepoint = 0x08;
        ptr += 9;
    } else if (*ptr == 'o' && ptr + 1 < end && ptr[1] >= '0' && ptr[1] <= '9') {
        ptr++;
        const char* next_ptr;
        if (!parse_octal_escape(ptr, end, &codepoint, &next_ptr)) {
            parser->error = EDN_ERROR_INVALID_CHARACTER;
            parser->error_message = "Invalid Octal escape sequence in character literal";
            parser->error_start = start;
            parser->error_end = ptr;
            return NULL;
        }
        ptr = next_ptr;
    }
#endif
    else if (*ptr == 'u' && ptr + 1 < end &&
             ((ptr[1] >= '0' && ptr[1] <= '9') || (ptr[1] >= 'a' && ptr[1] <= 'f') ||
              (ptr[1] >= 'A' && ptr[1] <= 'F'))) {
        ptr++;
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
        int digits_consumed;
        if (!parse_unicode_escape(ptr, end, &codepoint, &digits_consumed)) {
            parser->error = EDN_ERROR_INVALID_CHARACTER;
            parser->error_message = "Invalid Unicode escape sequence in character literal";
            parser->error_start = start;
            parser->error_end = ptr + 4;
            return NULL;
        }
        ptr += digits_consumed;
#else
        if (!parse_unicode_escape(ptr, end, &codepoint)) {
            parser->error = EDN_ERROR_INVALID_CHARACTER;
            parser->error_message = "Invalid Unicode escape sequence in character literal";
            parser->error_start = start;
            parser->error_end = ptr + 4;
            return NULL;
        }
        ptr += 4;
#endif
    } else {
        if (!is_valid_single_char(*ptr)) {
            parser->error = EDN_ERROR_INVALID_CHARACTER;
            parser->error_message = "Unsupported character literal";
            parser->error_start = start;
            parser->error_end = ptr + 1;
            return NULL;
        }
        codepoint = (uint32_t) (unsigned char) *ptr;
        ptr++;
    }

    if (codepoint > 0x10FFFF) {
        parser->error = EDN_ERROR_INVALID_CHARACTER;
        parser->error_message = "Unicode codepoint out of valid range";
        parser->error_start = start;
        parser->error_end = ptr;
        return NULL;
    }

    /* After parsing a character, we must be at end of input or at a delimiter */
    if (ptr < end && !is_delimiter((unsigned char) *ptr)) {
        parser->error = EDN_ERROR_INVALID_CHARACTER;
        parser->error_message =
            "Unsupported character - expected delimiter after character literal";
        parser->error_start = start;
        parser->error_end = ptr;
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
    value->source_start = start - parser->input;
    value->source_end = ptr - parser->input;

    parser->current = ptr;

    return value;
}
