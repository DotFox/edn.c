/**
 * EDN.C - String parsing with lazy decoding
 * 
 * Zero-copy string scanning with SIMD acceleration for quote/backslash detection.
 * Escape sequences decoded on-demand via edn_string_get() API.
 * Supports: \", \\, \n, \t, \r, \f, \b, \uXXXX (Unicode BMP, UTF-8 encoded).
 *
 * Text Block Parsing (Experimental Feature, requires EDN_ENABLE_TEXT_BLOCKS):
 *
 * This module also implements Java-style text blocks with automatic indentation stripping,
 * providing a convenient way to embed multi-line string literals in EDN documents.
 *
 * Syntax Rules:
 *   - Opening delimiter: """ followed by a mandatory newline character
 *   - Closing delimiter: """ (may be on its own line or after content)
 *   - Content: Any characters between delimiters
 *   - Escape sequence: Only \""" is recognized (unescapes to """)
 *
 * Indentation Stripping Algorithm:
 *   1. Calculate minimum common leading whitespace (spaces/tabs) across all lines
 *   2. Include blank lines and closing delimiter line in the calculation
 *   3. Strip this minimum indentation from each line
 *   4. Preserve relative indentation differences between lines
 *
 * Trailing Newline Behavior:
 *   - If closing """ is on its own line: trailing newline IS added to output
 *   - If closing """ follows content: trailing newline is NOT added
 *
 * Whitespace Handling:
 *   - Leading whitespace: stripped according to minimum indentation
 *   - Trailing whitespace: always removed from each line
 *   - Blank lines: preserved in output (but trailing spaces removed)
 *
 * Performance Optimizations:
 *   - SIMD-accelerated whitespace scanning (NEON on ARM64, SSE on x86_64)
 *   - SIMD-accelerated content scanning for special characters (\, ", \n)
 *   - Efficient single-pass parsing with minimal allocations
 *
 * Example:
 *   Input:  """
 *           SELECT * FROM "users"
 *           WHERE age > 21
 *           """
 *   Output: "SELECT * FROM "users"\nWHERE age > 21\n"
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

/* Platform-specific SIMD headers for text block parsing */
#ifdef EDN_ENABLE_TEXT_BLOCKS
#if defined(__wasm__) && defined(__wasm_simd128__)
#include <wasm_simd128.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <emmintrin.h>
#include <smmintrin.h>
#endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
static inline int msvc_ctz(unsigned int mask) {
    unsigned long index;
    if (_BitScanForward(&index, mask)) {
        return (int) index;
    }
    return 32;
}
#define CTZ(x) msvc_ctz(x)
#else
#define CTZ(x) __builtin_ctz(x)
#endif
#endif /* EDN_ENABLE_TEXT_BLOCKS */

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

/**
 * Read a string value from the parser.
 *
 * Handles both regular strings and text blocks (when EDN_ENABLE_TEXT_BLOCKS is enabled).
 * Uses SIMD-accelerated scanning for quote/backslash detection.
 * Uses lazy decoding - escape sequences are only processed when edn_string_get() is called.
 *
 * Returns: Parsed string value, or NULL on error.
 */
edn_value_t* edn_read_string(edn_parser_t* parser) {
#ifdef EDN_ENABLE_TEXT_BLOCKS
    /* Check for text block pattern: """\n */
    if (parser->current + 3 < parser->end && parser->current[0] == '"' &&
        parser->current[1] == '"' && parser->current[2] == '"' && parser->current[3] == '\n') {
        return edn_parse_text_block(parser);
    }
#endif

    const char* ptr = parser->current + 1;
    const char* start = ptr;

    bool has_escapes = false;
    const char* closing_quote = edn_simd_find_quote(ptr, parser->end, &has_escapes);

    if (!closing_quote) {
        parser->error = EDN_ERROR_INVALID_STRING;
        parser->error_message = "Unterminated string";
        return NULL;
    }

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_STRING;
    value->as.string.data = start;
    edn_string_set_length(value, closing_quote - start);
    edn_string_set_has_escapes(value, has_escapes);
    value->as.string.decoded = NULL;
    value->arena = parser->arena;

    parser->current = closing_quote + 1;

    return value;
}

