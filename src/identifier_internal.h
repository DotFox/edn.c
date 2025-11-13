/**
 * Identifier parsing internal utilities
 * 
 * Optimized delimiter detection using lookup table.
 */

#ifndef IDENTIFIER_INTERNAL_H
#define IDENTIFIER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Delimiter lookup table for fast character classification.
 * 
 * Single memory lookup replaces multiple branches.
 * Cache-friendly: entire table fits in L1 cache (256 bytes).
 * 
 * 1 = delimiter (stops identifier scanning)
 * 0 = valid identifier character
 */
static const uint8_t DELIMITER_TABLE[256] = {
    /* 0x00-0x1F: Control characters (all delimiters) */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

    /* 0x20-0x2F */
    1, /* 0x20: space - delimiter */
    0, /* 0x21: ! - valid */
    1, /* 0x22: " - delimiter (string) */
    1, /* 0x23: # - delimiter (dispatch) */
    0, /* 0x24: $ - valid */
    0, /* 0x25: % - valid */
    0, /* 0x26: & - valid */
    0, /* 0x27: ' - valid */
    1, /* 0x28: ( - delimiter (list) */
    1, /* 0x29: ) - delimiter (list) */
    0, /* 0x2A: * - valid */
    0, /* 0x2B: + - valid */
    1, /* 0x2C: , - delimiter (whitespace) */
    0, /* 0x2D: - - valid */
    0, /* 0x2E: . - valid */
    0, /* 0x2F: / - valid (namespace separator) */

    /* 0x30-0x3F: Digits and punctuation */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0-9: valid */
    0,                            /* 0x3A: : - valid (keyword prefix) */
    1,                            /* 0x3B: ; - delimiter (comment) */
    0,                            /* 0x3C: < - valid */
    0,                            /* 0x3D: = - valid */
    0,                            /* 0x3E: > - valid */
    0,                            /* 0x3F: ? - valid */

    /* 0x40-0x5F: Uppercase letters and symbols */
    0,                                              /* 0x40: @ - valid */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A-P */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                   /* Q-Z */
    1,                                              /* 0x5B: [ - delimiter (vector) */
    1,                                              /* 0x5C: \ - delimiter (character) */
    1,                                              /* 0x5D: ] - delimiter (vector) */
    0,                                              /* 0x5E: ^ - valid */
    0,                                              /* 0x5F: _ - valid */

    /* 0x60-0x7F: Lowercase letters and symbols */
    0,                                              /* 0x60: ` - valid */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* a-p */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                   /* q-z */
    1,                                              /* 0x7B: { - delimiter (map) */
    0,                                              /* 0x7C: | - valid */
    1,                                              /* 0x7D: } - delimiter (map) */
    0,                                              /* 0x7E: ~ - valid */
    1,                                              /* 0x7F: DEL - delimiter */

    /* 0x80-0xFF: Extended ASCII / UTF-8 continuation bytes (all valid) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Fast delimiter check using lookup table.
 * 
 * Single memory access, no branches.
 * Inlined for zero-cost abstraction.
 */
static inline bool is_delimiter(unsigned char c) {
    return DELIMITER_TABLE[c];
}

#endif /* IDENTIFIER_INTERNAL_H */
