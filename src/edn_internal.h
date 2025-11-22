/**
 * EDN.C - Internal header
 * 
 * Internal data structures and helper functions.
 */

#ifndef EDN_INTERNAL_H
#define EDN_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "edn.h"

/* Arena allocator constants and structures */
#define ARENA_INITIAL_SIZE (16 * 1024) /* Start with 16KB for small documents */
#define ARENA_MEDIUM_SIZE (64 * 1024)  /* 64KB blocks */
#define ARENA_LARGE_SIZE (256 * 1024)  /* 256KB for large documents */

/* Arena block structure - exposed for inline allocation */
typedef struct arena_block {
    struct arena_block* next;
    size_t used;
    size_t capacity;
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
    uint8_t data[];
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
} arena_block_t;

/* Arena allocator structure - exposed for inline allocation */
struct edn_arena {
    arena_block_t* current;
    arena_block_t* first;
    size_t next_block_size;
    size_t total_allocated;
};

typedef struct edn_arena edn_arena_t;

/* Map entry structure for key-value pairs */
typedef struct {
    edn_value_t* key;
    edn_value_t* value;
} edn_map_entry_t;

/* Internal value structure */
struct edn_value {
    edn_type_t type;
    uint64_t cached_hash; /* Cached hash value (0 = not computed yet) */
#ifdef EDN_ENABLE_METADATA
    edn_value_t* metadata;
#endif
    union {
        bool boolean;
        int64_t integer;
        struct {
            const char*
                digits; /* Pointer to digit string in input buffer (zero-copy, may contain underscores) */
            size_t length; /* Number of characters in digit string (including underscores) */
            bool negative; /* Sign bit */
            uint8_t radix; /* Number base (2-36, default 10) */
            char* cleaned; /* Lazy-cleaned string without underscores (NULL until needed) */
        } bigint;
        double floating;
        struct {
            const char*
                decimal; /* Pointer to decimal string in input buffer (zero-copy, may contain underscores) */
            size_t length; /* Number of characters in decimal string (including underscores) */
            bool negative; /* Sign bit */
            char* cleaned; /* Lazy-cleaned string without underscores (NULL until needed) */
        } bigdec;
#ifdef EDN_ENABLE_RATIO
        struct {
            int64_t numerator;
            int64_t denominator;
        } ratio;
#endif
        uint32_t character; /* Unicode codepoint */
        struct {
            const char* data;          /* Pointer to string content in input buffer (zero-copy) */
            uint64_t length_and_flags; /* Combined: length (bits 0-61), flags (bits 62-63) */
            /* bit 63: has_escapes - True if string contains escape sequences */
            /* bit 62: is_decoded - True if decoded pointer is set */
            /* bits 61-0: actual length (supports up to 2 PB strings) */
            char* decoded; /* Lazy-decoded string (NULL until needed) */
        } string;
        struct {
            const char* namespace; /* NULL if no namespace, points into input (zero-copy) */
            size_t ns_length;      /* 0 if no namespace */
            const char* name;      /* Points into input (zero-copy) */
            size_t name_length;    /* Length of name */
        } symbol;
        struct {
            const char* namespace; /* NULL if no namespace, points into input (zero-copy) */
            size_t ns_length;      /* 0 if no namespace */
            const char* name;      /* Points into input (zero-copy) */
            size_t name_length;    /* Length of name */
        } keyword;
        struct {
            edn_value_t** elements;
            size_t count;
        } list;
        struct {
            edn_value_t** elements;
            size_t count;
        } vector;
        struct {
            edn_value_t** keys;
            edn_value_t** values;
            size_t count;
        } map;
        struct {
            edn_value_t** elements;
            size_t count;
        } set;
        struct {
            const char* tag;
            size_t tag_length;
            edn_value_t* value;
        } tagged;
    } as;
    edn_arena_t* arena; /* Arena that owns this value */
};

