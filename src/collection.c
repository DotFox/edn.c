/**
 * EDN.C - Collection parsers
 */

#include <stdint.h>
#include <string.h>

#include "edn_internal.h"

/* Guard capacity multiplications against size_t overflow. */
static inline bool capacity_too_large(size_t count) {
    return count > (SIZE_MAX / sizeof(edn_value_t*));
}

typedef struct {
    edn_value_t** elements;
    size_t count;
    size_t capacity;
    edn_arena_t* arena;
    edn_value_t* inline_storage[8];
} edn_collection_builder_t;

static void edn_collection_builder_init(edn_collection_builder_t* builder, edn_arena_t* arena,
                                        size_t initial_capacity) {
    builder->count = 0;
    builder->arena = arena;

    if (initial_capacity <= 8) {
        builder->elements = builder->inline_storage;
        builder->capacity = 8;
    } else {
        builder->elements = edn_arena_alloc(arena, initial_capacity * sizeof(edn_value_t*));
        if (builder->elements != NULL) {
            builder->capacity = initial_capacity;
        } else {
            builder->elements = builder->inline_storage;
            builder->capacity = 8;
        }
    }
}

static bool edn_collection_builder_add(edn_collection_builder_t* builder, edn_value_t* value) {
    if (builder->count >= builder->capacity) {
        size_t new_capacity = builder->capacity + (builder->capacity >> 1);
        if (new_capacity <= builder->capacity) {
            new_capacity = builder->capacity + 8;
        }
        if (capacity_too_large(new_capacity)) {
            return false;
        }

        edn_value_t** new_elements =
            edn_arena_alloc(builder->arena, new_capacity * sizeof(edn_value_t*));
        if (new_elements == NULL) {
            return false;
        }

        if (builder->count > 0) {
            memcpy(new_elements, builder->elements, builder->count * sizeof(edn_value_t*));
        }

        builder->elements = new_elements;
        builder->capacity = new_capacity;
    }

    builder->elements[builder->count++] = value;
    return true;
}

static edn_value_t** edn_collection_builder_finish(edn_collection_builder_t* builder,
                                                   size_t* out_count) {
    *out_count = builder->count;

    if (builder->elements == builder->inline_storage && builder->count > 0) {
        edn_value_t** permanent =
            edn_arena_alloc(builder->arena, builder->count * sizeof(edn_value_t*));
        if (permanent != NULL) {
            memcpy(permanent, builder->inline_storage, builder->count * sizeof(edn_value_t*));
            return permanent;
        }
    }

    return builder->elements;
}

edn_value_t* edn_read_list(edn_parser_t* parser) {
    const char* value_start = parser->current;

    parser->current++;
    if (!edn_enter_depth(parser)) {
        return NULL;
    }

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_read_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            edn_leave_depth(parser);
            edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                 "Out of memory while building list", value_start, parser->current);
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        if (parser->error == EDN_ERROR_UNEXPECTED_EOF) {
            edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                                 "Unterminated list (missing ')')", value_start, parser->current);
        }
        edn_leave_depth(parser);
        return NULL;
    }

    if (parser->current >= parser->end) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                             "Unterminated list (missing ')')", value_start, parser->current);
        return NULL;
    }

    if (*parser->current != ')') {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNMATCHED_DELIMITER,
                             "Mismatched closing delimiter in list", value_start,
                             parser->current + 1);
        return NULL;
    }

    parser->current++;
    edn_leave_depth(parser);

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY, "Out of memory allocating list",
                             value_start, parser->current);
        return NULL;
    }

    value->type = EDN_TYPE_LIST;
    value->as.list.elements = elements;
    value->as.list.count = count;
    value->arena = parser->arena;
    value->source_start = value_start - parser->input;
    value->source_end = parser->current - parser->input;

    return value;
}

