#include <string.h>

#include "edn_internal.h"

/* Windows-specific intrinsics */
#if defined(_MSC_VER)
#include <intrin.h>

/* MSVC doesn't have __builtin_ctz, provide wrapper for _BitScanForward */
static inline int msvc_ctz(unsigned int mask) {
    unsigned long index;
    if (_BitScanForward(&index, mask)) {
        return (int) index;
    }
    return 32; /* No bit set */
}
#define CTZ(x) msvc_ctz(x)
#else
#define CTZ(x) __builtin_ctz(x)
#endif

/* SIMD whitespace skipping - platform-specific implementations */

#if defined(__aarch64__) || defined(_M_ARM64)
/* ARM64 NEON implementation */
#include <arm_neon.h>

/* Produce a 16-bit mask from a uint8x16_t, assuming lanes are 0x00 or 0xFF. */
static inline uint16_t edn_neon_movemask_u8(uint8x16_t input) {
    static const uint8x16_t bitmask = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};

    uint8x16_t tmp = vandq_u8(input, bitmask);

    uint8x8_t lo = vget_low_u8(tmp);
    uint8x8_t hi = vget_high_u8(tmp);

    uint16_t lo_mask = (uint16_t) vaddv_u8(lo);
    uint16_t hi_mask = (uint16_t) vaddv_u8(hi);

    return (uint16_t) (lo_mask | (hi_mask << 8));
}

/* SIMD function to find newline character in comment */
static inline const char* edn_simd_find_newline_neon(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with NEON */
    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);
        uint8x16_t newline = vceqq_u8(chunk, vdupq_n_u8('\n'));

        /* Check if we found a newline */
        uint64x2_t newline_64 = vreinterpretq_u64_u8(newline);
        uint64_t low = vgetq_lane_u64(newline_64, 0);
        uint64_t high = vgetq_lane_u64(newline_64, 1);

        if (low != 0 || high != 0) {
            /* Found newline in this chunk, find exact position */
            for (int i = 0; i < 16; i++) {
                if (ptr[i] == '\n') {
                    return ptr + i;
                }
            }
        }
        ptr += 16;
    }

    /* Scalar fallback for remaining bytes */
    while (ptr < end && *ptr != '\n') {
        ptr++;
    }
    return ptr;
}

const char* edn_simd_skip_whitespace(const char* ptr, const char* end) {
    while (ptr < end) {
        /* Check for line comment */
        if (*ptr == ';') {
            /* Use SIMD to find newline quickly */
            ptr++;
            ptr = edn_simd_find_newline_neon(ptr, end);
            if (ptr < end && *ptr == '\n') {
                ptr++; /* Skip the newline itself */
            }
            continue;
        }

        /* NEON processes 16 bytes at a time */
        if (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);

            /* Check for space (0x20), tab (0x09), newline (0x0A), VT (0x0B), formfeed (0x0C), carriage return (0x0D), comma (0x2C) */
            uint8x16_t is_space = vceqq_u8(chunk, vdupq_n_u8(0x20));
            uint8x16_t is_tab = vceqq_u8(chunk, vdupq_n_u8(0x09));
            uint8x16_t is_newline = vceqq_u8(chunk, vdupq_n_u8(0x0A));
            uint8x16_t is_vt = vceqq_u8(chunk, vdupq_n_u8(0x0B));
            uint8x16_t is_formfeed = vceqq_u8(chunk, vdupq_n_u8(0x0C));
            uint8x16_t is_cr = vceqq_u8(chunk, vdupq_n_u8(0x0D));
            uint8x16_t is_comma = vceqq_u8(chunk, vdupq_n_u8(0x2C));
            /* Separator characters: 0x1C-0x1F (FS, GS, RS, US) */
            uint8x16_t is_fs = vceqq_u8(chunk, vdupq_n_u8(0x1C));
            uint8x16_t is_gs = vceqq_u8(chunk, vdupq_n_u8(0x1D));
            uint8x16_t is_rs = vceqq_u8(chunk, vdupq_n_u8(0x1E));
            uint8x16_t is_us = vceqq_u8(chunk, vdupq_n_u8(0x1F));

            /* Combine all whitespace checks */
            uint8x16_t ws_group1 =
                vorrq_u8(vorrq_u8(is_space, is_tab), vorrq_u8(is_newline, is_vt));
            uint8x16_t ws_group2 = vorrq_u8(vorrq_u8(is_formfeed, is_cr), is_comma);
            uint8x16_t ws_group3 = vorrq_u8(vorrq_u8(is_fs, is_gs), vorrq_u8(is_rs, is_us));
            uint8x16_t is_ws = vorrq_u8(vorrq_u8(ws_group1, ws_group2), ws_group3);

            /* Check if all bytes are whitespace */
            uint64x2_t ws_64 = vreinterpretq_u64_u8(is_ws);
            uint64_t low = vgetq_lane_u64(ws_64, 0);
            uint64_t high = vgetq_lane_u64(ws_64, 1);

            if ((low & high) == 0xFFFFFFFFFFFFFFFFULL) {
                /* All 16 bytes are whitespace */
                ptr += 16;
                continue;
            }
        }

        /* Scalar fallback for remaining bytes */
        unsigned char c = (unsigned char) *ptr;
        /* Whitespace: 0x09-0x0D (tab, LF, VT, FF, CR), 0x1C-0x1F (FS, GS, RS, US), space, comma */
        if (c == ' ' || c == ',' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) {
            ptr++;
        } else {
            break;
        }
    }

    return ptr;
}

