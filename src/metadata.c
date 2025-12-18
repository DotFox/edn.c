/**
 * Metadata parsing for EDN.C
 *
 * Implements Clojure-style metadata syntax: ^{...} form
 * Requires EDN_ENABLE_METADATA compilation flag.
 */

#include <string.h>

#ifdef EDN_ENABLE_METADATA

#include "edn_internal.h"

edn_value_t* edn_read_metadata(edn_parser_t* parser) {
    /* Skip the ^ character */
    parser->current++;

    /* Step 1: Parse the metadata value */
    edn_value_t* meta_value = edn_parser_parse_value(parser);
    if (meta_value == NULL || parser->error != EDN_OK) {
        return NULL;
    }

    if (meta_value->type != EDN_TYPE_MAP && meta_value->type != EDN_TYPE_KEYWORD &&
        meta_value->type != EDN_TYPE_STRING && meta_value->type != EDN_TYPE_SYMBOL &&
        meta_value->type != EDN_TYPE_VECTOR) {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Metadata must be a map, keyword, string, symbol, or vector";
        return NULL;
    }

    /* Step 2: Parse the value to attach metadata to */
    edn_value_t* form = edn_parser_parse_value(parser);
    if (form == NULL || parser->error != EDN_OK) {
        return NULL;
    }

    /* Validate that metadata can be attached to this type */
    if (form->type != EDN_TYPE_LIST && form->type != EDN_TYPE_VECTOR &&
        form->type != EDN_TYPE_MAP && form->type != EDN_TYPE_SET && form->type != EDN_TYPE_TAGGED &&
        form->type != EDN_TYPE_SYMBOL) {
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message =
            "Metadata can only be attached to collections, tagged literals, and symbols";
        return NULL;
    }

    /* Step 3: Add to existing metadata or create new */
    if (form->metadata != NULL) {
        /* Form already has metadata - extend it */
        edn_value_t* existing_meta = form->metadata;
        size_t existing_count = existing_meta->as.map.count;

        /* Prepare new entries to merge */
        edn_value_t** new_keys = NULL;
        edn_value_t** new_values = NULL;
        size_t new_entries_count = 0;

        if (meta_value->type == EDN_TYPE_MAP) {
            new_keys = meta_value->as.map.keys;
            new_values = meta_value->as.map.values;
            new_entries_count = meta_value->as.map.count;
        } else if (meta_value->type == EDN_TYPE_KEYWORD) {
            /* Add {:keyword true} */
            edn_value_t* true_value = edn_arena_alloc_value(parser->arena);
            if (true_value == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata value";
                return NULL;
            }
            true_value->type = EDN_TYPE_BOOL;
            true_value->as.boolean = true;
            true_value->arena = parser->arena;
            true_value->metadata = NULL;

            new_keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            new_values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (new_keys == NULL || new_values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            new_keys[0] = meta_value;
            new_values[0] = true_value;
            new_entries_count = 1;
        } else if (meta_value->type == EDN_TYPE_VECTOR) {
            /* Create {:param-tags vector} */
            edn_value_t* param_tags_keyword = edn_arena_alloc_value(parser->arena);
            if (param_tags_keyword == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating :param-tags keyword";
                return NULL;
            }
            param_tags_keyword->type = EDN_TYPE_KEYWORD;
            param_tags_keyword->as.keyword.namespace = NULL;
            param_tags_keyword->as.keyword.ns_length = 0;
            param_tags_keyword->as.keyword.name = "param-tags";
            param_tags_keyword->as.keyword.name_length = 10;
            param_tags_keyword->arena = parser->arena;
            param_tags_keyword->metadata = NULL;

            new_keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            new_values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (new_keys == NULL || new_values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            new_keys[0] = param_tags_keyword;
            new_values[0] = meta_value;
            new_entries_count = 1;
        } else /* EDN_TYPE_STRING or EDN_TYPE_SYMBOL */ {
            /* Add {:tag value} */
            edn_value_t* tag_keyword = edn_arena_alloc_value(parser->arena);
            if (tag_keyword == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating :tag keyword";
                return NULL;
            }
            tag_keyword->type = EDN_TYPE_KEYWORD;
            tag_keyword->as.keyword.namespace = NULL;
            tag_keyword->as.keyword.ns_length = 0;
            tag_keyword->as.keyword.name = "tag";
            tag_keyword->as.keyword.name_length = 3;
            tag_keyword->arena = parser->arena;
            tag_keyword->metadata = NULL;

            new_keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            new_values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (new_keys == NULL || new_values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            new_keys[0] = tag_keyword;
            new_values[0] = meta_value;
            new_entries_count = 1;
        }

        /* Allocate worst-case space (all keys are unique) */
        size_t max_count = existing_count + new_entries_count;
        edn_value_t** merged_keys =
            edn_arena_alloc(parser->arena, max_count * sizeof(edn_value_t*));
        edn_value_t** merged_values =
            edn_arena_alloc(parser->arena, max_count * sizeof(edn_value_t*));
        if (merged_keys == NULL || merged_values == NULL) {
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory merging metadata";
            return NULL;
        }

        /* First, copy all new entries (highest precedence) */
        memcpy(merged_keys, new_keys, new_entries_count * sizeof(edn_value_t*));
        memcpy(merged_values, new_values, new_entries_count * sizeof(edn_value_t*));
        size_t merged_count = new_entries_count;

        /* Then, copy existing entries that don't have matching keys in new metadata */
        for (size_t i = 0; i < existing_count; i++) {
            edn_value_t* existing_key = existing_meta->as.map.keys[i];

            /* Check if this key exists in new metadata */
            bool found = false;
            for (size_t j = 0; j < new_entries_count; j++) {
                if (edn_value_equal(existing_key, new_keys[j])) {
                    found = true;
                    break;
                }
            }

            /* Only add if not found in new metadata (new overrides old) */
            if (!found) {
                merged_keys[merged_count] = existing_key;
                merged_values[merged_count] = existing_meta->as.map.values[i];
                merged_count++;
            }
        }

        /* Update the existing metadata map in place */
        existing_meta->as.map.keys = merged_keys;
        existing_meta->as.map.values = merged_values;
        existing_meta->as.map.count = merged_count;
    } else {
        /* No existing metadata - create new metadata map */
        edn_value_t* meta_map = edn_arena_alloc_value(parser->arena);
        if (meta_map == NULL) {
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory creating metadata map";
            return NULL;
        }
        meta_map->type = EDN_TYPE_MAP;
        meta_map->arena = parser->arena;
        meta_map->metadata = NULL;

        if (meta_value->type == EDN_TYPE_MAP) {
            /* Use the map directly */
            meta_map->as.map.keys = meta_value->as.map.keys;
            meta_map->as.map.values = meta_value->as.map.values;
            meta_map->as.map.count = meta_value->as.map.count;
        } else if (meta_value->type == EDN_TYPE_KEYWORD) {
            /* Create {:keyword true} */
            edn_value_t* true_value = edn_arena_alloc_value(parser->arena);
            if (true_value == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata value";
                return NULL;
            }
            true_value->type = EDN_TYPE_BOOL;
            true_value->as.boolean = true;
            true_value->arena = parser->arena;
            true_value->metadata = NULL;

            edn_value_t** keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            edn_value_t** values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (keys == NULL || values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            keys[0] = meta_value;
            values[0] = true_value;

            meta_map->as.map.keys = keys;
            meta_map->as.map.values = values;
            meta_map->as.map.count = 1;
        } else if (meta_value->type == EDN_TYPE_VECTOR) {
            /* Create {:param-tags vector} */
            edn_value_t* param_tags_keyword = edn_arena_alloc_value(parser->arena);
            if (param_tags_keyword == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating :param-tags keyword";
                return NULL;
            }
            param_tags_keyword->type = EDN_TYPE_KEYWORD;
            param_tags_keyword->as.keyword.namespace = NULL;
            param_tags_keyword->as.keyword.ns_length = 0;
            param_tags_keyword->as.keyword.name = "param-tags";
            param_tags_keyword->as.keyword.name_length = 10;
            param_tags_keyword->arena = parser->arena;
            param_tags_keyword->metadata = NULL;

            edn_value_t** keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            edn_value_t** values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (keys == NULL || values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            keys[0] = param_tags_keyword;
            values[0] = meta_value;

            meta_map->as.map.keys = keys;
            meta_map->as.map.values = values;
            meta_map->as.map.count = 1;
        } else /* EDN_TYPE_STRING or EDN_TYPE_SYMBOL */ {
            /* Create {:tag value} */
            edn_value_t* tag_keyword = edn_arena_alloc_value(parser->arena);
            if (tag_keyword == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating :tag keyword";
                return NULL;
            }
            tag_keyword->type = EDN_TYPE_KEYWORD;
            tag_keyword->as.keyword.namespace = NULL;
            tag_keyword->as.keyword.ns_length = 0;
            tag_keyword->as.keyword.name = "tag";
            tag_keyword->as.keyword.name_length = 3;
            tag_keyword->arena = parser->arena;
            tag_keyword->metadata = NULL;

            edn_value_t** keys = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            edn_value_t** values = edn_arena_alloc(parser->arena, sizeof(edn_value_t*));
            if (keys == NULL || values == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            keys[0] = tag_keyword;
            values[0] = meta_value;

            meta_map->as.map.keys = keys;
            meta_map->as.map.values = values;
            meta_map->as.map.count = 1;
        }

        form->metadata = meta_map;
    }

    return form;
}

#endif /* EDN_ENABLE_METADATA */