/* String packing flags and helper functions */
#define EDN_STRING_FLAG_HAS_ESCAPES (1ULL << 63)
#define EDN_STRING_FLAG_IS_DECODED (1ULL << 62)
#define EDN_STRING_LENGTH_MASK ((1ULL << 62) - 1)

static inline size_t edn_string_get_length(const edn_value_t* value) {
    return value->as.string.length_and_flags & EDN_STRING_LENGTH_MASK;
}

static inline bool edn_string_has_escapes(const edn_value_t* value) {
    return (value->as.string.length_and_flags & EDN_STRING_FLAG_HAS_ESCAPES) != 0;
}

static inline void edn_string_set_length(edn_value_t* value, size_t length) {
    value->as.string.length_and_flags =
        (value->as.string.length_and_flags & ~EDN_STRING_LENGTH_MASK) | length;
}

static inline void edn_string_set_has_escapes(edn_value_t* value, bool has_escapes) {
    if (has_escapes) {
        value->as.string.length_and_flags |= EDN_STRING_FLAG_HAS_ESCAPES;
    } else {
        value->as.string.length_and_flags &= ~EDN_STRING_FLAG_HAS_ESCAPES;
    }
}

/* Parser state */
typedef struct {
    const char* input;
    const char* current;
    const char* end;
    size_t line;
    size_t column;
    size_t depth; /* Current nesting depth for collections */
    edn_arena_t* arena;
    edn_error_t error;
    const char* error_message;
    /* Reader configuration (optional) */
    edn_reader_registry_t* reader_registry;
    edn_default_reader_mode_t default_reader_mode;
    /* Discard mode - when true, readers are not invoked */
    bool discard_mode;
} edn_parser_t;

edn_arena_t* edn_arena_create(void);
void edn_arena_destroy(edn_arena_t* arena);
void* edn_arena_alloc_slow(edn_arena_t* arena, size_t size);

static inline void* edn_arena_alloc(edn_arena_t* arena, size_t size) {
    if (!arena) {
        return NULL;
    }

    size = (size + 7) & ~7;

    arena_block_t* block = arena->current;

    if (block->used + size <= block->capacity) {
        void* ptr = block->data + block->used;
        block->used += size;
        return ptr;
    }

    return edn_arena_alloc_slow(arena, size);
}

static inline edn_value_t* edn_arena_alloc_value(edn_arena_t* arena) {
    edn_value_t* value = (edn_value_t*) edn_arena_alloc(arena, sizeof(edn_value_t));
    if (value) {
        value->cached_hash = 0;
#ifdef EDN_ENABLE_METADATA
        value->metadata = NULL;
#endif
    }
    return value;
}

/**
 * Delimiter lookup table for fast character classification.
 * 
 * Single memory lookup replaces multiple branches.
 * Cache-friendly: entire table fits in L1 cache (256 bytes).
 * 
 * Used by both identifier and character parsing.
 * 
 * 1 = delimiter (stops scanning)
 * 0 = valid character in identifiers
 */
