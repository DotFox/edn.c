/**
 * EDN.C - Interactive TUI EDN Viewer
 * 
 * An interactive terminal user interface for exploring EDN data.
 * Inspired by fx (https://github.com/antonmedv/fx)
 * 
 * Features:
 * - Interactive navigation with arrow keys
 * - Expand/collapse nested structures
 * - Syntax highlighting
 *
 * Usage:
 *   edn_tui [file]           # Open file in TUI
 *   edn_tui < file           # Read from stdin
 *   echo '{:a 1}' | edn_tui  # Read from pipe
 */

#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "../include/edn.h"

#define INITIAL_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE (1024 * 1024 * 100) /* 100MB limit */
#define MAX_PATH_DEPTH 256

/* ANSI escape codes */
#define CLEAR_SCREEN "\033[2J"
#define MOVE_CURSOR(row, col) printf("\033[%d;%dH", (row), (col))
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define CLEAR_LINE "\033[2K"
#define USE_ALT_SCREEN "\033[?1049h"  /* Switch to alternate screen buffer */
#define USE_MAIN_SCREEN "\033[?1049l" /* Switch back to main screen buffer */

/* Colors */
#define COLOR_RESET "\033[0m"
#define COLOR_NIL "\033[90m"
#define COLOR_BOOL "\033[35m"
#define COLOR_NUMBER "\033[36m"
#define COLOR_STRING "\033[32m"
#define COLOR_KEYWORD "\033[34m"
#define COLOR_SYMBOL "\033[33m"
#define COLOR_TAG "\033[35;1m"
#define COLOR_META "\033[36;1m"   /* Bright cyan for metadata */
#define COLOR_CURSOR "\033[7m"    /* Reverse video */
#define COLOR_PATH "\033[1;37m"   /* Bright white */
#define COLOR_STATUS "\033[1;34m" /* Bright blue */

/* Key codes */
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_RIGHT 67
#define KEY_LEFT 68
#define KEY_ENTER 10
#define KEY_SPACE 32
#define KEY_TAB 9
#define KEY_ESC 27
#define KEY_Q 'q'
#define KEY_F 'f'
#define KEY_SHIFT_F 'F'
#define KEY_M 'm'
#define KEY_SHIFT_M 'M'
#define KEY_SLASH '/'
#define KEY_N 'n'
#define KEY_SHIFT_N 'N'

/* Node display state */
typedef struct display_node {
    edn_value_t* value;
    edn_value_t* map_value;    /* For map entries, the value part (key is in 'value') */
    edn_value_t* tagged_value; /* For tagged literals, the wrapped value */
#ifdef EDN_ENABLE_METADATA
    edn_value_t* meta_value; /* For metadata, the metadata map */
#endif
    int depth;
    bool expanded;
    bool is_collection;
    size_t index;         /* Index in parent collection */
    const char* key_name; /* For map entries, NULL otherwise */
    size_t key_name_len;
    bool is_map_entry; /* True if this is a key-value pair from a map */
    bool is_tagged;    /* True if this is a tagged literal */
#ifdef EDN_ENABLE_METADATA
    bool is_metadata; /* True if this represents metadata */
#endif
    bool is_closing_bracket; /* True for closing bracket lines */
    struct display_node* parent;
} display_node_t;

/* Application state */
typedef struct {
    edn_value_t* root;
    edn_value_t* original_root; /* Original parsed root for reset */
#ifdef EDN_ENABLE_METADATA
    edn_value_t*
        metadata_view_original; /* Value whose metadata we're viewing (NULL if not in metadata view) */
#endif
    display_node_t** nodes; /* Flat list of visible nodes */
    size_t node_count;
    size_t node_capacity;
    size_t cursor_pos;    /* Current cursor position in nodes array */
    bool cursor_on_value; /* For map entries: true if cursor is on value column, false if on key */
    size_t scroll_offset; /* Top line visible on screen */
    int screen_height;
    int screen_width;
    char search_query[256];
    bool searching;
    bool running;
    /* Track expanded nodes by their value pointer */
    edn_value_t** expanded_values;
    size_t expanded_count;
    size_t expanded_capacity;
} app_state_t;

/* Terminal management */
static struct termios orig_termios;
static volatile sig_atomic_t cleanup_done = 0;

static void cleanup_terminal(void) {
    if (cleanup_done)
        return;
    cleanup_done = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf(USE_MAIN_SCREEN); /* Switch back to main screen buffer */
    printf(SHOW_CURSOR);
    fflush(stdout);
}

static void disable_raw_mode(void) {
    cleanup_terminal();
}

static void signal_handler(int signo) {
    (void) signo; /* Unused parameter */
    cleanup_terminal();
    exit(0);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    /* Set up signal handlers for clean exit */
    signal(SIGINT, signal_handler);  /* Ctrl+C */
    signal(SIGTERM, signal_handler); /* Kill/timeout */
    signal(SIGHUP, signal_handler);  /* Terminal hangup */

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf(USE_ALT_SCREEN); /* Switch to alternate screen buffer */
    printf(HIDE_CURSOR);
    fflush(stdout);
}

static void get_window_size(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

/* Read a key from stdin */
static int read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == KEY_ESC) {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1)
                return KEY_ESC;
            if (read(STDIN_FILENO, &seq[1], 1) != 1)
                return KEY_ESC;

            if (seq[0] == '[') {
                return seq[1];
            }
            return KEY_ESC;
        }
        return c;
    }
    return -1;
}

/* Forward declarations */
static void render_value(edn_value_t* val, char* buf, size_t bufsize, bool collapsed);
static bool is_collection(edn_value_t* val);

