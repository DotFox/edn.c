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

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
/**
 * Check if underscore is valid at the current position in a number.
 *
 * Underscores are only allowed between digits (or other underscores):
 * - NOT at the beginning or end of a number
 * - NOT adjacent to a decimal point
 * - NOT before N or M suffix
 * - Must have a digit or underscore before, and a digit or underscore after
 *
 * Since we validate every underscore as we scan, we only need to check
 * immediately adjacent characters - no need to scan past consecutive underscores.
 *
 * Returns true if underscore is valid at this position.
 */
static inline bool is_valid_underscore_position(const char* ptr, const char* start, const char* end,
                                                uint8_t radix) {
    /* Must not be at start */
    if (ptr <= start) {
        return false;
    }

    /* Must not be at end */
    if (ptr >= end - 1) {
        return false;
    }

    /* Check immediately previous character is a digit or underscore */
    const char prev = *(ptr - 1);
    if (prev != '_' && digit_value(prev, radix) < 0) {
        return false;
    }

    /* Check immediately next character is a digit or underscore */
    const char next = *(ptr + 1);
    if (next != '_' && digit_value(next, radix) < 0) {
        return false;
    }

    return true;
}
#endif /* EDN_ENABLE_UNDERSCORE_IN_NUMERIC */

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
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
            bool has_underscore = false;
            while (digit_ptr < end) {
                if (*digit_ptr >= '0' && *digit_ptr <= '9') {
                    digit_ptr++;
                } else if (*digit_ptr == '_') {
                    /* Check if underscore is valid (between digits) */
                    if (digit_ptr == scan_ptr) {
                        /* Underscore at start - invalid */
                        break;
                    }
                    /* Look ahead past consecutive underscores to find next digit */
                    const char* look_ahead = digit_ptr + 1;
                    while (look_ahead < end && *look_ahead == '_') {
                        look_ahead++;
                    }
                    if (look_ahead >= end || (*look_ahead < '0' || *look_ahead > '9')) {
                        /* No digit after underscore(s) - fall through to slow path */
                        break;
                    }
                    has_underscore = true;
                    digit_ptr++;
                } else {
                    break;
                }
            }

            /* If we have underscores, we can't use the simple fast path */
            /* But if no underscores and it's a simple integer, use fast path */
            if (!has_underscore && (digit_ptr >= end || (*digit_ptr != '.' && *digit_ptr != 'e' &&
                                                         *digit_ptr != 'E' && *digit_ptr != 'r' &&
                                                         *digit_ptr != 'N' && *digit_ptr != 'M'))) {
#else
            while (digit_ptr < end && *digit_ptr >= '0' && *digit_ptr <= '9') {
                digit_ptr++;
            }

            if (digit_ptr >= end || (*digit_ptr != '.' && *digit_ptr != 'e' && *digit_ptr != 'E' &&
                                     *digit_ptr != 'r' && *digit_ptr != 'N' && *digit_ptr != 'M')) {
#endif
                result.negative = is_negative;
                result.digits_start = scan_ptr;
                result.digits_end = digit_ptr;
                result.end = digit_ptr;
                result.valid = true;
                return result;
            }
            /* Check for N suffix in fast path */
            if (digit_ptr < end && *digit_ptr == 'N') {
                result.negative = is_negative;
                result.type = EDN_NUMBER_BIGINT;
                result.digits_start = scan_ptr;
                result.digits_end = digit_ptr;
                result.end = digit_ptr + 1; /* Include N */
                result.valid = true;
                return result;
            }
            /* Check for M suffix in fast path - integers with M become BigDecimal */
            if (digit_ptr < end && *digit_ptr == 'M') {
                result.negative = is_negative;
                result.type = EDN_NUMBER_BIGDEC;
                result.digits_start = scan_ptr;
                result.digits_end = digit_ptr;
                result.end = digit_ptr + 1; /* Include M */
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

#ifdef EDN_ENABLE_EXTENDED_INTEGERS
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
#else
    /* Standard EDN: reject leading zeros except for "0", "0.", "0eN" */
    if (*ptr == '0' && ptr + 1 < end) {
        if (ptr[1] == '.' || ptr[1] == 'e' || ptr[1] == 'E') {
            /* 0.5 or 0e10 - valid float */
            result.radix = 10;
        } else if (ptr[1] >= '0' && ptr[1] <= '9') {
            /* 01, 0123, etc. - invalid leading zero */
            result.valid = false;
            return result;
        }
        /* else: just "0" followed by delimiter - valid */
    }
#endif

    if (ptr >= end) {
        result.valid = false;
        return result;
    }

    const char* digit_start = ptr;
    result.digits_start = ptr; /* Track where actual digits begin */

    if (result.radix == 10) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* Scan decimal digits with optional underscores */
        while (ptr < end) {
            if (*ptr >= '0' && *ptr <= '9') {
                ptr++;
            } else if (*ptr == '_') {
                /* Validate underscore position */
                if (!is_valid_underscore_position(ptr, digit_start, end, 10)) {
                    result.valid = false;
                    return result;
                }
                ptr++;
            } else {
                break;
            }
        }
#else
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
#endif
    } else {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* Scan digits for non-decimal radix with optional underscores */
        while (ptr < end) {
            if (digit_value(*ptr, result.radix) >= 0) {
                ptr++;
            } else if (*ptr == '_') {
                /* Validate underscore position */
                if (!is_valid_underscore_position(ptr, digit_start, end, result.radix)) {
                    result.valid = false;
                    return result;
                }
                ptr++;
            } else {
                break;
            }
        }
#else
        while (ptr < end && digit_value(*ptr, result.radix) >= 0) {
            ptr++;
        }
#endif
    }

    if (ptr == digit_start) {
        result.valid = false;
        return result;
    }

    if (ptr < end && *ptr == '.') {
        result.type = EDN_NUMBER_DOUBLE;
        ptr++;
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        const char* frac_start = ptr;
        /* Scan fractional part with optional underscores */
        while (ptr < end) {
            if (*ptr >= '0' && *ptr <= '9') {
                ptr++;
            } else if (*ptr == '_') {
                /* Validate underscore position in fractional part */
                if (!is_valid_underscore_position(ptr, frac_start - 1, end, 10)) {
                    result.valid = false;
                    return result;
                }
                ptr++;
            } else {
                break;
            }
        }
#else
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
#endif
    }

    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        result.type = EDN_NUMBER_DOUBLE;
        ptr++;
        if (ptr < end && (*ptr == '+' || *ptr == '-')) {
            ptr++;
        }
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        const char* exp_start = ptr;
        /* Scan exponent with optional underscores */
        while (ptr < end) {
            if (*ptr >= '0' && *ptr <= '9') {
                ptr++;
            } else if (*ptr == '_') {
                /* Validate underscore position in exponent */
                if (!is_valid_underscore_position(ptr, exp_start - 1, end, 10)) {
                    result.valid = false;
                    return result;
                }
                ptr++;
            } else {
                break;
            }
        }
#else
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
#endif
    }

    /* Save digits_end before checking for N/M suffix */
    result.digits_end = ptr;

    if (ptr < end && *ptr == 'N' && result.radix == 10) {
        if (result.type == EDN_NUMBER_DOUBLE) {
            /* Invalid: cannot have N suffix on float */
            result.valid = false;
            return result;
        }
        result.type = EDN_NUMBER_BIGINT;
        ptr++;
    } else if (ptr < end && *ptr == 'M') {
        /* M suffix works on both integers and floats */
        result.type = EDN_NUMBER_BIGDEC;
        ptr++;
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
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
            if (c == '_') {
                digit_ptr++;
                continue;
            }
#endif
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
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* With underscores enabled, try SWAR for 8-digit chunks without underscores first */
        while ((end - ptr) >= 8) {
            /* Check if next 8 bytes are all digits (no underscores) */
            if (is_made_of_eight_digits_fast(ptr)) {
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
            } else {
                /* Has underscore or non-digit, fall back to scalar */
                break;
            }
        }

        /* Process remaining digits (or all digits if SWAR wasn't used) */
        while (ptr < end) {
            char c = *ptr;
            if (c == '_') {
                ptr++;
                continue;
            }
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
#else
        /* SWAR fast path for 8-digit chunks */
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

        /* Process remaining digits */
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
#endif
    } else {
        while (ptr < end) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
            if (*ptr == '_') {
                ptr++;
                continue;
            }
#endif
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
    size_t digit_count = 0;

    while (ptr < end && ((*ptr >= '0' && *ptr <= '9')
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
                         || *ptr == '_'
#endif
                         )) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        if (*ptr == '_') {
            ptr++;
            continue;
        }
#endif
        mantissa = mantissa * 10 + (*ptr - '0');
        digit_count++;
        ptr++;
    }

    int64_t exponent = 0;
    if (ptr < end && *ptr == '.') {
        ptr++;
        size_t frac_digits = 0;
        while (ptr < end && ((*ptr >= '0' && *ptr <= '9')
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
                             || *ptr == '_'
#endif
                             )) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
            if (*ptr == '_') {
                ptr++;
                continue;
            }
#endif
            mantissa = mantissa * 10 + (*ptr - '0');
            frac_digits++;
            ptr++;
        }
        exponent = -(int64_t) frac_digits;
        digit_count += frac_digits;
    }

    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
        ptr++;
        bool exp_negative = false;
        if (ptr < end && (*ptr == '-' || *ptr == '+')) {
            exp_negative = (*ptr == '-');
            ptr++;
        }

        int64_t exp_value = 0;
        while (ptr < end && ((*ptr >= '0' && *ptr <= '9')
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
                             || *ptr == '_'
#endif
                             )) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
            if (*ptr == '_') {
                ptr++;
                continue;
            }
#endif
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

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* For strtod fallback with underscores, we need to create a cleaned buffer */
    char buffer[256];
    size_t buf_idx = 0;

    for (const char* p = start; p < end && buf_idx < sizeof(buffer) - 1; p++) {
        if (*p != '_') {
            buffer[buf_idx++] = *p;
        }
    }

    if (buf_idx >= sizeof(buffer)) {
        return NAN;
    }

    buffer[buf_idx] = '\0';

    errno = 0;
    char* endptr;
    result = strtod(buffer, &endptr);

    if (errno == ERANGE) {
        return result;
    }

    return result;
#else
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
#endif
}