#elif defined(__x86_64__) || defined(_M_X64)
/* x86_64 SSE4.2 implementation */
#if defined(_MSC_VER)
#include <intrin.h> /* MSVC: includes all SSE/AVX intrinsics */
#else
#include <emmintrin.h> /* GCC/Clang: SSE2 (_mm_loadu_si128, _mm_set1_epi8) */
#include <smmintrin.h> /* GCC/Clang: SSE4.1 (_mm_cmpeq_epi8, etc.) */
#endif

/* SIMD function to find newline character in comment */
static inline const char* edn_simd_find_newline_sse(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with SSE */
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);
        __m128i newline = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));

        int mask = _mm_movemask_epi8(newline);
        if (mask != 0) {
            /* Found newline, find exact position using bit scan */
            int offset = CTZ(mask); /* Count trailing zeros */
            return ptr + offset;
        }
        ptr += 16;
    }

    /* Scalar fallback for remaining bytes */
    while (ptr < end && *ptr != '\n') {
        ptr++;
    }
    return ptr;
}

const char* edn_simd_skip_whitespace(const char* ptr, const char* end) {
    while (ptr < end) {
        /* Check for line comment */
        if (*ptr == ';') {
            /* Use SIMD to find newline quickly */
            ptr++;
            ptr = edn_simd_find_newline_sse(ptr, end);
            if (ptr < end && *ptr == '\n') {
                ptr++; /* Skip the newline itself */
            }
            continue;
        }

        /* SSE processes 16 bytes at a time */
        if (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);

            /* Check for whitespace characters */
            __m128i space = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
            __m128i tab = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
            __m128i newline = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
            __m128i vt = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\v'));
            __m128i formfeed = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\f'));
            __m128i cr = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));
            __m128i comma = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(','));
            /* Separator characters: 0x1C-0x1F (FS, GS, RS, US) */
            __m128i fs = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(0x1C));
            __m128i gs = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(0x1D));
            __m128i rs = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(0x1E));
            __m128i us = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(0x1F));

            /* Combine all whitespace checks */
            __m128i ws_group1 = _mm_or_si128(_mm_or_si128(space, tab), _mm_or_si128(newline, vt));
            __m128i ws_group2 = _mm_or_si128(_mm_or_si128(formfeed, cr), comma);
            __m128i ws_group3 = _mm_or_si128(_mm_or_si128(fs, gs), _mm_or_si128(rs, us));
            __m128i is_ws = _mm_or_si128(_mm_or_si128(ws_group1, ws_group2), ws_group3);

            /* Check if all bytes are whitespace */
            int mask = _mm_movemask_epi8(is_ws);
            if (mask == 0xFFFF) {
                /* All 16 bytes are whitespace */
                ptr += 16;
                continue;
            }
        }

        /* Scalar fallback for remaining bytes */
        unsigned char c = (unsigned char) *ptr;
        /* Whitespace: 0x09-0x0D (tab, LF, VT, FF, CR), 0x1C-0x1F (FS, GS, RS, US), space, comma */
        if (c == ' ' || c == ',' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) {
            ptr++;
        } else {
            break;
        }
    }

    return ptr;
}

