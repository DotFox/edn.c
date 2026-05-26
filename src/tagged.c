/**
 * EDN.C - Tagged literals
 */

#include <string.h>

#include "edn_internal.h"

edn_value_t* edn_read_tagged(edn_parser_t* parser) {
    const char* value_start = parser->current;

    /* Skip '#' and gate depth */
    parser->current++;
    if (!edn_enter_depth(parser)) {
        return NULL;
    }

    if (parser->current >= parser->end) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_UNEXPECTED_EOF,
                             "Unexpected end of input after '#' (expected tag)", value_start,
                             parser->current);
        return NULL;
    }

    char next = *parser->current;
    if (next == ' ' || next == '\t' || next == '\n' || next == '\r' || next == ',') {
        edn_leave_depth(parser);
        edn_parser_set_error(
            parser, EDN_ERROR_INVALID_SYNTAX,
            "Tagged literal tag must immediately follow '#' (no whitespace allowed)", value_start,
            parser->current);
        return NULL;
    }

    const char* tag_start = parser->current;

    edn_value_t* tag_value = edn_read_identifier(parser);
    if (tag_value == NULL) {
        edn_leave_depth(parser);
        return NULL; /* Error already set */
    }

    if (tag_value->type != EDN_TYPE_SYMBOL) {
        edn_leave_depth(parser);
        edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX, "Tagged literal must be a symbol",
                             value_start, parser->current);
        return NULL;
    }

    /* Extract tag string (from tag_start to current position) */
    const char* tag_end = parser->current;
    size_t tag_length = tag_end - tag_start;
    const char* tag_string = tag_start;

    edn_value_t* value = edn_read_value(parser);
    if (value == NULL) {
        edn_leave_depth(parser);
        if (parser->error == EDN_OK) {
            edn_parser_set_error(parser, EDN_ERROR_UNEXPECTED_EOF, "Tagged literal missing value",
                                 value_start, parser->current);
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
                edn_leave_depth(parser);
                edn_parser_set_error(parser, EDN_ERROR_INVALID_SYNTAX,
                                     error_msg ? error_msg : "Reader function failed", value_start,
                                     parser->current);
                return NULL;
            }

            /* Set source position on reader result */
            result->source_start = value_start - parser->input;
            result->source_end = parser->current - parser->input;

            edn_leave_depth(parser);
            return result;
        }

        /* No reader found - use default fallback */
        switch (parser->default_reader_mode) {
            case EDN_DEFAULT_READER_UNWRAP:
                edn_leave_depth(parser);
                return value;

            case EDN_DEFAULT_READER_ERROR:
                edn_leave_depth(parser);
                edn_parser_set_error(parser, EDN_ERROR_UNKNOWN_TAG, "No reader registered for tag",
                                     value_start, parser->current);
                return NULL;

            case EDN_DEFAULT_READER_PASSTHROUGH:
                /* Fall through to existing behavior */
                break;
        }
    }

    /* Default behavior: return EDN_TYPE_TAGGED */
    edn_leave_depth(parser);

    edn_value_t* tagged = edn_arena_alloc_value(parser->arena);
    if (tagged == NULL) {
        edn_parser_set_error(parser, EDN_ERROR_OUT_OF_MEMORY,
                             "Out of memory allocating tagged literal", value_start,
                             parser->current);
        return NULL;
    }

    tagged->type = EDN_TYPE_TAGGED;
    tagged->as.tagged.tag = tag_string;
    tagged->as.tagged.tag_length = tag_length;
    tagged->as.tagged.value = value;
    tagged->arena = parser->arena;
    tagged->source_start = value_start - parser->input;
    tagged->source_end = parser->current - parser->input;

    return tagged;
}
