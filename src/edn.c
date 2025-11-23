#include "../include/edn.h"

#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

edn_result_t edn_read(const char* input, size_t length) {
    return edn_read_with_options(input, length, NULL);
}

edn_result_t edn_read_with_options(const char* input, size_t length,
                                   const edn_parse_options_t* options) {
    edn_result_t result = {0};

    if (!input) {
        result.error = EDN_ERROR_INVALID_SYNTAX;
        result.error_message = "Input is NULL";
        return result;
    }

    if (length == 0) {
        length = strlen(input);
    }

    edn_parser_t parser;
    parser.input = input;
    parser.current = input;
    parser.end = input + length;
    parser.depth = 0;
    parser.arena = edn_arena_create();
    parser.error = EDN_OK;
    parser.error_message = NULL;

    /* Set reader configuration */
    if (options != NULL) {
        parser.reader_registry = options->reader_registry;
        parser.default_reader_mode = options->default_reader_mode;
    } else {
        parser.reader_registry = NULL;
        parser.default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH;
    }

    parser.discard_mode = false;

    result.value = edn_parser_parse_value(&parser);
    result.error = parser.error;
    result.error_message = parser.error_message;

    /* This needs refactoring. */
    if (result.error != EDN_OK) {
        edn_arena_t* temp_arena = edn_arena_create();
        if (temp_arena) {
            newline_positions_t* positions =
                newline_find_all_ex(input, length, NEWLINE_MODE_LF, temp_arena);
            if (positions) {
                /* Calculate current byte offset where error occurred */
                size_t byte_offset = parser.current - parser.input;
                document_position_t error_pos;
                if (newline_get_position(positions, byte_offset, &error_pos)) {
                    result.error_line = error_pos.line;
                    result.error_column = error_pos.column;
                } else {
                    result.error_line = 0;
                    result.error_column = 0;
                }
            } else {
                result.error_line = 0;
                result.error_column = 0;
            }
            edn_arena_destroy(temp_arena);
        } else {
            result.error_line = 0;
            result.error_column = 0;
        }
    } else {
        result.error_line = 0;
        result.error_column = 0;
    }

    /* Handle EOF error with eof_value option */
    if (result.error == EDN_ERROR_UNEXPECTED_EOF && options != NULL && options->eof_value != NULL) {
        if (parser.arena != NULL) {
            edn_arena_destroy(parser.arena);
        }
        result.value = options->eof_value;
        result.error = EDN_OK;
        result.error_message = NULL;
    } else {
        /* Free arena if parsing failed (no value was created) or if value is a singleton */
        if (parser.arena != NULL && (result.value == NULL || result.value->arena == NULL)) {
            edn_arena_destroy(parser.arena);
        }
    }

    return result;
}

void edn_free(edn_value_t* value) {
    if (!value || !value->arena) {
        return;
    }
    edn_arena_destroy(value->arena);
}

edn_type_t edn_type(const edn_value_t* value) {
    return value ? value->type : EDN_TYPE_NIL;
}

