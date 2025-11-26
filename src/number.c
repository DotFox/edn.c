/**
 * EDN.C - Number parsing
 *
 * Single entry point: edn_read_number()
 *
 * Performance optimizations:
 * - SWAR (SIMD Within A Register): 8-digit parallel parsing (5x faster for long integers)
 * - Clinger's fast path: Fast double parsing for 90% of floats (5-10x faster)
 * - Fast path for 1-3 digit integers (2-3x faster, no overflow checks)
 *
 * Zero-copy: stores pointers into input buffer (underscores preserved for BigInt/BigDecimal).
 * Supports: int64, BigInt, doubles, BigDecimal, hex, octal, radix, ratios.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
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

#if defined(EDN_ENABLE_EXTENDED_INTEGERS) || defined(EDN_ENABLE_UNDERSCORE_IN_NUMERIC)

/**
 * Check if character is a valid digit in current radix.
 */
static inline bool is_digit_in_radix(char c, uint8_t radix) {
    return digit_value(c, radix) >= 0;
}

#endif /* EDN_ENABLE_EXTENDED_INTEGERS || EDN_ENABLE_UNDERSCORE_IN_NUMERIC */

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
 * Peek next character without advancing parser.
 */
static inline char peek_char(edn_parser_t* parser) {
    if (parser->current >= parser->end) {
        return '\0';
    }
    return *parser->current;
}

/**
 * Advance parser by one character.
 */
static inline void advance_char(edn_parser_t* parser) {
    if (parser->current < parser->end) {
        parser->current++;
    }
}

/**
 * Set parser error.
 */
static inline void set_error(edn_parser_t* parser, const char* message) {
    parser->error = EDN_ERROR_INVALID_NUMBER;
    parser->error_message = message;
}

#ifdef EDN_ENABLE_RATIO
/*
 * Binary GCD algorithm (Stein's algorithm)
 */
static int64_t ratio_gcd(int64_t a, int64_t b) {
    /* Make both values positive for GCD calculation */
    if (a < 0)
        a = -a;
    if (b < 0)
        b = -b;

    /* Handle edge cases */
    if (a == 0)
        return b;
    if (b == 0)
        return a;

    /* Find common factor of 2 */
    int shift = 0;
    while (((a | b) & 1) == 0) {
        a >>= 1;
        b >>= 1;
        shift++;
    }

    /* Remove remaining factors of 2 from a */
    while ((a & 1) == 0) {
        a >>= 1;
    }

    /* Main loop */
    do {
        /* Remove factors of 2 from b */
        while ((b & 1) == 0) {
            b >>= 1;
        }

        /* Ensure a <= b, swap if needed */
        if (a > b) {
            int64_t temp = a;
            a = b;
            b = temp;
        }

        /* Subtract: b = b - a (both are odd) */
        b = b - a;
    } while (b != 0);

    /* Restore common factors of 2 */
    return a << shift;
}
#endif

/**
 * Helper to set BigInt fields.
 */
static inline void set_bigint(edn_value_t* value, const char* digits, size_t length, bool negative,
                              uint8_t radix) {
    value->type = EDN_TYPE_BIGINT;
    value->as.bigint.digits = digits;
    value->as.bigint.length = length;
    value->as.bigint.negative = negative;
    value->as.bigint.radix = radix;
    value->as.bigint.cleaned = NULL; /* Lazy cleaning */
}

/**
 * Helper to set BigDecimal fields.
 */
static inline void set_bigdec(edn_value_t* value, const char* decimal, size_t length,
                              bool negative) {
    value->type = EDN_TYPE_BIGDEC;
    value->as.bigdec.decimal = decimal;
    value->as.bigdec.length = length;
    value->as.bigdec.negative = negative;
    value->as.bigdec.cleaned = NULL; /* Lazy cleaning */
}

/**
 * Parse integer with overflow detection for int64_t.
 * 
 * Three-tier strategy:
 * 1. Ultra-fast: 1-3 decimal digits (no overflow check needed)
 * 2. SWAR: 8-digit chunks for long decimals (5x scalar speed)
 * 3. General: Scalar with full overflow detection
 * 
 * Skips underscores if EDN_ENABLE_UNDERSCORE_IN_NUMERIC is enabled.
 * Returns false on overflow.
 */