edn_value_t* edn_read_vector(edn_parser_t* parser) {
    const char* value_start = parser->current;

    parser->current++;
    if (!edn_enter_depth(parser)) {
        return NULL;
    }

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_read_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            edn_leave_depth(parser);
            edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                 "Out of memory while building vector", value_start,
                                 parser->current);
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        if (parser->error == EDN_ERROR_UNEXPECTED_EOF) {
            edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                                 "Unterminated vector (missing ']')", value_start, parser->current);
        }
        edn_leave_depth(parser);
        return NULL;
    }

    if (parser->current >= parser->end) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                             "Unterminated vector (missing ']')", value_start, parser->current);
        return NULL;
    }

    if (*parser->current != ']') {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNMATCHED_DELIMITER,
                             "Mismatched closing delimiter in vector", value_start,
                             parser->current + 1);
        return NULL;
    }

    parser->current++;
    edn_leave_depth(parser);

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY, "Out of memory allocating vector",
                             value_start, parser->current);
        return NULL;
    }

    value->type = EDN_TYPE_VECTOR;
    value->as.vector.elements = elements;
    value->as.vector.count = count;
    value->arena = parser->arena;
    value->source_start = value_start - parser->input;
    value->source_end = parser->current - parser->input;

    return value;
}

edn_value_t* edn_read_set(edn_parser_t* parser) {
    const char* value_start = parser->current;

    parser->current += 2;
    if (!edn_enter_depth(parser)) {
        return NULL;
    }

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_read_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            edn_leave_depth(parser);
            edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                 "Out of memory while building set", value_start, parser->current);
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        if (parser->error == EDN_ERROR_UNEXPECTED_EOF) {
            edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                                 "Unterminated set (missing '}')", value_start, parser->current);
        }
        edn_leave_depth(parser);
        return NULL;
    }

    if (parser->current >= parser->end) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                             "Unterminated set (missing '}')", value_start, parser->current);
        return NULL;
    }

    if (*parser->current != '}') {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNMATCHED_DELIMITER,
                             "Mismatched closing delimiter in set", value_start,
                             parser->current + 1);
        return NULL;
    }

    parser->current++;
    edn_leave_depth(parser);

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    /* Check for duplicate elements (EDN spec requirement) */
    if (count > 1 && edn_has_duplicates(elements, count)) {
        edn_parser_set_error(parser, EDN_ERROR_DUPLICATE_ELEMENT, "Set contains duplicate elements",
                             value_start, parser->current);
        return NULL;
    }

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY, "Out of memory allocating set",
                             value_start, parser->current);
        return NULL;
    }

    value->type = EDN_TYPE_SET;
    value->as.set.elements = elements;
    value->as.set.count = count;
    value->arena = parser->arena;
    value->source_start = value_start - parser->input;
    value->source_end = parser->current - parser->input;

    return value;
}

typedef struct {
    edn_value_t** keys;
    edn_value_t** values;
    size_t count;
    size_t capacity;
    edn_arena_t* arena;
    edn_value_t* inline_keys[8];
    edn_value_t* inline_values[8];
} edn_map_builder_t;

static void edn_map_builder_init(edn_map_builder_t* builder, edn_arena_t* arena,
                                 size_t initial_capacity) {
    builder->count = 0;
    builder->arena = arena;

    if (initial_capacity <= 8) {
        builder->keys = builder->inline_keys;
        builder->values = builder->inline_values;
        builder->capacity = 8;
    } else {
        builder->keys = edn_arena_alloc(arena, initial_capacity * sizeof(edn_value_t*));
        builder->values = edn_arena_alloc(arena, initial_capacity * sizeof(edn_value_t*));
        if (builder->keys != NULL && builder->values != NULL) {
            builder->capacity = initial_capacity;
        } else {
            builder->keys = builder->inline_keys;
            builder->values = builder->inline_values;
            builder->capacity = 8;
        }
    }
}

static bool edn_map_builder_add(edn_map_builder_t* builder, edn_value_t* key, edn_value_t* value) {
    if (builder->count >= builder->capacity) {
        size_t new_capacity = builder->capacity + (builder->capacity >> 1);
        if (new_capacity <= builder->capacity) {
            new_capacity = builder->capacity + 8;
        }
        if (capacity_too_large(new_capacity)) {
            return false;
        }

        edn_value_t** new_keys =
            edn_arena_alloc(builder->arena, new_capacity * sizeof(edn_value_t*));
        edn_value_t** new_values =
            edn_arena_alloc(builder->arena, new_capacity * sizeof(edn_value_t*));
        if (new_keys == NULL || new_values == NULL) {
            return false;
        }

        if (builder->count > 0) {
            memcpy(new_keys, builder->keys, builder->count * sizeof(edn_value_t*));
            memcpy(new_values, builder->values, builder->count * sizeof(edn_value_t*));
        }

        builder->keys = new_keys;
        builder->values = new_values;
        builder->capacity = new_capacity;
    }

    builder->keys[builder->count] = key;
    builder->values[builder->count] = value;
    builder->count++;

    return true;
}

