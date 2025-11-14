/**
 * EDN.C - Identifier parsing (symbols and keywords)
 * 
 * Unified parser for symbols and keywords with optional namespaces.
 * SIMD-accelerated for identifiers >16 bytes, scalar for common short cases.
 * Handles reserved words (nil, true, false) and namespace validation.
 */

#include <stdbool.h>
#include <string.h>

#include "edn_internal.h"

/**
 * Result of identifier scanning (internal).
 * 
 * Contains parsed namespace and name components. If namespace is NULL,
 * the identifier is non-namespaced. Special case: "/" has no namespace.
 */
typedef struct {
    const char* start;
    size_t length;
    const char* namespace;
    size_t ns_length;
    const char* name;
    size_t name_length;
    bool valid;
} edn_identifier_scan_t;

/**
 * Scan identifier from input buffer.
 * 
 * Uses scalar path for identifiers ≤16 bytes (80%+ of cases), SIMD for longer.
 * Finds first '/' for namespace split and stops at delimiter. Special handling
 * for "/" alone as valid symbol. Returns invalid result for empty identifiers
 * or malformed namespace syntax ("/foo" or "foo/").
 */
static edn_identifier_scan_t scan_identifier(const char* ptr, const char* end) {
    edn_identifier_scan_t result = {0};
    const char* start = ptr;
    const char* slash = NULL;

    size_t remaining = end - ptr;

    if (remaining <= 16) {
        while (ptr < end) {
            unsigned char c = (unsigned char) *ptr;
            if (is_delimiter(c)) {
                break;
            }
            if (c == '/' && !slash) {
                slash = ptr;
            }
            ptr++;
        }
    } else {
        edn_identifier_scan_result_t simd_result = edn_simd_scan_identifier(ptr, end);
        ptr = simd_result.end;
        slash = simd_result.first_slash;

        while (ptr < end) {
            unsigned char c = (unsigned char) *ptr;
            if (is_delimiter(c)) {
                break;
            }
            if (c == '/' && !slash) {
                slash = ptr;
            }
            ptr++;
        }
    }

    if (ptr == start) {
        result.valid = false;
        return result;
    }

    result.start = start;
    result.length = ptr - start;

    if (slash) {
        if (result.length == 1) {
            result.namespace = NULL;
            result.ns_length = 0;
            result.name = start;
            result.name_length = 1;
            result.valid = true;
            return result;
        }

        if (slash == start) {
            result.valid = false;
            return result;
        }

        if (slash == ptr - 1) {
            result.valid = false;
            return result;
        }

        result.namespace = start;
        result.ns_length = slash - start;
        result.name = slash + 1;
        result.name_length = ptr - (slash + 1);
    } else {
        result.namespace = NULL;
        result.ns_length = 0;
        result.name = start;
        result.name_length = result.length;
    }

    result.valid = true;
    return result;
}

/**
 * Create nil value.
 */
static edn_value_t* create_nil_value(edn_parser_t* parser) {
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_NIL;
    value->arena = parser->arena;
    return value;
}

/**
 * Create boolean value.
 */
static edn_value_t* create_bool_value(edn_parser_t* parser, bool val) {
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_BOOL;
    value->as.boolean = val;
    value->arena = parser->arena;
    return value;
}

/**
 * Create symbol value with optional namespace.
 */
static edn_value_t* create_symbol_value(edn_parser_t* parser, const char* namespace,
                                        size_t ns_length, const char* name, size_t name_length) {
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_SYMBOL;
    value->as.symbol.namespace = namespace;
    value->as.symbol.ns_length = ns_length;
    value->as.symbol.name = name;
    value->as.symbol.name_length = name_length;
    value->arena = parser->arena;
    return value;
}

/**
 * Create keyword value with optional namespace.
 */
static edn_value_t* create_keyword_value(edn_parser_t* parser, const char* namespace,
                                         size_t ns_length, const char* name, size_t name_length) {
    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (!value) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory";
        return NULL;
    }

    value->type = EDN_TYPE_KEYWORD;
    value->as.keyword.namespace = namespace;
    value->as.keyword.ns_length = ns_length;
    value->as.keyword.name = name;
    value->as.keyword.name_length = name_length;
    value->arena = parser->arena;
    return value;
}

/**
 * Parse identifier (symbol or keyword) from current parser position.
 * 
 * Handles:
 * - Non-namespaced keywords: :foo, :name
 * - Namespaced keywords: :ns/name, :ns.foo/bar
 * - Reserved symbols: nil, true, false
 * - Non-namespaced symbols: foo, bar, +, /
 * - Namespaced symbols: ns/name, ns.foo/bar
 * 
 * Fast path optimized for non-namespaced identifiers (80% of cases).
 * Validates namespace/name syntax and colon placement.
 */
edn_value_t* edn_parse_identifier(edn_parser_t* parser) {
    edn_identifier_scan_t scan = scan_identifier(parser->current, parser->end);

    if (!scan.valid) {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Invalid identifier";
        return NULL;
    }

    parser->current = scan.start + scan.length;

    if (!scan.namespace) {
        if (*scan.name == ':') {
            const char* kw_name = scan.name + 1;
            size_t kw_len = scan.name_length - 1;

            if (kw_len == 0) {
                parser->error = EDN_ERROR_INVALID_SYNTAX;
                parser->error_message = "Empty keyword name";
                return NULL;
            }

            if (*kw_name == ':') {
                parser->error = EDN_ERROR_INVALID_SYNTAX;
                parser->error_message = "Keyword name cannot start with ':'";
                return NULL;
            }

            return create_keyword_value(parser, NULL, 0, kw_name, kw_len);
        }

        const char* sym_name = scan.name;
        size_t sym_len = scan.name_length;

        switch (sym_len) {
            case 3:
                if (memcmp(sym_name, "nil", 3) == 0) {
                    return create_nil_value(parser);
                }
                break;
            case 4:
                if (memcmp(sym_name, "true", 4) == 0) {
                    return create_bool_value(parser, true);
                }
                break;
            case 5:
                if (memcmp(sym_name, "false", 5) == 0) {
                    return create_bool_value(parser, false);
                }
                break;
        }

        return create_symbol_value(parser, NULL, 0, sym_name, sym_len);
    }

    if (*scan.namespace == ':') {
        const char* kw_ns = scan.namespace + 1;
        size_t kw_ns_len = scan.ns_length - 1;
        const char* kw_name = scan.name;
        size_t kw_name_len = scan.name_length;

        if (kw_ns_len == 0) {
            parser->error = EDN_ERROR_INVALID_SYNTAX;
            parser->error_message = "Empty namespace in keyword";
            return NULL;
        }

        if (*kw_ns == ':') {
            parser->error = EDN_ERROR_INVALID_SYNTAX;
            parser->error_message = "Keyword namespace cannot start with ':'";
            return NULL;
        }

        return create_keyword_value(parser, kw_ns, kw_ns_len, kw_name, kw_name_len);
    }

    return create_symbol_value(parser, scan.namespace, scan.ns_length, scan.name, scan.name_length);
}