/* Get compact string representation of a value */
static void render_value(edn_value_t* val, char* buf, size_t bufsize, bool collapsed) {
    if (val == NULL) {
        snprintf(buf, bufsize, "nil");
        return;
    }

    switch (edn_type(val)) {
        case EDN_TYPE_NIL:
            snprintf(buf, bufsize, "nil");
            break;
        case EDN_TYPE_BOOL: {
            bool b;
            if (edn_bool_get(val, &b)) {
                snprintf(buf, bufsize, "%s", b ? "true" : "false");
            } else {
                snprintf(buf, bufsize, "<invalid bool>");
            }
            break;
        }
        case EDN_TYPE_INT: {
            int64_t num;
            edn_int64_get(val, &num);
            snprintf(buf, bufsize, "%lld", (long long) num);
            break;
        }
        case EDN_TYPE_BIGINT: {
            size_t len;
            bool negative;
            uint8_t radix;
            const char* digits = edn_bigint_get(val, &len, &negative, &radix);
            if (digits) {
                if (negative) {
                    snprintf(buf, bufsize, "-%.*sN", (int) len, digits);
                } else {
                    snprintf(buf, bufsize, "%.*sN", (int) len, digits);
                }
            } else {
                snprintf(buf, bufsize, "<invalid bigint>");
            }
            break;
        }
        case EDN_TYPE_FLOAT: {
            double num;
            edn_double_get(val, &num);
            snprintf(buf, bufsize, "%g", num);
            break;
        }
        case EDN_TYPE_BIGDEC: {
            size_t len;
            bool negative;
            const char* decimal = edn_bigdec_get(val, &len, &negative);
            if (decimal) {
                if (negative) {
                    snprintf(buf, bufsize, "-%.*sM", (int) len, decimal);
                } else {
                    snprintf(buf, bufsize, "%.*sM", (int) len, decimal);
                }
            } else {
                snprintf(buf, bufsize, "<invalid bigdec>");
            }
            break;
        }
#ifdef EDN_ENABLE_RATIO
        case EDN_TYPE_RATIO: {
            int64_t numerator, denominator;
            if (edn_ratio_get(val, &numerator, &denominator)) {
                snprintf(buf, bufsize, "%lld/%lld", (long long) numerator, (long long) denominator);
            } else {
                snprintf(buf, bufsize, "<invalid ratio>");
            }
            break;
        }
#endif
        case EDN_TYPE_CHARACTER: {
            uint32_t codepoint;
            if (edn_character_get(val, &codepoint)) {
                /* Handle special named characters */
                switch (codepoint) {
                    case '\n':
                        snprintf(buf, bufsize, "\\newline");
                        break;
                    case '\t':
                        snprintf(buf, bufsize, "\\tab");
                        break;
                    case '\r':
                        snprintf(buf, bufsize, "\\return");
                        break;
                    case ' ':
                        snprintf(buf, bufsize, "\\space");
                        break;
                    default:
                        if (codepoint < 32 || codepoint == 127) {
                            /* Control characters - use hex notation */
                            snprintf(buf, bufsize, "\\u%04X", codepoint);
                        } else if (codepoint < 128) {
                            /* Printable ASCII */
                            snprintf(buf, bufsize, "\\%c", (char) codepoint);
                        } else if (codepoint <= 0x10FFFF) {
                            /* Unicode character - encode as UTF-8 */
                            buf[0] = '\\';
                            int len = 1;
                            if (codepoint < 0x80) {
                                buf[len++] = (char) codepoint;
                            } else if (codepoint < 0x800) {
                                buf[len++] = (char) (0xC0 | (codepoint >> 6));
                                buf[len++] = (char) (0x80 | (codepoint & 0x3F));
                            } else if (codepoint < 0x10000) {
                                buf[len++] = (char) (0xE0 | (codepoint >> 12));
                                buf[len++] = (char) (0x80 | ((codepoint >> 6) & 0x3F));
                                buf[len++] = (char) (0x80 | (codepoint & 0x3F));
                            } else {
                                buf[len++] = (char) (0xF0 | (codepoint >> 18));
                                buf[len++] = (char) (0x80 | ((codepoint >> 12) & 0x3F));
                                buf[len++] = (char) (0x80 | ((codepoint >> 6) & 0x3F));
                                buf[len++] = (char) (0x80 | (codepoint & 0x3F));
                            }
                            buf[len] = '\0';
                        } else {
                            /* Invalid codepoint */
                            snprintf(buf, bufsize, "\\u%04X", codepoint);
                        }
                }
            } else {
                snprintf(buf, bufsize, "<invalid char>");
            }
            break;
        }
        case EDN_TYPE_STRING: {
            size_t len;
            const char* str = edn_string_get(val, &len);

            /* Escape special characters for display */
            size_t out_pos = 0;
            buf[out_pos++] = '"';

            for (size_t i = 0; i < len && out_pos < bufsize - 5; i++) {
                unsigned char c = (unsigned char) str[i];

                /* Handle common escape sequences */
                if (c == '\n') {
                    buf[out_pos++] = '\\';
                    buf[out_pos++] = 'n';
                } else if (c == '\t') {
                    buf[out_pos++] = '\\';
                    buf[out_pos++] = 't';
                } else if (c == '\r') {
                    buf[out_pos++] = '\\';
                    buf[out_pos++] = 'r';
                } else if (c == '\\') {
                    buf[out_pos++] = '\\';
                    buf[out_pos++] = '\\';
                } else if (c == '"') {
                    buf[out_pos++] = '\\';
                    buf[out_pos++] = '"';
                } else if (c < 32 || c == 127) {
                    /* Control characters as hex */
                    snprintf(buf + out_pos, bufsize - out_pos, "\\x%02X", c);
                    out_pos += 4;
                } else {
                    /* Regular character (including UTF-8 continuation bytes 0x80-0xFF) */
                    buf[out_pos++] = c;
                }

                /* Truncate if too long */
                if (i == 27 && len > 30) {
                    buf[out_pos++] = '.';
                    buf[out_pos++] = '.';
                    buf[out_pos++] = '.';
                    break;
                }
            }

            buf[out_pos++] = '"';
            buf[out_pos] = '\0';
            break;
        }
        case EDN_TYPE_KEYWORD: {
            const char* ns;
            const char* name;
            size_t ns_len, name_len;
            edn_keyword_get(val, &ns, &ns_len, &name, &name_len);
            if (ns != NULL && ns_len > 0) {
                snprintf(buf, bufsize, ":%.*s/%.*s", (int) ns_len, ns, (int) name_len, name);
            } else {
                snprintf(buf, bufsize, ":%.*s", (int) name_len, name);
            }
            break;
        }
        case EDN_TYPE_SYMBOL: {
            const char* ns;
            const char* name;
            size_t ns_len, name_len;
            edn_symbol_get(val, &ns, &ns_len, &name, &name_len);
            if (ns != NULL && ns_len > 0) {
                snprintf(buf, bufsize, "%.*s/%.*s", (int) ns_len, ns, (int) name_len, name);
            } else {
                snprintf(buf, bufsize, "%.*s", (int) name_len, name);
            }
            break;
        }
        case EDN_TYPE_VECTOR: {
            size_t count = edn_vector_count(val);
            if (collapsed) {
                snprintf(buf, bufsize, "[ ... ] %zu element%s", count, count == 1 ? "" : "s");
            } else {
                /* Show compact representation for inline display (e.g., map keys) */
                if (count == 0) {
                    snprintf(buf, bufsize, "[]");
                } else if (count <= 3) {
                    /* Show small vectors inline */
                    char temp[256];
                    int pos = 0;
                    pos += snprintf(buf, bufsize, "[");
                    for (size_t i = 0; i < count && pos < (int) bufsize - 10; i++) {
                        edn_value_t* elem = edn_vector_get(val, i);
                        render_value(elem, temp, sizeof(temp), false);
                        if (i > 0)
                            pos += snprintf(buf + pos, bufsize - pos, " ");
                        pos += snprintf(buf + pos, bufsize - pos, "%s", temp);
                    }
                    snprintf(buf + pos, bufsize - pos, "]");
                } else {
                    snprintf(buf, bufsize, "[...]");
                }
            }
            break;
        }
        case EDN_TYPE_LIST: {
            size_t count = edn_list_count(val);
            if (collapsed) {
                snprintf(buf, bufsize, "( ... ) %zu element%s", count, count == 1 ? "" : "s");
            } else {
                /* Show compact representation for inline display (e.g., map keys) */
                if (count == 0) {
                    snprintf(buf, bufsize, "()");
                } else if (count <= 3) {
                    /* Show small lists inline */
                    char temp[256];
                    int pos = 0;
                    pos += snprintf(buf, bufsize, "(");
                    for (size_t i = 0; i < count && pos < (int) bufsize - 10; i++) {
                        edn_value_t* elem = edn_list_get(val, i);
                        render_value(elem, temp, sizeof(temp), false);
                        if (i > 0)
                            pos += snprintf(buf + pos, bufsize - pos, " ");
                        pos += snprintf(buf + pos, bufsize - pos, "%s", temp);
                    }
                    snprintf(buf + pos, bufsize - pos, ")");
                } else {
                    snprintf(buf, bufsize, "(...)");
                }
            }
            break;
        }
        case EDN_TYPE_MAP: {
            size_t count = edn_map_count(val);
            if (collapsed) {
                snprintf(buf, bufsize, "{ ... } %zu key%s", count, count == 1 ? "" : "s");
            } else {
                /* Show compact representation for inline display (e.g., map keys) */
                if (count == 0) {
                    snprintf(buf, bufsize, "{}");
                } else if (count <= 2) {
                    /* Show small maps inline */
                    char temp_key[256];
                    char temp_val[256];
                    int pos = 0;
                    pos += snprintf(buf, bufsize, "{");
                    for (size_t i = 0; i < count && pos < (int) bufsize - 20; i++) {
                        edn_value_t* key = edn_map_get_key(val, i);
                        edn_value_t* value = edn_map_get_value(val, i);
                        render_value(key, temp_key, sizeof(temp_key), false);
                        render_value(value, temp_val, sizeof(temp_val), false);
                        if (i > 0)
                            pos += snprintf(buf + pos, bufsize - pos, " ");
                        pos += snprintf(buf + pos, bufsize - pos, "%s %s", temp_key, temp_val);
                    }
                    snprintf(buf + pos, bufsize - pos, "}");
                } else {
                    snprintf(buf, bufsize, "{...}");
                }
            }
            break;
        }
        case EDN_TYPE_SET: {
            size_t count = edn_set_count(val);
            if (collapsed) {
                snprintf(buf, bufsize, "#{ ... } %zu element%s", count, count == 1 ? "" : "s");
            } else {
                /* Show compact representation for inline display (e.g., map keys) */
                if (count == 0) {
                    snprintf(buf, bufsize, "#{}");
                } else if (count <= 3) {
                    /* Show small sets inline */
                    char temp[256];
                    int pos = 0;
                    pos += snprintf(buf, bufsize, "#{");
                    for (size_t i = 0; i < count && pos < (int) bufsize - 10; i++) {
                        edn_value_t* elem = edn_set_get(val, i);
                        render_value(elem, temp, sizeof(temp), false);
                        if (i > 0)
                            pos += snprintf(buf + pos, bufsize - pos, " ");
                        pos += snprintf(buf + pos, bufsize - pos, "%s", temp);
                    }
                    snprintf(buf + pos, bufsize - pos, "}");
                } else {
                    snprintf(buf, bufsize, "#{...}");
                }
            }
            break;
        }
        case EDN_TYPE_TAGGED: {
            const char* tag;
            size_t tag_len;
            edn_value_t* wrapped;
            edn_tagged_get(val, &tag, &tag_len, &wrapped);

            if (collapsed) {
                /* When collapsed, show tag with ellipsis if value is a collection */
                if (is_collection(wrapped)) {
                    snprintf(buf, bufsize, "#%.*s ...", (int) tag_len, tag);
                } else {
                    /* For non-collections, show the full tagged literal */
                    char wrapped_str[256];
                    render_value(wrapped, wrapped_str, sizeof(wrapped_str), false);
                    snprintf(buf, bufsize, "#%.*s %s", (int) tag_len, tag, wrapped_str);
                }
            } else {
                /* When not collapsed (inline display like map keys), show full tagged literal */
                char wrapped_str[256];
                render_value(wrapped, wrapped_str, sizeof(wrapped_str), false);
                snprintf(buf, bufsize, "#%.*s %s", (int) tag_len, tag, wrapped_str);
            }
            break;
        }
        default:
            snprintf(buf, bufsize, "<unknown>");
    }
}

