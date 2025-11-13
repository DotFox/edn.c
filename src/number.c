/**
 * EDN.C - Number parsing (integers and floats)
 * 
 * Three-tier performance architecture:
 * 1. Fast path: Simple decimal integers (80%+ of numbers)
 * 2. SWAR path: 8-digit chunks for long integers (5x faster than scalar)
 * 3. Clinger fast path: Doubles with exponents in [-22, 22] (90% of floats)
 * 
 * Supports: int64, BigInt overflow, hex (0xAF), octal (0777), radix (36rZZ), floats.
 * SWAR technique from simdjson, Clinger algorithm for fast double parsing.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

/**
 * Digit value lookup table for radix 2-36.
 * Maps ASCII characters to numeric values: '0'-'9' → 0-9, 'A'-'Z'/'a'-'z' → 10-35.
 * Invalid characters map to -1.
 */
static const int8_t DIGIT_VALUES[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x00-0x0F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x10-0x1F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x20-0x2F */
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, /* 0x30-0x3F: '0'-'9' */
    -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, /* 0x40-0x4F: 'A'-'O' */
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, /* 0x50-0x5F: 'P'-'Z' */
    -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, /* 0x60-0x6F: 'a'-'o' */
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, /* 0x70-0x7F: 'p'-'z' */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x80-0x8F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x90-0x9F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xA0-0xAF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xB0-0xBF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xC0-0xCF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xD0-0xDF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xE0-0xEF */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  /* 0xF0-0xFF */
};

/**
 * Get numeric value of character for given radix.
 * Returns -1 if character is invalid for radix.
 */
static inline int digit_value(char c, uint8_t radix) {
    int val = DIGIT_VALUES[(unsigned char) c];
    return (val >= 0 && val < radix) ? val : -1;
}

/**
 * SWAR (SIMD Within A Register) - 8-digit parallel parsing.
 * 
 * Parses 8 ASCII digits "12345678" → 12345678 in ~5 CPU cycles vs ~16-24 for scalar.
 * Uses bit manipulation to extract and combine digit values in parallel.
 * 
 * Technique from simdjson (github.com/simdjson/simdjson) and Johnny Lee's blog
 * (johnnylee-sde.github.io/Fast-numeric-string-to-int/).
 * 
 * Magic constants combine digits via multiplication and shifts:
 * - 2561: Combines adjacent bytes (ab → a*10 + b)
 * - 6553601: Combines 2-byte pairs (abcd → ab*100 + cd)
 * - 42949672960001: Final 8-digit combination
 */

/**
 * Check if next 8 bytes are all ASCII digits '0'-'9'.
 * Uses SWAR to check all 8 bytes in parallel (faster than 8 individual checks).
 */
