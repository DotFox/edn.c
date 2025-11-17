/**
 * Metadata parsing for EDN.C
 *
 * Implements Clojure-style metadata syntax: ^{...} form
 * Requires EDN_ENABLE_METADATA compilation flag.
 */

#include <string.h>

#ifdef EDN_ENABLE_METADATA

#include "edn_internal.h"

edn_value_t* edn_parse_metadata(edn_parser_t* parser) {
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

        size_t new_entries_count = 0;
        if (meta_value->type == EDN_TYPE_MAP) {
            new_entries_count = meta_value->as.map.count;
        } else {
            /* Keyword, string, symbol, or vector all expand to single entry */
            new_entries_count = 1;
        }

        size_t total_count = existing_count + new_entries_count;
        edn_value_t** extended_keys =
            edn_arena_alloc(parser->arena, total_count * sizeof(edn_value_t*));
        edn_value_t** extended_values =
            edn_arena_alloc(parser->arena, total_count * sizeof(edn_value_t*));
        if (extended_keys == NULL || extended_values == NULL) {
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory extending metadata";
            return NULL;
        }

        /* Copy existing entries (lower precedence) */
        memcpy(extended_keys, existing_meta->as.map.keys, existing_count * sizeof(edn_value_t*));
        memcpy(extended_values, existing_meta->as.map.values,
               existing_count * sizeof(edn_value_t*));

        /* Add new entries (higher precedence) at the end */
        if (meta_value->type == EDN_TYPE_MAP) {
            /* Copy all entries from the metadata map */
            memcpy(extended_keys + existing_count, meta_value->as.map.keys,
                   new_entries_count * sizeof(edn_value_t*));
            memcpy(extended_values + existing_count, meta_value->as.map.values,
                   new_entries_count * sizeof(edn_value_t*));
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

            extended_keys[existing_count] = meta_value;
            extended_values[existing_count] = true_value;
        } else if (meta_value->type == EDN_TYPE_VECTOR) {
            /* Add {:param-tags vector} */
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

            extended_keys[existing_count] = param_tags_keyword;
            extended_values[existing_count] = meta_value;
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

            extended_keys[existing_count] = tag_keyword;
            extended_values[existing_count] = meta_value;
        }

        /* Update the existing metadata map in place */
        existing_meta->as.map.keys = extended_keys;
        existing_meta->as.map.values = extended_values;
        existing_meta->as.map.count = total_count;
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