/*
 * =============================================================================
 * Text Block Parsing (EDN_ENABLE_TEXT_BLOCKS)
 * =============================================================================
 */

#ifdef EDN_ENABLE_TEXT_BLOCKS

typedef struct {
    const char* line_start;    /* Start of line (including whitespace prefix) */
    size_t line_length;        /* Total line length (excluding newline) */
    const char* content_start; /* Start of content (after whitespace), NULL if no content */
    size_t content_length;     /* Length of content (excluding newline), 0 if no content */
    bool has_newline;          /* True if line ends with \n */
    bool needs_escaping;       /* True if line contains \""" escape sequence */
    bool terminal;             /* True if line ends with """ */
} text_block_line_t;

#if defined(__wasm__) && defined(__wasm_simd128__)

static inline const char* simd_scan_line_content(const char* ptr, const char* end) {
    while (ptr + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);
        v128_t is_newline = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\n'));
        v128_t is_quote = wasm_i8x16_eq(chunk, wasm_i8x16_splat('"'));
        v128_t is_backslash = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\\'));

        v128_t specials = wasm_v128_or(wasm_v128_or(is_newline, is_quote), is_backslash);
        int mask = wasm_i8x16_bitmask(specials);

        if (mask != 0) {
            int offset = CTZ((unsigned int) mask);
            return ptr + offset;
        }
        ptr += 16;
    }
    return ptr;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

static inline uint16_t neon_movemask_u8(uint8x16_t input) {
    static const uint8x16_t bitmask = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    uint8x16_t tmp = vandq_u8(input, bitmask);
    uint8x8_t lo = vget_low_u8(tmp);
    uint8x8_t hi = vget_high_u8(tmp);
    uint16_t lo_mask = (uint16_t) vaddv_u8(lo);
    uint16_t hi_mask = (uint16_t) vaddv_u8(hi);
    return (uint16_t) (lo_mask | (hi_mask << 8));
}

static inline const char* simd_scan_line_content(const char* ptr, const char* end) {
    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);
        uint8x16_t is_newline = vceqq_u8(chunk, vdupq_n_u8('\n'));
        uint8x16_t is_quote = vceqq_u8(chunk, vdupq_n_u8('"'));
        uint8x16_t is_backslash = vceqq_u8(chunk, vdupq_n_u8('\\'));

        uint8x16_t specials = vorrq_u8(vorrq_u8(is_newline, is_quote), is_backslash);
        uint16_t mask = neon_movemask_u8(specials);

        if (mask != 0) {
            int offset = CTZ((unsigned int) mask);
            return ptr + offset;
        }
        ptr += 16;
    }
    return ptr;
}

#elif defined(__x86_64__) || defined(_M_X64)

static inline const char* simd_scan_line_content(const char* ptr, const char* end) {
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);
        __m128i is_newline = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
        __m128i is_quote = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('"'));
        __m128i is_backslash = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\\'));

        __m128i specials = _mm_or_si128(_mm_or_si128(is_newline, is_quote), is_backslash);
        int mask = _mm_movemask_epi8(specials);

        if (mask != 0) {
            int offset = CTZ((unsigned int) mask);
            return ptr + offset;
        }
        ptr += 16;
    }
    return ptr;
}

#else

static inline const char* simd_scan_line_content(const char* ptr, const char* end) {
    (void) end;
    return ptr;
}

#endif