#else
/* Scalar fallback for other platforms */
const char* edn_simd_skip_whitespace(const char* ptr, const char* end) {
    while (ptr < end) {
        char c = *ptr;

        /* Handle line comments */
        if (c == ';') {
            /* Skip until newline or EOF */
            ptr++;
            while (ptr < end && *ptr != '\n') {
                ptr++;
            }
            if (ptr < end && *ptr == '\n') {
                ptr++; /* Skip the newline itself */
            }
            continue;
        }

        /* Handle regular whitespace */
        /* Whitespace: 0x09-0x0D (tab, LF, VT, FF, CR), 0x1C-0x1F (FS, GS, RS, US), space, comma */
        unsigned char uc = (unsigned char) c;
        if (c == ' ' || c == ',' || (uc >= 0x09 && uc <= 0x0D) || (uc >= 0x1C && uc <= 0x1F)) {
            ptr++;
        } else {
            break;
        }
    }
    return ptr;
}

#endif

/* SIMD function to find closing quote in string */
#if defined(__aarch64__) || defined(_M_ARM64)

const char* edn_simd_find_quote(const char* ptr, const char* end, bool* out_has_backslash) {
    bool has_backslash = false;

    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);
        uint8x16_t quote_v = vceqq_u8(chunk, vdupq_n_u8('"'));
        uint8x16_t bs_v = vceqq_u8(chunk, vdupq_n_u8('\\'));
        uint8x16_t specials = vorrq_u8(quote_v, bs_v);

        uint16_t bs_mask = edn_neon_movemask_u8(bs_v);
        uint16_t special_mask = edn_neon_movemask_u8(specials);

        if (special_mask == 0u) {
            /* No '"' or '\\' in this block */
            ptr += 16;
            continue;
        }

        /* Something interesting in this block */
        int idx = CTZ((unsigned int) special_mask); /* 0..15 */
        char c = ptr[idx];

        if (c == '\\') {
            has_backslash = true;
            if (ptr + idx + 1 >= end) {
                /* trailing backslash without following char */
                return NULL;
            }
            ptr += idx + 2;
            continue;
        }

        /* Must be a '"' (we don't have any other specials) */
        if (out_has_backslash) {
            *out_has_backslash = has_backslash || (bs_mask != 0);
        }
        return ptr + idx;
    }

    /* Scalar tail for remaining bytes (<16) */
    while (ptr < end) {
        char c = *ptr;
        if (c == '\\') {
            has_backslash = true;
            if (ptr + 1 >= end) {
                return NULL; /* trailing backslash */
            }
            ptr += 2;
        } else if (c == '"') {
            if (out_has_backslash) {
                *out_has_backslash = has_backslash;
            }
            return ptr;
        } else {
            ++ptr;
        }
    }

    return NULL; /* Not found */
}

#elif defined(__x86_64__) || defined(_M_X64)

