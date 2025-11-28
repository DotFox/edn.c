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

#if defined(__wasm__) && defined(__wasm_simd128__)

#include <wasm_simd128.h>

static inline const char* edn_simd_find_newline_wasm(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with WASM SIMD128 */
    while (ptr + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);
        v128_t newline = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\n'));

        int mask = wasm_i8x16_bitmask(newline);
        if (mask != 0) {
            int offset = CTZ((unsigned int) mask);
            return ptr + offset;
        }
        ptr += 16;
    }

    while (ptr < end && *ptr != '\n') {
        ptr++;
    }
    return ptr;
}

const char* edn_simd_skip_whitespace(const char* ptr, const char* end) {
    while (ptr < end) {
        if (*ptr == ';') {
            ptr++;
            ptr = edn_simd_find_newline_wasm(ptr, end);
            if (ptr < end && *ptr == '\n') {
                ptr++;
            }
            continue;
        }

        if (ptr + 16 <= end) {
            v128_t chunk = wasm_v128_load((const v128_t*) ptr);

            /* Range check for 0x09-0x0D (tab, LF, VT, FF, CR)
             * Remap to 0x76-0x7C then signed compare with 0x75 and 0x7D */
            v128_t shifted1 = wasm_i8x16_add(chunk, wasm_i8x16_splat(0x7F - 0x0D));
            v128_t in_range1 =
                wasm_v128_and(wasm_i8x16_gt(shifted1, wasm_i8x16_splat(0x7F - 0x0D + 0x09 - 1)),
                              wasm_i8x16_lt(shifted1, wasm_i8x16_splat((int8_t) 0x80)));

            /* Range check for 0x1C-0x20 (FS, GS, RS, US, space)
             * Remap to 0x7B-0x7F then signed compare with 0x7A */
            v128_t shifted2 = wasm_i8x16_add(chunk, wasm_i8x16_splat(0x7F - 0x20));
            v128_t in_range2 = wasm_i8x16_gt(shifted2, wasm_i8x16_splat(0x7F - 0x20 + 0x1C - 1));

            /* Individual check for comma */
            v128_t is_comma = wasm_i8x16_eq(chunk, wasm_i8x16_splat(0x2C));

            v128_t is_ws = wasm_v128_or(wasm_v128_or(in_range1, in_range2), is_comma);

            int mask = wasm_i8x16_bitmask(is_ws);
            if (mask == 0xFFFF) {
                ptr += 16;
                continue;
            }
        }

        unsigned char c = (unsigned char) *ptr;
        if ((c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x20) || c == ',') {
            ptr++;
        } else {
            break;
        }
    }

    return ptr;
}