static text_block_line_t* edn_parse_text_block_line(edn_parser_t* parser) {
    const char* scan = parser->current;
    const char* end = parser->end;
    const char* line_start = scan;
    const char* p = scan;

#if defined(__wasm__) && defined(__wasm_simd128__)
    while (p + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) p);
        v128_t is_space = wasm_i8x16_eq(chunk, wasm_i8x16_splat(' '));
        v128_t is_tab = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\t'));
        v128_t is_ws = wasm_v128_or(is_space, is_tab);
        int mask = wasm_i8x16_bitmask(is_ws);

        if (mask == 0xFFFF) {
            p += 16;
        } else if (mask == 0) {
            break;
        } else {
            int offset = CTZ((unsigned int) (~mask & 0xFFFF));
            p += offset;
            break;
        }
    }
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__x86_64__) || defined(_M_X64)
    while (p + 16 <= end) {
#if defined(__aarch64__) || defined(_M_ARM64)
        uint8x16_t chunk = vld1q_u8((const uint8_t*) p);
        uint8x16_t is_space = vceqq_u8(chunk, vdupq_n_u8(' '));
        uint8x16_t is_tab = vceqq_u8(chunk, vdupq_n_u8('\t'));
        uint8x16_t is_ws = vorrq_u8(is_space, is_tab);
        uint16_t mask = neon_movemask_u8(is_ws);

        if (mask == 0xFFFF) {
            /* All 16 bytes are whitespace, continue scanning */
            p += 16;
        } else if (mask == 0) {
            /* No whitespace found in this chunk, stop SIMD scanning */
            break;
        } else {
            /* Mixed whitespace and non-whitespace, find first non-whitespace byte */
            int offset = CTZ((unsigned int) (~mask & 0xFFFF));
            p += offset;
            break;
        }
#else /* x86_64 */
        __m128i chunk = _mm_loadu_si128((const __m128i*) p);
        __m128i is_space = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
        __m128i is_tab = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
        __m128i is_ws = _mm_or_si128(is_space, is_tab);
        int mask = _mm_movemask_epi8(is_ws);

        if (mask == 0xFFFF) {
            /* All 16 bytes are whitespace, continue scanning */
            p += 16;
        } else if (mask == 0) {
            /* No whitespace found in this chunk, stop SIMD scanning */
            break;
        } else {
            /* Mixed whitespace and non-whitespace, find first non-whitespace byte */
            int offset = CTZ((unsigned int) (~mask & 0xFFFF));
            p += offset;
            break;
        }
#endif
    }
#endif

    /* Scalar tail: process remaining bytes one at a time */
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }

    const char* content_start = p; /* Mark where actual content begins */
    bool needs_escaping = false;   /* Track if we encounter \""" sequences */

    while (p < end) {
        const char* simd_end = simd_scan_line_content(p, end);
        p = simd_end;

        /* Scalar processing: handle special characters byte by byte */
        while (p < end) {
            char c = *p;

            /* Detect escape sequence: \""" (backslash followed by three quotes) */
            if (c == '\\' && p + 3 < end && p[1] == '"' && p[2] == '"' && p[3] == '"') {
                needs_escaping = true; /* Mark that this line needs unescape processing */
                p += 4;                /* Skip past the entire escape sequence */
                continue;
            }

            /* Detect closing delimiter: """ (three consecutive quotes) */
            if (c == '"' && p + 3 <= end && p[1] == '"' && p[2] == '"') {
                /* Closing delimiter found - this is the last "line" before text block ends.
                 * It may contain only whitespace (if """ is on its own line) or content. */
                text_block_line_t* line = malloc(sizeof(text_block_line_t));
                if (!line) {
                    parser->error = EDN_ERROR_OUT_OF_MEMORY;
                    parser->error_message = "Out of memory allocating line";
                    return NULL;
                }
                line->line_start = line_start;
                line->line_length = p - line_start;
                /* If no content before """, content_start will equal p, so set to NULL */
                line->content_start = (content_start < p) ? content_start : NULL;
                line->content_length = (content_start < p) ? (p - content_start) : 0;
                line->has_newline = false; /* No newline after this line */
                line->needs_escaping = needs_escaping;
                line->terminal = true;
                parser->current = p + 3; /* Move past closing delimiter */
                return line;
            }

            /* Detect newline: \n (normal line ending) */
            if (c == '\n') {
                /* Normal line ending - allocate and return line structure */
                text_block_line_t* line = malloc(sizeof(text_block_line_t));
                if (!line) {
                    parser->error = EDN_ERROR_OUT_OF_MEMORY;
                    parser->error_message = "Out of memory allocating line";
                    return NULL;
                }
                line->line_start = line_start;
                line->line_length = p - line_start;
                /* Blank lines have content_start == p, so we set content_start to NULL */
                line->content_start = (content_start < p) ? content_start : NULL;
                line->content_length = (content_start < p) ? (p - content_start) : 0;
                line->has_newline = true;
                line->needs_escaping = needs_escaping;
                line->terminal = false;
                parser->current = p + 1; /* Move past newline character */
                return line;
            }

            p++;
        }
    }

    /* Reached EOF without finding closing """ or newline - this is an error */
    parser->error = EDN_ERROR_INVALID_STRING;
    parser->error_message = "Unterminated text block (EOF during line parsing)";
    return NULL;
}

