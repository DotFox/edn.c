/**
 * EDN.C - CLI EDN Reader
 * 
 * A command-line tool that parses EDN from stdin or file and pretty-prints the result.
 * 
 * Usage:
 *   edn_cli [file]           # Parse file
 *   edn_cli < file           # Parse from stdin
 *   echo '{:a 1}' | edn_cli  # Parse from stdin
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/edn.h"

#define INITIAL_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE (1024 * 1024 * 100) /* 100MB limit */

/* Pretty-print configuration */
typedef struct {
    bool use_colors;
} print_options_t;

/* ANSI color codes */
#define COLOR_RESET "\033[0m"
#define COLOR_NIL "\033[90m"     /* Gray */
#define COLOR_BOOL "\033[35m"    /* Magenta */
#define COLOR_NUMBER "\033[36m"  /* Cyan */
#define COLOR_STRING "\033[32m"  /* Green */
#define COLOR_KEYWORD "\033[34m" /* Blue */
#define COLOR_SYMBOL "\033[33m"  /* Yellow */
#define COLOR_TAG "\033[35;1m"   /* Bright Magenta */

/* Forward declaration */
static void print_value(const edn_value_t* value, int indent, const print_options_t* opts);

/* Print indentation - Clojure style uses 1 space for collection elements */
static void print_indent(int level) {
    for (int i = 0; i < level; i++) {
        putchar(' ');
    }
}

/* Print nil */
static void print_nil(const print_options_t* opts) {
    if (opts->use_colors)
        printf("%s", COLOR_NIL);
    printf("nil");
    if (opts->use_colors)
        printf("%s", COLOR_RESET);
}

/* Print boolean */
static void print_bool(const edn_value_t* value, const print_options_t* opts) {
    /* Access internal structure - bool is stored in value->as.boolean */
    /* This is a workaround until edn_bool_get() is added to public API */
    struct edn_value_internal {
        edn_type_t type;
        uint64_t cached_hash;
#ifdef EDN_ENABLE_METADATA
        edn_value_t* metadata;
#endif
        union {
            bool boolean;
            /* ... other fields ... */
        } as;
    };

    const struct edn_value_internal* internal = (const struct edn_value_internal*) value;
    bool val = internal->as.boolean;

    if (opts->use_colors)
        printf("%s", COLOR_BOOL);
    printf("%s", val ? "true" : "false");
    if (opts->use_colors)
        printf("%s", COLOR_RESET);
}