const char* edn_simd_find_quote(const char* ptr, const char* end, bool* out_has_backslash) {
    bool has_backslash = false;

    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);
        __m128i quote_v = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('"'));
        __m128i bs_v = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\\'));

        int quote_mask = _mm_movemask_epi8(quote_v);
        int bs_mask = _mm_movemask_epi8(bs_v);
        int special_mask = quote_mask | bs_mask;

        if (special_mask == 0) {
            /* No '"' or '\\' in this block */
            ptr += 16;
            continue;
        }

        /* Something interesting in this block */
        int idx = CTZ((unsigned int) special_mask); /* 0..15 */
        char c = ptr[idx];

        if (c == '\\') {
            has_backslash = true;
            if (ptr + idx + 1 >= end) {
                /* trailing backslash without following char */
                return NULL;
            }
            ptr += idx + 2;
            continue;
        }

        /* Must be a '"' (we don't have any other specials) */
        if (out_has_backslash) {
            *out_has_backslash = has_backslash || (bs_mask != 0);
        }
        return ptr + idx;
    }

    /* Scalar tail for remaining bytes (<16) */
    while (ptr < end) {
        char c = *ptr;
        if (c == '\\') {
            has_backslash = true;
            if (ptr + 1 >= end) {
                return NULL; /* trailing backslash */
            }
            ptr += 2;
        } else if (c == '"') {
            if (out_has_backslash) {
                *out_has_backslash = has_backslash;
            }
            return ptr;
        } else {
            ++ptr;
        }
    }

    return NULL;
}

#else
/* Scalar fallback: scan for closing quote and track whether any '\' appeared.
   ptr points to first char after initial '"'. */

const char* edn_simd_find_quote(const char* ptr, const char* end, bool* out_has_backslash) {
    bool has_backslash = false;

    while (ptr < end) {
        char c = *ptr;
        if (c == '\\') {
            has_backslash = true;
            if (ptr + 1 >= end) {
                /* trailing backslash with no following char -> invalid string */
                return NULL;
            }
            ptr += 2; /* skip escaped character */
        } else if (c == '"') {
            if (out_has_backslash) {
                *out_has_backslash = has_backslash;
            }
            return ptr;
        } else {
            ptr++;
        }
    }

    /* Unterminated string */
    return NULL;
}

#endif

/* SIMD digit scanning for number parsing */
#if defined(__aarch64__) || defined(_M_ARM64)

const char* edn_simd_scan_digits(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with NEON */
    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);

        /* Check if all bytes are digits ('0'-'9' = 0x30-0x39) */
        uint8x16_t ge_0 = vcgeq_u8(chunk, vdupq_n_u8('0'));
        uint8x16_t le_9 = vcleq_u8(chunk, vdupq_n_u8('9'));
        uint8x16_t is_digit = vandq_u8(ge_0, le_9);

        /* Build a 16-bit mask: bit i == 1 → byte i is a digit */
        uint16_t mask = edn_neon_movemask_u8(is_digit);

        if (mask == 0xFFFFu) {
            /* All 16 bytes are digits */
            ptr += 16;
            continue;
        }

        if (mask == 0u) {
            /* First byte is already non-digit */
            return ptr;
        }

        /* Some digits, then a non-digit: find the first non-digit */
        unsigned int non_mask = (~mask) & 0xFFFFu;
        int idx = CTZ(non_mask); /* index of first 0 bit (non-digit) */

        return ptr + idx;
    }

    while (ptr < end) {
        unsigned char c = (unsigned char) *ptr;
        if (c < '0' || c > '9') {
            break;
        }
        ++ptr;
    }

    return ptr;
}

#elif defined(__x86_64__) || defined(_M_X64)

const char* edn_simd_scan_digits(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with SSE */
    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);
        __m128i ge_0 = _mm_cmpgt_epi8(chunk, _mm_set1_epi8('0' - 1));
        __m128i le_9 = _mm_cmplt_epi8(chunk, _mm_set1_epi8('9' + 1));
        __m128i is_digit = _mm_and_si128(ge_0, le_9);

        int mask = _mm_movemask_epi8(is_digit);
        if (mask == 0xFFFF) {
            /* All 16 bytes are digits */
            ptr += 16;
        } else {
            /* Found non-digit, find exact position */
            int offset = CTZ(~mask);
            return ptr + offset;
        }
    }

    /* Scalar fallback for remaining bytes */
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
        ptr++;
    }
    return ptr;
}

