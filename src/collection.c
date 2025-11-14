/**
 * EDN.C - Collection parsers
 */

#include <string.h>

#include "edn_internal.h"

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

edn_value_t* edn_parse_list(edn_parser_t* parser) {
    parser->current++;
    parser->depth++;

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_parser_parse_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            parser->depth--;
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory while building list";
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        parser->depth--;
        return NULL;
    }

    if (parser->current >= parser->end) {
        parser->depth--;
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unterminated list (missing ')')";
        return NULL;
    }

    if (*parser->current != ')') {
        parser->depth--;
        parser->error = EDN_ERROR_UNMATCHED_DELIMITER;
        parser->error_message = "Mismatched closing delimiter in list";
        return NULL;
    }

    parser->current++;
    parser->depth--;

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating list";
        return NULL;
    }

    value->type = EDN_TYPE_LIST;
    value->as.list.elements = elements;
    value->as.list.count = count;
    value->arena = parser->arena;

    return value;
}

edn_value_t* edn_parse_vector(edn_parser_t* parser) {
    parser->current++;
    parser->depth++;

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_parser_parse_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            parser->depth--;
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory while building vector";
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        parser->depth--;
        return NULL;
    }

    if (parser->current >= parser->end) {
        parser->depth--;
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unterminated vector (missing ']')";
        return NULL;
    }

    if (*parser->current != ']') {
        parser->depth--;
        parser->error = EDN_ERROR_UNMATCHED_DELIMITER;
        parser->error_message = "Mismatched closing delimiter in vector";
        return NULL;
    }

    parser->current++;
    parser->depth--;

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating vector";
        return NULL;
    }

    value->type = EDN_TYPE_VECTOR;
    value->as.vector.elements = elements;
    value->as.vector.count = count;
    value->arena = parser->arena;

    return value;
}

edn_value_t* edn_parse_set(edn_parser_t* parser) {
    parser->current += 2;
    parser->depth++;

    edn_collection_builder_t builder;
    edn_collection_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* element = edn_parser_parse_value(parser);
        if (element == NULL) {
            break;
        }

        if (!edn_collection_builder_add(&builder, element)) {
            parser->depth--;
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory while building set";
            return NULL;
        }
    }

    if (parser->error != EDN_OK) {
        parser->depth--;
        return NULL;
    }

    if (parser->current >= parser->end) {
        parser->depth--;
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unterminated set (missing '}')";
        return NULL;
    }

    if (*parser->current != '}') {
        parser->depth--;
        parser->error = EDN_ERROR_UNMATCHED_DELIMITER;
        parser->error_message = "Mismatched closing delimiter in set";
        return NULL;
    }

    parser->current++;
    parser->depth--;

    size_t count;
    edn_value_t** elements = edn_collection_builder_finish(&builder, &count);

    /* Check for duplicate elements (EDN spec requirement) */
    if (count > 1 && edn_has_duplicates(elements, count)) {
        parser->error = EDN_ERROR_DUPLICATE_ELEMENT;
        parser->error_message = "Set contains duplicate elements";
        return NULL;
    }

    edn_value_t* value = edn_arena_alloc_value(parser->arena);
    if (value == NULL) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating set";
        return NULL;
    }

    value->type = EDN_TYPE_SET;
    value->as.set.elements = elements;
    value->as.set.count = count;
    value->arena = parser->arena;

    return value;
}

typedef struct {
    edn_map_entry_t* entries;
    size_t count;
    size_t capacity;
    edn_arena_t* arena;
    edn_map_entry_t inline_storage[8];
} edn_map_builder_t;