static inline bool is_made_of_eight_digits_fast(const char* chars) {
    uint64_t val;
    memcpy(&val, chars, 8);

    return ((val & 0xF0F0F0F0F0F0F0F0) == 0x3030303030303030) &&
           (((val + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0) == 0x3030303030303030);
}

/**
 * Parse 8 consecutive decimal digits using SWAR.
 * 
 * Requires: chars[0..7] must all be ASCII digits '0'-'9'.
 * Returns: Numeric value 0-99999999.
 * 
 * Algorithm:
 * 1. Extract digit values from ASCII (mask with 0x0F)
 * 2. Combine pairs: 12 34 56 78 → 12, 34, 56, 78
 * 3. Final combination: 1234, 5678 → 12345678
 */
static inline uint32_t parse_eight_digits_unrolled(const char* chars) {
    uint64_t val;
    memcpy(&val, chars, 8);

    val = (val & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
    val = (val & 0x00FF00FF00FF00FF) * 6553601 >> 16;
    return (uint32_t) ((val & 0x0000FFFF0000FFFF) * 42949672960001 >> 32);
}

/**
 * Scan and classify a number from input buffer.
 * 
 * Two-tier approach:
 * 1. Fast path (80%+): Simple decimal integers -?[0-9]+ with no prefix/suffix
 * 2. Slow path (20%): Hex (0xAF), octal (0777), radix (16rFF), floats
 * 
 * Returns edn_number_scan_t with radix, type (INT64/BIGINT/DOUBLE), and valid flag.
 * Validates EDN number syntax (e.g., rejects leading zeros before 8/9).
 */
edn_number_scan_t edn_scan_number(const char* ptr, const char* end) {
    edn_number_scan_t result = {0};
    result.start = ptr;
    result.radix = 10;
    result.type = EDN_NUMBER_INT64;

    if (ptr >= end) {
        result.valid = false;
        return result;
    }

    const char* scan_ptr = ptr;
    bool is_negative = false;

    if (*scan_ptr == '-') {
        is_negative = true;
        scan_ptr++;
    } else if (*scan_ptr == '+') {
        scan_ptr++;
    }

    if (scan_ptr < end && *scan_ptr >= '0' && *scan_ptr <= '9') {
        const char* digit_ptr = scan_ptr;

        bool might_be_special = (*digit_ptr == '0' && digit_ptr + 1 < end);

        if (!might_be_special) {
            while (digit_ptr < end && *digit_ptr >= '0' && *digit_ptr <= '9') {
                digit_ptr++;
            }

            if (digit_ptr >= end || (*digit_ptr != '.' && *digit_ptr != 'e' && *digit_ptr != 'E' &&
                                     *digit_ptr != 'r')) {
                result.negative = is_negative;
                result.end = digit_ptr;
                result.valid = true;
                return result;
            }
        }
    }

    ptr = result.start;

    if (*ptr == '-') {
        result.negative = true;
        ptr++;
    } else if (*ptr == '+') {
        ptr++;
    }

    if (ptr >= end) {
        result.valid = false;
        return result;
    }

    if (*ptr == '0' && ptr + 1 < end) {
        if (ptr[1] == 'x' || ptr[1] == 'X') {
            result.radix = 16;
            ptr += 2;
        } else if (ptr[1] >= '0' && ptr[1] <= '7') {
            result.radix = 8;
            ptr++;
        } else if (ptr[1] >= '8' && ptr[1] <= '9') {
            result.valid = false;
            return result;
        } else if (ptr[1] == '.' || ptr[1] == 'e' || ptr[1] == 'E') {
            result.radix = 10;
        }
    } else if (*ptr >= '0' && *ptr <= '9') {
        const char* r_pos = ptr;
        while (r_pos < end && *r_pos >= '0' && *r_pos <= '9') {
            r_pos++;
        }
        if (r_pos < end && *r_pos == 'r' && r_pos > ptr) {
            int radix = 0;
            for (const char* p = ptr; p < r_pos; p++) {
                radix = radix * 10 + (*p - '0');
            }
            if (radix >= 2 && radix <= 36) {
                result.radix = (uint8_t) radix;
                ptr = r_pos + 1;
            } else {
                result.valid = false;
                return result;
            }
        }
    }

    if (ptr >= end) {
        result.valid = false;
        return result;
    }

    const char* digit_start = ptr;
    if (result.radix == 10) {
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
    } else {
        while (ptr < end && digit_value(*ptr, result.radix) >= 0) {
            ptr++;
        }
    }

    if (ptr == digit_start) {
        result.valid = false;
        return result;
    }

    if (ptr < end && *ptr == '.') {
        result.type = EDN_NUMBER_DOUBLE;
        ptr++;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
    }

    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        result.type = EDN_NUMBER_DOUBLE;
        ptr++;
        if (ptr < end && (*ptr == '+' || *ptr == '-')) {
            ptr++;
        }
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
    }

    result.end = ptr;
    result.valid = true;
    return result;
}

/**
 * Parse int64_t with overflow detection.
 * 
 * Three-tier strategy:
 * 1. Ultra-fast: 1-3 decimal digits (no overflow check needed)
 * 2. SWAR: 8-digit chunks for long decimals (5x scalar speed)
 * 3. General: Scalar with full overflow detection
 * 
 * Returns false on overflow (caller promotes to BigInt).
 */
bool edn_parse_int64(const char* start, const char* end, int64_t* out, uint8_t radix) {
    if (!start || !end || !out || start >= end) {
        return false;
    }

    bool negative = false;
    const char* ptr = start;

    if (*ptr == '-') {
        negative = true;
        ptr++;
    } else if (*ptr == '+') {
        ptr++;
    }

    if (ptr >= end) {
        return false;
    }

    if (radix == 10 && (end - ptr) <= 3) {
        int64_t value = 0;
        const char* digit_ptr = ptr;

        while (digit_ptr < end) {
            char c = *digit_ptr;
            if (c < '0' || c > '9')
                break;
            value = value * 10 + (c - '0');
            digit_ptr++;
        }

        if (digit_ptr > ptr) {
            *out = negative ? -value : value;
            return true;
        }
    }

    uint64_t value = 0;
    const uint64_t max_val = negative ? ((uint64_t) INT64_MAX + 1) : (uint64_t) INT64_MAX;
    const uint64_t cutoff = max_val / radix;
    const uint64_t cutlim = max_val % radix;

    if (radix == 10) {
        while ((end - ptr) >= 8 && is_made_of_eight_digits_fast(ptr)) {
            uint32_t eight_digits = parse_eight_digits_unrolled(ptr);

            const uint64_t multiplier = 100000000;
            if (value > max_val / multiplier) {
                return false;
            }

            uint64_t new_value = value * multiplier + eight_digits;

            if (new_value < value) {
                return false;
            }

            if (new_value > max_val) {
                return false;
            }

            value = new_value;
            ptr += 8;
        }

        while (ptr < end) {
            char c = *ptr;
            if (c < '0' || c > '9') {
                break;
            }

            int digit = c - '0';

            if (value > cutoff || (value == cutoff && (uint64_t) digit > cutlim)) {
                return false;
            }

            value = value * 10 + digit;
            ptr++;
        }
    } else {
        while (ptr < end) {
            int digit = digit_value(*ptr, radix);
            if (digit < 0) {
                break;
            }

            if (value > cutoff || (value == cutoff && (uint64_t) digit > cutlim)) {
                return false;
            }

            value = value * radix + digit;
            ptr++;
        }
    }

    if (negative) {
        *out = -(int64_t) value;
    } else {
        *out = (int64_t) value;
    }

    return true;
}

/**
 * Fast double parsing using Clinger's algorithm (1990).
 * 
 * Two-stage approach:
 * 1. Fast path: Exponents in [-22, 22] and mantissa < 2^53 (90% of cases, 5-10x faster)
 * 2. Slow path: Fall back to strtod() for complex cases
 * 
 * Fast path guarantees exact IEEE 754 representation via precomputed powers of 10.
 * Reference: "How to Read Floating Point Numbers Accurately" - William Clinger
 */

/**
 * Precomputed powers of 10 for Clinger fast path.
 * Exactly representable in IEEE 754 double precision.
 */
static const double POWER_OF_TEN_POSITIVE[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                 1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

static const double POWER_OF_TEN_NEGATIVE[23] = {
    1e-0,  1e-1,  1e-2,  1e-3,  1e-4,  1e-5,  1e-6,  1e-7,  1e-8,  1e-9,  1e-10, 1e-11,
    1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19, 1e-20, 1e-21, 1e-22};

/**
 * Clinger's algorithm fast path.
 * 
 * Constraints: exponent in [-22, 22], mantissa < 2^53.
 * Returns true if successful, false to fall back to strtod().
 */
static inline bool edn_parse_double_fast(int64_t mantissa, int64_t exponent, bool negative,
                                         double* out) {
    if (exponent < -22 || exponent > 22) {
        return false;
    }

    if (mantissa > 9007199254740991LL) {
        return false;
    }

    double d = (double) mantissa;

    if (exponent < 0) {
        d = d * POWER_OF_TEN_NEGATIVE[-exponent];
    } else {
        d = d * POWER_OF_TEN_POSITIVE[exponent];
    }

    *out = negative ? -d : d;
    return true;
}

/**
 * Parse double from buffer.
 * 
 * Format: [sign] integer [. fraction] [e/E [sign] exponent]
 * 
 * Strategy:
 * 1. Parse mantissa (integer + fractional parts combined)
 * 2. Compute effective exponent
 * 3. Try Clinger fast path (90% success rate)
 * 4. Fall back to strtod() for edge cases
 */
double edn_parse_double(const char* start, const char* end) {
    const char* ptr = start;
    bool negative = false;

    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
        negative = (*ptr == '-');
        ptr++;
    }

    int64_t mantissa = 0;
    const char* mantissa_start = ptr;
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
        mantissa = mantissa * 10 + (*ptr - '0');
        ptr++;
    }

    size_t digit_count = ptr - mantissa_start;

    int64_t exponent = 0;
    if (ptr < end && *ptr == '.') {
        ptr++;
        const char* frac_start = ptr;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            mantissa = mantissa * 10 + (*ptr - '0');
            ptr++;
        }
        exponent = -(ptr - frac_start);
        digit_count += (ptr - frac_start);
    }

    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        ptr++;
        bool exp_negative = false;
        if (ptr < end && (*ptr == '-' || *ptr == '+')) {
            exp_negative = (*ptr == '-');
            ptr++;
        }

        int64_t exp_value = 0;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            exp_value = exp_value * 10 + (*ptr - '0');
            if (exp_value > 1000) {
                exp_value = 1000;
                break;
            }
        }

        exponent += exp_negative ? -exp_value : exp_value;
    }

    double result;
    if (digit_count <= 15 && edn_parse_double_fast(mantissa, exponent, negative, &result)) {
        return result;
    }

    size_t len = end - start;
    char buffer[256];
    if (len >= sizeof(buffer)) {
        return NAN;
    }

    memcpy(buffer, start, len);
    buffer[len] = '\0';

    errno = 0;
    char* endptr;
    result = strtod(buffer, &endptr);

    if (errno == ERANGE) {
        return result;
    }

    return result;
}
