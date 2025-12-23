/**
 * EDN.C - Discard reader macro
 */

#include "edn_internal.h"

edn_value_t* edn_read_discarded_value(edn_parser_t* parser) {
    const char* start = parser->current;
    parser->current += 2;

    /* Enable discard mode to prevent reader invocation */
    bool old_discard_mode = parser->discard_mode;
    parser->discard_mode = true;

    edn_value_t* discarded = edn_read_value(parser);

    /* Restore discard mode */
    parser->discard_mode = old_discard_mode;

    if (discarded == NULL) {
        if (parser->error == EDN_OK) {
            parser->error = EDN_ERROR_INVALID_DISCARD;
            parser->error_message = "Discard macro missing value";
            parser->error_start = start;
            parser->error_end = start + 2;
        }
        return NULL;
    }

    return NULL;
}