/* Get color for value type */
static const char* get_value_color(edn_value_t* val) {
    if (val == NULL)
        return COLOR_NIL;

    switch (edn_type(val)) {
        case EDN_TYPE_NIL:
            return COLOR_NIL;
        case EDN_TYPE_BOOL:
            return COLOR_BOOL;
        case EDN_TYPE_INT:
        case EDN_TYPE_BIGINT:
        case EDN_TYPE_BIGDEC:
        case EDN_TYPE_FLOAT:
#ifdef EDN_ENABLE_RATIO
        case EDN_TYPE_RATIO:
#endif
            return COLOR_NUMBER;
        case EDN_TYPE_CHARACTER:
        case EDN_TYPE_STRING:
            return COLOR_STRING;
        case EDN_TYPE_KEYWORD:
            return COLOR_KEYWORD;
        case EDN_TYPE_SYMBOL:
            return COLOR_SYMBOL;
        case EDN_TYPE_TAGGED:
            return COLOR_TAG;
        default:
            return COLOR_RESET;
    }
}

/* Check if value is a collection */
static bool is_collection(edn_value_t* val) {
    if (val == NULL)
        return false;
    edn_type_t type = edn_type(val);
    return type == EDN_TYPE_VECTOR || type == EDN_TYPE_LIST || type == EDN_TYPE_MAP ||
           type == EDN_TYPE_SET;
}

/* Check if value is in expanded list */
static bool is_expanded(app_state_t* state, edn_value_t* val) {
    for (size_t i = 0; i < state->expanded_count; i++) {
        if (state->expanded_values[i] == val) {
            return true;
        }
    }
    return false;
}

/* Add value to expanded list */
static void mark_expanded(app_state_t* state, edn_value_t* val) {
    /* Check if already expanded */
    if (is_expanded(state, val))
        return;

    /* Grow array if needed */
    if (state->expanded_count >= state->expanded_capacity) {
        state->expanded_capacity =
            state->expanded_capacity == 0 ? 64 : state->expanded_capacity * 2;
        state->expanded_values =
            realloc(state->expanded_values, state->expanded_capacity * sizeof(edn_value_t*));
    }

    state->expanded_values[state->expanded_count++] = val;
}