static bool parse_int64_from_buffer(const char* start, const char* end, int64_t* out, uint8_t radix,
                                    bool negative) {
    const char* ptr = start;

    /* Ultra-fast path: 1-3 decimal digits (no overflow check needed) */
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

    /* SWAR fast path: 8-digit chunks for decimal radix */
    if (radix == 10) {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* With underscores enabled, try SWAR for 8-digit chunks without underscores */
        while ((end - ptr) >= 8) {
            /* Check if next 8 bytes are all digits (no underscores) */
            if (is_made_of_eight_digits_fast(ptr)) {
                uint32_t eight_digits = parse_eight_digits_unrolled(ptr);

                const uint64_t multiplier = 100000000;
                if (value > max_val / multiplier) {
                    return false; /* Overflow */
                }

                uint64_t new_value = value * multiplier + eight_digits;

                if (new_value < value) {
                    return false; /* Overflow */
                }

                if (new_value > max_val) {
                    return false; /* Overflow */
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
                return false; /* Overflow */
            }

            value = value * 10 + digit;
            ptr++;
        }
#else
        /* Without underscores, always try SWAR first */
        while ((end - ptr) >= 8) {
            if (is_made_of_eight_digits_fast(ptr)) {
                uint32_t eight_digits = parse_eight_digits_unrolled(ptr);

                const uint64_t multiplier = 100000000;
                if (value > max_val / multiplier) {
                    return false; /* Overflow */
                }

                uint64_t new_value = value * multiplier + eight_digits;

                if (new_value < value) {
                    return false; /* Overflow */
                }

                if (new_value > max_val) {
                    return false; /* Overflow */
                }

                value = new_value;
                ptr += 8;
            } else {
                break;
            }
        }

        /* Process remaining digits */
        while (ptr < end) {
            char c = *ptr;
            if (c < '0' || c > '9') {
                break;
            }

            int digit = c - '0';

            if (value > cutoff || (value == cutoff && (uint64_t) digit > cutlim)) {
                return false; /* Overflow */
            }

            value = value * 10 + digit;
            ptr++;
        }
#endif
    } else {
        /* Non-decimal radix: use scalar loop with overflow detection */
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
                return false; /* Overflow */
            }

            value = value * radix + digit;
            ptr++;
        }
    }

    *out = negative ? -(int64_t) value : (int64_t) value;
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
static inline bool parse_double_fast(int64_t mantissa, int64_t exponent, bool negative,
                                     double* out) {
    if (exponent < -22 || exponent > 22) {
        return false;
    }

    if (mantissa > 9007199254740991LL) { /* 2^53 - 1 */
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
 * 
 * Skips underscores if EDN_ENABLE_UNDERSCORE_IN_NUMERIC is enabled.
 */
static double parse_double_from_buffer(const char* start, const char* end) {
    const char* ptr = start;
    bool negative = false;

    /* Parse sign */
    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
        negative = (*ptr == '-');
        ptr++;
    }

    /* Parse integer and fractional parts into mantissa */
    int64_t mantissa = 0;
    size_t digit_count = 0;

    /* Integer part */
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

    /* Fractional part */
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

    /* Exponent part */
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
            ptr++;
        }

        exponent += exp_negative ? -exp_value : exp_value;
    }

    /* Try Clinger fast path (90% of cases) */
    double result;
    if (digit_count <= 15 && parse_double_fast(mantissa, exponent, negative, &result)) {
        return result;
    }

    /* Fall back to strtod() for edge cases */
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* For strtod fallback with underscores, we need to create a cleaned buffer */
    char buffer[512];
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
        return result; /* Infinity or underflow to zero */
    }

    return result;
#else
    /* No underscores, can use buffer directly */
    size_t len = end - start;
    char buffer[512];

    if (len >= sizeof(buffer)) {
        return NAN;
    }

    memcpy(buffer, start, len);
    buffer[len] = '\0';

    errno = 0;
    char* endptr;
    result = strtod(buffer, &endptr);

    if (errno == ERANGE) {
        return result; /* Infinity or underflow to zero */
    }

    return result;
#endif
}

/**
 * Main entry point for number parsing.
 *
 * Precondition: parser->current points to sign or first digit of number.
 * Uses zero-copy approach: stores pointers into input buffer.
 */
