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
        edn_map_entry_t* extended_entries =
            edn_arena_alloc(parser->arena, total_count * sizeof(edn_map_entry_t));
        if (extended_entries == NULL) {
            parser->error = EDN_ERROR_OUT_OF_MEMORY;
            parser->error_message = "Out of memory extending metadata";
            return NULL;
        }

        /* Copy existing entries (lower precedence) */
        memcpy(extended_entries, existing_meta->as.map.entries,
               existing_count * sizeof(edn_map_entry_t));

        /* Add new entries (higher precedence) at the end */
        if (meta_value->type == EDN_TYPE_MAP) {
            /* Copy all entries from the metadata map */
            memcpy(extended_entries + existing_count, meta_value->as.map.entries,
                   new_entries_count * sizeof(edn_map_entry_t));
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

            extended_entries[existing_count].key = meta_value;
            extended_entries[existing_count].value = true_value;
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

            extended_entries[existing_count].key = param_tags_keyword;
            extended_entries[existing_count].value = meta_value;
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

            extended_entries[existing_count].key = tag_keyword;
            extended_entries[existing_count].value = meta_value;
        }

        /* Update the existing metadata map in place */
        existing_meta->as.map.entries = extended_entries;
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
            meta_map->as.map.entries = meta_value->as.map.entries;
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

            edn_map_entry_t* entry = edn_arena_alloc(parser->arena, sizeof(edn_map_entry_t));
            if (entry == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            entry->key = meta_value;
            entry->value = true_value;

            meta_map->as.map.entries = entry;
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

            edn_map_entry_t* entry = edn_arena_alloc(parser->arena, sizeof(edn_map_entry_t));
            if (entry == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            entry->key = param_tags_keyword;
            entry->value = meta_value;

            meta_map->as.map.entries = entry;
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

            edn_map_entry_t* entry = edn_arena_alloc(parser->arena, sizeof(edn_map_entry_t));
            if (entry == NULL) {
                parser->error = EDN_ERROR_OUT_OF_MEMORY;
                parser->error_message = "Out of memory creating metadata entry";
                return NULL;
            }
            entry->key = tag_keyword;
            entry->value = meta_value;

            meta_map->as.map.entries = entry;
            meta_map->as.map.count = 1;
        }

        form->metadata = meta_map;
    }

    return form;
}

#endif /* EDN_ENABLE_METADATA */
