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
        /* edn_read_value returns NULL with error==EDN_OK in one other case:
         * a closing delimiter encountered inside a collection. That is NOT a
         * valid discarded form — "#_)" must be an error, not a silent no-op.
         * Point error_end at the position immediately after the "#_" token,
         * which is where the discarded value should have begun. */
        if (parser->error == EDN_OK) {
            edn_parser_set_error(parser, EDN_ERROR_INVALID_DISCARD, "Discard macro missing value",
                                 start, start + 2);
        }
        return NULL;
    }

    return NULL;
}