#else

const char* edn_simd_scan_digits(const char* ptr, const char* end) {
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
        ptr++;
    }
    return ptr;
}

#endif

/* SIMD identifier scanner - finds first delimiter AND first slash */
/* Delimiter detection now provided by identifier_internal.h lookup table */

#if defined(__aarch64__) || defined(_M_ARM64)

edn_identifier_scan_result_t edn_simd_scan_identifier(const char* ptr, const char* end) {
    edn_identifier_scan_result_t result = {.end = ptr, .first_slash = NULL};

    size_t remaining = end - ptr;

    /* Fast path for short identifiers (common case: :id, :name, :type, etc.)
     * If total remaining input is ≤8 bytes, use scalar (no SIMD overhead)
     * This handles: end of buffer, short keywords
     */
    if (remaining <= 8) {
        while (ptr < end) {
            char c = *ptr;
            if (is_delimiter(c)) {
                result.end = ptr;
                return result;
            }
            if (c == '/' && !result.first_slash) {
                result.first_slash = ptr;
            }
            ptr++;
        }
        result.end = ptr;
        return result;
    }

    /* NEON: Process 16 bytes at a time for longer identifiers
     * 
     * Optimization strategy:
     * 1. Use range checks to quickly eliminate most valid identifier chars
     * 2. Only check specific delimiters if we're in the delimiter range
     * 3. Use vectorized operations to minimize branches
     */
    while (remaining >= 16) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);

        /* Check for slash (namespace separator) - always needed */
        uint8x16_t is_slash = vceqq_u8(chunk, vdupq_n_u8('/'));

        /* Most identifier chars are alphanumeric or symbols in valid ranges.
         * Delimiters cluster in specific ASCII ranges:
         * - Control chars: 0x00-0x1F (especially \t=0x09, \n=0x0A, \r=0x0D)
         * - Structural: space=0x20, "=0x22, #=0x23, (=0x28, )=0x29, ,=0x2C
         * - Brackets: [=0x5B, \=0x5C, ]=0x5D, ^=0x5E
         * - Braces: {=0x7B, }=0x7D
         * - Other: ;=0x3B, @=0x40, `=0x60, ~=0x7E
         * 
         * Fast path: Check if ALL bytes are in "definitely valid" range
         * Most common valid chars: A-Z (0x41-0x5A), a-z (0x61-0x7A), 0-9 (0x30-0x39)
         * Plus common symbols: -, _, ., !, *, +, =, <, >, ?, $, %, &, |
         */

        /* Check for common delimiters using range comparisons */
        /* Control chars: < 0x20 (catches tab, newline, CR) */
        uint8x16_t is_control = vcltq_u8(chunk, vdupq_n_u8(0x20));

        /* Structural delimiters in 0x20-0x2F range */
        uint8x16_t is_space = vceqq_u8(chunk, vdupq_n_u8(' '));
        uint8x16_t is_quote = vceqq_u8(chunk, vdupq_n_u8('"'));
        uint8x16_t is_hash = vceqq_u8(chunk, vdupq_n_u8('#'));
        uint8x16_t is_lparen = vceqq_u8(chunk, vdupq_n_u8('('));
        uint8x16_t is_rparen = vceqq_u8(chunk, vdupq_n_u8(')'));
        uint8x16_t is_comma = vceqq_u8(chunk, vdupq_n_u8(','));

        /* Other structural delimiters */
        uint8x16_t is_semi = vceqq_u8(chunk, vdupq_n_u8(';'));
        uint8x16_t is_at = vceqq_u8(chunk, vdupq_n_u8('@'));
        uint8x16_t is_lbracket = vceqq_u8(chunk, vdupq_n_u8('['));
        uint8x16_t is_backslash = vceqq_u8(chunk, vdupq_n_u8('\\'));
        uint8x16_t is_rbracket = vceqq_u8(chunk, vdupq_n_u8(']'));
        uint8x16_t is_caret = vceqq_u8(chunk, vdupq_n_u8('^'));
        uint8x16_t is_backtick = vceqq_u8(chunk, vdupq_n_u8('`'));
        uint8x16_t is_lbrace = vceqq_u8(chunk, vdupq_n_u8('{'));
        uint8x16_t is_rbrace = vceqq_u8(chunk, vdupq_n_u8('}'));
        uint8x16_t is_tilde = vceqq_u8(chunk, vdupq_n_u8('~'));

        /* Combine all delimiter checks efficiently
         * Group by OR operations to reduce instruction count */
        uint8x16_t delim_low =
            vorrq_u8(vorrq_u8(is_control, is_space), vorrq_u8(is_quote, is_hash));

        uint8x16_t delim_mid1 =
            vorrq_u8(vorrq_u8(is_lparen, is_rparen), vorrq_u8(is_comma, is_semi));

        uint8x16_t delim_mid2 =
            vorrq_u8(vorrq_u8(is_at, is_lbracket), vorrq_u8(is_backslash, is_rbracket));

        uint8x16_t delim_high = vorrq_u8(vorrq_u8(is_caret, is_backtick),
                                         vorrq_u8(vorrq_u8(is_lbrace, is_rbrace), is_tilde));

        /* Final combination */
        uint8x16_t delim =
            vorrq_u8(vorrq_u8(delim_low, delim_mid1), vorrq_u8(delim_mid2, delim_high));

        /* Convert to bitmasks for quick testing */
        uint64x2_t delim_64 = vreinterpretq_u64_u8(delim);
        uint64x2_t slash_64 = vreinterpretq_u64_u8(is_slash);

        uint64_t delim_low_bits = vgetq_lane_u64(delim_64, 0);
        uint64_t delim_high_bits = vgetq_lane_u64(delim_64, 1);
        uint64_t slash_low_bits = vgetq_lane_u64(slash_64, 0);
        uint64_t slash_high_bits = vgetq_lane_u64(slash_64, 1);

        bool has_delim = (delim_low_bits || delim_high_bits);
        bool has_slash = (slash_low_bits || slash_high_bits);

        /* Fast path: no delimiters or slashes in this chunk */
        if (!has_delim && (!has_slash || result.first_slash)) {
            ptr += 16;
            remaining -= 16;
            continue;
        }

        /* Slow path: found delimiter or slash, find exact position */
        for (int i = 0; i < 16; i++) {
            unsigned char c = (unsigned char) ptr[i];

            /* Check for first slash (only if we haven't found one) */
            if (c == '/' && !result.first_slash) {
                result.first_slash = ptr + i;
            }

            /* Check for delimiter (ends identifier) using lookup table */
            if (is_delimiter(c)) {
                result.end = ptr + i;
                return result;
            }
        }

        ptr += 16;
        remaining -= 16;
    }

    /* Scalar tail: process remaining bytes (0-15) */
    while (ptr < end) {
        unsigned char c = (unsigned char) *ptr;

        /* Check for first slash */
        if (c == '/' && !result.first_slash) {
            result.first_slash = ptr;
        }

        /* Check for delimiter using lookup table */
        if (is_delimiter(c)) {
            result.end = ptr;
            return result;
        }

        ptr++;
    }

    result.end = ptr;
    return result;
}

#else

/* Scalar fallback */
edn_identifier_scan_result_t edn_simd_scan_identifier(const char* ptr, const char* end) {
    edn_identifier_scan_result_t result = {.end = ptr, .first_slash = NULL};

    while (ptr < end) {
        char c = *ptr;
        if (is_delimiter(c)) {
            result.end = ptr;
            return result;
        }
        if (c == '/' && !result.first_slash) {
            result.first_slash = ptr;
        }
        ptr++;
    }

    result.end = ptr;
    return result;
}

#endif