bool edn_parser_skip_whitespace(edn_parser_t* parser) {
    parser->current = edn_simd_skip_whitespace(parser->current, parser->end);
    return parser->current < parser->end;
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

typedef enum {
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_STRING,
    CHAR_TYPE_CHARACTER,
    CHAR_TYPE_LIST_OPEN,
    CHAR_TYPE_VECTOR_OPEN,
    CHAR_TYPE_MAP_OPEN,
    CHAR_TYPE_HASH,
    CHAR_TYPE_SIGN,
    CHAR_TYPE_DIGIT,
    CHAR_TYPE_DELIMITER,
#ifdef EDN_ENABLE_METADATA
    CHAR_TYPE_METADATA,
#endif
} char_dispatch_type_t;

static const char_dispatch_type_t char_dispatch_table[256] = {
    /* 0x00-0x1F: Control characters → identifier (will fail parsing) */
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,
    CHAR_TYPE_IDENTIFIER,

    /* 0x20-0x2F: Space and punctuation */
    [' '] = CHAR_TYPE_IDENTIFIER,
    ['!'] = CHAR_TYPE_IDENTIFIER,
    ['"'] = CHAR_TYPE_STRING,
    ['#'] = CHAR_TYPE_HASH,
    ['$'] = CHAR_TYPE_IDENTIFIER,
    ['%'] = CHAR_TYPE_IDENTIFIER,
    ['&'] = CHAR_TYPE_IDENTIFIER,
    ['\''] = CHAR_TYPE_IDENTIFIER,
    ['('] = CHAR_TYPE_LIST_OPEN,
    [')'] = CHAR_TYPE_DELIMITER,
    ['*'] = CHAR_TYPE_IDENTIFIER,
    ['+'] = CHAR_TYPE_SIGN,
    [','] = CHAR_TYPE_IDENTIFIER,
    ['-'] = CHAR_TYPE_SIGN,
    ['.'] = CHAR_TYPE_IDENTIFIER,
    ['/'] = CHAR_TYPE_IDENTIFIER,

    /* 0x30-0x39: Digits */
    ['0'] = CHAR_TYPE_DIGIT,
    ['1'] = CHAR_TYPE_DIGIT,
    ['2'] = CHAR_TYPE_DIGIT,
    ['3'] = CHAR_TYPE_DIGIT,
    ['4'] = CHAR_TYPE_DIGIT,
    ['5'] = CHAR_TYPE_DIGIT,
    ['6'] = CHAR_TYPE_DIGIT,
    ['7'] = CHAR_TYPE_DIGIT,
    ['8'] = CHAR_TYPE_DIGIT,
    ['9'] = CHAR_TYPE_DIGIT,

    /* 0x3A-0x40: More punctuation */
    [':'] = CHAR_TYPE_IDENTIFIER,
    [';'] = CHAR_TYPE_IDENTIFIER,
    ['<'] = CHAR_TYPE_IDENTIFIER,
    ['='] = CHAR_TYPE_IDENTIFIER,
    ['>'] = CHAR_TYPE_IDENTIFIER,
    ['?'] = CHAR_TYPE_IDENTIFIER,
    ['@'] = CHAR_TYPE_IDENTIFIER,

    /* 0x41-0x5A: Uppercase letters */
    ['A'] = CHAR_TYPE_IDENTIFIER,
    ['B'] = CHAR_TYPE_IDENTIFIER,
    ['C'] = CHAR_TYPE_IDENTIFIER,
    ['D'] = CHAR_TYPE_IDENTIFIER,
    ['E'] = CHAR_TYPE_IDENTIFIER,
    ['F'] = CHAR_TYPE_IDENTIFIER,
    ['G'] = CHAR_TYPE_IDENTIFIER,
    ['H'] = CHAR_TYPE_IDENTIFIER,
    ['I'] = CHAR_TYPE_IDENTIFIER,
    ['J'] = CHAR_TYPE_IDENTIFIER,
    ['K'] = CHAR_TYPE_IDENTIFIER,
    ['L'] = CHAR_TYPE_IDENTIFIER,
    ['M'] = CHAR_TYPE_IDENTIFIER,
    ['N'] = CHAR_TYPE_IDENTIFIER,
    ['O'] = CHAR_TYPE_IDENTIFIER,
    ['P'] = CHAR_TYPE_IDENTIFIER,
    ['Q'] = CHAR_TYPE_IDENTIFIER,
    ['R'] = CHAR_TYPE_IDENTIFIER,
    ['S'] = CHAR_TYPE_IDENTIFIER,
    ['T'] = CHAR_TYPE_IDENTIFIER,
    ['U'] = CHAR_TYPE_IDENTIFIER,
    ['V'] = CHAR_TYPE_IDENTIFIER,
    ['W'] = CHAR_TYPE_IDENTIFIER,
    ['X'] = CHAR_TYPE_IDENTIFIER,
    ['Y'] = CHAR_TYPE_IDENTIFIER,
    ['Z'] = CHAR_TYPE_IDENTIFIER,

    /* 0x5B-0x60: Brackets and punctuation */
    ['['] = CHAR_TYPE_VECTOR_OPEN,
    ['\\'] = CHAR_TYPE_CHARACTER,
    [']'] = CHAR_TYPE_DELIMITER,
#ifdef EDN_ENABLE_METADATA
    ['^'] = CHAR_TYPE_METADATA,
#else
    ['^'] = CHAR_TYPE_IDENTIFIER,
#endif
    ['_'] = CHAR_TYPE_IDENTIFIER,
    ['`'] = CHAR_TYPE_IDENTIFIER,

    /* 0x61-0x7A: Lowercase letters */
    ['a'] = CHAR_TYPE_IDENTIFIER,
    ['b'] = CHAR_TYPE_IDENTIFIER,
    ['c'] = CHAR_TYPE_IDENTIFIER,
    ['d'] = CHAR_TYPE_IDENTIFIER,
    ['e'] = CHAR_TYPE_IDENTIFIER,
    ['f'] = CHAR_TYPE_IDENTIFIER,
    ['g'] = CHAR_TYPE_IDENTIFIER,
    ['h'] = CHAR_TYPE_IDENTIFIER,
    ['i'] = CHAR_TYPE_IDENTIFIER,
    ['j'] = CHAR_TYPE_IDENTIFIER,
    ['k'] = CHAR_TYPE_IDENTIFIER,
    ['l'] = CHAR_TYPE_IDENTIFIER,
    ['m'] = CHAR_TYPE_IDENTIFIER,
    ['n'] = CHAR_TYPE_IDENTIFIER,
    ['o'] = CHAR_TYPE_IDENTIFIER,
    ['p'] = CHAR_TYPE_IDENTIFIER,
    ['q'] = CHAR_TYPE_IDENTIFIER,
    ['r'] = CHAR_TYPE_IDENTIFIER,
    ['s'] = CHAR_TYPE_IDENTIFIER,
    ['t'] = CHAR_TYPE_IDENTIFIER,
    ['u'] = CHAR_TYPE_IDENTIFIER,
    ['v'] = CHAR_TYPE_IDENTIFIER,
    ['w'] = CHAR_TYPE_IDENTIFIER,
    ['x'] = CHAR_TYPE_IDENTIFIER,
    ['y'] = CHAR_TYPE_IDENTIFIER,
    ['z'] = CHAR_TYPE_IDENTIFIER,

    /* 0x7B-0x7F: More brackets and punctuation */
    ['{'] = CHAR_TYPE_MAP_OPEN,
    ['|'] = CHAR_TYPE_IDENTIFIER,
    ['}'] = CHAR_TYPE_DELIMITER,
    ['~'] = CHAR_TYPE_IDENTIFIER,
    [127] = CHAR_TYPE_IDENTIFIER,

    /* 0x80-0xFF: Extended ASCII → identifier */
    [128] = CHAR_TYPE_IDENTIFIER,
    [129] = CHAR_TYPE_IDENTIFIER,
    [130] = CHAR_TYPE_IDENTIFIER,
    [131] = CHAR_TYPE_IDENTIFIER,
    [132] = CHAR_TYPE_IDENTIFIER,
    [133] = CHAR_TYPE_IDENTIFIER,
    [134] = CHAR_TYPE_IDENTIFIER,
    [135] = CHAR_TYPE_IDENTIFIER,
    [136] = CHAR_TYPE_IDENTIFIER,
    [137] = CHAR_TYPE_IDENTIFIER,
    [138] = CHAR_TYPE_IDENTIFIER,
    [139] = CHAR_TYPE_IDENTIFIER,
    [140] = CHAR_TYPE_IDENTIFIER,
    [141] = CHAR_TYPE_IDENTIFIER,
    [142] = CHAR_TYPE_IDENTIFIER,
    [143] = CHAR_TYPE_IDENTIFIER,
    [144] = CHAR_TYPE_IDENTIFIER,
    [145] = CHAR_TYPE_IDENTIFIER,
    [146] = CHAR_TYPE_IDENTIFIER,
    [147] = CHAR_TYPE_IDENTIFIER,
    [148] = CHAR_TYPE_IDENTIFIER,
    [149] = CHAR_TYPE_IDENTIFIER,
    [150] = CHAR_TYPE_IDENTIFIER,
    [151] = CHAR_TYPE_IDENTIFIER,
    [152] = CHAR_TYPE_IDENTIFIER,
    [153] = CHAR_TYPE_IDENTIFIER,
    [154] = CHAR_TYPE_IDENTIFIER,
    [155] = CHAR_TYPE_IDENTIFIER,
    [156] = CHAR_TYPE_IDENTIFIER,
    [157] = CHAR_TYPE_IDENTIFIER,
    [158] = CHAR_TYPE_IDENTIFIER,
    [159] = CHAR_TYPE_IDENTIFIER,
    [160] = CHAR_TYPE_IDENTIFIER,
    [161] = CHAR_TYPE_IDENTIFIER,
    [162] = CHAR_TYPE_IDENTIFIER,
    [163] = CHAR_TYPE_IDENTIFIER,
    [164] = CHAR_TYPE_IDENTIFIER,
    [165] = CHAR_TYPE_IDENTIFIER,
    [166] = CHAR_TYPE_IDENTIFIER,
    [167] = CHAR_TYPE_IDENTIFIER,
    [168] = CHAR_TYPE_IDENTIFIER,
    [169] = CHAR_TYPE_IDENTIFIER,
    [170] = CHAR_TYPE_IDENTIFIER,
    [171] = CHAR_TYPE_IDENTIFIER,
    [172] = CHAR_TYPE_IDENTIFIER,
    [173] = CHAR_TYPE_IDENTIFIER,
    [174] = CHAR_TYPE_IDENTIFIER,
    [175] = CHAR_TYPE_IDENTIFIER,
    [176] = CHAR_TYPE_IDENTIFIER,
    [177] = CHAR_TYPE_IDENTIFIER,
    [178] = CHAR_TYPE_IDENTIFIER,
    [179] = CHAR_TYPE_IDENTIFIER,
    [180] = CHAR_TYPE_IDENTIFIER,
    [181] = CHAR_TYPE_IDENTIFIER,
    [182] = CHAR_TYPE_IDENTIFIER,
    [183] = CHAR_TYPE_IDENTIFIER,
    [184] = CHAR_TYPE_IDENTIFIER,
    [185] = CHAR_TYPE_IDENTIFIER,
    [186] = CHAR_TYPE_IDENTIFIER,
    [187] = CHAR_TYPE_IDENTIFIER,
    [188] = CHAR_TYPE_IDENTIFIER,
    [189] = CHAR_TYPE_IDENTIFIER,
    [190] = CHAR_TYPE_IDENTIFIER,
    [191] = CHAR_TYPE_IDENTIFIER,
    [192] = CHAR_TYPE_IDENTIFIER,
    [193] = CHAR_TYPE_IDENTIFIER,
    [194] = CHAR_TYPE_IDENTIFIER,
    [195] = CHAR_TYPE_IDENTIFIER,
    [196] = CHAR_TYPE_IDENTIFIER,
    [197] = CHAR_TYPE_IDENTIFIER,
    [198] = CHAR_TYPE_IDENTIFIER,
    [199] = CHAR_TYPE_IDENTIFIER,
    [200] = CHAR_TYPE_IDENTIFIER,
    [201] = CHAR_TYPE_IDENTIFIER,
    [202] = CHAR_TYPE_IDENTIFIER,
    [203] = CHAR_TYPE_IDENTIFIER,
    [204] = CHAR_TYPE_IDENTIFIER,
    [205] = CHAR_TYPE_IDENTIFIER,
    [206] = CHAR_TYPE_IDENTIFIER,
    [207] = CHAR_TYPE_IDENTIFIER,
    [208] = CHAR_TYPE_IDENTIFIER,
    [209] = CHAR_TYPE_IDENTIFIER,
    [210] = CHAR_TYPE_IDENTIFIER,
    [211] = CHAR_TYPE_IDENTIFIER,
    [212] = CHAR_TYPE_IDENTIFIER,
    [213] = CHAR_TYPE_IDENTIFIER,
    [214] = CHAR_TYPE_IDENTIFIER,
    [215] = CHAR_TYPE_IDENTIFIER,
    [216] = CHAR_TYPE_IDENTIFIER,
    [217] = CHAR_TYPE_IDENTIFIER,
    [218] = CHAR_TYPE_IDENTIFIER,
    [219] = CHAR_TYPE_IDENTIFIER,
    [220] = CHAR_TYPE_IDENTIFIER,
    [221] = CHAR_TYPE_IDENTIFIER,
    [222] = CHAR_TYPE_IDENTIFIER,
    [223] = CHAR_TYPE_IDENTIFIER,
    [224] = CHAR_TYPE_IDENTIFIER,
    [225] = CHAR_TYPE_IDENTIFIER,
    [226] = CHAR_TYPE_IDENTIFIER,
    [227] = CHAR_TYPE_IDENTIFIER,
    [228] = CHAR_TYPE_IDENTIFIER,
    [229] = CHAR_TYPE_IDENTIFIER,
    [230] = CHAR_TYPE_IDENTIFIER,
    [231] = CHAR_TYPE_IDENTIFIER,
    [232] = CHAR_TYPE_IDENTIFIER,
    [233] = CHAR_TYPE_IDENTIFIER,
    [234] = CHAR_TYPE_IDENTIFIER,
    [235] = CHAR_TYPE_IDENTIFIER,
    [236] = CHAR_TYPE_IDENTIFIER,
    [237] = CHAR_TYPE_IDENTIFIER,
    [238] = CHAR_TYPE_IDENTIFIER,
    [239] = CHAR_TYPE_IDENTIFIER,
    [240] = CHAR_TYPE_IDENTIFIER,
    [241] = CHAR_TYPE_IDENTIFIER,
    [242] = CHAR_TYPE_IDENTIFIER,
    [243] = CHAR_TYPE_IDENTIFIER,
    [244] = CHAR_TYPE_IDENTIFIER,
    [245] = CHAR_TYPE_IDENTIFIER,
    [246] = CHAR_TYPE_IDENTIFIER,
    [247] = CHAR_TYPE_IDENTIFIER,
    [248] = CHAR_TYPE_IDENTIFIER,
    [249] = CHAR_TYPE_IDENTIFIER,
    [250] = CHAR_TYPE_IDENTIFIER,
    [251] = CHAR_TYPE_IDENTIFIER,
    [252] = CHAR_TYPE_IDENTIFIER,
    [253] = CHAR_TYPE_IDENTIFIER,
    [254] = CHAR_TYPE_IDENTIFIER,
    [255] = CHAR_TYPE_IDENTIFIER,
};

static edn_value_t* parse_string_value(edn_parser_t* parser) {
#ifdef EDN_ENABLE_TEXT_BLOCKS
    /* Check for text block pattern: """\n */
    if (parser->current + 3 < parser->end && parser->current[0] == '"' &&
        parser->current[1] == '"' && parser->current[2] == '"' && parser->current[3] == '\n') {
        return edn_parse_text_block(parser);
    }
#endif

    edn_string_scan_t scan = edn_parse_string_lazy(parser->current, parser->end);

    if (!scan.valid) {
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
    value->as.string.data = scan.start;
    edn_string_set_length(value, scan.end - scan.start);
    edn_string_set_has_escapes(value, scan.has_escapes);
    value->as.string.decoded = NULL;
    value->arena = parser->arena;

    parser->current = scan.end + 1;

    return value;
}

#ifdef EDN_ENABLE_RATIO
/*
 * Binary GCD algorithm (Stein's algorithm)
 */
static int64_t edn_gcd(int64_t a, int64_t b) {
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

static edn_value_t* parse_number_value(edn_parser_t* parser) {
    edn_value_t* value = edn_read_number(parser);

    if (!value) {
        /* Error already set by edn_read_number */
        return NULL;
    }

#ifdef EDN_ENABLE_RATIO
    /* Check if this could be a ratio (numerator/denominator) */
    if (value->type == EDN_TYPE_INT && parser->current < parser->end && *parser->current == '/') {
        int64_t numerator = value->as.integer;

        /* Move past the / */
        parser->current++;

        /* Parse denominator */
        edn_value_t* denom_value = edn_read_number(parser);
        if (!denom_value) {
            return NULL;
        }

        if (denom_value->type != EDN_TYPE_INT) {
            parser->error = EDN_ERROR_INVALID_NUMBER;
            parser->error_message = "Ratio denominator must be an integer";
            return NULL;
        }

        int64_t denominator = denom_value->as.integer;

        if (denominator == 0) {
            parser->error = EDN_ERROR_INVALID_NUMBER;
            parser->error_message = "Ratio denominator cannot be zero";
            return NULL;
        }

        if (denominator < 0) {
            parser->error = EDN_ERROR_INVALID_NUMBER;
            parser->error_message = "Ratio denominator must be positive";
            return NULL;
        }

        /* Reduce the ratio to lowest terms using GCD */
        int64_t g = edn_gcd(numerator, denominator);
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

        /* Return as ratio */
        value->type = EDN_TYPE_RATIO;
        value->as.ratio.numerator = numerator;
        value->as.ratio.denominator = denominator;
        return value;
    }
#endif

    /* Validate that number is followed by valid delimiter or EOF */
    if (parser->current < parser->end) {
        unsigned char next = (unsigned char) *parser->current;

        bool valid_delimiter = false;

        /* Whitespace: space, tab, newline, CR, comma, etc. */
        if (next == ' ' || next == ',' || next == ';' || (next >= 0x09 && next <= 0x0D) ||
            (next >= 0x1C && next <= 0x1F)) {
            valid_delimiter = true;
        }
        /* Structural delimiters: ), ], }, ", #, (, [ */
        else if (next == ')' || next == ']' || next == '}' || next == '"' || next == '#' ||
                 next == '(' || next == '[') {
            valid_delimiter = true;
        }
#ifdef EDN_ENABLE_RATIO
        /* When ratio support is enabled, '/' is also valid (for ratios) */
        else if (next == '/' && value->type == EDN_TYPE_INT) {
            valid_delimiter = true;
        }
#endif

        if (!valid_delimiter) {
            parser->error = EDN_ERROR_INVALID_NUMBER;
            parser->error_message = "Number must be followed by whitespace or delimiter";
            return NULL;
        }
    }

    return value;
}

edn_value_t* edn_parser_parse_value(edn_parser_t* parser) {
    if (parser->current < parser->end) {
        unsigned char c = (unsigned char) *parser->current;
        /* Quick check for whitespace: 0x09-0x0D, 0x1C-0x1F, space, comma, semicolon */
        if (c == ' ' || c == ',' || c == ';' || (c >= 0x09 && c <= 0x0D) ||
            (c >= 0x1C && c <= 0x1F)) {
            if (!edn_parser_skip_whitespace(parser)) {
                parser->error = EDN_ERROR_UNEXPECTED_EOF;
                parser->error_message = "Unexpected end of input";
                return NULL;
            }
        }
    } else {
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unexpected end of input";
        return NULL;
    }

    unsigned char c = (unsigned char) *parser->current;
    char_dispatch_type_t dispatch_type = char_dispatch_table[c];

    switch (dispatch_type) {
        case CHAR_TYPE_STRING:
            return parse_string_value(parser);

        case CHAR_TYPE_CHARACTER:
            return edn_parse_character(parser);

        case CHAR_TYPE_LIST_OPEN:
            return edn_parse_list(parser);

        case CHAR_TYPE_VECTOR_OPEN:
            return edn_parse_vector(parser);

        case CHAR_TYPE_MAP_OPEN:
            return edn_parse_map(parser);

        case CHAR_TYPE_HASH:
            /* Hash requires lookahead: #{ (set), ## (symbolic), #_ (discard), #: (namespaced map), # (tagged) */
            if (parser->current + 1 < parser->end) {
                char next = parser->current[1];
                if (next == '{') {
                    return edn_parse_set(parser);
                } else if (next == '#') {
                    return edn_parse_symbolic_value(parser);
                } else if (next == '_') {
                    /* Discard next form and parse the one after it */
                    edn_value_t* discarded = edn_parse_discard_value(parser);
                    /* edn_parse_discard_value returns NULL on success (value was discarded) */
                    /* If there was an error, it's set in parser->error */
                    if (parser->error != EDN_OK) {
                        return NULL; /* Error during discard */
                    }
                    (void) discarded; /* Suppress unused variable warning */
                    /* Recursively parse the next value (which may itself be another discard) */
                    return edn_parser_parse_value(parser);
                }
#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX
                else if (next == ':') {
                    /* Namespaced map syntax: #:ns{...} */
                    return edn_parse_namespaced_map(parser);
                }
#endif
            }
            return edn_parse_tagged(parser);

        case CHAR_TYPE_SIGN:
            /* + or - requires lookahead to distinguish number from identifier */
            if (parser->current + 1 < parser->end && is_digit(parser->current[1])) {
                return parse_number_value(parser);
            }
            return edn_parse_identifier(parser);

        case CHAR_TYPE_DIGIT:
            return parse_number_value(parser);

        case CHAR_TYPE_DELIMITER:
            /* Closing delimiters: ), ], } */
            if (parser->depth == 0) {
                /* Unmatched closing delimiter at top level */
                parser->error = EDN_ERROR_UNMATCHED_DELIMITER;
                if (c == ')') {
                    parser->error_message = "Unmatched closing delimiter ')'";
                } else if (c == ']') {
                    parser->error_message = "Unmatched closing delimiter ']'";
                } else {
                    parser->error_message = "Unmatched closing delimiter '}'";
                }
                return NULL;
            }
            /* Inside collection - let collection parser handle it */
            return NULL;

#ifdef EDN_ENABLE_METADATA
        case CHAR_TYPE_METADATA:
            return edn_parse_metadata(parser);
#endif

        case CHAR_TYPE_IDENTIFIER:
        default:
            /* Default: identifier (symbol, keyword, nil, true, false) */
            return edn_parse_identifier(parser);
    }
}

const char* edn_string_get(const edn_value_t* value, size_t* length) {
    if (!value || value->type != EDN_TYPE_STRING) {
        if (length)
            *length = 0;
        return NULL;
    }

    /* Fast path: no escapes, return pointer directly (zero-copy) */
    if (!edn_string_has_escapes(value)) {
        size_t str_length = edn_string_get_length(value);
        if (length)
            *length = str_length;

        /* Need to add null terminator for zero-copy strings */
        /* For now, we'll need to allocate. In future, we can optimize this */
        if (!value->as.string.decoded) {
            /* Allocate and copy with null terminator */
            char* copy = edn_arena_alloc(value->arena, str_length + 1);
            if (copy) {
                memcpy(copy, value->as.string.data, str_length);
                copy[str_length] = '\0';
                /* Cast away const - we're modifying cached field */
                ((edn_value_t*) value)->as.string.decoded = copy;
            }
        }
        return value->as.string.decoded;
    }

    /* Slow path: has escapes, decode if not already cached */
    if (!value->as.string.decoded) {
        size_t str_length = edn_string_get_length(value);
        char* decoded = edn_decode_string(value->arena, value->as.string.data, str_length);
        if (!decoded) {
            if (length)
                *length = 0;
            return NULL;
        }
        /* Cast away const - we're modifying cached field */
        ((edn_value_t*) value)->as.string.decoded = decoded;
    }

    if (length) {
        *length = strlen(value->as.string.decoded);
    }
    return value->as.string.decoded;
}

bool edn_int64_get(const edn_value_t* value, int64_t* out) {
    if (!value || !out || value->type != EDN_TYPE_INT) {
        return false;
    }
    *out = value->as.integer;
    return true;
}

bool edn_is_nil(const edn_value_t* value) {
    return value && value->type == EDN_TYPE_NIL;
}

bool edn_bool_get(const edn_value_t* value, bool* out) {
    if (!value || !out || value->type != EDN_TYPE_BOOL) {
        return false;
    }
    *out = value->as.boolean;
    return true;
}

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
/**
 * Clean underscores from BigInt/BigDecimal digit string.
 * Allocates and caches cleaned string in arena.
 */
static const char* clean_number_string(const char* digits, size_t length, edn_arena_t* arena,
                                       char** cleaned_cache) {
    /* Check if already cleaned */
    if (*cleaned_cache) {
        return *cleaned_cache;
    }

    /* Safety check */
    if (!digits || !arena) {
        return NULL;
    }

    /* Check if there are any underscores to clean */
    bool has_underscore = false;
    for (size_t i = 0; i < length; i++) {
        if (digits[i] == '_') {
            has_underscore = true;
            break;
        }
    }

    /* No underscores - can return original, but don't cache const pointer */
    if (!has_underscore) {
        return digits;
    }

    /* Allocate cleaned buffer */
    char* cleaned = (char*) edn_arena_alloc(arena, length + 1);
    if (!cleaned) {
        return NULL;
    }

    /* Copy without underscores */
    size_t j = 0;
    for (size_t i = 0; i < length; i++) {
        if (digits[i] != '_') {
            cleaned[j++] = digits[i];
        }
    }
    cleaned[j] = '\0';

    /* Cache the result */
    *cleaned_cache = cleaned;
    return cleaned;
}
#endif

const char* edn_bigint_get(const edn_value_t* value, size_t* length, bool* negative,
                           uint8_t* radix) {
    if (!value || value->type != EDN_TYPE_BIGINT) {
        if (length)
            *length = 0;
        if (negative)
            *negative = false;
        if (radix)
            *radix = 10;
        return NULL;
    }

    if (negative)
        *negative = value->as.bigint.negative;
    if (radix)
        *radix = value->as.bigint.radix;

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* Clean underscores lazily */
    if (!value->arena) {
        /* No arena - can't allocate cleaned string, return raw */
        if (length)
            *length = value->as.bigint.length;
        return value->as.bigint.digits;
    }

    const char* digits =
        clean_number_string(value->as.bigint.digits, value->as.bigint.length, value->arena,
                            &((edn_value_t*) value)->as.bigint.cleaned);
    if (!digits) {
        if (length)
            *length = 0;
        return NULL;
    }

    if (length) {
        /* If we returned a cleaned string (cached), it has null terminator */
        /* If we returned the original, we must use the stored length */
        if (((edn_value_t*) value)->as.bigint.cleaned != NULL) {
            *length = strlen(digits); /* Cleaned string - use strlen */
        } else {
            *length = value->as.bigint.length; /* Original string - use stored length */
        }
    }

    return digits;
#else
    if (length)
        *length = value->as.bigint.length;

    return value->as.bigint.digits;
#endif
}

bool edn_double_get(const edn_value_t* value, double* out) {
    if (!value || !out || value->type != EDN_TYPE_FLOAT) {
        return false;
    }
    *out = value->as.floating;
    return true;
}

const char* edn_bigdec_get(const edn_value_t* value, size_t* length, bool* negative) {
    if (!value || value->type != EDN_TYPE_BIGDEC) {
        if (length)
            *length = 0;
        if (negative)
            *negative = false;
        return NULL;
    }

    if (negative)
        *negative = value->as.bigdec.negative;

#ifdef EDN_ENABLE_UNDERSCORE_IN_NUMERIC
    /* Clean underscores lazily */
    if (!value->arena) {
        /* No arena - can't allocate cleaned string, return raw */
        if (length)
            *length = value->as.bigdec.length;
        return value->as.bigdec.decimal;
    }

    const char* decimal =
        clean_number_string(value->as.bigdec.decimal, value->as.bigdec.length, value->arena,
                            &((edn_value_t*) value)->as.bigdec.cleaned);
    if (!decimal) {
        if (length)
            *length = 0;
        return NULL;
    }

    if (length) {
        /* If we returned a cleaned string (cached), it has null terminator */
        /* If we returned the original, we must use the stored length */
        if (((edn_value_t*) value)->as.bigdec.cleaned != NULL) {
            *length = strlen(decimal); /* Cleaned string - use strlen */
        } else {
            *length = value->as.bigdec.length; /* Original string - use stored length */
        }
    }

    return decimal;
#else
    if (length)
        *length = value->as.bigdec.length;

    return value->as.bigdec.decimal;
#endif
}

#ifdef EDN_ENABLE_RATIO
bool edn_ratio_get(const edn_value_t* value, int64_t* numerator, int64_t* denominator) {
    if (!value || value->type != EDN_TYPE_RATIO) {
        return false;
    }
    if (numerator)
        *numerator = value->as.ratio.numerator;
    if (denominator)
        *denominator = value->as.ratio.denominator;
    return true;
}
#endif

bool edn_number_as_double(const edn_value_t* value, double* out) {
    if (!value || !out) {
        return false;
    }

    switch (value->type) {
        case EDN_TYPE_INT:
            *out = (double) value->as.integer;
            return true;

        case EDN_TYPE_BIGINT:
            /* Convert BigInt string to double (may lose precision) */
            {
                /* Simple conversion - user should use proper BigInt library for precision */
                double result = 0.0;
                const char* ptr = value->as.bigint.digits;
                size_t len = value->as.bigint.length;
                uint8_t radix = value->as.bigint.radix;

                for (size_t i = 0; i < len; i++) {
                    int digit;
                    if (ptr[i] >= '0' && ptr[i] <= '9') {
                        digit = ptr[i] - '0';
                    } else if (ptr[i] >= 'a' && ptr[i] <= 'z') {
                        digit = 10 + (ptr[i] - 'a');
                    } else if (ptr[i] >= 'A' && ptr[i] <= 'Z') {
                        digit = 10 + (ptr[i] - 'A');
                    } else {
                        continue;
                    }
                    result = result * radix + digit;
                }

                *out = value->as.bigint.negative ? -result : result;
                return true;
            }

        case EDN_TYPE_FLOAT:
            *out = value->as.floating;
            return true;

        case EDN_TYPE_BIGDEC:
            /* Convert BigDecimal string to double (may lose precision) */
            {
                /* Use standard strtod for conversion */
                char buffer[512];
                size_t len = value->as.bigdec.length;
                if (len >= sizeof(buffer)) {
                    len = sizeof(buffer) - 1;
                }
                memcpy(buffer, value->as.bigdec.decimal, len);
                buffer[len] = '\0';

                double result = strtod(buffer, NULL);
                *out = value->as.bigdec.negative ? -result : result;
                return true;
            }

#ifdef EDN_ENABLE_RATIO
        case EDN_TYPE_RATIO:
            /* Convert ratio to double */
            if (value->as.ratio.denominator == 0) {
                return false;
            }
            *out = (double) value->as.ratio.numerator / (double) value->as.ratio.denominator;
            return true;
#endif

        default:
            return false;
    }
}

bool edn_character_get(const edn_value_t* value, uint32_t* out) {
    if (!value || !out || value->type != EDN_TYPE_CHARACTER) {
        return false;
    }
    *out = value->as.character;
    return true;
}

/* Type Predicates */

bool edn_is_string(const edn_value_t* value) {
    return value && value->type == EDN_TYPE_STRING;
}

bool edn_is_number(const edn_value_t* value) {
    if (!value) {
        return false;
    }
    switch (value->type) {
        case EDN_TYPE_INT:
        case EDN_TYPE_BIGINT:
        case EDN_TYPE_FLOAT:
        case EDN_TYPE_BIGDEC:
#ifdef EDN_ENABLE_RATIO
        case EDN_TYPE_RATIO:
#endif
            return true;
        default:
            return false;
    }
}

bool edn_is_integer(const edn_value_t* value) {
    return value && (value->type == EDN_TYPE_INT || value->type == EDN_TYPE_BIGINT);
}

bool edn_is_collection(const edn_value_t* value) {
    if (!value) {
        return false;
    }
    switch (value->type) {
        case EDN_TYPE_LIST:
        case EDN_TYPE_VECTOR:
        case EDN_TYPE_MAP:
        case EDN_TYPE_SET:
            return true;
        default:
            return false;
    }
}

/* String Utilities */

bool edn_string_equals(const edn_value_t* value, const char* str) {
    if (!value || !str || value->type != EDN_TYPE_STRING) {
        return false;
    }

    size_t len;
    const char* edn_str = edn_string_get(value, &len);
    if (!edn_str) {
        return false;
    }

    size_t str_len = strlen(str);
    if (len != str_len) {
        return false;
    }

    return memcmp(edn_str, str, len) == 0;
}

bool edn_symbol_get(const edn_value_t* value, const char** namespace, size_t* ns_length,
                    const char** name, size_t* name_length) {
    if (!value || !name || value->type != EDN_TYPE_SYMBOL) {
        return false;
    }

    if (namespace)
        *namespace = value->as.symbol.namespace;
    if (ns_length)
        *ns_length = value->as.symbol.ns_length;
    *name = value->as.symbol.name;
    if (name_length)
        *name_length = value->as.symbol.name_length;

    return true;
}

bool edn_keyword_get(const edn_value_t* value, const char** namespace, size_t* ns_length,
                     const char** name, size_t* name_length) {
    if (!value || !name || value->type != EDN_TYPE_KEYWORD) {
        return false;
    }

    if (namespace)
        *namespace = value->as.keyword.namespace;
    if (ns_length)
        *ns_length = value->as.keyword.ns_length;
    *name = value->as.keyword.name;
    if (name_length)
        *name_length = value->as.keyword.name_length;

    return true;
}

/* Collection API functions */

size_t edn_list_count(const edn_value_t* value) {
    if (!value || value->type != EDN_TYPE_LIST) {
        return 0;
    }
    return value->as.list.count;
}

edn_value_t* edn_list_get(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_LIST) {
        return NULL;
    }
    if (index >= value->as.list.count) {
        return NULL;
    }
    return value->as.list.elements[index];
}

size_t edn_vector_count(const edn_value_t* value) {
    if (!value || value->type != EDN_TYPE_VECTOR) {
        return 0;
    }
    return value->as.vector.count;
}

edn_value_t* edn_vector_get(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_VECTOR) {
        return NULL;
    }
    if (index >= value->as.vector.count) {
        return NULL;
    }
    return value->as.vector.elements[index];
}

size_t edn_set_count(const edn_value_t* value) {
    if (!value || value->type != EDN_TYPE_SET) {
        return 0;
    }
    return value->as.set.count;
}

edn_value_t* edn_set_get(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_SET) {
        return NULL;
    }
    if (index >= value->as.set.count) {
        return NULL;
    }
    return value->as.set.elements[index];
}