/* Remove value from expanded list */
static void mark_collapsed(app_state_t* state, edn_value_t* val) {
    for (size_t i = 0; i < state->expanded_count; i++) {
        if (state->expanded_values[i] == val) {
            /* Remove by shifting remaining elements */
            for (size_t j = i; j < state->expanded_count - 1; j++) {
                state->expanded_values[j] = state->expanded_values[j + 1];
            }
            state->expanded_count--;
            return;
        }
    }
}

/* Add a node to the display list */
static void add_display_node(app_state_t* state, edn_value_t* value, int depth, bool expanded,
                             size_t index, const char* key_name, size_t key_name_len,
                             bool is_map_entry, bool is_closing_bracket, edn_value_t* map_value,
                             bool is_tagged, edn_value_t* tagged_value
#ifdef EDN_ENABLE_METADATA
                             ,
                             bool is_metadata, edn_value_t* meta_value
#endif
                             ,
                             display_node_t* parent) {
    if (state->node_count >= state->node_capacity) {
        state->node_capacity *= 2;
        state->nodes = realloc(state->nodes, state->node_capacity * sizeof(display_node_t*));
    }

    display_node_t* node = malloc(sizeof(display_node_t));
    node->value = value;
    node->map_value = map_value;
    node->tagged_value = tagged_value;
#ifdef EDN_ENABLE_METADATA
    node->meta_value = meta_value;
    node->is_metadata = is_metadata;
#endif
    node->depth = depth;
    node->expanded = expanded;
    node->is_collection = is_collection(value);
    node->index = index;
    node->key_name = key_name;
    node->key_name_len = key_name_len;
    node->is_map_entry = is_map_entry;
    node->is_tagged = is_tagged;
    node->is_closing_bracket = is_closing_bracket;
    node->parent = parent;

    state->nodes[state->node_count++] = node;
}

/* Build the display node tree (flatten for display) */
static void build_node_list_internal(app_state_t* state, edn_value_t* value, int depth,
                                     bool expanded, display_node_t* parent, bool skip_self) {
    if (value == NULL || depth > MAX_PATH_DEPTH)
        return;

    /* Check if this value should be expanded */
    if (is_collection(value)) {
        expanded = is_expanded(state, value);
    }

    size_t current_index = state->node_count;
    display_node_t* current_node = NULL;

    /* Handle tagged literals specially */
    if (!skip_self && edn_type(value) == EDN_TYPE_TAGGED) {
        const char* tag;
        size_t tag_len;
        edn_value_t* wrapped;
        edn_tagged_get(value, &tag, &tag_len, &wrapped);

        /* Add tagged literal as a single node showing tag and wrapped value */
        add_display_node(state, value, depth, false, 0, NULL, 0, false, false, NULL, true, wrapped
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);

        /* If wrapped value is a collection and expanded, show its children */
        if (is_collection(wrapped) && is_expanded(state, wrapped)) {
            build_node_list_internal(state, wrapped, depth, false, parent, true);
        }
        return;
    }

    /* Add self node unless we're skipping it (map value case) */
    if (!skip_self) {
        add_display_node(state, value, depth, expanded, 0, NULL, 0, false, false, NULL, false, NULL
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);

        if (!expanded || !is_collection(value))
            return;

        current_node = state->nodes[current_index];
    } else {
        /* When skipping self, we still need a parent reference */
        current_node = parent;

        if (!expanded || !is_collection(value))
            return;
    }

    edn_type_t type = edn_type(value);

    if (type == EDN_TYPE_VECTOR) {
        size_t count = edn_vector_count(value);
        for (size_t i = 0; i < count; i++) {
            edn_value_t* elem = edn_vector_get(value, i);
            build_node_list_internal(state, elem, depth + 1, false, current_node, false);
        }
        /* Add closing bracket */
        add_display_node(state, value, depth, false, 0, NULL, 0, false, true, NULL, false, NULL
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);
    } else if (type == EDN_TYPE_LIST) {
        size_t count = edn_list_count(value);
        for (size_t i = 0; i < count; i++) {
            edn_value_t* elem = edn_list_get(value, i);
            build_node_list_internal(state, elem, depth + 1, false, current_node, false);
        }
        /* Add closing bracket */
        add_display_node(state, value, depth, false, 0, NULL, 0, false, true, NULL, false, NULL
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);
    } else if (type == EDN_TYPE_MAP) {
        size_t count = edn_map_count(value);
        for (size_t i = 0; i < count; i++) {
            edn_value_t* key = edn_map_get_key(value, i);
            edn_value_t* val = edn_map_get_value(value, i);

            /* For maps, create a single node for the key-value pair */
            const char* key_name = NULL;
            size_t key_name_len = 0;
            if (edn_type(key) == EDN_TYPE_KEYWORD) {
                edn_keyword_get(key, NULL, NULL, &key_name, &key_name_len);
            }

            /* Add a single node representing the key-value pair */
            add_display_node(state, key, depth + 1, false, i, key_name, key_name_len, true, false,
                             val, false, NULL
#ifdef EDN_ENABLE_METADATA
                             ,
                             false, NULL
#endif
                             ,
                             current_node);

            /* If the key is a tagged literal with collection, handle expansion */
            if (edn_type(key) == EDN_TYPE_TAGGED) {
                const char* tag;
                size_t tag_len;
                edn_value_t* wrapped;
                edn_tagged_get(key, &tag, &tag_len, &wrapped);

                if (is_collection(wrapped) && is_expanded(state, wrapped)) {
                    build_node_list_internal(state, wrapped, depth + 2, false, current_node, true);
                }
            } else if (is_collection(key) && is_expanded(state, key)) {
                /* If the key is a collection and expanded, show its children */
                build_node_list_internal(state, key, depth + 2, false, current_node, true);
            }

            /* If the value is a tagged literal with collection, handle expansion */
            if (edn_type(val) == EDN_TYPE_TAGGED) {
                const char* tag;
                size_t tag_len;
                edn_value_t* wrapped;
                edn_tagged_get(val, &tag, &tag_len, &wrapped);

                if (is_collection(wrapped) && is_expanded(state, wrapped)) {
                    build_node_list_internal(state, wrapped, depth + 2, false, current_node, true);
                }
            } else if (is_collection(val) && is_expanded(state, val)) {
                /* If the value is a collection and expanded, show its children */
                /* Skip the opening node since it's already shown on the map entry line */
                build_node_list_internal(state, val, depth + 2, false, current_node, true);
            }
        }
        /* Add closing bracket */
        add_display_node(state, value, depth, false, 0, NULL, 0, false, true, NULL, false, NULL
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);
    } else if (type == EDN_TYPE_SET) {
        size_t count = edn_set_count(value);
        for (size_t i = 0; i < count; i++) {
            edn_value_t* elem = edn_set_get(value, i);
            build_node_list_internal(state, elem, depth + 1, false, current_node, false);
        }
        /* Add closing bracket */
        add_display_node(state, value, depth, false, 0, NULL, 0, false, true, NULL, false, NULL
#ifdef EDN_ENABLE_METADATA
                         ,
                         false, NULL
#endif
                         ,
                         parent);
    }
}

