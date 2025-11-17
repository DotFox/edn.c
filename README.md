# EDN.C

A fast, zero-copy EDN (Extensible Data Notation) parser written in C11 with SIMD acceleration.

[![CI](https://github.com/DeLaGuardo/edn.c/workflows/CI/badge.svg)](https://github.com/DeLaGuardo/edn.c/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

## Features

- **🚀 Fast**: SIMD-accelerated parsing with NEON (ARM64) and SSE4.2 (x86_64) support
- **💾 Zero-copy**: Minimal allocations, references input data where possible
- **🎯 Simple API**: Easy-to-use interface with comprehensive type support
- **🧹 Memory-safe**: Arena allocator for efficient cleanup - single `edn_free()` call
- **🔧 Zero Dependencies**: Pure C11 with standard library only
- **✅ Fully Tested**: 340+ tests across 24 test suites
- **📖 UTF-8 Native**: All string inputs and outputs are UTF-8 encoded
- **🏷️ Tagged Literals**: Extensible data types with custom reader support
- **🗺️ Map Namespace Syntax**: Clojure-compatible `#:ns{...}` syntax (optional, disabled by default)
- **🔤 Extended Characters**: `\formfeed`, `\backspace`, and octal `\oNNN` literals (optional, disabled by default)
- **📝 Metadata**: Clojure-style metadata `^{...}` syntax (optional, disabled by default)
- **📄 Text Blocks**: Java-style multi-line text blocks `"""\n...\n"""` (experimental, disabled by default)

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Whitespace and Control Characters](#whitespace-and-control-characters)
- [API Reference](#api-reference)
  - [Core Functions](#core-functions)
  - [Type System](#type-system)
  - [Scalar Types](#scalar-types)
  - [Collections](#collections)
  - [Tagged Literals](#tagged-literals)
  - [Custom Readers](#custom-readers)
  - [Map Namespace Syntax](#map-namespace-syntax)
  - [Extended Character Literals](#extended-character-literals)
  - [Metadata](#metadata)
  - [Text Blocks](#text-blocks)
- [Examples](#examples)
- [Building](#building)
- [Performance](#performance)
- [Contributing](#contributing)
- [License](#license)

## Installation

### Requirements

- C11 compatible compiler (GCC 4.9+, Clang 3.1+, MSVC 2015+)
- Make (Unix/macOS) or CMake (Windows/cross-platform)
- Supported platforms:
  - **macOS** (Apple Silicon M1/M2/M3, Intel) - NEON/SSE4.2 SIMD
  - **Linux** (ARM64, x86_64) - NEON/SSE4.2 SIMD
  - **Windows** (x86_64, ARM64) - NEON/SSE4.2 SIMD via MSVC/MinGW/Clang

### Build Library

**Unix/macOS/Linux:**
```bash
# Clone the repository
git clone https://github.com/DotFox/edn.c.git
cd edn.c

# Build static library (libedn.a)
make

# Run tests to verify build
make test
```

**Windows:**
```powershell
# Clone the repository
git clone https://github.com/DotFox/edn.c.git
cd edn.c

# Build with CMake (works with MSVC, MinGW, Clang)
.\build.bat

# Or use PowerShell script
.\build.ps1 -Test
```

See **[docs/WINDOWS.md](docs/WINDOWS.md)** for detailed Windows build instructions.

### Integrate Into Your Project

**Option 1: Link static library**

```bash
# Compile your code
gcc -o myapp myapp.c -I/path/to/edn.c/include -L/path/to/edn.c -ledn

# Or add to your Makefile
CFLAGS += -I/path/to/edn.c/include
LDFLAGS += -L/path/to/edn.c -ledn
```

**Option 2: Include source directly**

Copy `include/edn.h` and all files from `src/` into your project and compile them together.

## Quick Start

```c
#include "edn.h"
#include <stdio.h>

int main(void) {
    const char *input = "{:name \"Alice\" :age 30 :languages [:clojure :rust]}";
    
    // Parse EDN string
    edn_result_t result = edn_parse(input, 0);
    
    if (result.error != EDN_OK) {
        fprintf(stderr, "Parse error at line %zu, column %zu: %s\n",
                result.error_line, result.error_column, result.error_message);
        return 1;
    }
    
    // Access the parsed map
    edn_value_t *map = result.value;
    printf("Parsed map with %zu entries\n", edn_map_count(map));
    
    // Look up a value by key
    edn_result_t key_result = edn_parse(":name", 0);
    edn_value_t *name_value = edn_map_lookup(map, key_result.value);
    
    if (name_value != NULL && edn_type(name_value) == EDN_TYPE_STRING) {
        size_t len;
        const char *name = edn_string_get(name_value, &len);
        printf("Name: %.*s\n", (int)len, name);
    }
    
    // Clean up - frees all allocated memory
    edn_free(key_result.value);
    edn_free(map);
    
    return 0;
}
```

**Output:**
```
Parsed map with 3 entries
Name: Alice
```

## Whitespace and Control Characters

EDN.C follows Clojure's exact behavior for whitespace and control character handling:

### Whitespace Characters

The following characters act as **whitespace delimiters** (separate tokens):

| Character | Hex  | Name                 | Common Use          |
|-----------|------|----------------------|---------------------|
| ` `       | 0x20 | Space                | Standard spacing    |
| `\t`      | 0x09 | Tab                  | Indentation         |
| `\n`      | 0x0A | Line Feed (LF)       | Unix line ending    |
| `\r`      | 0x0D | Carriage Return (CR) | Windows line ending |
| `\f`      | 0x0C | Form Feed            | Page break          |
| `\v`      | 0x0B | Vertical Tab         | Vertical spacing    |
| `,`       | 0x2C | Comma                | Optional separator  |
| FS        | 0x1C | File Separator       | Data separation     |
| GS        | 0x1D | Group Separator      | Data separation     |
| RS        | 0x1E | Record Separator     | Data separation     |
| US        | 0x1F | Unit Separator       | Data separation     |

**Examples:**
```c
// All of these parse as vectors with 3 elements:
edn_parse("[1 2 3]", 0);          // spaces
edn_parse("[1,2,3]", 0);          // commas
edn_parse("[1\t2\n3]", 0);        // tabs and newlines
edn_parse("[1\f2\x1C3]", 0);      // formfeed and file separator
```

### Control Characters in Identifiers

Control characters `0x00-0x1F` (except whitespace delimiters) are **valid in identifiers** (symbols and keywords):

**Valid identifier characters:**
- `0x00` - `0x08`: NUL, SOH, STX, ETX, EOT, ENQ, ACK, BEL, Backspace
- `0x0E` - `0x1B`: Shift Out through Escape

**Examples:**
```c
// Backspace in symbol - valid!
edn_result_t r = edn_parse("[\bfoo]", 0);  // 1-element vector
edn_vector_count(r.value);  // Returns 1
edn_free(r.value);

// Control characters in middle of identifier
const char input[] = {'[', 'f', 'o', 'o', 0x08, 'b', 'a', 'r', ']', 0};
r = edn_parse(input, sizeof(input) - 1);
edn_vector_count(r.value);  // Returns 1 (symbol: "foo\bbar")
edn_free(r.value);

// Versus whitespace - separates into 2 elements
edn_result_t r2 = edn_parse("[foo\tbar]", 0);  // Tab is whitespace
edn_vector_count(r2.value);  // Returns 2 (symbols: "foo" and "bar")
edn_free(r2.value);
```

**Note on null bytes (`0x00`):** When using string literals with `strlen()`, null bytes will truncate the string. Always pass explicit length for data containing null bytes:

```c
const char data[] = {'[', 'a', 0x00, 'b', ']', 0};
edn_result_t r = edn_parse(data, 5);  // Pass exact length: 5 bytes (excluding terminator)
```

## API Reference

### Core Functions

#### `edn_parse()`

Parse EDN from a UTF-8 string.

```c
edn_result_t edn_parse(const char *input, size_t length);
```

**Parameters:**
- `input`: UTF-8 encoded string containing EDN data (must remain valid for zero-copy strings)
- `length`: Length of input in bytes, or `0` to use `strlen(input)`

**Returns:** `edn_result_t` containing:
- `value`: Parsed EDN value (NULL on error)
- `error`: Error code (`EDN_OK` on success)
- `error_line`, `error_column`: Error location (1-indexed)
- `error_message`: Human-readable error description

**Important:** The returned value must be freed with `edn_free()`.

#### `edn_free()`

Free an EDN value and all associated memory.

```c
void edn_free(edn_value_t *value);
```

**Parameters:**
- `value`: Value to free (may be NULL)

**Note:** This frees the entire value tree. Do not call `free()` on individual values.

#### `edn_type()`

Get the type of an EDN value.

```c
edn_type_t edn_type(const edn_value_t *value);
```

**Returns:** One of:
- `EDN_TYPE_NIL`
- `EDN_TYPE_BOOL`
- `EDN_TYPE_INT` (int64_t)
- `EDN_TYPE_BIGINT` (arbitrary precision)
- `EDN_TYPE_FLOAT` (double)
- `EDN_TYPE_CHARACTER` (Unicode codepoint)
- `EDN_TYPE_STRING`
- `EDN_TYPE_SYMBOL`
- `EDN_TYPE_KEYWORD`
- `EDN_TYPE_LIST`
- `EDN_TYPE_VECTOR`
- `EDN_TYPE_MAP`
- `EDN_TYPE_SET`
- `EDN_TYPE_TAGGED`

### Type System

#### Error Codes

```c
typedef enum {
    EDN_OK = 0,                    // Success
    EDN_ERROR_INVALID_SYNTAX,      // Syntax error
    EDN_ERROR_UNEXPECTED_EOF,      // Unexpected end of input
    EDN_ERROR_OUT_OF_MEMORY,       // Allocation failure
    EDN_ERROR_INVALID_UTF8,        // Invalid UTF-8 sequence
    EDN_ERROR_INVALID_NUMBER,      // Malformed number
    EDN_ERROR_INVALID_STRING,      // Malformed string
    EDN_ERROR_INVALID_ESCAPE,      // Invalid escape sequence
    EDN_ERROR_UNMATCHED_DELIMITER, // Mismatched brackets
    EDN_ERROR_UNKNOWN_TAG,         // Unregistered tag (with ERROR mode)
    EDN_ERROR_DUPLICATE_KEY,       // Map has duplicate keys
    EDN_ERROR_DUPLICATE_ELEMENT    // Set has duplicate elements
} edn_error_t;
```

### Scalar Types

#### Strings

```c
const char *edn_string_get(const edn_value_t *value, size_t *length);
```

Get UTF-8 string data. Returns NULL if value is not a string.

**Lazy decoding:** For strings without escapes, returns a pointer into the original input (zero-copy). For strings with escapes (`\n`, `\t`, `\"`, etc.), decodes and caches the result on first call.

**Example:**
```c
edn_result_t r = edn_parse("\"Hello, world!\"", 0);
size_t len;
const char *str = edn_string_get(r.value, &len);
printf("%.*s\n", (int)len, str);
edn_free(r.value);
```

#### Integers

```c
bool edn_int64_get(const edn_value_t *value, int64_t *out);
```

Get int64_t value. Returns `true` if value is `EDN_TYPE_INT`, `false` otherwise.

**Example:**
```c
edn_result_t r = edn_parse("42", 0);
int64_t num;
if (edn_int64_get(r.value, &num)) {
    printf("Number: %lld\n", (long long)num);
}
edn_free(r.value);
```

#### Big Integers

```c
const char *edn_bigint_get(const edn_value_t *value, size_t *length,
                           bool *negative, uint8_t *radix);
```

Get big integer digit string for use with external libraries (GMP, OpenSSL BIGNUM, etc.).

**Parameters:**
- `value`: EDN big integer value
- `length`: Output for digit string length (may be NULL)
- `negative`: Output for sign flag (may be NULL)
- `radix`: Output for number base: 10, 16, 8, or 2 (may be NULL)

**Returns:** Digit string, or NULL if not a big integer.

**Example:**
```c
edn_result_t r = edn_parse("999999999999999999999999999", 0);
size_t len;
bool neg;
uint8_t radix;
const char *digits = edn_bigint_get(r.value, &len, &neg, &radix);
if (digits) {
    printf("%s%.*s (base %d)\n", neg ? "-" : "", (int)len, digits, radix);
}
edn_free(r.value);
```

#### Floating Point

```c
bool edn_double_get(const edn_value_t *value, double *out);
```

Get double value. Returns `true` if value is `EDN_TYPE_FLOAT`, `false` otherwise.

```c
bool edn_number_as_double(const edn_value_t *value, double *out);
```

Convert any numeric type (INT, BIGINT, FLOAT) to double. May lose precision for large big integers.

**Example:**
```c
edn_result_t r = edn_parse("3.14159", 0);
double num;
if (edn_double_get(r.value, &num)) {
    printf("Pi: %.5f\n", num);
}
edn_free(r.value);
```

#### Characters

```c
bool edn_character_get(const edn_value_t *value, uint32_t *out);
```

Get Unicode codepoint. Returns `true` if value is `EDN_TYPE_CHARACTER`, `false` otherwise.

**Example:**
```c
// Named characters: \newline, \tab, \space, \return
edn_result_t r1 = edn_parse("\\newline", 0);
uint32_t cp1;
edn_character_get(r1.value, &cp1);  // cp1 = 0x0A

// Unicode: \uXXXX or literal character
edn_result_t r2 = edn_parse("\\u03B1", 0);  // Greek alpha
uint32_t cp2;
edn_character_get(r2.value, &cp2);  // cp2 = 0x03B1

edn_free(r1.value);
edn_free(r2.value);
```

#### Symbols

```c
bool edn_symbol_get(const edn_value_t *value,
                    const char **namespace, size_t *ns_length,
                    const char **name, size_t *name_length);
```

Get symbol components. Returns `true` if value is `EDN_TYPE_SYMBOL`, `false` otherwise.

**Example:**
```c
// Simple symbol
edn_result_t r1 = edn_parse("foo", 0);
const char *name;
size_t name_len;
edn_symbol_get(r1.value, NULL, NULL, &name, &name_len);
printf("Symbol: %.*s\n", (int)name_len, name);

// Namespaced symbol
edn_result_t r2 = edn_parse("clojure.core/map", 0);
const char *ns, *n;
size_t ns_len, n_len;
edn_symbol_get(r2.value, &ns, &ns_len, &n, &n_len);
printf("Symbol: %.*s/%.*s\n", (int)ns_len, ns, (int)n_len, n);

edn_free(r1.value);
edn_free(r2.value);
```

#### Keywords

```c
bool edn_keyword_get(const edn_value_t *value,
                     const char **namespace, size_t *ns_length,
                     const char **name, size_t *name_length);
```

Get keyword components. Returns `true` if value is `EDN_TYPE_KEYWORD`, `false` otherwise.

**Example:**
```c
edn_result_t r = edn_parse(":name", 0);
const char *name;
size_t name_len;
edn_keyword_get(r.value, NULL, NULL, &name, &name_len);
printf("Keyword: :%.*s\n", (int)name_len, name);
edn_free(r.value);
```

### Collections

#### Lists

Ordered sequences: `(1 2 3)`

```c
size_t edn_list_count(const edn_value_t *value);
edn_value_t *edn_list_get(const edn_value_t *value, size_t index);
```

**Example:**
```c
edn_result_t r = edn_parse("(1 2 3)", 0);
size_t count = edn_list_count(r.value);

for (size_t i = 0; i < count; i++) {
    edn_value_t *elem = edn_list_get(r.value, i);
    int64_t num;
    if (edn_int64_get(elem, &num)) {
        printf("%lld ", (long long)num);
    }
}
printf("\n");
edn_free(r.value);
```

#### Vectors

Indexed sequences: `[1 2 3]`

```c
size_t edn_vector_count(const edn_value_t *value);
edn_value_t *edn_vector_get(const edn_value_t *value, size_t index);
```

**Example:**
```c
edn_result_t r = edn_parse("[\"a\" \"b\" \"c\"]", 0);
size_t count = edn_vector_count(r.value);

for (size_t i = 0; i < count; i++) {
    edn_value_t *elem = edn_vector_get(r.value, i);
    size_t len;
    const char *str = edn_string_get(elem, &len);
    printf("[%zu] = %.*s\n", i, (int)len, str);
}
edn_free(r.value);
```

#### Sets

Unique elements: `#{:a :b :c}`

```c
size_t edn_set_count(const edn_value_t *value);
edn_value_t *edn_set_get(const edn_value_t *value, size_t index);
bool edn_set_contains(const edn_value_t *value, const edn_value_t *element);
```

**Note:** Sets reject duplicate elements during parsing. Iteration order is implementation-defined.

**Example:**
```c
edn_result_t r = edn_parse("#{:a :b :c}", 0);
printf("Set has %zu elements\n", edn_set_count(r.value));

edn_result_t key = edn_parse(":a", 0);
if (edn_set_contains(r.value, key.value)) {
    printf(":a is in set\n");
}

edn_free(key.value);
edn_free(r.value);
```

#### Maps

Key-value pairs: `{:foo 1 :bar 2}`

```c
size_t edn_map_count(const edn_value_t *value);
edn_value_t *edn_map_get_key(const edn_value_t *value, size_t index);
edn_value_t *edn_map_get_value(const edn_value_t *value, size_t index);
edn_value_t *edn_map_lookup(const edn_value_t *value, const edn_value_t *key);
bool edn_map_contains_key(const edn_value_t *value, const edn_value_t *key);
```

**Note:** Maps reject duplicate keys during parsing. Iteration order is implementation-defined.

**Example:**
```c
edn_result_t r = edn_parse("{:name \"Alice\" :age 30}", 0);

// Iterate over all entries
size_t count = edn_map_count(r.value);
for (size_t i = 0; i < count; i++) {
    edn_value_t *key = edn_map_get_key(r.value, i);
    edn_value_t *val = edn_map_get_value(r.value, i);
    
    const char *key_name;
    size_t key_len;
    edn_keyword_get(key, NULL, NULL, &key_name, &key_len);
    printf(":%.*s => ", (int)key_len, key_name);
    
    if (edn_type(val) == EDN_TYPE_STRING) {
        size_t val_len;
        const char *str = edn_string_get(val, &val_len);
        printf("\"%.*s\"\n", (int)val_len, str);
    } else if (edn_type(val) == EDN_TYPE_INT) {
        int64_t num;
        edn_int64_get(val, &num);
        printf("%lld\n", (long long)num);
    }
}

// Lookup by key
edn_result_t key = edn_parse(":name", 0);
edn_value_t *name = edn_map_lookup(r.value, key.value);
if (name != NULL) {
    size_t len;
    const char *str = edn_string_get(name, &len);
    printf("Name: %.*s\n", (int)len, str);
}

edn_free(key.value);
edn_free(r.value);
```

### Tagged Literals

Tagged literals provide extensibility: `#tag value`

#### Basic Tagged Literal Access

```c
bool edn_tagged_get(const edn_value_t *value,
                    const char **tag, size_t *tag_length,
                    edn_value_t **tagged_value);
```

**Example:**
```c
edn_result_t r = edn_parse("#inst \"2024-01-01T00:00:00Z\"", 0);

const char *tag;
size_t tag_len;
edn_value_t *wrapped;

if (edn_tagged_get(r.value, &tag, &tag_len, &wrapped)) {
    printf("Tag: %.*s\n", (int)tag_len, tag);
    
    size_t str_len;
    const char *str = edn_string_get(wrapped, &str_len);
    printf("Value: %.*s\n", (int)str_len, str);
}

edn_free(r.value);
```

### Custom Readers

Transform tagged literals during parsing with custom reader functions.

#### Reader Registry Functions

```c
// Create and destroy registry
edn_reader_registry_t *edn_reader_registry_create(void);
void edn_reader_registry_destroy(edn_reader_registry_t *registry);

// Register/unregister readers
bool edn_reader_register(edn_reader_registry_t *registry,
                         const char *tag, edn_reader_fn reader);
void edn_reader_unregister(edn_reader_registry_t *registry, const char *tag);
edn_reader_fn edn_reader_lookup(const edn_reader_registry_t *registry,
                                const char *tag);
```

#### Reader Function Type

```c
typedef edn_value_t *(*edn_reader_fn)(edn_value_t *value,
                                      edn_arena_t *arena,
                                      const char **error_message);
```

A reader function receives the wrapped value and transforms it into a new representation. On error, set `error_message` to a static string and return NULL.

#### Parse Options

```c
typedef struct {
    edn_reader_registry_t *reader_registry;  // Optional reader registry
    edn_default_reader_mode_t default_reader_mode;
} edn_parse_options_t;

edn_result_t edn_parse_with_options(const char *input, size_t length,
                                    const edn_parse_options_t *options);
```

**Default reader modes:**
- `EDN_DEFAULT_READER_PASSTHROUGH`: Return `EDN_TYPE_TAGGED` for unregistered tags (default)
- `EDN_DEFAULT_READER_UNWRAP`: Discard tag, return wrapped value
- `EDN_DEFAULT_READER_ERROR`: Fail with `EDN_ERROR_UNKNOWN_TAG`

#### Reader Example

```c
#include "edn.h"
#include "../src/edn_internal.h"  // For edn_arena_alloc

// Reader that uppercases keywords
static edn_value_t *upper_reader(edn_value_t *value, edn_arena_t *arena,
                                 const char **error_message) {
    if (edn_type(value) != EDN_TYPE_KEYWORD) {
        *error_message = "#upper requires keyword";
        return NULL;
    }

    const char *name;
    size_t name_len;
    edn_keyword_get(value, NULL, NULL, &name, &name_len);

    // Allocate uppercase name in arena
    char *upper = (char *)edn_arena_alloc(arena, name_len + 1);
    if (!upper) {
        *error_message = "Out of memory";
        return NULL;
    }

    for (size_t i = 0; i < name_len; i++) {
        char c = name[i];
        upper[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    upper[name_len] = '\0';

    // Create new keyword value
    edn_value_t *result = edn_arena_alloc_value(arena);
    if (!result) {
        *error_message = "Out of memory";
        return NULL;
    }

    result->type = EDN_TYPE_KEYWORD;
    result->as.keyword.name = upper;
    result->as.keyword.name_length = name_len;
    result->as.keyword.namespace = NULL;
    result->as.keyword.ns_length = 0;
    result->arena = arena;

    return result;
}

int main(void) {
    // Create registry and register reader
    edn_reader_registry_t *registry = edn_reader_registry_create();
    edn_reader_register(registry, "upper", upper_reader);

    // Parse with custom reader
    edn_parse_options_t opts = {
        .reader_registry = registry,
        .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH
    };

    edn_result_t r = edn_parse_with_options("#upper :hello", 0, &opts);
    if (r.error == EDN_OK) {
        const char *name;
        size_t len;
        edn_keyword_get(r.value, NULL, NULL, &name, &len);
        printf(":%.*s\n", (int)len, name);  // Output: :HELLO
    }

    edn_free(r.value);
    edn_reader_registry_destroy(registry);
    return 0;
}
```

See `examples/reader.c` for more complete examples including timestamp conversion, vector extraction, and namespaced tags.

### Map Namespace Syntax

EDN.C supports Clojure's map namespace syntax extension, which allows you to specify a namespace that gets automatically applied to all non-namespaced keyword keys in a map.

**Syntax:** `#:namespace{:key1 val1 :key2 val2}`

**Example:**
```c
edn_result_t result = edn_parse("#:person{:name \"Alice\" :age 30}", 0);
// Equivalent to: {:person/name "Alice" :person/age 30}

if (result.error == EDN_OK) {
    edn_value_t* map = result.value;
    
    // Keys are automatically namespaced
    edn_value_t* key1 = edn_map_get_key(map, 0);
    const char *ns, *name;
    size_t ns_len, name_len;
    edn_keyword_get(key1, &ns, &ns_len, &name, &name_len);
    
    printf(":%.*s/%.*s\n", (int)ns_len, ns, (int)name_len, name);
    // Output: :person/name
    
    edn_free(map);
}
```

**Rules:**
- Both keyword and symbol keys without an existing namespace are transformed
- Keys with existing namespaces are preserved: `#:foo{:x 1 :bar/y 2}` → `{:foo/x 1 :bar/y 2}`
- Symbol keys are also namespaced: `#:foo{x 1 y 2}` → `{foo/x 1 foo/y 2}`
- Mixed keys work correctly: `#:foo{x 1 :y 2}` → `{foo/x 1 :foo/y 2}`
- Non-keyword/non-symbol keys are not transformed: `#:foo{"x" 1 :y 2}` → `{"x" 1 :foo/y 2}`
- The namespace keyword cannot itself have a namespace

**Build Configuration:**

This feature is disabled by default. To enable it:
```bash
make MAP_NAMESPACE_SYNTAX=1
```

When disabled (default), `#:foo{...}` will fail to parse.

See `examples/example_namespaced_map.c` for more details.

### Extended Character Literals

EDN.C supports optional extended character literal features that are disabled by default for strict EDN compliance.

**Extended named characters:**
- `\formfeed` - Form feed control character (U+000C)
- `\backspace` - Backspace control character (U+0008)

**Octal escape sequences (Clojure-compatible):**
- `\oN` - Where N is 1-3 octal digits (0-7)
- If `\o` is followed by any digit, attempts octal parsing
- Digits 8 or 9 cause "Invalid octal escape sequence in character literal" error
- Examples:
  - `\o7` - Bell character (U+0007)
  - `\o12` - Line feed (U+000A)
  - `\o101` - Uppercase 'A' (U+0041)
  - `\o377` - Maximum value (U+00FF / 255)
  - `\o` alone - Parses as character 'o'
  - `\o8` - Error: Invalid octal character

**Example:**
```c
edn_result_t result = edn_parse("\\formfeed", 0);
if (result.error == EDN_OK) {
    uint32_t codepoint;
    edn_character_get(result.value, &codepoint);
    printf("U+%04X\n", codepoint);  // Output: U+000C
    edn_free(result.value);
}

// Octal escapes
result = edn_parse("[\\o101 \\o102 \\o103]", 0);
// Parses as vector ['A', 'B', 'C']
```

**Build Configuration:**

This feature is disabled by default. To enable it:

**Make:**
```bash
make EXTENDED_CHARACTERS=1
```

**CMake:**
```bash
cmake -DEDN_ENABLE_EXTENDED_CHARACTERS=ON ..
make
```

When disabled (default):
- `\formfeed` and `\backspace` will fail to parse
- `\oNNN` will fail to parse
- Standard character literals still work: `\newline`, `\tab`, `\space`, `\return`, `\uXXXX`, etc.

See `examples/example_extended_characters.c` for more details.

### Metadata

EDN.C supports Clojure-style metadata syntax, which allows attaching metadata maps to values.

**Syntax variants:**

1. **Map metadata**: `^{:key val} form` - metadata is the map itself
2. **Keyword shorthand**: `^:keyword form` - expands to `{:keyword true}`
3. **String tag**: `^"string" form` - expands to `{:tag "string"}`
4. **Symbol tag**: `^symbol form` - expands to `{:tag symbol}`
5. **Vector param-tags**: `^[type1 type2] form` - expands to `{:param-tags [type1 type2]}`

**Chaining**: Multiple metadata can be chained: `^meta1 ^meta2 form` - metadata maps are merged from right to left.

**Example:**
```c
#include "edn.h"
#include <stdio.h>

int main(void) {
    // Parse with keyword shorthand
    edn_result_t result = edn_parse("^:private my-var", 0);

    if (result.error == EDN_OK) {
        // Check if value has metadata
        if (edn_value_has_meta(result.value)) {
            edn_value_t* meta = edn_value_meta(result.value);

            // Metadata is always a map
            printf("Metadata entries: %zu\n", edn_map_count(meta));

            // Look up specific metadata key
            edn_result_t key = edn_parse(":private", 0);
            edn_value_t* val = edn_map_lookup(meta, key.value);
            // val will be boolean true

            edn_free(key.value);
        }

        edn_free(result.value);
    }

    return 0;
}
```

**More examples:**
```c
// Map metadata
edn_parse("^{:doc \"A function\" :test true} my-fn", 0);

// String tag
edn_parse("^\"String\" [1 2 3]", 0);
// Expands to: ^{:tag "String"} [1 2 3]

// Symbol tag
edn_parse("^Vector [1 2 3]", 0);
// Expands to: ^{:tag Vector} [1 2 3]

// Vector param-tags
edn_parse("^[String long _] my-fn", 0);
// Expands to: ^{:param-tags [String long _]} my-fn

// Chained metadata
edn_parse("^:private ^:dynamic ^{:doc \"My var\"} x", 0);
// All metadata merged into one map
```

**Supported value types:**

Metadata can only be attached to:
- Collections: lists, vectors, maps, sets
- Tagged literals
- Symbols

**Note:** Metadata cannot be attached to scalar values (nil, booleans, numbers, strings, keywords).

**API:**
```c
// Check if value has metadata
bool edn_value_has_meta(const edn_value_t* value);

// Get metadata map (returns NULL if no metadata)
edn_value_t* edn_value_meta(const edn_value_t* value);
```

**Build Configuration:**

This feature is disabled by default. To enable it:

**Make:**
```bash
make METADATA=1
```

**CMake:**
```bash
cmake -DEDN_ENABLE_METADATA=ON ..
make
```

When disabled (default):
- `^` is treated as a valid character in identifiers (symbols/keywords)
- `^test` parses as a symbol named "^test"
- Metadata API functions are not available

**Note:** Metadata is a Clojure language feature, not part of the official EDN specification. It's provided here for compatibility with Clojure's reader.

See `examples/example_metadata.c` for more details.

### Text Blocks

**Experimental feature** that adds Java-style multi-line text blocks with automatic indentation stripping to EDN. Requires `EDN_ENABLE_TEXT_BLOCKS` compilation flag (disabled by default).

Text blocks start with three double quotes followed by a newline (`"""\n`) and end with three double quotes (`"""`):

```edn
{:query """
    SELECT *
      FROM users
    WHERE age > 21
    """}
```

**Features:**
- Automatic indentation stripping (common leading whitespace removed)
- Closing `"""` position determines base indentation level
- Closing on own line adds trailing newline, on same line doesn't
- Trailing whitespace automatically removed from each line
- Minimal escaping: only `\"""` to include literal triple quotes
- Returns standard EDN string (no special type needed)

**Example:**
```c
#include "edn.h"
#include <stdio.h>

int main(void) {
    const char* input =
        "{:sql \"\"\"\n"
        "       SELECT * FROM users\n"
        "       WHERE age > 21\n"
        "       ORDER BY name\n"
        "       \"\"\""}";

    edn_result_t result = edn_parse(input, 0);

    if (result.error == EDN_OK) {
        edn_result_t key = edn_parse(":sql", 0);
        edn_value_t* val = edn_map_lookup(result.value, key.value);

        // Text block returns a regular string with indentation stripped
        size_t len;
        const char* sql = edn_string_get(val, &len);
        printf("%s\n", sql);
        // Output:
        // SELECT * FROM users
        // WHERE age > 21
        // ORDER BY name

        edn_free(key.value);
        edn_free(result.value);
    }

    return 0;
}
```

**Indentation Rules (Java JEP 378)**:
1. Find minimum indentation across all non-blank lines
2. Closing `"""` position also determines indentation
3. Strip that amount from each line
4. If closing `"""` is on its own line, add trailing `\n`

```edn
{:foo """
        line1
       line2
      line3
      """}
```
Result: `{:foo "  line1\n line2\nline3\n"}` (min indent 6, trailing newline added)

```edn
{:foo """
        line1
       line2
      line3"""}
```
Result: `{:foo "  line1\n line2\nline3"}` (min indent 6, no trailing newline)

**Build Configuration:**

This feature is disabled by default. To enable it:

**Make:**
```bash
make TEXT_BLOCKS=1
```

**CMake:**
```bash
cmake -DEDN_ENABLE_TEXT_BLOCKS=ON ..
make
```

When disabled (default):
- `"""\n` pattern is parsed as a regular string
- No automatic indentation processing

**Note:** Text blocks are an experimental feature and not part of the official EDN specification.

See `examples/example_text_block.c` for more examples.

## Examples

### Complete Working Example

```c
#include "edn.h"
#include <stdio.h>
#include <string.h>

void print_value(edn_value_t *val, int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (edn_type(val)) {
        case EDN_TYPE_NIL:
            printf("nil\n");
            break;
        case EDN_TYPE_BOOL:
            // Note: Use internal API or add edn_bool_get() to public API
            printf("bool\n");
            break;
        case EDN_TYPE_INT: {
            int64_t num;
            edn_int64_get(val, &num);
            printf("%lld\n", (long long)num);
            break;
        }
        case EDN_TYPE_FLOAT: {
            double num;
            edn_double_get(val, &num);
            printf("%g\n", num);
            break;
        }
        case EDN_TYPE_STRING: {
            size_t len;
            const char *str = edn_string_get(val, &len);
            printf("\"%.*s\"\n", (int)len, str);
            break;
        }
        case EDN_TYPE_KEYWORD: {
            const char *name;
            size_t len;
            edn_keyword_get(val, NULL, NULL, &name, &len);
            printf(":%.*s\n", (int)len, name);
            break;
        }
        case EDN_TYPE_VECTOR: {
            printf("[\n");
            size_t count = edn_vector_count(val);
            for (size_t i = 0; i < count; i++) {
                print_value(edn_vector_get(val, i), indent + 1);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("]\n");
            break;
        }
        case EDN_TYPE_MAP: {
            printf("{\n");
            size_t count = edn_map_count(val);
            for (size_t i = 0; i < count; i++) {
                print_value(edn_map_get_key(val, i), indent + 1);
                print_value(edn_map_get_value(val, i), indent + 1);
            }
            for (int i = 0; i < indent; i++) printf("  ");
            printf("}\n");
            break;
        }
        default:
            printf("<other type>\n");
    }
}

int main(void) {
    const char *edn = 
        "{:users [{:name \"Alice\" :age 30}\n"
        "         {:name \"Bob\" :age 25}]\n"
        " :status :active}";
    
    edn_result_t result = edn_parse(edn, 0);
    
    if (result.error != EDN_OK) {
        fprintf(stderr, "Error at %zu:%zu - %s\n",
                result.error_line, result.error_column, result.error_message);
        return 1;
    }
    
    printf("Parsed EDN structure:\n");
    print_value(result.value, 0);
    
    edn_free(result.value);
    return 0;
}
```

More examples available in the `examples/` directory.

## Building

### Standard Build (Unix/macOS/Linux)

```bash
# Build library (libedn.a)
make

# Build and run all tests
make test

# Build and run single test
make test/test_numbers
./test/test_numbers

# Build with debug symbols and sanitizers (ASAN/UBSAN)
make DEBUG=1

# Run benchmarks
make bench          # Quick benchmark
make bench-all      # All benchmarks

# Clean build artifacts
make clean

# Show build configuration
make info
```

### Windows Build

EDN.C fully supports Windows with MSVC, MinGW, and Clang. Choose your preferred method:

**Quick Start (CMake - Recommended):**
```powershell
# Using the provided build script
.\build.bat

# Or with PowerShell
.\build.ps1 -Test
```

**Manual CMake Build:**
```powershell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
ctest -C Release
```

**Visual Studio:**
- Open `CMakeLists.txt` in Visual Studio 2019+
- Build → Build All (Ctrl+Shift+B)

For detailed Windows build instructions, see **[docs/WINDOWS.md](docs/WINDOWS.md)**.

### Build Options

- `DEBUG=1` - Enable debug symbols, ASAN, and UBSAN
- `SANITIZE=1` - Enable sanitizers without full debug build
- `OPTIMIZE=0` - Disable optimizations
- `VERBOSE=1` - Show full compiler commands

### Code Formatting

```bash
# Auto-format all C files (run before committing!)
make format

# Check if formatting is needed without modifying
make format-check
```

### LSP Support

```bash
# Generate compile_commands.json for LSP (requires bear or compiledb)
make compile-commands
```

## Performance

EDN.C is designed for high performance with several optimizations:

- **SIMD acceleration**: Vectorized whitespace scanning, comment skipping, and identifier parsing
- **Zero-copy strings**: String values without escapes point directly into input buffer
- **Lazy decoding**: Escape sequences decoded only when accessed via `edn_string_get()`
- **Arena allocation**: Single bulk allocation and deallocation eliminates malloc overhead
- **Efficient collections**: Maps and sets use sorted arrays with binary search

**Typical performance on Apple M1** (from microbenchmarks):
- Whitespace skipping: 1-5 ns per operation
- Number parsing: 10-30 ns per number
- String parsing: 15-50 ns per string
- Identifier parsing: 10-25 ns per symbol/keyword

See `bench/` directory for detailed benchmarking tools and results.

## Project Status

**Current version**: 1.0.0 (Release Candidate)

✅ **Complete features:**
- Full EDN specification support
- All scalar types (nil, bool, int, bigint, float, character, string, symbol, keyword)
- All collection types (lists, vectors, maps, sets)
- Tagged literals with custom reader support
- Discard forms `#_`
- Comments (`;` line comments)
- Duplicate detection for maps/sets
- Deep structural equality
- SIMD optimization for ARM64 (NEON) and x86_64 (SSE4.2)
- Cross-platform support (Unix, macOS, Linux, Windows)
- Optional Clojure extensions (disabled by default):
  - Map namespace syntax `#:ns{...}`
  - Extended character literals (`\formfeed`, `\backspace`, `\oNNN`)
  - Metadata `^{...}` syntax

✅ **Testing:**
- 340+ tests across 24 test suites
- Memory safety verified with ASAN/UBSAN
- Edge case coverage (empty collections, deeply nested structures, Unicode, etc.)

📋 **Roadmap:**
- Performance profiling and further optimization
- Extended documentation and tutorials
- Public helper functions (`edn_bool_get()`, `edn_nil_p()`, etc.)
- EOF configuration
- Streaming/Incremental parsing
- Additional SIMD Platform Support:
  - 32-bit x86 (i386/i686) `__i386__, _M_IX86. mostly the same as x86-64`
  - 32-bit ARM (ARMv7) `__arm__, _M_ARM. mostly the same as ARM64 NEON`
  - WebAssembly SIMD128 `__wasm__, __wasm32__, __wasm64__. uses <wasm_simd128.h>`
  - RISC-V Vector Extension (RVV) `__riscv, __riscv_vector. uses <riscv_vector.h>`
- Extra features:
  - octal number notation ("0713" => 459)
  - hexadecimal number notation ("0xFF" => 255)
  - radix number notation ("2r1111" => 15, "36rabcxyz" => 623741435)
  - float trailing dot ("1." => 1.0, "1.M" => 1.0M)
  - ratio numbers ("1/2" => 0.5)
  - octal escape ("\"\\176\"" => "~")
- Extra-extra features (EXPERIMENTAL!):
  - support text blocks for raw multiline strings

## Contributing

Contributions are welcome! Please:

1. Run `make format` before committing (auto-formats with clang-format)
2. Ensure all tests pass with `make test`
3. Add tests for new features
4. Follow the existing code style (K&R, 4 spaces, C11, see `.clang-format`)

## Documentation

- [docs/DESIGN.md](docs/DESIGN.md) - Architecture and design philosophy
- [docs/READER_DESIGN.md](docs/READER_DESIGN.md) - Custom reader implementation

## License

MIT License

Copyright (c) 2025 [Kirill Chernyshov]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgments

- EDN specification: https://github.com/edn-format/edn
- Inspired by Clojure's EDN implementation
- SWAR (SIMD Within A Register) digit parsing technique from [simdjson](https://github.com/simdjson/simdjson)
- Fast double parsing using Clinger's algorithm (William D. Clinger, 1990) - "How to Read Floating Point Numbers Accurately"
- SIMD optimization patterns from high-performance JSON parsers (simdjson, yyjson)

---

**Questions or issues?** Please open an issue on GitHub or consult the documentation in the `docs/` directory.