#elif defined(__aarch64__) || defined(_M_ARM64)
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

            /* Range check for 0x09-0x0D (tab, LF, VT, FF, CR)
             * Remap to 0x76-0x7A then signed compare with 0x75 */
            int8x16_t shifted1 = vreinterpretq_s8_u8(vaddq_u8(chunk, vdupq_n_u8(0x7F - 0x0D)));
            uint8x16_t in_range1 = vandq_u8(
                vreinterpretq_u8_s8(vcgtq_s8(shifted1, vdupq_n_s8(0x7F - 0x0D + 0x09 - 1))),
                vreinterpretq_u8_s8(vcltq_s8(shifted1, vdupq_n_s8((int8_t) 0x80))));

            /* Range check for 0x1C-0x20 (FS, GS, RS, US, space)
             * Remap to 0x7B-0x7F then signed compare with 0x7A */
            int8x16_t shifted2 = vreinterpretq_s8_u8(vaddq_u8(chunk, vdupq_n_u8(0x7F - 0x20)));
            uint8x16_t in_range2 =
                vreinterpretq_u8_s8(vcgtq_s8(shifted2, vdupq_n_s8(0x7F - 0x20 + 0x1C - 1)));

            /* Individual check for comma */
            uint8x16_t is_comma = vceqq_u8(chunk, vdupq_n_u8(0x2C));

            uint8x16_t is_ws = vorrq_u8(vorrq_u8(in_range1, in_range2), is_comma);

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
        if ((c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x20) || c == ',') {
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

            /* Range check for 0x09-0x0D (tab, LF, VT, FF, CR)
             * Remap to 0x76-0x7A then signed compare with 0x75 */
            __m128i shifted1 = _mm_add_epi8(chunk, _mm_set1_epi8(0x7F - 0x0D));
            __m128i in_range1 =
                _mm_and_si128(_mm_cmpgt_epi8(shifted1, _mm_set1_epi8(0x7F - 0x0D + 0x09 - 1)),
                              _mm_cmplt_epi8(shifted1, _mm_set1_epi8((char) 0x80)));

            /* Range check for 0x1C-0x20 (FS, GS, RS, US, space)
             * Remap to 0x7B-0x7F then signed compare with 0x7A */
            __m128i shifted2 = _mm_add_epi8(chunk, _mm_set1_epi8(0x7F - 0x20));
            __m128i in_range2 = _mm_cmpgt_epi8(shifted2, _mm_set1_epi8(0x7F - 0x20 + 0x1C - 1));

            /* Individual check for comma */
            __m128i is_comma = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(','));

            __m128i is_ws = _mm_or_si128(_mm_or_si128(in_range1, in_range2), is_comma);

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
        if ((c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x20) || c == ',') {
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
        /* Whitespace: 0x09-0x0D (tab, LF, VT, FF, CR), 0x1C-0x20 (FS, GS, RS, US, space), comma */
        unsigned char uc = (unsigned char) c;
        if ((uc >= 0x09 && uc <= 0x0D) || (uc >= 0x1C && uc <= 0x20) || c == ',') {
            ptr++;
        } else {
            break;
        }
    }
    return ptr;
}

#endif

#if defined(__wasm__) && defined(__wasm_simd128__)