bool edn_set_contains(const edn_value_t* value, const edn_value_t* element) {
    if (!value || value->type != EDN_TYPE_SET || !element) {
        return false;
    }

    for (size_t i = 0; i < value->as.set.count; i++) {
        if (edn_value_equal(value->as.set.elements[i], element)) {
            return true;
        }
    }

    return false;
}

size_t edn_map_count(const edn_value_t* value) {
    if (!value || value->type != EDN_TYPE_MAP) {
        return 0;
    }
    return value->as.map.count;
}

edn_value_t* edn_map_get_key(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_MAP) {
        return NULL;
    }
    if (index >= value->as.map.count) {
        return NULL;
    }
    return value->as.map.keys[index];
}

edn_value_t* edn_map_get_value(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_MAP) {
        return NULL;
    }
    if (index >= value->as.map.count) {
        return NULL;
    }
    return value->as.map.values[index];
}

edn_value_t* edn_map_lookup(const edn_value_t* value, const edn_value_t* key) {
    if (!value || value->type != EDN_TYPE_MAP || !key) {
        return NULL;
    }

    for (size_t i = 0; i < value->as.map.count; i++) {
        if (edn_value_equal(value->as.map.keys[i], key)) {
            return value->as.map.values[i];
        }
    }

    return NULL;
}