/* Wrapper function */
static void build_node_list(app_state_t* state, edn_value_t* value, int depth, bool expanded,
                            display_node_t* parent) {
    build_node_list_internal(state, value, depth, expanded, parent, false);
}

/* Rebuild the display node list */
static void rebuild_nodes(app_state_t* state) {
    /* Free old nodes */
    for (size_t i = 0; i < state->node_count; i++) {
        free(state->nodes[i]);
    }
    state->node_count = 0;

    /* Rebuild */
    build_node_list(state, state->root, 0, true, NULL);
}

/* Draw a single line */
static void draw_node_line(app_state_t* state, size_t node_idx, int screen_row, bool is_cursor) {
    display_node_t* node = state->nodes[node_idx];

    MOVE_CURSOR(screen_row, 1);
    printf(CLEAR_LINE);

    /* Indentation */
    for (int i = 0; i < node->depth; i++) {
        printf("  ");
    }

    /* Handle closing brackets */
    if (node->is_closing_bracket) {
        printf("  "); /* Space where expansion indicator would be */
        edn_type_t type = edn_type(node->value);
        if (type == EDN_TYPE_VECTOR) {
            printf("]");
        } else if (type == EDN_TYPE_LIST) {
            printf(")");
        } else if (type == EDN_TYPE_MAP) {
            printf("}");
        } else if (type == EDN_TYPE_SET) {
            printf("}");
        }
    } else if (node->is_tagged) {
        /* Tagged literal: show tag and value on same line */
        printf("  "); /* No expansion indicator for tagged literal itself */

        /* Render tag */
        char tag_str[256];
        render_value(node->value, tag_str, sizeof(tag_str), false);
        printf("%s%s%s ", COLOR_TAG, tag_str, COLOR_RESET);

        /* Show expansion indicator for collection values */
        if (is_collection(node->tagged_value)) {
            printf("%s ", is_expanded(state, node->tagged_value) ? "▼" : "▶");
        }

        /* Render wrapped value */
        char val_str[256];
        bool value_is_collapsed =
            is_collection(node->tagged_value) && !is_expanded(state, node->tagged_value);
        render_value(node->tagged_value, val_str, sizeof(val_str), value_is_collapsed);

        printf("%s%s%s", get_value_color(node->tagged_value), val_str, COLOR_RESET);
    } else if (node->is_map_entry) {
        /* Map entry: render as two columns */
        printf("  "); /* No expansion indicator for map entries themselves */

        /* Calculate column widths */
        int col_width = state->screen_width / 2 - (node->depth * 2) - 4;
        if (col_width < 10)
            col_width = 10;

        /* Determine which column has the cursor */
        bool key_has_cursor = is_cursor && !state->cursor_on_value;
        bool value_has_cursor = is_cursor && state->cursor_on_value;

        /* === KEY COLUMN === */
        /* Build the key string first to calculate its display width */
        char key_display[512];
        int key_pos = 0;

        edn_value_t* key = node->value;
        bool key_expanded = false;

        if (edn_type(key) == EDN_TYPE_TAGGED) {
            const char* tag;
            size_t tag_len;
            edn_value_t* wrapped;
            edn_tagged_get(key, &tag, &tag_len, &wrapped);

            key_expanded = is_collection(wrapped) && is_expanded(state, wrapped);

            /* Add tag */
            key_pos += snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "#%.*s ",
                                (int) tag_len, tag);

            /* Add expansion indicator */
            if (is_collection(wrapped)) {
                key_pos += snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s ",
                                    key_expanded ? "▼" : "▶");

                /* Add opening bracket or collapsed form */
                if (key_expanded) {
                    edn_type_t wtype = edn_type(wrapped);
                    if (wtype == EDN_TYPE_VECTOR) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "[");
                    } else if (wtype == EDN_TYPE_LIST) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "(");
                    } else if (wtype == EDN_TYPE_MAP) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "{");
                    } else if (wtype == EDN_TYPE_SET) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "#{");
                    }
                } else {
                    char key_str[256];
                    render_value(wrapped, key_str, sizeof(key_str), true);
                    key_pos += snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s",
                                        key_str);
                }
            } else {
                char key_str[256];
                render_value(wrapped, key_str, sizeof(key_str), false);
                key_pos +=
                    snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s", key_str);
            }
        } else {
            key_expanded = is_collection(key) && is_expanded(state, key);

            /* Add expansion indicator */
            if (is_collection(key)) {
                key_pos += snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s ",
                                    key_expanded ? "▼" : "▶");

                /* Add opening bracket or collapsed form */
                if (key_expanded) {
                    edn_type_t ktype = edn_type(key);
                    if (ktype == EDN_TYPE_VECTOR) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "[");
                    } else if (ktype == EDN_TYPE_LIST) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "(");
                    } else if (ktype == EDN_TYPE_MAP) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "{");
                    } else if (ktype == EDN_TYPE_SET) {
                        key_pos +=
                            snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "#{");
                    }
                } else {
                    char key_str[256];
                    render_value(key, key_str, sizeof(key_str), true);
                    key_pos += snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s",
                                        key_str);
                }
            } else {
                char key_str[256];
                render_value(key, key_str, sizeof(key_str), false);
                key_pos +=
                    snprintf(key_display + key_pos, sizeof(key_display) - key_pos, "%s", key_str);
            }
        }

        /* Now render the key with proper highlighting */
        if (key_has_cursor)
            printf(COLOR_CURSOR);

        if (edn_type(key) == EDN_TYPE_TAGGED) {
            const char* tag;
            size_t tag_len;
            edn_value_t* wrapped;
            edn_tagged_get(key, &tag, &tag_len, &wrapped);

            printf("%s#%.*s%s ", COLOR_TAG, (int) tag_len, tag, COLOR_RESET);
            if (key_has_cursor)
                printf(COLOR_CURSOR);

            if (is_collection(wrapped)) {
                printf("%s ", key_expanded ? "▼" : "▶");

                /* Show only opening bracket if expanded, otherwise show collapsed form */
                if (key_expanded) {
                    edn_type_t wtype = edn_type(wrapped);
                    if (wtype == EDN_TYPE_VECTOR) {
                        printf("[");
                    } else if (wtype == EDN_TYPE_LIST) {
                        printf("(");
                    } else if (wtype == EDN_TYPE_MAP) {
                        printf("{");
                    } else if (wtype == EDN_TYPE_SET) {
                        printf("#{");
                    }
                } else {
                    char key_str[256];
                    render_value(wrapped, key_str, sizeof(key_str), true);
                    printf("%s%s%s", get_value_color(wrapped), key_str, COLOR_RESET);
                    if (key_has_cursor)
                        printf(COLOR_CURSOR);
                }
            } else {
                char key_str[256];
                render_value(wrapped, key_str, sizeof(key_str), false);
                printf("%s%s%s", get_value_color(wrapped), key_str, COLOR_RESET);
                if (key_has_cursor)
                    printf(COLOR_CURSOR);
            }
        } else {
            if (is_collection(key)) {
                printf("%s ", key_expanded ? "▼" : "▶");

                /* Show only opening bracket if expanded, otherwise show collapsed form */
                if (key_expanded) {
                    edn_type_t ktype = edn_type(key);
                    if (ktype == EDN_TYPE_VECTOR) {
                        printf("[");
                    } else if (ktype == EDN_TYPE_LIST) {
                        printf("(");
                    } else if (ktype == EDN_TYPE_MAP) {
                        printf("{");
                    } else if (ktype == EDN_TYPE_SET) {
                        printf("#{");
                    }
                } else {
                    char key_str[256];
                    render_value(key, key_str, sizeof(key_str), true);
                    printf("%s%s%s", get_value_color(key), key_str, COLOR_RESET);
                    if (key_has_cursor)
                        printf(COLOR_CURSOR);
                }
            } else {
                char key_str[256];
                render_value(key, key_str, sizeof(key_str), false);
                printf("%s%s%s", get_value_color(key), key_str, COLOR_RESET);
                if (key_has_cursor)
                    printf(COLOR_CURSOR);
            }
        }

        if (key_has_cursor)
            printf(COLOR_RESET);

        /* Calculate actual display width (approximately) */
        /* This counts visible characters, ignoring ANSI escape sequences */
        int display_width = strlen(key_display);

        /* Pad to align the value column */
        int padding_needed = col_width - display_width;
        if (padding_needed < 1)
            padding_needed = 1;
        for (int i = 0; i < padding_needed; i++) {
            printf(" ");
        }

        /* === VALUE COLUMN === */
        if (value_has_cursor)
            printf(COLOR_CURSOR);

        edn_value_t* val = node->map_value;
        bool val_expanded = false;

        /* Check if value is a tagged literal */
        if (edn_type(val) == EDN_TYPE_TAGGED) {
            const char* tag;
            size_t tag_len;
            edn_value_t* wrapped;
            edn_tagged_get(val, &tag, &tag_len, &wrapped);

            val_expanded = is_collection(wrapped) && is_expanded(state, wrapped);

            /* Render tag */
            printf("%s#%.*s%s ", COLOR_TAG, (int) tag_len, tag, COLOR_RESET);
            if (value_has_cursor)
                printf(COLOR_CURSOR);

            /* Show expansion indicator for collection values */
            if (is_collection(wrapped)) {
                printf("%s ", val_expanded ? "▼" : "▶");

                /* Show only opening bracket if expanded, otherwise show collapsed form */
                if (val_expanded) {
                    edn_type_t wtype = edn_type(wrapped);
                    if (wtype == EDN_TYPE_VECTOR) {
                        printf("[");
                    } else if (wtype == EDN_TYPE_LIST) {
                        printf("(");
                    } else if (wtype == EDN_TYPE_MAP) {
                        printf("{");
                    } else if (wtype == EDN_TYPE_SET) {
                        printf("#{");
                    }
                } else {
                    char val_str[256];
                    render_value(wrapped, val_str, sizeof(val_str), true);
                    printf("%s%s%s", get_value_color(wrapped), val_str, COLOR_RESET);
                    if (value_has_cursor)
                        printf(COLOR_CURSOR);
                }
            } else {
                char val_str[256];
                render_value(wrapped, val_str, sizeof(val_str), false);
                printf("%s%s%s", get_value_color(wrapped), val_str, COLOR_RESET);
                if (value_has_cursor)
                    printf(COLOR_CURSOR);
            }
        } else {
            val_expanded = is_collection(val) && is_expanded(state, val);

            /* Show expansion indicator for collection values */
            if (is_collection(val)) {
                printf("%s ", val_expanded ? "▼" : "▶");

                /* Show only opening bracket if expanded, otherwise show collapsed form */
                if (val_expanded) {
                    edn_type_t vtype = edn_type(val);
                    if (vtype == EDN_TYPE_VECTOR) {
                        printf("[");
                    } else if (vtype == EDN_TYPE_LIST) {
                        printf("(");
                    } else if (vtype == EDN_TYPE_MAP) {
                        printf("{");
                    } else if (vtype == EDN_TYPE_SET) {
                        printf("#{");
                    }
                } else {
                    char val_str[256];
                    render_value(val, val_str, sizeof(val_str), true);
                    printf("%s%s%s", get_value_color(val), val_str, COLOR_RESET);
                    if (value_has_cursor)
                        printf(COLOR_CURSOR);
                }
            } else {
                char val_str[256];
                render_value(val, val_str, sizeof(val_str), false);
                printf("%s%s%s", get_value_color(val), val_str, COLOR_RESET);
                if (value_has_cursor)
                    printf(COLOR_CURSOR);
            }
        }

        if (value_has_cursor)
            printf(COLOR_RESET);
    } else {
        /* Regular value or collection */
        if (is_cursor)
            printf(COLOR_CURSOR);

        /* Expansion indicator for collections */
        if (node->is_collection) {
            printf("%s ", node->expanded ? "▼" : "▶");
        } else {
            printf("  ");
        }

        /* Value */
        char value_str[256];
        render_value(node->value, value_str, sizeof(value_str), !node->expanded);

        printf("%s%s%s", get_value_color(node->value), value_str, COLOR_RESET);

        if (is_cursor)
            printf(COLOR_RESET);
    }
}

