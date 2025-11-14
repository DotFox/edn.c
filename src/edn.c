#include "../include/edn.h"

#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

edn_result_t edn_parse(const char* input, size_t length) {
    return edn_parse_with_options(input, length, NULL);
}

edn_result_t edn_parse_with_options(const char* input, size_t length,
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
    parser.line = 1;
    parser.column = 1;
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
    result.error_line = parser.line;
    result.error_column = parser.column;
    result.error_message = parser.error_message;

    /* Free arena if parsing failed (no value was created) */
    if (result.value == NULL && parser.arena != NULL) {
        edn_arena_destroy(parser.arena);
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
    ['^'] = CHAR_TYPE_IDENTIFIER,
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
    value->as.string.length = scan.end - scan.start;
    value->as.string.has_escapes = scan.has_escapes;
    value->as.string.decoded = NULL;
    value->arena = parser->arena;

    parser->current = scan.end + 1;

    return value;
}

static edn_value_t* parse_number_value(edn_parser_t* parser) {
    edn_number_scan_t scan = edn_scan_number(parser->current, parser->end);

    if (!scan.valid) {
        parser->error = EDN_ERROR_INVALID_NUMBER;
        parser->error_message = "Invalid number format";
        return NULL;
    }

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->arena = parser->arena;

    if (scan.type == EDN_NUMBER_INT64) {
        int64_t num;
        if (!edn_parse_int64(scan.start, scan.end, &num, scan.radix)) {
            value->type = EDN_TYPE_BIGINT;
            value->as.bigint.digits = scan.start;
            value->as.bigint.length = scan.end - scan.start;
            value->as.bigint.negative = scan.negative;
            value->as.bigint.radix = scan.radix;
        } else {
            value->type = EDN_TYPE_INT;
            value->as.integer = num;
        }
    } else if (scan.type == EDN_NUMBER_BIGINT) {
        value->type = EDN_TYPE_BIGINT;
        value->as.bigint.digits = scan.start;
        value->as.bigint.length = scan.end - scan.start;
        value->as.bigint.negative = scan.negative;
        value->as.bigint.radix = scan.radix;
    } else if (scan.type == EDN_NUMBER_DOUBLE) {
        value->type = EDN_TYPE_FLOAT;
        value->as.floating = edn_parse_double(scan.start, scan.end);
    } else {
        parser->error = EDN_ERROR_INVALID_NUMBER;
        parser->error_message = "Invalid number type";
        return NULL;
    }

    parser->current = scan.end;
    return value;
}

edn_value_t* edn_parser_parse_value(edn_parser_t* parser) {
    if (parser->current < parser->end) {
        char c = *parser->current;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == ';') {
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
    if (!value->as.string.has_escapes) {
        if (length)
            *length = value->as.string.length;

        /* Need to add null terminator for zero-copy strings */
        /* For now, we'll need to allocate. In future, we can optimize this */
        if (!value->as.string.decoded) {
            /* Allocate and copy with null terminator */
            char* copy = edn_arena_alloc(value->arena, value->as.string.length + 1);
            if (copy) {
                memcpy(copy, value->as.string.data, value->as.string.length);
                copy[value->as.string.length] = '\0';
                /* Cast away const - we're modifying cached field */
                ((edn_value_t*) value)->as.string.decoded = copy;
            }
        }
        return value->as.string.decoded;
    }

    /* Slow path: has escapes, decode if not already cached */
    if (!value->as.string.decoded) {
        char* decoded =
            edn_decode_string(value->arena, value->as.string.data, value->as.string.length);
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

    if (length)
        *length = value->as.bigint.length;
    if (negative)
        *negative = value->as.bigint.negative;
    if (radix)
        *radix = value->as.bigint.radix;

    return value->as.bigint.digits;
}

bool edn_double_get(const edn_value_t* value, double* out) {
    if (!value || !out || value->type != EDN_TYPE_FLOAT) {
        return false;
    }
    *out = value->as.floating;
    return true;
}

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
    return value->as.map.entries[index].key;
}

edn_value_t* edn_map_get_value(const edn_value_t* value, size_t index) {
    if (!value || value->type != EDN_TYPE_MAP) {
        return NULL;
    }
    if (index >= value->as.map.count) {
        return NULL;
    }
    return value->as.map.entries[index].value;
}

edn_value_t* edn_map_lookup(const edn_value_t* value, const edn_value_t* key) {
    if (!value || value->type != EDN_TYPE_MAP || !key) {
        return NULL;
    }

    for (size_t i = 0; i < value->as.map.count; i++) {
        if (edn_value_equal(value->as.map.entries[i].key, key)) {
            return value->as.map.entries[i].value;
        }
    }

    return NULL;
}

bool edn_map_contains_key(const edn_value_t* value, const edn_value_t* key) {
    if (!value || value->type != EDN_TYPE_MAP || !key) {
        return false;
    }

    for (size_t i = 0; i < value->as.map.count; i++) {
        if (edn_value_equal(value->as.map.entries[i].key, key)) {
            return true;
        }
    }

    return false;
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