static void edn_map_builder_finish(edn_map_builder_t* builder, edn_value_t*** out_keys,
                                   edn_value_t*** out_values, size_t* out_count) {
    *out_count = builder->count;

    /* If using inline storage and we have entries, allocate permanent storage */
    if (builder->keys == builder->inline_keys && builder->count > 0) {
        edn_value_t** permanent_keys =
            edn_arena_alloc(builder->arena, builder->count * sizeof(edn_value_t*));
        edn_value_t** permanent_values =
            edn_arena_alloc(builder->arena, builder->count * sizeof(edn_value_t*));
        if (permanent_keys != NULL && permanent_values != NULL) {
            memcpy(permanent_keys, builder->inline_keys, builder->count * sizeof(edn_value_t*));
            memcpy(permanent_values, builder->inline_values, builder->count * sizeof(edn_value_t*));
            *out_keys = permanent_keys;
            *out_values = permanent_values;
            return;
        }
    }

    *out_keys = builder->keys;
    *out_values = builder->values;
}

static edn_value_t* edn_read_map_internal(edn_parser_t* parser, const char* value_start,
                                          const char* ns_name, size_t ns_length) {
    parser->current++;
    if (!edn_enter_depth(parser)) {
        return NULL;
    }

    edn_map_builder_t builder;
    edn_map_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* key = edn_read_value(parser);
        if (key == NULL) {
            if (parser->error != EDN_OK) {
                if (parser->error == EDN_ERROR_UNEXPECTED_EOF) {
                    edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                                         ns_name != NULL
                                             ? "Unterminated namespaced map (missing '}')"
                                             : "Unterminated map (missing '}')",
                                         value_start, parser->current);
                }
                edn_leave_depth(parser);
                return NULL;
            }
            break;
        }

        edn_value_t* value = edn_read_value(parser);
        if (value == NULL) {
            edn_leave_depth(parser);
            if (parser->error == EDN_OK) {
                edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX,
                                     "Map has odd number of elements (key without value)",
                                     value_start, parser->current);
            } else if (parser->error == EDN_ERROR_UNEXPECTED_EOF) {
                edn_parser_set_error(parser, EDN_ERROR_UNTERMINATED_COLLECTION,
                                     ns_name != NULL ? "Unterminated namespaced map (missing '}')"
                                                     : "Unterminated map (missing '}')",
                                     value_start, parser->current);
            }
            return NULL;
        }

        edn_value_t* final_key = key;

        if (ns_name != NULL && key->type == EDN_TYPE_KEYWORD) {
            if (key->as.keyword.namespace == NULL) {
                final_key = edn_arena_alloc_value(parser->arena);
                if (final_key == NULL) {
                    edn_leave_depth(parser);
                    edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                         "Out of memory allocating namespaced keyword", value_start,
                                         parser->current);
                    return NULL;
                }

                final_key->type = EDN_TYPE_KEYWORD;
                final_key->as.keyword.namespace = ns_name;
                final_key->as.keyword.ns_length = ns_length;
                final_key->as.keyword.name = key->as.keyword.name;
                final_key->as.keyword.name_length = key->as.keyword.name_length;
                final_key->arena = parser->arena;
            } else if (key->as.keyword.ns_length == 1 && key->as.keyword.namespace[0] == '_') {
                final_key = edn_arena_alloc_value(parser->arena);
                if (final_key == NULL) {
                    edn_leave_depth(parser);
                    edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                         "Out of memory allocating keyword", value_start,
                                         parser->current);
                    return NULL;
                }

                final_key->type = EDN_TYPE_KEYWORD;
                final_key->as.keyword.namespace = NULL;
                final_key->as.keyword.ns_length = 0;
                final_key->as.keyword.name = key->as.keyword.name;
                final_key->as.keyword.name_length = key->as.keyword.name_length;
                final_key->arena = parser->arena;
            }
        }

        if (ns_name != NULL && key->type == EDN_TYPE_SYMBOL) {
            if (key->as.symbol.namespace == NULL) {
                final_key = edn_arena_alloc_value(parser->arena);
                if (final_key == NULL) {
                    edn_leave_depth(parser);
                    edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                         "Out of memory allocating namespaced symbol", value_start,
                                         parser->current);
                    return NULL;
                }

                final_key->type = EDN_TYPE_SYMBOL;
                final_key->as.symbol.namespace = ns_name;
                final_key->as.symbol.ns_length = ns_length;
                final_key->as.symbol.name = key->as.symbol.name;
                final_key->as.symbol.name_length = key->as.symbol.name_length;
                final_key->arena = parser->arena;
            } else if (key->as.symbol.ns_length == 1 && key->as.symbol.namespace[0] == '_') {
                final_key = edn_arena_alloc_value(parser->arena);
                if (final_key == NULL) {
                    edn_leave_depth(parser);
                    edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                         "Out of memory allocating symbol", value_start,
                                         parser->current);
                    return NULL;
                }

                final_key->type = EDN_TYPE_SYMBOL;
                final_key->as.symbol.namespace = NULL;
                final_key->as.symbol.ns_length = 0;
                final_key->as.symbol.name = key->as.symbol.name;
                final_key->as.symbol.name_length = key->as.symbol.name_length;
                final_key->arena = parser->arena;
            }
        }

        if (!edn_map_builder_add(&builder, final_key, value)) {
            edn_leave_depth(parser);
            edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                                 "Out of memory while building map", value_start, parser->current);
            return NULL;
        }
    }

    if (parser->current >= parser->end) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNEXPECTED_EOF,
                             ns_name != NULL ? "Unterminated namespaced map (missing '}')"
                                             : "Unterminated map (missing '}')",
                             value_start, parser->current);
        return NULL;
    }

    if (*parser->current != '}') {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNMATCHED_DELIMITER,
                             ns_name != NULL ? "Mismatched closing delimiter in namespaced map"
                                             : "Mismatched closing delimiter in map",
                             value_start, parser->current + 1);
        return NULL;
    }

    parser->current++;
    edn_leave_depth(parser);

    edn_value_t** keys;
    edn_value_t** values;
    size_t count;
    edn_map_builder_finish(&builder, &keys, &values, &count);

    /* Check for duplicate keys (EDN spec requirement) */
    if (count > 1) {
        if (edn_has_duplicates(keys, count)) {
            edn_parser_set_error(parser, EDN_ERROR_DUPLICATE_KEY,
                                 ns_name != NULL ? "Namespaced map contains duplicate keys"
                                                 : "Map contains duplicate keys",
                                 value_start, parser->current);
            return NULL;
        }
    }

    edn_value_t* result = edn_arena_alloc_value(parser->arena);
    if (result == NULL) {
        edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY, "Out of memory allocating map",
                             value_start, parser->current);
        return NULL;
    }

    result->type = EDN_TYPE_MAP;
    result->as.map.keys = keys;
    result->as.map.values = values;
    result->as.map.count = count;
    result->arena = parser->arena;
    result->source_start = value_start - parser->input;
    result->source_end = parser->current - parser->input;

    return result;
}