/* Draw the entire screen */
static void draw_screen(app_state_t* state) {
    printf(CLEAR_SCREEN);

    /* Title bar */
    MOVE_CURSOR(1, 1);
    printf(COLOR_STATUS "EDN Viewer - arrows:navigate (L/R for map cells), Enter/Space/Tab:expand, "
                        "f:focus, F:unfocus"
#ifdef EDN_ENABLE_METADATA
                        ", m:metadata, M:back"
#endif
                        ", q:quit" COLOR_RESET);

    /* Content area */
    int content_start = 2;
    int content_height = state->screen_height - 3; /* Reserve space for title and status */

    for (int i = 0; i < content_height && (state->scroll_offset + i) < state->node_count; i++) {
        size_t node_idx = state->scroll_offset + i;
        bool is_cursor = (node_idx == state->cursor_pos);
        draw_node_line(state, node_idx, content_start + i, is_cursor);
    }

    /* Status bar */
    MOVE_CURSOR(state->screen_height, 1);
    printf(CLEAR_LINE);
#ifdef EDN_ENABLE_METADATA
    if (state->metadata_view_original != NULL) {
        printf(COLOR_STATUS "Node %zu/%zu [METADATA VIEW - press Shift+M to return]" COLOR_RESET,
               state->cursor_pos + 1, state->node_count);
    } else
#endif
        if (state->root != state->original_root) {
        printf(COLOR_STATUS "Node %zu/%zu [FOCUSED - press Shift+F to reset]" COLOR_RESET,
               state->cursor_pos + 1, state->node_count);
    } else {
        printf(COLOR_STATUS "Node %zu/%zu" COLOR_RESET, state->cursor_pos + 1, state->node_count);
    }

    fflush(stdout);
}