bool edn_map_contains_key(const edn_value_t* value, const edn_value_t* key) {
    if (!value || value->type != EDN_TYPE_MAP || !key) {
        return false;
    }

    for (size_t i = 0; i < value->as.map.count; i++) {
        if (edn_value_equal(value->as.map.keys[i], key)) {
            return true;
        }
    }

    return false;
}

/* Map Convenience Functions */

edn_value_t* edn_map_get_keyword(const edn_value_t* map, const char* keyword) {
    if (!map || !keyword || map->type != EDN_TYPE_MAP) {
        return NULL;
    }

    edn_value_t temp_key;
    temp_key.type = EDN_TYPE_KEYWORD;
    temp_key.as.keyword.namespace = NULL;
    temp_key.as.keyword.ns_length = 0;
    temp_key.as.keyword.name = keyword;
    temp_key.as.keyword.name_length = strlen(keyword);
    temp_key.arena = NULL;
    temp_key.cached_hash = 0;
#ifdef EDN_ENABLE_METADATA
    temp_key.metadata = NULL;
#endif

    return edn_map_lookup(map, &temp_key);
}

edn_value_t* edn_map_get_namespaced_keyword(const edn_value_t* map, const char* namespace,
                                            const char* name) {
    if (!map || !namespace || !name || map->type != EDN_TYPE_MAP) {
        return NULL;
    }

    edn_value_t temp_key;
    temp_key.type = EDN_TYPE_KEYWORD;
    temp_key.as.keyword.namespace = namespace;
    temp_key.as.keyword.ns_length = strlen(namespace);
    temp_key.as.keyword.name = name;
    temp_key.as.keyword.name_length = strlen(name);
    temp_key.arena = NULL;
    temp_key.cached_hash = 0;
#ifdef EDN_ENABLE_METADATA
    temp_key.metadata = NULL;
#endif

    return edn_map_lookup(map, &temp_key);
}