const char* edn_simd_find_quote(const char* ptr, const char* end, bool* out_has_backslash) {
    bool has_backslash = false;

    while (ptr + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);
        v128_t quote_v = wasm_i8x16_eq(chunk, wasm_i8x16_splat('"'));
        v128_t bs_v = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\\'));
        v128_t specials = wasm_v128_or(quote_v, bs_v);

        int bs_mask = wasm_i8x16_bitmask(bs_v);
        int special_mask = wasm_i8x16_bitmask(specials);

        if (special_mask == 0) {
            ptr += 16;
            continue;
        }

        int idx = CTZ((unsigned int) special_mask);
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

        if (out_has_backslash) {
            *out_has_backslash = has_backslash || (bs_mask != 0);
        }
        return ptr + idx;
    }

    while (ptr < end) {
        char c = *ptr;
        if (c == '\\') {
            has_backslash = true;
            if (ptr + 1 >= end) {
                return NULL;
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

#elif defined(__aarch64__) || defined(_M_ARM64)

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
#if defined(__wasm__) && defined(__wasm_simd128__)

const char* edn_simd_scan_digits(const char* ptr, const char* end) {
    while (ptr + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);

        /* Check if all bytes are digits ('0'-'9' = 0x30-0x39)
         * Remap to 0x76-0x7F then signed compare with 0x75 */
        v128_t shifted = wasm_i8x16_add(chunk, wasm_i8x16_splat(0x7F - '9'));
        v128_t is_digit = wasm_i8x16_gt(shifted, wasm_i8x16_splat(0x7F - '9' + '0' - 1));

        int mask = wasm_i8x16_bitmask(is_digit);

        if (mask == 0xFFFF) {
            ptr += 16;
            continue;
        }

        if (mask == 0) {
            return ptr;
        }

        unsigned int non_mask = (~mask) & 0xFFFF;
        int idx = CTZ(non_mask);

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

#elif defined(__aarch64__) || defined(_M_ARM64)

const char* edn_simd_scan_digits(const char* ptr, const char* end) {
    /* Process 16 bytes at a time with NEON */
    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);

        /* Check if all bytes are digits ('0'-'9' = 0x30-0x39)
         * Remap to 0x76-0x7F then signed compare with 0x75 */
        int8x16_t shifted = vreinterpretq_s8_u8(vaddq_u8(chunk, vdupq_n_u8(0x7F - '9')));
        uint8x16_t is_digit =
            vreinterpretq_u8_s8(vcgtq_s8(shifted, vdupq_n_s8(0x7F - '9' + '0' - 1)));

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

        /* Check if all bytes are digits ('0'-'9' = 0x30-0x39)
         * Remap to 0x76-0x7F then signed compare with 0x75 */
        __m128i shifted = _mm_add_epi8(chunk, _mm_set1_epi8(0x7F - '9'));
        __m128i is_digit = _mm_cmpgt_epi8(shifted, _mm_set1_epi8(0x7F - '9' + '0' - 1));

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

#if defined(__wasm__) && defined(__wasm_simd128__)

edn_identifier_scan_result_t edn_simd_scan_identifier(const char* ptr, const char* end) {
    edn_identifier_scan_result_t result = {
        .end = ptr, .first_slash = NULL, .has_adjacent_colons = false};

    size_t remaining = end - ptr;
    bool prev_was_colon = false;

    if (remaining <= 8) {
        while (ptr < end) {
            char c = *ptr;
            if (is_delimiter(c)) {
                result.end = ptr;
                return result;
            }
            if (c == ':') {
                if (prev_was_colon) {
                    result.has_adjacent_colons = true;
                }
                prev_was_colon = true;
            } else {
                prev_was_colon = false;
            }
            if (c == '/' && !result.first_slash) {
                result.first_slash = ptr;
            }
            ptr++;
        }
        result.end = ptr;
        return result;
    }

    while (remaining >= 16) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);

        v128_t is_slash = wasm_i8x16_eq(chunk, wasm_i8x16_splat('/'));
        v128_t is_colon = wasm_i8x16_eq(chunk, wasm_i8x16_splat(':'));

        v128_t is_control = wasm_u8x16_lt(chunk, wasm_i8x16_splat(0x20));

        v128_t is_space = wasm_i8x16_eq(chunk, wasm_i8x16_splat(' '));
        v128_t is_quote = wasm_i8x16_eq(chunk, wasm_i8x16_splat('"'));
        v128_t is_hash = wasm_i8x16_eq(chunk, wasm_i8x16_splat('#'));
        v128_t is_lparen = wasm_i8x16_eq(chunk, wasm_i8x16_splat('('));
        v128_t is_rparen = wasm_i8x16_eq(chunk, wasm_i8x16_splat(')'));
        v128_t is_comma = wasm_i8x16_eq(chunk, wasm_i8x16_splat(','));
        v128_t is_semi = wasm_i8x16_eq(chunk, wasm_i8x16_splat(';'));
        v128_t is_at = wasm_i8x16_eq(chunk, wasm_i8x16_splat('@'));
        v128_t is_lbracket = wasm_i8x16_eq(chunk, wasm_i8x16_splat('['));
        v128_t is_backslash = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\\'));
        v128_t is_rbracket = wasm_i8x16_eq(chunk, wasm_i8x16_splat(']'));
        v128_t is_caret = wasm_i8x16_eq(chunk, wasm_i8x16_splat('^'));
        v128_t is_backtick = wasm_i8x16_eq(chunk, wasm_i8x16_splat('`'));
        v128_t is_lbrace = wasm_i8x16_eq(chunk, wasm_i8x16_splat('{'));
        v128_t is_rbrace = wasm_i8x16_eq(chunk, wasm_i8x16_splat('}'));
        v128_t is_tilde = wasm_i8x16_eq(chunk, wasm_i8x16_splat('~'));

        v128_t delim_low =
            wasm_v128_or(wasm_v128_or(is_control, is_space), wasm_v128_or(is_quote, is_hash));

        v128_t delim_mid1 =
            wasm_v128_or(wasm_v128_or(is_lparen, is_rparen), wasm_v128_or(is_comma, is_semi));

        v128_t delim_mid2 =
            wasm_v128_or(wasm_v128_or(is_at, is_lbracket), wasm_v128_or(is_backslash, is_rbracket));

        v128_t delim_high =
            wasm_v128_or(wasm_v128_or(is_caret, is_backtick),
                         wasm_v128_or(wasm_v128_or(is_lbrace, is_rbrace), is_tilde));

        v128_t delim =
            wasm_v128_or(wasm_v128_or(delim_low, delim_mid1), wasm_v128_or(delim_mid2, delim_high));

        int delim_mask = wasm_i8x16_bitmask(delim);
        int slash_mask = wasm_i8x16_bitmask(is_slash);
        int colon_mask = wasm_i8x16_bitmask(is_colon);

        bool has_delim = (delim_mask != 0);
        bool has_slash = (slash_mask != 0);
        bool has_colon = (colon_mask != 0);

        /* Check for adjacent colons using bit manipulation */
        if (has_colon && !result.has_adjacent_colons) {
            /* Check if prev_was_colon and first bit is colon */
            if (prev_was_colon && (colon_mask & 1)) {
                result.has_adjacent_colons = true;
            }
            /* Check for adjacent colons within the chunk: (mask & (mask >> 1)) != 0 */
            if ((colon_mask & (colon_mask >> 1)) != 0) {
                result.has_adjacent_colons = true;
            }
            /* Update prev_was_colon for next iteration */
            prev_was_colon = (colon_mask & 0x8000) != 0;
        } else if (!has_colon) {
            prev_was_colon = false;
        }

        if (!has_delim && (!has_slash || result.first_slash)) {
            ptr += 16;
            remaining -= 16;
            continue;
        }

        for (int i = 0; i < 16; i++) {
            unsigned char c = (unsigned char) ptr[i];

            if (c == '/' && !result.first_slash) {
                result.first_slash = ptr + i;
            }

            if (is_delimiter(c)) {
                result.end = ptr + i;
                return result;
            }
        }

        ptr += 16;
        remaining -= 16;
    }

    while (ptr < end) {
        unsigned char c = (unsigned char) *ptr;

        if (c == ':') {
            if (prev_was_colon) {
                result.has_adjacent_colons = true;
            }
            prev_was_colon = true;
        } else {
            prev_was_colon = false;
        }

        if (c == '/' && !result.first_slash) {
            result.first_slash = ptr;
        }

        if (is_delimiter(c)) {
            result.end = ptr;
            return result;
        }

        ptr++;
    }

    result.end = ptr;
    return result;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

edn_identifier_scan_result_t edn_simd_scan_identifier(const char* ptr, const char* end) {
    edn_identifier_scan_result_t result = {
        .end = ptr, .first_slash = NULL, .has_adjacent_colons = false};

    size_t remaining = end - ptr;
    bool prev_was_colon = false;

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
            if (c == ':') {
                if (prev_was_colon) {
                    result.has_adjacent_colons = true;
                }
                prev_was_colon = true;
            } else {
                prev_was_colon = false;
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

        /* Check for colon (for adjacent colon detection) */
        uint8x16_t is_colon = vceqq_u8(chunk, vdupq_n_u8(':'));

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

        /* Check for adjacent colons using bitmask */
        uint16_t colon_mask = edn_neon_movemask_u8(is_colon);
        bool has_colon = (colon_mask != 0);

        if (has_colon && !result.has_adjacent_colons) {
            /* Check if prev_was_colon and first bit is colon */
            if (prev_was_colon && (colon_mask & 1)) {
                result.has_adjacent_colons = true;
            }
            /* Check for adjacent colons within the chunk: (mask & (mask >> 1)) != 0 */
            if ((colon_mask & (colon_mask >> 1)) != 0) {
                result.has_adjacent_colons = true;
            }
            /* Update prev_was_colon for next iteration */
            prev_was_colon = (colon_mask & 0x8000) != 0;
        } else if (!has_colon) {
            prev_was_colon = false;
        }

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

        if (c == ':') {
            if (prev_was_colon) {
                result.has_adjacent_colons = true;
            }
            prev_was_colon = true;
        } else {
            prev_was_colon = false;
        }

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
    edn_identifier_scan_result_t result = {
        .end = ptr, .first_slash = NULL, .has_adjacent_colons = false};
    bool prev_was_colon = false;

    while (ptr < end) {
        char c = *ptr;
        if (is_delimiter(c)) {
            result.end = ptr;
            return result;
        }
        if (c == ':') {
            if (prev_was_colon) {
                result.has_adjacent_colons = true;
            }
            prev_was_colon = true;
        } else {
            prev_was_colon = false;
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