static void edn_map_builder_init(edn_map_builder_t* builder, edn_arena_t* arena,
                                 size_t initial_capacity) {
    builder->count = 0;
    builder->arena = arena;

    if (initial_capacity <= 8) {
        builder->entries = builder->inline_storage;
        builder->capacity = 8;
    } else {
        builder->entries = edn_arena_alloc(arena, initial_capacity * sizeof(edn_map_entry_t));
        if (builder->entries != NULL) {
            builder->capacity = initial_capacity;
        } else {
            builder->entries = builder->inline_storage;
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

        edn_map_entry_t* new_entries =
            edn_arena_alloc(builder->arena, new_capacity * sizeof(edn_map_entry_t));
        if (new_entries == NULL) {
            return false;
        }

        if (builder->count > 0) {
            memcpy(new_entries, builder->entries, builder->count * sizeof(edn_map_entry_t));
        }

        builder->entries = new_entries;
        builder->capacity = new_capacity;
    }

    builder->entries[builder->count].key = key;
    builder->entries[builder->count].value = value;
    builder->count++;
    return true;
}

static edn_map_entry_t* edn_map_builder_finish(edn_map_builder_t* builder, size_t* out_count) {
    *out_count = builder->count;

    if (builder->entries == builder->inline_storage && builder->count > 0) {
        edn_map_entry_t* permanent =
            edn_arena_alloc(builder->arena, builder->count * sizeof(edn_map_entry_t));
        if (permanent != NULL) {
            memcpy(permanent, builder->inline_storage, builder->count * sizeof(edn_map_entry_t));
            return permanent;
        }
    }

    return builder->entries;
}

static edn_value_t* edn_parse_map_internal(edn_parser_t* parser, const char* ns_name,
                                           size_t ns_length) {
    parser->current++;
    parser->depth++;

    edn_map_builder_t builder;
    edn_map_builder_init(&builder, parser->arena, 8);

    while (true) {
        edn_value_t* key = edn_parser_parse_value(parser);
        if (key == NULL) {
            if (parser->error != EDN_OK) {
                parser->depth--;
                return NULL;
            }
            break;
        }

        edn_value_t* value = edn_parser_parse_value(parser);
        if (value == NULL) {
            parser->depth--;
            if (parser->error == EDN_OK) {
                parser->error = EDN_ERROR_INVALID_SYNTAX;
                parser->error_message = "Map has odd number of elements (key without value)";
            }
            return NULL;
        }

        edn_value_t* final_key = key;
        if (ns_name != NULL && key->type == EDN_TYPE_KEYWORD && key->as.keyword.namespace == NULL) {
            final_key = edn_arena_alloc_value(parser->arena);
            if (final_key == NULL) {
                parser->depth--;
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory allocating namespaced keyword";
                return NULL;
            }

            final_key->type = EDN_TYPE_KEYWORD;
            final_key->as.keyword.namespace = ns_name;
            final_key->as.keyword.ns_length = ns_length;
            final_key->as.keyword.name = key->as.keyword.name;
            final_key->as.keyword.name_length = key->as.keyword.name_length;
            final_key->arena = parser->arena;
        }

        if (!edn_map_builder_add(&builder, final_key, value)) {
            parser->depth--;
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory while building map";
            return NULL;
        }
    }

    if (parser->current >= parser->end) {
        parser->depth--;
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = ns_name != NULL ? "Unterminated namespaced map (missing '}')"
                                                : "Unterminated map (missing '}')";
        return NULL;
    }

    if (*parser->current != '}') {
        parser->depth--;
        parser->error = EDN_ERROR_UNMATCHED_DELIMITER;
        parser->error_message = ns_name != NULL ? "Mismatched closing delimiter in namespaced map"
                                                : "Mismatched closing delimiter in map";
        return NULL;
    }

    parser->current++;
    parser->depth--;

    size_t count;
    edn_map_entry_t* entries = edn_map_builder_finish(&builder, &count);

    /* Check for duplicate keys (EDN spec requirement) */
    if (count > 1) {
        /* Extract keys into temporary array for duplicate checking */
        edn_value_t** keys = edn_arena_alloc(parser->arena, count * sizeof(edn_value_t*));
        if (keys == NULL) {
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory checking map keys";
            return NULL;
        }

        for (size_t i = 0; i < count; i++) {
            keys[i] = entries[i].key;
        }

        if (edn_has_duplicates(keys, count)) {
            parser->error = EDN_ERROR_DUPLICATE_KEY;
            parser->error_message = ns_name != NULL ? "Namespaced map contains duplicate keys"
                                                    : "Map contains duplicate keys";
            return NULL;
        }
    }

    edn_value_t* result = edn_arena_alloc_value(parser->arena);
    if (result == NULL) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating map";
        return NULL;
    }

    result->type = EDN_TYPE_MAP;
    result->as.map.entries = entries;
    result->as.map.count = count;
    result->arena = parser->arena;

    return result;
}

edn_value_t* edn_parse_map(edn_parser_t* parser) {
    return edn_parse_map_internal(parser, NULL, 0);
}

#ifdef EDN_ENABLE_MAP_NAMESPACE_SYNTAX

edn_value_t* edn_parse_namespaced_map(edn_parser_t* parser) {
    parser->current++;

    edn_value_t* ns_keyword = edn_parser_parse_value(parser);
    if (ns_keyword == NULL) {
        /* Parsing failed, error already set */
        return NULL;
    }

    if (ns_keyword->type != EDN_TYPE_KEYWORD) {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Namespaced map must start with a keyword";
        return NULL;
    }

    if (ns_keyword->as.keyword.namespace != NULL) {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Namespaced map keyword cannot have a namespace";
        return NULL;
    }

    const char* ns_name = ns_keyword->as.keyword.name;
    size_t ns_length = ns_keyword->as.keyword.name_length;

    edn_parser_skip_whitespace(parser);

    if (parser->current >= parser->end || *parser->current != '{') {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Namespaced map must be followed by '{'";
        return NULL;
    }

    return edn_parse_map_internal(parser, ns_name, ns_length);
}

#endif /* EDN_ENABLE_MAP_NAMESPACE_SYNTAX */
