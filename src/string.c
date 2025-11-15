/**
 * EDN.C - String parsing with lazy decoding
 * 
 * Zero-copy string scanning with SIMD acceleration for quote/backslash detection.
 * Escape sequences decoded on-demand via edn_string_get() API.
 * Supports: \", \\, \n, \t, \r, \f, \b, \uXXXX (Unicode BMP, UTF-8 encoded).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

/**
 * Parse string with lazy escape decoding (zero-copy).
 * 
 * Scans for closing quote using SIMD, checks for backslashes with SIMD.
 * Returns pointer into input buffer (no allocation). Decoding happens
 * later via edn_decode_string() if has_escapes is true.
 * 
 * Returns: edn_string_scan_t with start/end pointers and has_escapes flag.
 */
edn_string_scan_t edn_parse_string_lazy(const char* ptr, const char* end) {
    edn_string_scan_t result = {0};

    ptr++;
    result.start = ptr;

    bool has_escapes = false;
    const char* closing_quote = edn_simd_find_quote(ptr, end, &has_escapes);

    if (!closing_quote) {
        result.valid = false;
        return result;
    }

    result.end = closing_quote;
    result.valid = true;
    result.has_escapes = has_escapes;

    return result;
}

/**
 * Decode single escape sequence from string.
 * 
 * Handles: \", \\, \n, \t, \r, \f, \b, \uXXXX (Unicode).
 * Unicode escapes (\uXXXX) are converted to UTF-8 encoding (1-3 bytes).
 * Surrogate pairs (0xD800-0xDFFF) are rejected.
 * 
 * Updates *ptr past the escape sequence, appends decoded bytes to *out.
 * Returns false on invalid escape or malformed Unicode.
 */
static bool decode_escape_sequence(const char** ptr, const char* end, char** out) {
    const char* p = *ptr;

    if (p >= end) {
        return false;
    }

    char c = *p++;

    switch (c) {
        case '"':
            *(*out)++ = '"';
            break;
        case '\\':
            *(*out)++ = '\\';
            break;
        case 'n':
            *(*out)++ = '\n';
            break;
        case 't':
            *(*out)++ = '\t';
            break;
        case 'r':
            *(*out)++ = '\r';
            break;
        case 'f':
            *(*out)++ = '\f';
            break;
        case 'b':
            *(*out)++ = '\b';
            break;
        case 'u': {
            if (p + 4 > end) {
                return false;
            }

            uint32_t codepoint = 0;
            for (int i = 0; i < 4; i++) {
                char h = *p++;
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
                codepoint = (codepoint << 4) | digit;
            }

            if (codepoint <= 0x7F) {
                *(*out)++ = (char) codepoint;
            } else if (codepoint <= 0x7FF) {
                *(*out)++ = (char) (0xC0 | (codepoint >> 6));
                *(*out)++ = (char) (0x80 | (codepoint & 0x3F));
            } else {
                if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                    return false;
                }
                *(*out)++ = (char) (0xE0 | (codepoint >> 12));
                *(*out)++ = (char) (0x80 | ((codepoint >> 6) & 0x3F));
                *(*out)++ = (char) (0x80 | (codepoint & 0x3F));
            }
            break;
        }
        default:
            return false;
    }

    *ptr = p;
    return true;
}

/**
 * Decode escaped string into arena-allocated buffer.
 * 
 * Processes escape sequences: \", \\, \n, \t, \r, \f, \b, \uXXXX.
 * Unicode escapes converted to UTF-8 (output may be shorter than input).
 * 
 * Returns: Null-terminated decoded string, or NULL on invalid escape.
 */
char* edn_decode_string(edn_arena_t* arena, const char* data, size_t length) {
    char* decoded = edn_arena_alloc(arena, length + 1);
    if (!decoded) {
        return NULL;
    }

    const char* ptr = data;
    const char* end = data + length;
    char* out = decoded;

    while (ptr < end) {
        if (*ptr == '\\') {
            ptr++;
            if (!decode_escape_sequence(&ptr, end, &out)) {
                return NULL;
            }
        } else {
            *out++ = *ptr++;
        }
    }

    *out = '\0';
    return decoded;
}