/* Print integer */
static void print_int(const edn_value_t* value, const print_options_t* opts) {
    int64_t num;
    if (edn_int64_get(value, &num)) {
        if (opts->use_colors)
            printf("%s", COLOR_NUMBER);
        printf("%lld", (long long) num);
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print big integer */
static void print_bigint(const edn_value_t* value, const print_options_t* opts) {
    size_t len;
    bool negative;
    uint8_t radix;
    const char* digits = edn_bigint_get(value, &len, &negative, &radix);

    if (digits) {
        if (opts->use_colors)
            printf("%s", COLOR_NUMBER);
        if (negative)
            printf("-");

        /* Print with radix prefix for non-decimal */
        if (radix == 16)
            printf("0x");
        else if (radix == 8)
            printf("0");
        else if (radix == 2)
            printf("0b");

        printf("%.*s", (int) len, digits);
        printf("N"); /* BigInt suffix */
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print float */
static void print_float(const edn_value_t* value, const print_options_t* opts) {
    double num;
    if (edn_double_get(value, &num)) {
        if (opts->use_colors)
            printf("%s", COLOR_NUMBER);
        printf("%g", num);
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print big decimal */
static void print_bigdec(const edn_value_t* value, const print_options_t* opts) {
    size_t len;
    bool negative;
    const char* decimal = edn_bigdec_get(value, &len, &negative);

    if (decimal) {
        if (opts->use_colors)
            printf("%s", COLOR_NUMBER);
        if (negative)
            printf("-");
        printf("%.*s", (int) len, decimal);
        printf("M"); /* BigDecimal suffix */
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print character */
static void print_character(const edn_value_t* value, const print_options_t* opts) {
    uint32_t codepoint;
    if (edn_character_get(value, &codepoint)) {
        if (opts->use_colors)
            printf("%s", COLOR_STRING);
        printf("\\");

        /* Special named characters */
        switch (codepoint) {
            case '\n':
                printf("newline");
                break;
            case '\t':
                printf("tab");
                break;
            case '\r':
                printf("return");
                break;
            case ' ':
                printf("space");
                break;
            default:
                if (codepoint < 32 || codepoint == 127) {
                    /* Control characters */
                    printf("u%04X", codepoint);
                } else if (codepoint < 128) {
                    /* Printable ASCII */
                    printf("%c", (char) codepoint);
                } else if (codepoint <= 0x10FFFF) {
                    /* Unicode character - encode as UTF-8 */
                    char utf8[5];
                    int len = 0;
                    if (codepoint < 0x80) {
                        utf8[len++] = (char) codepoint;
                    } else if (codepoint < 0x800) {
                        utf8[len++] = (char) (0xC0 | (codepoint >> 6));
                        utf8[len++] = (char) (0x80 | (codepoint & 0x3F));
                    } else if (codepoint < 0x10000) {
                        utf8[len++] = (char) (0xE0 | (codepoint >> 12));
                        utf8[len++] = (char) (0x80 | ((codepoint >> 6) & 0x3F));
                        utf8[len++] = (char) (0x80 | (codepoint & 0x3F));
                    } else {
                        utf8[len++] = (char) (0xF0 | (codepoint >> 18));
                        utf8[len++] = (char) (0x80 | ((codepoint >> 12) & 0x3F));
                        utf8[len++] = (char) (0x80 | ((codepoint >> 6) & 0x3F));
                        utf8[len++] = (char) (0x80 | (codepoint & 0x3F));
                    }
                    utf8[len] = '\0';
                    printf("%s", utf8);
                } else {
                    /* Invalid codepoint */
                    printf("u%04X", codepoint);
                }
        }

        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print string */
static void print_string(const edn_value_t* value, const print_options_t* opts) {
    size_t len;
    const char* str = edn_string_get(value, &len);

    if (str) {
        if (opts->use_colors)
            printf("%s", COLOR_STRING);
        printf("\"");

        /* Print with escape sequences */
        for (size_t i = 0; i < len; i++) {
            char c = str[i];
            switch (c) {
                case '\n':
                    printf("\\n");
                    break;
                case '\t':
                    printf("\\t");
                    break;
                case '\r':
                    printf("\\r");
                    break;
                case '\\':
                    printf("\\\\");
                    break;
                case '"':
                    printf("\\\"");
                    break;
                default:
                    putchar(c);
                    break;
            }
        }

        printf("\"");
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print keyword */
static void print_keyword(const edn_value_t* value, const print_options_t* opts) {
    const char* ns;
    const char* name;
    size_t ns_len, name_len;

    if (edn_keyword_get(value, &ns, &ns_len, &name, &name_len)) {
        if (opts->use_colors)
            printf("%s", COLOR_KEYWORD);
        printf(":");
        if (ns != NULL) {
            printf("%.*s/", (int) ns_len, ns);
        }
        printf("%.*s", (int) name_len, name);
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print symbol */
static void print_symbol(const edn_value_t* value, const print_options_t* opts) {
    const char* ns;
    const char* name;
    size_t ns_len, name_len;

    if (edn_symbol_get(value, &ns, &ns_len, &name, &name_len)) {
        if (opts->use_colors)
            printf("%s", COLOR_SYMBOL);
        if (ns != NULL) {
            printf("%.*s/", (int) ns_len, ns);
        }
        printf("%.*s", (int) name_len, name);
        if (opts->use_colors)
            printf("%s", COLOR_RESET);
    }
}

/* Print list */
static void print_list(const edn_value_t* value, int indent, const print_options_t* opts) {
    size_t count = edn_list_count(value);

    printf("(");
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            if (i > 0)
                printf(" ");
            print_value(edn_list_get(value, i), indent, opts);
        }
    }
    printf(")");
}

/* Print vector */
static void print_vector(const edn_value_t* value, int indent, const print_options_t* opts) {
    size_t count = edn_vector_count(value);

    printf("[");
    if (count > 0) {
        /* Clojure style: first element on same line, rest aligned below
         * Small vectors (≤3 elements) stay inline
         * Example: [1        or    [1 2 3]
         *           2
         *           3]
         */
        bool multiline = count > 3;

        if (multiline) {
            /* First element on same line as [ */
            print_value(edn_vector_get(value, 0), indent + 1, opts);

            /* Rest of elements aligned with first */
            for (size_t i = 1; i < count; i++) {
                printf("\n");
                print_indent(indent + 1); /* Align with first element */
                print_value(edn_vector_get(value, i), indent + 1, opts);
            }
        } else {
            /* Inline for small vectors */
            for (size_t i = 0; i < count; i++) {
                if (i > 0)
                    printf(" ");
                print_value(edn_vector_get(value, i), indent, opts);
            }
        }
    }
    printf("]");
}

/* Print set */
static void print_set(const edn_value_t* value, int indent, const print_options_t* opts) {
    size_t count = edn_set_count(value);

    printf("#{");
    if (count > 0) {
        /* Clojure style: first element on same line, rest aligned below
         * Example: #{:a       or    #{:a :b :c}
         *            :b
         *            :c}
         */
        bool multiline = count > 3;

        if (multiline) {
            /* First element on same line as #{ */
            print_value(edn_set_get(value, 0), indent + 2, opts);

            /* Rest of elements aligned with first */
            for (size_t i = 1; i < count; i++) {
                printf("\n");
                print_indent(indent + 2); /* Align with first element (2 chars: #{) */
                print_value(edn_set_get(value, i), indent + 2, opts);
            }
        } else {
            /* Inline for small sets */
            for (size_t i = 0; i < count; i++) {
                if (i > 0)
                    printf(" ");
                print_value(edn_set_get(value, i), indent, opts);
            }
        }
    }
    printf("}");
}

/* Print map */
static void print_map(const edn_value_t* value, int indent, const print_options_t* opts) {
    size_t count = edn_map_count(value);

    printf("{");
    if (count > 0) {
        /* Clojure style: first key-value pair on same line, rest aligned below
         * Example: {:foo 1        or    {:foo 1 :bar 2}
         *           :bar 2
         *           :baz 3}
         */
        bool multiline = count > 2;

        if (multiline) {
            /* First key-value pair on same line as { */
            print_value(edn_map_get_key(value, 0), indent + 1, opts);
            printf(" ");
            print_value(edn_map_get_value(value, 0), indent + 1, opts);

            /* Rest of key-value pairs aligned with first key */
            for (size_t i = 1; i < count; i++) {
                printf("\n");
                print_indent(indent + 1); /* Align with first key */
                print_value(edn_map_get_key(value, i), indent + 1, opts);
                printf(" ");
                print_value(edn_map_get_value(value, i), indent + 1, opts);
            }
        } else {
            /* Inline for small maps */
            for (size_t i = 0; i < count; i++) {
                if (i > 0)
                    printf(" ");
                print_value(edn_map_get_key(value, i), indent, opts);
                printf(" ");
                print_value(edn_map_get_value(value, i), indent, opts);
            }
        }
    }
    printf("}");
}

/* Print tagged literal */
static void print_tagged(const edn_value_t* value, int indent, const print_options_t* opts) {
    const char* tag;
    size_t tag_len;
    edn_value_t* wrapped;

    if (edn_tagged_get(value, &tag, &tag_len, &wrapped)) {
        if (opts->use_colors)
            printf("%s", COLOR_TAG);
        printf("#%.*s ", (int) tag_len, tag);
        if (opts->use_colors)
            printf("%s", COLOR_RESET);

        print_value(wrapped, indent, opts);
    }
}

/* Print any EDN value */
static void print_value(const edn_value_t* value, int indent, const print_options_t* opts) {
    if (value == NULL) {
        print_nil(opts);
        return;
    }

    switch (edn_type(value)) {
        case EDN_TYPE_NIL:
            print_nil(opts);
            break;
        case EDN_TYPE_BOOL:
            print_bool(value, opts);
            break;
        case EDN_TYPE_INT:
            print_int(value, opts);
            break;
        case EDN_TYPE_BIGINT:
            print_bigint(value, opts);
            break;
        case EDN_TYPE_FLOAT:
            print_float(value, opts);
            break;
        case EDN_TYPE_BIGDEC:
            print_bigdec(value, opts);
            break;
        case EDN_TYPE_CHARACTER:
            print_character(value, opts);
            break;
        case EDN_TYPE_STRING:
            print_string(value, opts);
            break;
        case EDN_TYPE_KEYWORD:
            print_keyword(value, opts);
            break;
        case EDN_TYPE_SYMBOL:
            print_symbol(value, opts);
            break;
        case EDN_TYPE_LIST:
            print_list(value, indent, opts);
            break;
        case EDN_TYPE_VECTOR:
            print_vector(value, indent, opts);
            break;
        case EDN_TYPE_MAP:
            print_map(value, indent, opts);
            break;
        case EDN_TYPE_SET:
            print_set(value, indent, opts);
            break;
        case EDN_TYPE_TAGGED:
            print_tagged(value, indent, opts);
            break;
        default:
            printf("<unknown type>");
            break;
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

        size_t read = fread(buffer + size, 1, space, fp);
        size += read;

        if (read < space) {
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

/* Print usage */
static void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS] [FILE]\n", program_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Parse and pretty-print EDN data from file or stdin.\n");
    fprintf(stderr, "Uses Clojure-style formatting with single-space indentation.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help        Show this help message\n");
    fprintf(stderr, "  -c, --color       Enable colored output (default if tty)\n");
    fprintf(stderr, "  -C, --no-color    Disable colored output\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s data.edn                    Parse file\n", program_name);
    fprintf(stderr, "  %s < data.edn                  Parse from stdin\n", program_name);
    fprintf(stderr, "  echo '{:a 1}' | %s             Parse from pipe\n", program_name);
    fprintf(stderr, "  %s --no-color data.edn         Disable colors\n", program_name);
}

int main(int argc, char** argv) {
    const char* filename = NULL;
    print_options_t opts = {
        .use_colors = isatty(fileno(stdout)) /* Auto-detect terminal */
    };

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0) {
            opts.use_colors = true;
        } else if (strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--no-color") == 0) {
            opts.use_colors = false;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (filename == NULL) {
            filename = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple input files specified\n");
            print_usage(argv[0]);
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

        /* Show error context */
        if (result.error_line > 0 && result.error_line <= input_size) {
            const char* line_start = input_data;
            size_t current_line = 1;

            /* Find the error line */
            while (current_line < result.error_line && *line_start != '\0') {
                if (*line_start == '\n') {
                    current_line++;
                }
                line_start++;
            }

            /* Find line end */
            const char* line_end = line_start;
            while (*line_end != '\0' && *line_end != '\n') {
                line_end++;
            }

            /* Print line with error marker */
            fprintf(stderr, "\n%zu | %.*s\n", result.error_line, (int) (line_end - line_start),
                    line_start);
            fprintf(stderr, "    ");
            for (size_t i = 1; i < result.error_column; i++) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "^\n");
        }

        free(input_data);
        return 1;
    }

    /* Pretty-print result */
    print_value(result.value, 0, &opts);
    printf("\n");

    /* Cleanup */
    edn_free(result.value);
    free(input_data);

    return 0;
}