edn_value_t* edn_read_map(edn_parser_t* parser) {
    const char* value_start = parser->current;
    return edn_read_map_internal(parser, value_start, NULL, 0);
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION

edn_value_t* edn_read_namespaced_map(edn_parser_t* parser) {
    const char* value_start = parser->current;

    parser->current++;

    edn_value_t* ns_keyword = edn_read_value(parser);
    if (ns_keyword == NULL) {
        /* Parsing failed, error already set */
        return NULL;
    }

    if (ns_keyword->type != EDN_TYPE_KEYWORD) {
        edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX,
                             "Namespaced map must start with a keyword", value_start,
                             parser->current);
        return NULL;
    }

    if (ns_keyword->as.keyword.namespace != NULL) {
        edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX,
                             "Namespaced map keyword cannot have a namespace", value_start,
                             parser->current);
        return NULL;
    }

    const char* ns_name = ns_keyword->as.keyword.name;
    size_t ns_length = ns_keyword->as.keyword.name_length;

    edn_skip_whitespace(parser);

    if (parser->current >= parser->end || *parser->current != '{') {
        edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX,
                             "Namespaced map must be followed by '{'", value_start,
                             parser->current);
        return NULL;
    }

    return edn_read_map_internal(parser, value_start, ns_name, ns_length);
}

#endif /* EDN_ENABLE_CLOJURE_EXTENSION */