edn_value_t* edn_parse_text_block(edn_parser_t* parser) {
    /* Skip opening delimiter (""") and mandatory newline character */
    parser->current += 4;

    /* Allocate initial line buffer (grows exponentially as needed) */
    size_t lines_capacity = 16; /* Initial capacity: 16 lines */
    text_block_line_t** lines = malloc(lines_capacity * sizeof(text_block_line_t*));
    if (!lines) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating line buffer";
        return NULL;
    }

    size_t line_count = 0;
    size_t lwp = 0; /* Minimum indentation: Longest common Whitespace Prefix */

    while (parser->current < parser->end) {
        /* Grow line buffer if we've reached capacity (double the size) */
        if (line_count >= lines_capacity) {
            size_t new_capacity = lines_capacity * 2;
            text_block_line_t** new_lines =
                realloc(lines, new_capacity * sizeof(text_block_line_t*));
            if (!new_lines) {
                /* Free already allocated lines */
                for (size_t j = 0; j < line_count; j++) {
                    free(lines[j]);
                }
                free(lines);
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory growing line buffer";
                return NULL;
            }
            lines = new_lines;
            lines_capacity = new_capacity;
        }

        text_block_line_t* parsed = edn_parse_text_block_line(parser);

        /* Check for parser error (OOM or EOF during line parsing) */
        if (parser->error != EDN_OK) {
            /* Free any already allocated lines */
            for (size_t j = 0; j < line_count; j++) {
                free(lines[j]);
            }
            free(lines);
            return NULL;
        }

        /* Store the parsed line in our buffer */
        lines[line_count] = parsed;

        /* Calculate leading whitespace for this line:
         * - If line has content: distance from line_start to content_start
         * - If blank line: total line length (all whitespace) */
        size_t ws_prefix = parsed->content_start ? (parsed->content_start - parsed->line_start)
                                                 : parsed->line_length;

        if (parsed->content_length > 0 || parsed->terminal) {
            /* Line has content OR is closing delimiter - include in lwp calculation */
            if (lwp == 0 || ws_prefix < lwp) {
                lwp = ws_prefix;
            }
        }

        line_count++;

        if (parsed->terminal) {
            break;
        }
    }

    size_t total_len = 0;
    bool any_escapes = false;

    for (size_t i = 0; i < line_count; i++) {
        text_block_line_t* line = lines[i];

        size_t trimmed_content_length = 0;
        size_t ws_prefix = 0;

        if (line->content_start != NULL) {
            /* Line has content - trim trailing whitespace */
            const char* content_end = line->content_start + line->content_length;
            while (content_end > line->content_start &&
                   (content_end[-1] == ' ' || content_end[-1] == '\t')) {
                content_end--;
            }
            trimmed_content_length = content_end - line->content_start;

            /* Calculate leading whitespace and how much remains after stripping lwp */
            ws_prefix = line->content_start - line->line_start;
        } else {
            /* Blank line - no content to trim */
            trimmed_content_length = 0;
            ws_prefix = line->line_length; /* All whitespace */
        }

        /* Calculate remaining whitespace after stripping minimum indentation */
        size_t remaining_ws = (ws_prefix > lwp) ? (ws_prefix - lwp) : 0;

        /* Add to total: remaining whitespace + trimmed content */
        total_len += remaining_ws + trimmed_content_length;

        /* Add newline if this line ends with \n */
        if (line->has_newline) {
            total_len++;
        }

        /* Track if any line needs escape processing */
        if (line->needs_escaping) {
            any_escapes = true;
        }
    }

    char* result = edn_arena_alloc(parser->arena, total_len + 1); /* +1 for null terminator */
    if (!result) {
        /* Free allocated lines before returning */
        for (size_t j = 0; j < line_count; j++) {
            free(lines[j]);
        }
        free(lines);
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating result string";
        return NULL;
    }

    char* dst = result;

    for (size_t i = 0; i < line_count; i++) {
        text_block_line_t* line = lines[i];

        size_t trimmed_content_length = 0;
        size_t ws_prefix = 0;
        size_t ws_to_strip = 0;
        size_t remaining_ws = 0;
        const char* content_end = NULL;

        if (line->content_start != NULL) {
            /* --- Process line with content --- */

            /* Step 1: Trim trailing whitespace from content */
            content_end = line->content_start + line->content_length;
            while (content_end > line->content_start &&
                   (content_end[-1] == ' ' || content_end[-1] == '\t')) {
                content_end--;
            }
            trimmed_content_length = content_end - line->content_start;

            /* Step 2: Calculate whitespace stripping */
            ws_prefix = line->content_start - line->line_start;
            ws_to_strip = (ws_prefix > lwp) ? lwp : ws_prefix;
            remaining_ws = ws_prefix - ws_to_strip;

            /* Step 3: Copy remaining leading whitespace (if any) */
            if (remaining_ws > 0) {
                memcpy(dst, line->line_start + ws_to_strip, remaining_ws);
                dst += remaining_ws;
            }

            /* Step 4: Copy content with escape handling if needed */
            if (line->needs_escaping) {
                /* Process content and unescape \""" sequences */
                const char* src = line->content_start;
                const char* src_end = content_end; /* Use trimmed end */

                while (src < src_end) {
                    /* Check for \""" escape sequence */
                    if (src + 3 < src_end && src[0] == '\\' && src[1] == '"' && src[2] == '"' &&
                        src[3] == '"') {
                        /* Unescape: \""" becomes """ */
                        *dst++ = '"';
                        *dst++ = '"';
                        *dst++ = '"';
                        src += 4;
                    } else {
                        *dst++ = *src++;
                    }
                }
            } else {
                /* No escapes - simple memory copy */
                if (trimmed_content_length > 0) {
                    memcpy(dst, line->content_start, trimmed_content_length);
                    dst += trimmed_content_length;
                }
            }
        }
        /* else: Blank line with no content - nothing to copy except newline */

        /* Step 5: Add newline if present */
        if (line->has_newline) {
            *dst++ = '\n';
        }
    }

    *dst = '\0'; /* Null terminate the result string */

    /* Clean up: free all line structures */
    for (size_t j = 0; j < line_count; j++) {
        free(lines[j]);
    }
    free(lines);

    /* Create and populate EDN string value */
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating value";
        return NULL;
    }

    value->type = EDN_TYPE_STRING;
    value->as.string.data = result;
    edn_string_set_length(value, total_len);
    edn_string_set_has_escapes(value, any_escapes);
    value->as.string.decoded = result; /* Text blocks are already decoded */
    value->arena = parser->arena;

    return value;
}

#endif /* EDN_ENABLE_TEXT_BLOCKS */