edn_value_t* edn_read_number(edn_parser_t* parser) {
    const char* start = parser->current;
    const char* digits_start = start;
    bool negative = false;
    uint8_t radix = 10;
    bool has_decimal_point = false;
    bool has_exponent = false;

    char c = peek_char(parser);

    /* Parse sign */
    if (c == '-' || c == '+') {
        negative = (c == '-');
        advance_char(parser);
        digits_start = parser->current;
        c = peek_char(parser);
    }

    /* Check for radix notation: NrDDDD (e.g., 16rFF, 2r1010) */
#ifdef EDN_ENABLE_EXTENDED_INTEGERS
    if (c >= '0' && c <= '9') {
        /* Look ahead for 'r' to detect radix notation */
        const char* r_pos = parser->current;
        while (r_pos < parser->end && *r_pos >= '0' && *r_pos <= '9') {
            r_pos++;
        }

        if (r_pos < parser->end && *r_pos == 'r' && r_pos > parser->current) {
            /* Parse radix value */
            int radix_val = 0;
            for (const char* p = parser->current; p < r_pos; p++) {
                radix_val = radix_val * 10 + (*p - '0');
            }

            if (radix_val >= 2 && radix_val <= 36) {
                /* Valid radix notation */
                radix = (uint8_t) radix_val;
                parser->current = r_pos + 1; /* Skip past 'r' */
                digits_start = parser->current;
                c = peek_char(parser);
                goto parse_radix_digits;
            } else {
                set_error(parser, "Radix must be between 2 and 36");
                return NULL;
            }
        }
    }
#endif

    /* Check for zero prefix */
    if (c == '0') {
        advance_char(parser);
        c = peek_char(parser);

#ifdef EDN_ENABLE_EXTENDED_INTEGERS

        /* Skip all following zeros */
        while (c == '0') {
            advance_char(parser);
            c = peek_char(parser);
        }

        if (c == 'x' || c == 'X') {
            /* Hexadecimal: 0xFF */
            radix = 16;
            advance_char(parser);
            digits_start = parser->current;
            goto parse_hex_digits;
        }

        if (c >= '1' && c <= '7') {
            /* Octal: 0777 */
            radix = 8;
            goto parse_octal_digits;
        }

        if (c == '8' || c == '9') {
            /* Invalid: 08 or 09 */
            set_error(parser, "Invalid octal digit");
            return NULL;
        }
#else
        /* Standard EDN: reject leading zeros before digits */
        if (c >= '0' && c <= '9') {
            set_error(parser, "Leading zeros not allowed");
            return NULL;
        }
#endif

        /* Check what follows zero */
        if (c == '.') {
            /* Float: 0.5 */
            has_decimal_point = true;
            goto parse_decimal_part;
        }


        if (c == 'N') {
            /* BigInt: 0N */
            advance_char(parser);
            goto create_bigint_zero;
        }

        if (c == 'M') {
            /* BigDecimal: 0M */
            advance_char(parser);
            goto create_bigdec_zero;
        }

        if (c == 'e' || c == 'E') {
            /* Scientific: 0e10 */
            has_exponent = true;
            goto parse_exponent;
        }

#ifdef EDN_ENABLE_RATIO
        if (c == '/') {
            /* Ratio: 0/N */
            goto create_ratio_zero;
        }
#endif

        /* Just zero */
        goto create_integer_zero;
    }

    /* Parse decimal digits */
    while (c != '\0' && !is_delimiter((unsigned char) c)) {
        if (c >= '0' && c <= '9') {
            advance_char(parser);
            c = peek_char(parser);
        }
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        else if (c == '_') {
            advance_char(parser);
            c = peek_char(parser);
            /* Next must be digit or underscore */
            if (c != '_' && (c < '0' || c > '9')) {
                set_error(parser, "Invalid underscore position");
                return NULL;
            }
        }
#endif
        else {
            break;
        }
    }

    /* Check for decimal point */
    if (c == '.') {
        has_decimal_point = true;
    parse_decimal_part:
        advance_char(parser);
        c = peek_char(parser);

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* Underscore immediately after decimal point is invalid */
        if (c == '_') {
            set_error(parser, "Underscore cannot be adjacent to decimal point");
            return NULL;
        }
#endif

        /* Parse fractional digits */
        while ((c >= '0' && c <= '9')
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
               || c == '_'
#endif
        ) {
            advance_char(parser);
            c = peek_char(parser);
        }
    }

    /* Check for exponent */
    if (c == 'e' || c == 'E') {
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        /* Check if previous character was underscore */
        if (parser->current > digits_start && *(parser->current - 1) == '_') {
            set_error(parser, "Underscore cannot be adjacent to exponent");
            return NULL;
        }
#endif
        has_exponent = true;
    parse_exponent:
        advance_char(parser);
        c = peek_char(parser);

        /* Optional sign */
        if (c == '+' || c == '-') {
            advance_char(parser);
            c = peek_char(parser);
        }

        /* Exponent digits */
        if (c < '0' || c > '9') {
            set_error(parser, "Expected exponent digits");
            return NULL;
        }

        while ((c >= '0' && c <= '9')
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
               || c == '_'
#endif
        ) {
            advance_char(parser);
            c = peek_char(parser);
        }
    }

    /* Check for suffixes */
    const char* digits_end = parser->current;
    bool is_bigint_suffix = false;
    bool is_bigdec_suffix = false;
#ifdef EDN_ENABLE_RATIO
    bool is_ratio = false;
    const char* denom_start = NULL;
    const char* denom_end = NULL;
#endif

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* Check if previous character was underscore before suffix */
    if (c == 'N' || c == 'M' || c == '/') {
        if (parser->current > digits_start && *(parser->current - 1) == '_') {
            set_error(parser, "Underscore cannot be adjacent to suffix");
            return NULL;
        }
    }
#endif

    if (c == 'N' && radix == 10 && !has_decimal_point && !has_exponent) {
        is_bigint_suffix = true;
        advance_char(parser);
    } else if (c == 'M') {
        is_bigdec_suffix = true;
        advance_char(parser);
    }
#ifdef EDN_ENABLE_RATIO
    else if (c == '/' && radix == 10 && !has_decimal_point && !has_exponent) {
        /* Ratio notation: numerator/denominator */
        is_ratio = true;
        advance_char(parser);
        denom_start = parser->current;
        c = peek_char(parser);

        /* Must have at least one digit */
        if (parser->current >= parser->end || c < '0' || c > '9') {
            set_error(parser, "Expected digit after '/' in ratio");
            return NULL;
        }

        /* Check for leading zero */
        if (c == '0') {
            advance_char(parser);
            c = peek_char(parser);
            if (parser->current >= parser->end || is_delimiter((unsigned char) c)) {
                set_error(parser, "Divide by zero");
                return NULL;
            }

            if (c >= '0' && c <= '9') {
                set_error(parser, "Leading zeros not allowed in ratio denominator");
                return NULL;
            }

            set_error(parser, "Invalid character in ratio denominator");
            return NULL;
        }

        /* Parse denominator digits */
        while (c >= '0' && c <= '9') {
            advance_char(parser);
            c = peek_char(parser);
        }

        denom_end = parser->current;

        if (c == 'N' || c == 'M' || c == '/') {
            set_error(parser, "Suffix not allowed on ratio denominator");
            return NULL;
        }

        if (parser->current < parser->end && !is_delimiter((unsigned char) c)) {
            set_error(parser, "Invalid character after ratio denominator");
            return NULL;
        }
    }
#endif

    /* Create value based on type */
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;

#ifdef EDN_ENABLE_RATIO
    if (is_ratio) {
        /* Try to parse numerator and denominator as int64 */
        int64_t numerator;
        int64_t denominator;
        bool numer_overflow =
            !parse_int64_from_buffer(digits_start, digits_end, &numerator, 10, negative);
        bool denom_overflow =
            !parse_int64_from_buffer(denom_start, denom_end, &denominator, 10, false);

        if (numer_overflow || denom_overflow) {
            if (!denom_overflow && denominator == 1) {
                set_bigint(value, digits_start, digits_end - digits_start, negative, 10);
            } else {
                /* Overflow - create BigRatio (store as strings) */
                value->type = EDN_TYPE_BIGRATIO;
                value->as.bigratio.numerator = digits_start;
                value->as.bigratio.numer_length = digits_end - digits_start;
                value->as.bigratio.numer_negative = negative;
                value->as.bigratio.denominator = denom_start;
                value->as.bigratio.denom_length = denom_end - denom_start;
            }
        } else {
            int64_t g = ratio_gcd(numerator, denominator);
            if (g > 1) {
                numerator /= g;
                denominator /= g;
            }

            /* If numerator is 0, return 0 */
            if (numerator == 0) {
                value->type = EDN_TYPE_INT;
                value->as.integer = 0;
                return value;
            }

            /* If denominator is 1, return the integer */
            if (denominator == 1) {
                value->type = EDN_TYPE_INT;
                value->as.integer = numerator;
                return value;
            }

            value->type = EDN_TYPE_RATIO;
            value->as.ratio.numerator = numerator;
            value->as.ratio.denominator = denominator;
        }
    } else
#endif
        if (is_bigdec_suffix) {
        /* BigDecimal - store pointer to digits with underscores */
        set_bigdec(value, digits_start, digits_end - digits_start, negative);
    } else if (is_bigint_suffix) {
        /* BigInt - store pointer to digits with underscores */
        set_bigint(value, digits_start, digits_end - digits_start, negative, radix);
    } else if (has_decimal_point || has_exponent) {
        /* Double */
        value->type = EDN_TYPE_FLOAT;
        value->as.floating = parse_double_from_buffer(start, digits_end);
    } else {
        /* Try to fit in int64 */
        int64_t num;
        if (parse_int64_from_buffer(digits_start, digits_end, &num, radix, negative)) {
            value->type = EDN_TYPE_INT;
            value->as.integer = num;
        } else {
            /* Overflow - becomes BigInt */
            set_bigint(value, digits_start, digits_end - digits_start, negative, radix);
        }
    }

    return value;

#ifdef EDN_ENABLE_EXTENDED_INTEGERS
    /* Radix notation parsing (2r1010, 16rFF, 36rZZ, etc.) */
parse_radix_digits:
    is_bigint_suffix = false; /* Reset suffix flag */
    is_bigdec_suffix = false; /* Reset suffix flag */
    c = peek_char(parser);

    /* Must have at least one digit */
    if (!is_digit_in_radix(c, radix)) {
        set_error(parser, "Expected digit after radix specifier");
        return NULL;
    }

    while (c != '\0' && !is_delimiter((unsigned char) c)) {
        if (is_digit_in_radix(c, radix)) {
            advance_char(parser);
            c = peek_char(parser);
        }
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        else if (c == '_') {
            advance_char(parser);
            c = peek_char(parser);
            /* Next must be digit or underscore */
            if (!is_digit_in_radix(c, radix) && c != '_') {
                set_error(parser, "Invalid underscore position");
                return NULL;
            }
        }
#endif
        else {
            break;
        }
    }

    digits_end = parser->current;

    /* Radix notation cannot have N suffix (N might be a digit in base > 23) */
    /* But can have M suffix */
    if (c == 'M') {
        is_bigdec_suffix = true;
        advance_char(parser);
    }

    /* Reject ratio notation with non-decimal radix */
    if (c == '/') {
        set_error(parser, "Ratio notation not allowed with radix integers");
        return NULL;
    }

    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;

    if (is_bigdec_suffix) {
        set_bigdec(value, digits_start, digits_end - digits_start, negative);
    } else {
        int64_t num;
        if (parse_int64_from_buffer(digits_start, digits_end, &num, radix, negative)) {
            value->type = EDN_TYPE_INT;
            value->as.integer = num;
        } else {
            set_bigint(value, digits_start, digits_end - digits_start, negative, radix);
        }
    }

    return value;

    /* Hexadecimal parsing */
parse_hex_digits:
    is_bigint_suffix = false; /* Reset suffix flag */
    is_bigdec_suffix = false; /* Reset suffix flag */
    c = peek_char(parser);

    /* Must have at least one hex digit */
    if (!is_digit_in_radix(c, 16)) {
        set_error(parser, "Expected hex digit after 0x");
        return NULL;
    }

    while (c != '\0' && !is_delimiter((unsigned char) c)) {
        if (is_digit_in_radix(c, 16)) {
            advance_char(parser);
            c = peek_char(parser);
        }
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        else if (c == '_') {
            advance_char(parser);
            c = peek_char(parser);
        }
#endif
        else {
            break;
        }
    }

    digits_end = parser->current;

    /* Check for N or M suffix */
    if (c == 'N') {
        /* N suffix forces BigInt */
        is_bigint_suffix = true;
        advance_char(parser);
        c = peek_char(parser);
    } else if (c == 'M') {
        is_bigdec_suffix = true;
        advance_char(parser);
        c = peek_char(parser);
    }

    /* Reject ratio notation with hexadecimal */
    if (c == '/') {
        set_error(parser, "Ratio notation not allowed with hexadecimal integers");
        return NULL;
    }

    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;

    if (is_bigdec_suffix) {
        set_bigdec(value, digits_start, digits_end - digits_start, negative);
    } else if (is_bigint_suffix) {
        /* N suffix forces BigInt representation */
        set_bigint(value, digits_start, digits_end - digits_start, negative, 16);
    } else {
        int64_t num;
        if (parse_int64_from_buffer(digits_start, digits_end, &num, 16, negative)) {
            value->type = EDN_TYPE_INT;
            value->as.integer = num;
        } else {
            set_bigint(value, digits_start, digits_end - digits_start, negative, 16);
        }
    }

    return value;

    /* Octal parsing */
parse_octal_digits:
    is_bigint_suffix = false; /* Reset suffix flag */
    is_bigdec_suffix = false; /* Reset suffix flag */
    c = peek_char(parser);

    /* Must have at least one octal digit */
    if (!is_digit_in_radix(c, 8)) {
        set_error(parser, "Expected octal digit");
        return NULL;
    }

    while (c != '\0' && !is_delimiter((unsigned char) c)) {
        if (is_digit_in_radix(c, 8)) {
            advance_char(parser);
            c = peek_char(parser);
        }
#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
        else if (c == '_') {
            advance_char(parser);
            c = peek_char(parser);
        }
#endif
        else {
            break;
        }
    }

    digits_end = parser->current;

    /* Check for N or M suffix */
    if (c == 'N') {
        /* N suffix forces BigInt */
        is_bigint_suffix = true;
        advance_char(parser);
        c = peek_char(parser);
    } else if (c == 'M') {
        is_bigdec_suffix = true;
        advance_char(parser);
        c = peek_char(parser); /* Update c after advancing past M */
    }

    /* Reject ratio notation with octal */
    if (c == '/') {
        set_error(parser, "Ratio notation not allowed with octal integers");
        return NULL;
    }

    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;

    if (is_bigdec_suffix) {
        set_bigdec(value, digits_start, digits_end - digits_start, negative);
    } else if (is_bigint_suffix) {
        /* N suffix forces BigInt representation */
        set_bigint(value, digits_start, digits_end - digits_start, negative, 8);
    } else {
        int64_t num;
        if (parse_int64_from_buffer(digits_start, digits_end, &num, 8, negative)) {
            value->type = EDN_TYPE_INT;
            value->as.integer = num;
        } else {
            set_bigint(value, digits_start, digits_end - digits_start, negative, 8);
        }
    }

    return value;
#endif

    /* Special cases for zero */
create_integer_zero:
    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;
    value->type = EDN_TYPE_INT;
    value->as.integer = 0;
    return value;

create_bigint_zero:
    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;
    set_bigint(value, "0", 1, negative, 10);
    return value;

create_bigdec_zero:
    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;
    set_bigdec(value, "0", 1, negative);
    return value;

#ifdef EDN_ENABLE_RATIO
create_ratio_zero:
    /* Parse denominator after '/' for 0/N ratio */
    /* Clojure behavior: 0/N returns integer 0, not a ratio */
    advance_char(parser);
    c = peek_char(parser);

    /* Must have at least one digit */
    if (parser->current >= parser->end || c < '0' || c > '9') {
        set_error(parser, "Expected digit after '/' in ratio");
        return NULL;
    }

    /* Check for leading zero */
    if (c == '0') {
        advance_char(parser);
        c = peek_char(parser);
        /* Single zero is divide by zero error */
        if (parser->current >= parser->end || is_delimiter((unsigned char) c)) {
            set_error(parser, "Divide by zero");
            return NULL;
        }
        /* Leading zero followed by more digits is invalid */
        if (c >= '0' && c <= '9') {
            set_error(parser, "Leading zeros not allowed in ratio denominator");
            return NULL;
        }
        /* Any other character after 0 is invalid for denominator */
        set_error(parser, "Invalid character in ratio denominator");
        return NULL;
    }

    /* Parse denominator digits (we validate, but don't need the value since numerator is 0) */
    while (c >= '0' && c <= '9') {
        advance_char(parser);
        c = peek_char(parser);
    }

    /* No suffixes allowed on denominator */
    if (c == 'N' || c == 'M' || c == '/') {
        set_error(parser, "Suffix not allowed on ratio denominator");
        return NULL;
    }

    /* Must be followed by EOF, whitespace or delimiter */
    if (parser->current < parser->end && !is_delimiter((unsigned char) c)) {
        set_error(parser, "Invalid character after ratio denominator");
        return NULL;
    }

    /* 0/N returns integer 0 (Clojure behavior) */
    value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        set_error(parser, "Out of memory");
        return NULL;
    }
    value->arena = parser->arena;
    value->type = EDN_TYPE_INT;
    value->as.integer = 0;
    return value;
#endif
}
