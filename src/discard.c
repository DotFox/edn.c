/**
 * EDN.C - Discard reader macro
 */

#include "edn_internal.h"

edn_value_t* edn_parse_discard_value(edn_parser_t* parser) {
    parser->current += 2;

    /* Enable discard mode to prevent reader invocation */
    bool old_discard_mode = parser->discard_mode;
    parser->discard_mode = true;

    edn_value_t* discarded = edn_parser_parse_value(parser);

    /* Restore discard mode */
    parser->discard_mode = old_discard_mode;

    if (discarded == NULL) {
        if (parser->error == EDN_OK) {
            parser->error = EDN_ERROR_UNEXPECTED_EOF;
            parser->error_message = "Discard macro missing value";
        }
        return NULL;
    }

    return NULL;
}