/* Handle key press */
static void handle_key(app_state_t* state, int key) {
    int content_height = state->screen_height - 3;

    switch (key) {
        case KEY_Q:
        case KEY_ESC:
            state->running = false;
            break;

        case KEY_UP:
            if (state->cursor_pos > 0) {
                state->cursor_pos--;
                state->cursor_on_value = false; /* Reset to key column when moving vertically */

                if (state->cursor_pos < state->scroll_offset) {
                    state->scroll_offset = state->cursor_pos;
                }
            }
            break;

        case KEY_DOWN:
            if (state->cursor_pos < state->node_count - 1) {
                state->cursor_pos++;
                state->cursor_on_value = false; /* Reset to key column when moving vertically */

                if (state->cursor_pos >= state->scroll_offset + content_height) {
                    state->scroll_offset = state->cursor_pos - content_height + 1;
                }
            }
            break;

        case KEY_LEFT: {
            /* Move from value column to key column in map entries */
            display_node_t* node = state->nodes[state->cursor_pos];
            if (node->is_map_entry && state->cursor_on_value) {
                state->cursor_on_value = false;
            }
            break;
        }

        case KEY_RIGHT: {
            /* Move from key column to value column in map entries */
            display_node_t* node = state->nodes[state->cursor_pos];
            if (node->is_map_entry && !state->cursor_on_value) {
                state->cursor_on_value = true;
            }
            break;
        }

        case KEY_ENTER:
        case KEY_SPACE:
        case KEY_TAB: {
            display_node_t* node = state->nodes[state->cursor_pos];

            /* Handle tagged literals - expand the wrapped value if it's a collection */
            if (node->is_tagged && node->tagged_value != NULL) {
                if (is_collection(node->tagged_value)) {
                    if (is_expanded(state, node->tagged_value)) {
                        mark_collapsed(state, node->tagged_value);
                    } else {
                        mark_expanded(state, node->tagged_value);
                    }
                    rebuild_nodes(state);
                    if (state->cursor_pos >= state->node_count) {
                        state->cursor_pos = state->node_count - 1;
                    }
                }
            } else if (node->is_map_entry) {
                /* Handle map entries based on which column the cursor is in */
                if (state->cursor_on_value) {
                    /* Expand/collapse the value */
                    edn_value_t* val = node->map_value;

                    if (edn_type(val) == EDN_TYPE_TAGGED) {
                        const char* tag;
                        size_t tag_len;
                        edn_value_t* wrapped;
                        edn_tagged_get(val, &tag, &tag_len, &wrapped);

                        if (is_collection(wrapped)) {
                            if (is_expanded(state, wrapped)) {
                                mark_collapsed(state, wrapped);
                            } else {
                                mark_expanded(state, wrapped);
                            }
                            rebuild_nodes(state);
                            if (state->cursor_pos >= state->node_count) {
                                state->cursor_pos = state->node_count - 1;
                            }
                        }
                    } else if (is_collection(val)) {
                        if (is_expanded(state, val)) {
                            mark_collapsed(state, val);
                        } else {
                            mark_expanded(state, val);
                        }
                        rebuild_nodes(state);
                        if (state->cursor_pos >= state->node_count) {
                            state->cursor_pos = state->node_count - 1;
                        }
                    }
                } else {
                    /* Expand/collapse the key */
                    edn_value_t* key = node->value;

                    if (edn_type(key) == EDN_TYPE_TAGGED) {
                        const char* tag;
                        size_t tag_len;
                        edn_value_t* wrapped;
                        edn_tagged_get(key, &tag, &tag_len, &wrapped);

                        if (is_collection(wrapped)) {
                            if (is_expanded(state, wrapped)) {
                                mark_collapsed(state, wrapped);
                            } else {
                                mark_expanded(state, wrapped);
                            }
                            rebuild_nodes(state);
                            if (state->cursor_pos >= state->node_count) {
                                state->cursor_pos = state->node_count - 1;
                            }
                        }
                    } else if (is_collection(key)) {
                        if (is_expanded(state, key)) {
                            mark_collapsed(state, key);
                        } else {
                            mark_expanded(state, key);
                        }
                        rebuild_nodes(state);
                        if (state->cursor_pos >= state->node_count) {
                            state->cursor_pos = state->node_count - 1;
                        }
                    }
                }
            } else if (node->is_collection) {
                /* Toggle expanded state for regular collections */
                if (node->expanded) {
                    mark_collapsed(state, node->value);
                } else {
                    mark_expanded(state, node->value);
                }
                rebuild_nodes(state);
                /* Try to keep cursor on same logical node */
                if (state->cursor_pos >= state->node_count) {
                    state->cursor_pos = state->node_count - 1;
                }
            }
            break;
        }

        case KEY_F:
        case KEY_SHIFT_F: {
            /* Focus or unfocus */
            if (key == KEY_F) {
                /* Focus on current value */
                display_node_t* node = state->nodes[state->cursor_pos];

                /* Determine which value to focus on */
                edn_value_t* focus_target = NULL;

                if (node->is_closing_bracket) {
                    /* Can't focus on closing bracket */
                    break;
                } else if (node->is_tagged) {
                    /* Focus on the tagged literal itself */
                    focus_target = node->value;
                } else if (node->is_map_entry) {
                    /* Focus on either the key or value depending on cursor position */
                    if (state->cursor_on_value) {
                        focus_target = node->map_value;
                    } else {
                        focus_target = node->value;
                    }
                } else {
                    /* Focus on the value itself */
                    focus_target = node->value;
                }

                if (focus_target && focus_target != state->root) {
                    state->root = focus_target;
                    /* Auto-expand if it's a collection */
                    if (is_collection(focus_target) && !is_expanded(state, focus_target)) {
                        mark_expanded(state, focus_target);
                    }
                    state->cursor_pos = 0;
                    state->cursor_on_value = false;
                    state->scroll_offset = 0;
                    rebuild_nodes(state);
                }
            } else {
                /* Shift+F: Reset to original root */
                if (state->root != state->original_root) {
                    state->root = state->original_root;
#ifdef EDN_ENABLE_METADATA
                    state->metadata_view_original = NULL;
#endif
                    state->cursor_pos = 0;
                    state->cursor_on_value = false;
                    state->scroll_offset = 0;
                    rebuild_nodes(state);
                }
            }
            break;
        }

#ifdef EDN_ENABLE_METADATA
        case KEY_M:
        case KEY_SHIFT_M: {
            if (key == KEY_M) {
                /* View metadata of current value */
                display_node_t* node = state->nodes[state->cursor_pos];

                /* Determine which value to get metadata from */
                edn_value_t* target = NULL;

                if (node->is_closing_bracket) {
                    /* Can't get metadata from closing bracket */
                    break;
                } else if (node->is_tagged) {
                    /* Get metadata from the tagged literal itself */
                    target = node->value;
                } else if (node->is_map_entry) {
                    /* Get metadata from either the key or value depending on cursor position */
                    if (state->cursor_on_value) {
                        target = node->map_value;
                    } else {
                        target = node->value;
                    }
                } else {
                    /* Get metadata from the value itself */
                    target = node->value;
                }

                if (target) {
                    edn_value_t* meta = edn_value_meta(target);

                    /* Store the original value we're viewing metadata for */
                    state->metadata_view_original = target;

                    /* If no metadata, create a nil value to display */
                    if (meta == NULL) {
                        /* Parse "nil" to create a nil value for display */
                        edn_result_t nil_result = edn_parse("nil", 3);
                        if (nil_result.error == EDN_OK) {
                            state->root = nil_result.value;
                        }
                    } else {
                        state->root = meta;
                        /* Auto-expand if it's a collection */
                        if (is_collection(meta) && !is_expanded(state, meta)) {
                            mark_expanded(state, meta);
                        }
                    }

                    state->cursor_pos = 0;
                    state->cursor_on_value = false;
                    state->scroll_offset = 0;
                    rebuild_nodes(state);
                }
            } else {
                /* Shift+M: Return from metadata view */
                if (state->metadata_view_original != NULL) {
                    /* If we created a nil value for display, free it */
                    if (edn_type(state->root) == EDN_TYPE_NIL && !edn_value_has_meta(state->root)) {
                        edn_free(state->root);
                    }

                    state->root = state->original_root;
                    state->metadata_view_original = NULL;
                    state->cursor_pos = 0;
                    state->cursor_on_value = false;
                    state->scroll_offset = 0;
                    rebuild_nodes(state);
                }
            }
            break;
        }
#endif
    }
}