edn_value_t* edn_map_get_string_key(const edn_value_t* map, const char* key) {
    if (!map || !key || map->type != EDN_TYPE_MAP) {
        return NULL;
    }

    /* Create temporary string value for lookup */
    edn_value_t temp_key;
    temp_key.type = EDN_TYPE_STRING;
    temp_key.as.string.data = key;
    size_t key_len = strlen(key);
    temp_key.as.string.length_and_flags = key_len; /* No escapes, not decoded */
    temp_key.as.string.decoded = NULL;
    temp_key.arena = NULL;
    temp_key.cached_hash = 0;
#ifdef EDN_ENABLE_METADATA
    temp_key.metadata = NULL;
#endif

    return edn_map_lookup(map, &temp_key);
}

/* Tagged Literal API */

bool edn_tagged_get(const edn_value_t* value, const char** tag, size_t* tag_length,
                    edn_value_t** tagged_value) {
    if (!value || !tag || !tagged_value || value->type != EDN_TYPE_TAGGED) {
        return false;
    }

    *tag = value->as.tagged.tag;
    if (tag_length)
        *tag_length = value->as.tagged.tag_length;
    *tagged_value = value->as.tagged.value;

    return true;
}

/* Metadata API */

#ifdef EDN_ENABLE_METADATA
edn_value_t* edn_value_meta(const edn_value_t* value) {
    if (!value) {
        return NULL;
    }
    return value->metadata;
}

bool edn_value_has_meta(const edn_value_t* value) {
    if (!value) {
        return false;
    }
    return value->metadata != NULL;
}
#endif