static const uint8_t DELIMITER_TABLE[256] = {
    /* 0x00-0x1F: Control characters */
    /* Following Clojure behavior: */
    /* - Whitespace delimiters: 0x09-0x0D (tab, LF, VT, FF, CR) and 0x1C-0x1F (FS, GS, RS, US) */
    /* - Valid in identifiers: 0x00-0x08, 0x0E-0x1B */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,

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
 * 
 * Used by identifier and character parsing.
 */
static inline bool is_delimiter(unsigned char c) {
    return DELIMITER_TABLE[c];
}

bool edn_parser_skip_whitespace(edn_parser_t* parser);
edn_value_t* edn_parser_parse_value(edn_parser_t* parser);

const char* edn_simd_skip_whitespace(const char* ptr, const char* end);
const char* edn_simd_find_quote(const char* ptr, const char* end, bool* out_has_backslash);

/* String parsing functions */
typedef struct {
    const char* start; /* Start of string content (after opening quote) */
    const char* end;   /* End of string content (before closing quote) */
    bool has_escapes;  /* True if backslash found in string */
    bool valid;        /* True if string is valid (found closing quote) */
} edn_string_scan_t;

edn_string_scan_t edn_parse_string_lazy(const char* ptr, const char* end);
char* edn_decode_string(edn_arena_t* arena, const char* data, size_t length);

/* Number parsing functions */
typedef enum {
    EDN_NUMBER_INT64,  /* Fits in int64_t */
    EDN_NUMBER_BIGINT, /* Overflow, needs BigInt */
    EDN_NUMBER_DOUBLE, /* Has decimal point or exponent */
    EDN_NUMBER_BIGDEC, /* BigDecimal - exact precision with M suffix */
    EDN_NUMBER_INVALID /* Parse error */
} edn_number_type_t;

typedef struct {
    const char* start;        /* Start of entire number (includes sign) */
    const char* end;          /* End of entire number (includes N suffix if present) */
    const char* digits_start; /* Start of actual digits (after sign and radix prefix) */
    const char* digits_end;   /* End of digits (excludes N suffix, for BigInt storage) */
    edn_number_type_t type;   /* Detected type */
    uint8_t radix;            /* Number base (2-36, default 10) */
    bool negative;            /* Sign */
    bool valid;               /* True if valid number */
} edn_number_scan_t;

edn_value_t* edn_read_number(edn_parser_t* parser);

/* SIMD number helper */
const char* edn_simd_scan_digits(const char* ptr, const char* end);

/* Character parsing function */
edn_value_t* edn_parse_character(edn_parser_t* parser);

/* Identifier parsing functions */
typedef struct {
    const char* end;         /* Pointer to first delimiter (end of identifier) */
    const char* first_slash; /* Pointer to first '/', or NULL if none found */
} edn_identifier_scan_result_t;

edn_identifier_scan_result_t edn_simd_scan_identifier(const char* ptr, const char* end);
edn_value_t* edn_parse_identifier(edn_parser_t* parser);

/* Symbolic value parsing function */
edn_value_t* edn_parse_symbolic_value(edn_parser_t* parser);

/* Value equality and comparison functions */
bool edn_value_equal(const edn_value_t* a, const edn_value_t* b);
int edn_value_compare(const void* a, const void* b);
uint64_t edn_value_hash(const edn_value_t* value);

/* Uniqueness checking (for sets and maps) */
bool edn_has_duplicates(edn_value_t** elements, size_t count);

/* Collection parsers */
edn_value_t* edn_parse_list(edn_parser_t* parser);
edn_value_t* edn_parse_vector(edn_parser_t* parser);
edn_value_t* edn_parse_set(edn_parser_t* parser);
edn_value_t* edn_parse_map(edn_parser_t* parser);

/* Tagged literal parser */
edn_value_t* edn_parse_tagged(edn_parser_t* parser);

/* Discard reader macro parser */
edn_value_t* edn_parse_discard_value(edn_parser_t* parser);

/* Internal reader lookup (for non-null-terminated tag strings) */
edn_reader_fn edn_reader_lookup_internal(const edn_reader_registry_t* registry, const char* tag,
                                         size_t tag_length);

/* Namespaced map parser (Clojure extension, requires EDN_ENABLE_MAP_NAMESPACE_SYNTAX) */
#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX
edn_value_t* edn_parse_namespaced_map(edn_parser_t* parser);
#endif

/* Metadata parser (Clojure extension, requires EDN_ENABLE_METADATA) */
#ifdef EDN_ENABLE_METADATA
edn_value_t* edn_parse_metadata(edn_parser_t* parser);
#endif

/* Text block parser (experimental, requires EDN_ENABLE_TEXT_BLOCKS) */
#ifdef EDN_ENABLE_TEXT_BLOCKS
edn_value_t* edn_parse_text_block(edn_parser_t* parser);
#endif

#endif /* EDN_INTERNAL_H */