/* Main event loop */
static void run_tui(app_state_t* state) {
    state->running = true;

    while (state->running) {
        get_window_size(&state->screen_height, &state->screen_width);
        draw_screen(state);

        int key = read_key();
        if (key != -1) {
            handle_key(state, key);
        }

        usleep(10000); /* 10ms sleep to reduce CPU usage */
    }
}

/* Read entire input into buffer */
static char* read_input(FILE* fp, size_t* out_size) {
    size_t capacity = INITIAL_BUFFER_SIZE;
    size_t size = 0;
    char* buffer = malloc(capacity);

    if (buffer == NULL) {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }

    while (true) {
        size_t space = capacity - size;
        if (space < 1024) {
            if (capacity >= MAX_BUFFER_SIZE) {
                fprintf(stderr, "Error: Input too large (max %d MB)\n",
                        MAX_BUFFER_SIZE / (1024 * 1024));
                free(buffer);
                return NULL;
            }

            size_t new_capacity = capacity * 2;
            if (new_capacity > MAX_BUFFER_SIZE) {
                new_capacity = MAX_BUFFER_SIZE;
            }

            char* new_buffer = realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                fprintf(stderr, "Error: Out of memory\n");
                free(buffer);
                return NULL;
            }

            buffer = new_buffer;
            capacity = new_capacity;
            space = capacity - size;
        }

        size_t read_size = fread(buffer + size, 1, space, fp);
        size += read_size;

        if (read_size < space) {
            if (feof(fp)) {
                break;
            } else if (ferror(fp)) {
                fprintf(stderr, "Error: Failed to read input\n");
                free(buffer);
                return NULL;
            }
        }
    }

    *out_size = size;
    return buffer;
}

int main(int argc, char** argv) {
    const char* filename = NULL;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [FILE]\n", argv[0]);
            printf("\n");
            printf("Interactive TUI for exploring EDN data.\n");
            printf("\n");
            printf("Controls:\n");
            printf("  Arrow Up/Down     Navigate\n");
            printf("  Arrow Left/Right  Navigate between map key and value cells\n");
            printf("  Enter/Space/Tab   Expand/collapse\n");
            printf("  f                 Focus on current value\n");
            printf("  F (Shift+f)       Reset to original view\n");
#ifdef EDN_ENABLE_METADATA
            printf("  m                 View metadata of current value\n");
            printf("  M (Shift+m)       Return from metadata view\n");
#endif
            printf("  q/ESC             Quit\n");
            printf("\n");
            printf("Examples:\n");
            printf("  %s data.edn\n", argv[0]);
            printf("  %s < data.edn\n", argv[0]);
            printf("  echo '{:a 1}' | %s\n", argv[0]);
            return 0;
        } else if (filename == NULL) {
            filename = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple input files specified\n");
            return 1;
        }
    }

    /* Open input file or use stdin */
    FILE* input = stdin;
    if (filename != NULL) {
        input = fopen(filename, "r");
        if (input == NULL) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
            return 1;
        }
    }

    /* Read entire input */
    size_t input_size;
    char* input_data = read_input(input, &input_size);

    if (filename != NULL) {
        fclose(input);
    }

    if (input_data == NULL) {
        return 1;
    }

    /* Parse EDN */
    edn_result_t result = edn_parse(input_data, input_size);

    if (result.error != EDN_OK) {
        fprintf(stderr, "Parse error at line %zu, column %zu:\n", result.error_line,
                result.error_column);
        fprintf(stderr, "  %s\n", result.error_message);
        free(input_data);
        return 1;
    }

    /* Initialize application state */
    app_state_t state = {0};
    state.root = result.value;
    state.original_root = result.value; /* Save original for reset */
#ifdef EDN_ENABLE_METADATA
    state.metadata_view_original = NULL;
#endif
    state.node_capacity = 1024;
    state.nodes = malloc(state.node_capacity * sizeof(display_node_t*));
    state.cursor_pos = 0;
    state.cursor_on_value = false;
    state.scroll_offset = 0;
    state.search_query[0] = '\0';
    state.searching = false;
    state.expanded_capacity = 64;
    state.expanded_values = malloc(state.expanded_capacity * sizeof(edn_value_t*));
    state.expanded_count = 0;

    /* Mark root as initially expanded */
    if (is_collection(state.root)) {
        mark_expanded(&state, state.root);
    }

    /* Build initial node list */
    rebuild_nodes(&state);

    /* Run TUI */
    enable_raw_mode();
    run_tui(&state);
    disable_raw_mode();

    /* Cleanup */
    for (size_t i = 0; i < state.node_count; i++) {
        free(state.nodes[i]);
    }
    free(state.nodes);
    free(state.expanded_values);
    edn_free(result.value);
    free(input_data);

    return 0;
}
