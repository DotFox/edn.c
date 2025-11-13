/**
 * EDN.C - Tagged literals
 */

#include <string.h>

#include "edn_internal.h"

edn_value_t* edn_parse_tagged(edn_parser_t* parser) {
    /* Skip '#' and increment depth */
    parser->current++;
    parser->depth++;

    if (parser->current >= parser->end) {
        parser->depth--;
        parser->error = EDN_ERROR_UNEXPECTED_EOF;
        parser->error_message = "Unexpected end of input after '#' (expected tag)";
        return NULL;
    }

    char next = *parser->current;
    if (next == ' ' || next == '\t' || next == '\n' || next == '\r' || next == ',') {
        parser->depth--;
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message =
            "Tagged literal tag must immediately follow '#' (no whitespace allowed)";
        return NULL;
    }

    const char* tag_start = parser->current;

    edn_value_t* tag_value = edn_parse_identifier(parser);
    if (tag_value == NULL) {
        parser->depth--;
        return NULL; /* Error already set */
    }

    if (tag_value->type != EDN_TYPE_SYMBOL) {
        parser->depth--;
        parser->error = EDN_ERROR_INVALID_SYNTAX;
        parser->error_message = "Tagged literal must be a symbol";
        return NULL;
    }

    /* Extract tag string (from tag_start to current position) */
    const char* tag_end = parser->current;
    size_t tag_length = tag_end - tag_start;
    const char* tag_string = tag_start;

    edn_value_t* value = edn_parser_parse_value(parser);
    if (value == NULL) {
        parser->depth--;
        if (parser->error == EDN_OK) {
            parser->error = EDN_ERROR_UNEXPECTED_EOF;
            parser->error_message = "Tagged literal missing value";
        }
        return NULL;
    }

    /* Check if reader registry is provided and not in discard mode */
    if (parser->reader_registry != NULL && !parser->discard_mode) {
        edn_reader_fn reader =
            edn_reader_lookup_internal(parser->reader_registry, tag_string, tag_length);

        if (reader != NULL) {
            /* Invoke custom reader */
            const char* error_msg = NULL;
            edn_value_t* result = reader(value, parser->arena, &error_msg);

            if (result == NULL) {
                parser->depth--;
                parser->error = EDN_ERROR_INVALID_SYNTAX;
                parser->error_message = error_msg ? error_msg : "Reader function failed";
                return NULL;
            }

            parser->depth--;
            return result;
        }

        /* No reader found - use default fallback */
        switch (parser->default_reader_mode) {
            case EDN_DEFAULT_READER_UNWRAP:
                parser->depth--;
                return value;

            case EDN_DEFAULT_READER_ERROR:
                parser->depth--;
                parser->error = EDN_ERROR_UNKNOWN_TAG;
                parser->error_message = "No reader registered for tag";
                return NULL;

            case EDN_DEFAULT_READER_PASSTHROUGH:
                /* Fall through to existing behavior */
                break;
        }
    }

    /* Default behavior: return EDN_TYPE_TAGGED */
    parser->depth--;

    edn_value_t* tagged = edn_arena_alloc_value(parser->arena);
    if (tagged == NULL) {
        parser->error = EDN_ERROR_OUT_OF_MEMORY;
        parser->error_message = "Out of memory allocating tagged literal";
        return NULL;
    }

    tagged->type = EDN_TYPE_TAGGED;
    tagged->as.tagged.tag = tag_string;
    tagged->as.tagged.tag_length = tag_length;
    tagged->as.tagged.value = value;
    tagged->arena = parser->arena;

    return tagged;
}
