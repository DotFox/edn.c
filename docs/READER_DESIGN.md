# Reader Design for Tagged Literals

## Problem Statement

Currently, the EDN parser returns tagged literals as raw `EDN_TYPE_TAGGED` values containing:
- A tag string (e.g., "inst", "uuid", "myapp/custom")
- The wrapped EDN value

This requires the caller to manually inspect each tagged literal and convert it to the appropriate target format. However, the EDN specification defines **readers** as functions that transform tagged literals during parsing.

## Goals

1. **Caller-provided readers**: Allow callers to register custom reader functions for specific tags
2. **Default fallback**: Provide sensible default behavior when no reader is registered
3. **Zero overhead when unused**: Readers should be optional - existing API remains unchanged
4. **Type safety**: Reader functions should be type-safe and return proper EDN values
5. **Error handling**: Readers can fail gracefully with descriptive error messages
6. **Performance**: Minimal overhead for tag lookup and function dispatch

## Design

### Reader Function Signature

```c
/**
 * Reader function for tagged literals.
 * 
 * Transforms a tagged literal value into its target representation.
 * 
 * @param tag The tag symbol (e.g., "inst", "uuid", "myapp/custom")
 * @param tag_length Length of the tag string
 * @param value The wrapped EDN value (e.g., string, map, vector)
 * @param arena Arena allocator for creating new values
 * @param error_message Output parameter for error message (set to NULL for success)
 * @return Transformed EDN value, or NULL on error
 * 
 * The returned value must be allocated from the provided arena.
 * On error, set error_message to a static string and return NULL.
 */
typedef edn_value_t* (*edn_reader_fn)(
    const char* tag,
    size_t tag_length,
    edn_value_t* value,
    edn_arena_t* arena,
    const char** error_message
);
```

### Reader Registry

```c
/**
 * Reader registry for tagged literals.
 * 
 * Maps tag names to reader functions.
 */
typedef struct edn_reader_registry edn_reader_registry_t;

/**
 * Create a new reader registry.
 * 
 * @return New registry, or NULL on allocation failure
 */
edn_reader_registry_t* edn_reader_registry_create(void);

/**
 * Destroy a reader registry and free all associated memory.
 * 
 * @param registry Registry to destroy (may be NULL)
 */
void edn_reader_registry_destroy(edn_reader_registry_t* registry);

/**
 * Register a reader function for a tag.
 * 
 * If a reader is already registered for this tag, it will be replaced.
 * 
 * @param registry Reader registry
 * @param tag Tag name (e.g., "inst", "uuid", "myapp/custom")
 * @param tag_length Length of tag name (or 0 to use strlen)
 * @param reader Reader function
 * @return true on success, false on allocation failure
 */
bool edn_reader_register(
    edn_reader_registry_t* registry,
    const char* tag,
    size_t tag_length,
    edn_reader_fn reader
);

/**
 * Unregister a reader function for a tag.
 * 
 * @param registry Reader registry
 * @param tag Tag name
 * @param tag_length Length of tag name (or 0 to use strlen)
 */
void edn_reader_unregister(
    edn_reader_registry_t* registry,
    const char* tag,
    size_t tag_length
);

/**
 * Look up a reader function for a tag.
 * 
 * @param registry Reader registry
 * @param tag Tag name
 * @param tag_length Length of tag name
 * @return Reader function, or NULL if not found
 */
edn_reader_fn edn_reader_lookup(
    const edn_reader_registry_t* registry,
    const char* tag,
    size_t tag_length
);
```

### Parse Options

```c
/**
 * Default fallback behavior for unregistered tags.
 */
typedef enum {
    /**
     * Return EDN_TYPE_TAGGED value (current behavior).
     * Caller must handle conversion manually.
     */
    EDN_DEFAULT_READER_PASSTHROUGH,
    
    /**
     * Return the wrapped value, discarding the tag.
     * Useful for ignoring unknown tags during parsing.
     */
    EDN_DEFAULT_READER_UNWRAP,
    
    /**
     * Fail with EDN_ERROR_UNKNOWN_TAG error.
     * Useful for strict validation.
     */
    EDN_DEFAULT_READER_ERROR
} edn_default_reader_mode_t;

/**
 * Parse options for configuring parser behavior.
 */
typedef struct {
    /**
     * Optional reader registry for tagged literals.
     * If NULL, all tags use default fallback.
     */
    edn_reader_registry_t* reader_registry;
    
    /**
     * Default behavior for tags without registered readers.
     */
    edn_default_reader_mode_t default_reader_mode;
} edn_parse_options_t;

/**
 * Parse EDN with custom options.
 * 
 * @param input UTF-8 encoded string containing EDN data
 * @param length Length of input in bytes (or 0 to use strlen)
 * @param options Parse options (or NULL for defaults)
 * @return Parse result containing value or error information
 */
edn_result_t edn_parse_with_options(
    const char* input,
    size_t length,
    const edn_parse_options_t* options
);
```

### Default Behavior

When `options` is NULL or `reader_registry` is NULL:
- Behavior is identical to current implementation
- All tagged literals return `EDN_TYPE_TAGGED` values
- No reader functions are invoked
- **Zero overhead** - no lookup, no dispatch

When `reader_registry` is provided:
1. Parser encounters `#tag value`
2. Look up reader function for "tag" in registry
3. If found: invoke reader function with value
4. If not found: use `default_reader_mode`:
   - `PASSTHROUGH`: return `EDN_TYPE_TAGGED` (current behavior)
   - `UNWRAP`: return `value` directly
   - `ERROR`: fail with `EDN_ERROR_UNKNOWN_TAG`

## Implementation Strategy

### Data Structures

```c
/* Reader registry entry */
typedef struct edn_reader_entry {
    char* tag;                    /* Owned copy of tag name */
    size_t tag_length;            /* Length of tag */
    edn_reader_fn reader;         /* Reader function */
    struct edn_reader_entry* next; /* For hash table chaining */
} edn_reader_entry_t;

/* Reader registry */
struct edn_reader_registry {
    edn_reader_entry_t** buckets; /* Hash table */
    size_t bucket_count;          /* Number of buckets */
    size_t entry_count;           /* Total entries */
};
```

### Hash Table

- Use simple FNV-1a hash for tag lookup
- Initial size: 16 buckets (sufficient for most use cases)
- No dynamic resizing needed (most applications register < 10 tags)
- Chaining for collision resolution

### Parser Integration

Modify `edn_parse_tagged()` in `src/tagged.c`:

```c
edn_value_t* edn_parse_tagged(edn_parser_t* parser) {
    /* ... existing tag parsing ... */
    
    /* Check if reader registry is provided */
    if (parser->reader_registry != NULL) {
        edn_reader_fn reader = edn_reader_lookup(
            parser->reader_registry,
            tag_string,
            tag_length
        );
        
        if (reader != NULL) {
            /* Invoke custom reader */
            const char* error_msg = NULL;
            edn_value_t* result = reader(
                tag_string,
                tag_length,
                value,
                parser->arena,
                &error_msg
            );
            
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
    /* ... existing code ... */
}
```

### Parser State Extension

Add to `edn_parser_t` in `src/edn_internal.h`:

```c
typedef struct {
    /* ... existing fields ... */
    
    /* Reader configuration (optional) */
    edn_reader_registry_t* reader_registry;
    edn_default_reader_mode_t default_reader_mode;
} edn_parser_t;
```

## Example Usage

### Basic Reader Registration

```c
/* Create registry */
edn_reader_registry_t* registry = edn_reader_registry_create();

/* Register instant reader */
edn_reader_register(registry, "inst", 4, my_inst_reader);

/* Parse with readers */
edn_parse_options_t opts = {
    .reader_registry = registry,
    .default_reader_mode = EDN_DEFAULT_READER_PASSTHROUGH
};

edn_result_t result = edn_parse_with_options("#inst \"2024-01-01T00:00:00Z\"", 0, &opts);
/* result.value is now the transformed instant value, not EDN_TYPE_TAGGED */

/* Cleanup */
edn_free(result.value);
edn_reader_registry_destroy(registry);
```

### Custom Reader Implementation

```c
/* Reader that converts #inst strings to Unix timestamp integers */
edn_value_t* inst_reader(
    const char* tag,
    size_t tag_length,
    edn_value_t* value,
    edn_arena_t* arena,
    const char** error_message
) {
    /* Validate input is a string */
    if (edn_type(value) != EDN_TYPE_STRING) {
        *error_message = "#inst requires string value";
        return NULL;
    }
    
    /* Get string content */
    size_t str_len;
    const char* str = edn_string_get(value, &str_len);
    
    /* Parse ISO8601 timestamp (simplified) */
    int64_t timestamp = parse_iso8601(str, str_len);
    if (timestamp < 0) {
        *error_message = "Invalid ISO8601 timestamp";
        return NULL;
    }
    
    /* Allocate new integer value */
    edn_value_t* result = edn_arena_alloc_value(arena);
    if (result == NULL) {
        *error_message = "Out of memory";
        return NULL;
    }
    
    result->type = EDN_TYPE_INT;
    result->as.integer = timestamp;
    result->arena = arena;
    
    return result;
}
```

### Strict Mode (Fail on Unknown Tags)

```c
edn_reader_registry_t* registry = edn_reader_registry_create();
edn_reader_register(registry, "inst", 4, my_inst_reader);
edn_reader_register(registry, "uuid", 4, my_uuid_reader);

edn_parse_options_t opts = {
    .reader_registry = registry,
    .default_reader_mode = EDN_DEFAULT_READER_ERROR  /* Strict */
};

/* This will succeed */
edn_result_t r1 = edn_parse_with_options("#inst \"...\"", 0, &opts);

/* This will fail with EDN_ERROR_UNKNOWN_TAG */
edn_result_t r2 = edn_parse_with_options("#unknown 42", 0, &opts);
assert(r2.error == EDN_ERROR_UNKNOWN_TAG);
```

### Lenient Mode (Ignore Unknown Tags)

```c
edn_parse_options_t opts = {
    .reader_registry = registry,
    .default_reader_mode = EDN_DEFAULT_READER_UNWRAP  /* Lenient */
};

/* #unknown tag is ignored, returns integer 42 directly */
edn_result_t result = edn_parse_with_options("#unknown 42", 0, &opts);
assert(edn_type(result.value) == EDN_TYPE_INT);
```

## Performance Considerations

### Hash Table Lookup

- FNV-1a hash: ~10-20 CPU cycles
- Bucket lookup: ~1-2 memory accesses
- String comparison (on collision): ~5-10 cycles per comparison
- **Total overhead**: ~20-50 CPU cycles per tagged literal

For reference, parsing overhead:
- SIMD whitespace skip: ~5-15 ns
- Number parsing: ~10-50 ns
- String parsing: ~20-100 ns
- Tagged literal with reader: ~30-100 ns (acceptable)

### Memory Overhead

Registry structure (16 buckets):
- `edn_reader_registry_t`: 24 bytes (buckets ptr + counts)
- Buckets array: 16 × 8 = 128 bytes
- Per entry: ~40 bytes (tag copy + metadata + next ptr)
- **Total for 5 readers**: ~344 bytes (negligible)

### Optimization: Inline Common Tags

For maximum performance, we could provide pre-compiled readers for common tags:

```c
/* Built-in readers (optional) */
edn_reader_fn edn_reader_inst(void);    /* ISO8601 → int64 Unix timestamp */
edn_reader_fn edn_reader_uuid(void);    /* UUID string → binary format */

/* Quick setup helper */
void edn_reader_register_defaults(edn_reader_registry_t* registry) {
    edn_reader_register(registry, "inst", 4, edn_reader_inst());
    edn_reader_register(registry, "uuid", 4, edn_reader_uuid());
}
```

## Backward Compatibility

**Existing API is unchanged:**
- `edn_parse()` continues to work exactly as before
- All existing code compiles without modification
- `EDN_TYPE_TAGGED` values are still accessible via `edn_tagged_get()`

**New API is opt-in:**
- `edn_parse_with_options()` is only for applications that need readers
- Registry creation and management is explicit
- No performance impact when readers are not used

## Testing Strategy

### Unit Tests

1. **Registry operations** (`test/test_reader_registry.c`):
   - Create/destroy registry
   - Register/unregister readers
   - Lookup existing and missing readers
   - Hash collision handling
   - Memory cleanup

2. **Reader invocation** (`test/test_reader_parsing.c`):
   - Parse with registered reader
   - Reader transformation success
   - Reader error handling
   - Default fallback modes (passthrough, unwrap, error)
   - Nested tagged literals with readers

3. **Built-in readers** (`test/test_builtin_readers.c`):
   - `#inst` reader (if implemented)
   - `#uuid` reader (if implemented)
   - Edge cases and error conditions

4. **Integration** (`test/test_reader_integration.c`):
   - Multiple readers in single registry
   - Tagged literals in collections with readers
   - Mixed registered and unregistered tags
   - Performance benchmarks

### Benchmark Tests

Add to `bench/bench_reader.c`:
- Parsing with no readers (baseline)
- Parsing with 1 reader (common case)
- Parsing with 10 readers (heavy use case)
- Reader lookup performance
- Tagged literal transformation overhead

## Implementation Checklist

- [ ] Define `edn_reader_fn` typedef in `include/edn.h`
- [ ] Define `edn_reader_registry_t` opaque type in `include/edn.h`
- [ ] Define `edn_default_reader_mode_t` enum in `include/edn.h`
- [ ] Define `edn_parse_options_t` struct in `include/edn.h`
- [ ] Add registry API declarations to `include/edn.h`
- [ ] Add `edn_parse_with_options()` declaration to `include/edn.h`
- [ ] Create `src/reader.c` with registry implementation
- [ ] Add reader fields to `edn_parser_t` in `src/edn_internal.h`
- [ ] Modify `edn_parse_tagged()` in `src/tagged.c` to use readers
- [ ] Update `edn_parser_init()` in `src/edn.c` to accept options
- [ ] Implement `edn_parse_with_options()` in `src/edn.c`
- [ ] Update `Makefile` to include `src/reader.c`
- [ ] Write `test/test_reader_registry.c`
- [ ] Write `test/test_reader_parsing.c`
- [ ] Write `bench/bench_reader.c`
- [ ] Update `examples/comprehensive.c` with reader usage
- [ ] Update `README.md` with reader documentation

## Future Enhancements

### 1. Standard Readers Library

Provide optional standard readers as a separate compilation unit:

```c
/* src/readers/standard.c */
edn_reader_fn edn_standard_reader_inst(void);
edn_reader_fn edn_standard_reader_uuid(void);
```

### 2. Reader Context

Allow readers to access parser context for better error reporting:

```c
typedef struct {
    size_t line;
    size_t column;
    void* user_data;  /* Caller-provided context */
} edn_reader_context_t;

typedef edn_value_t* (*edn_reader_fn)(
    const char* tag,
    size_t tag_length,
    edn_value_t* value,
    edn_arena_t* arena,
    const edn_reader_context_t* context,
    const char** error_message
);
```

### 3. Namespace Wildcards

Support registering readers for entire namespaces:

```c
/* Register reader for all tags in namespace */
edn_reader_register_namespace(registry, "myapp", my_namespace_handler);

/* Matches: #myapp/foo, #myapp/bar, etc. */
```

### 4. Reader Composition

Allow readers to delegate to other readers:

```c
/* Reader can invoke parser recursively */
edn_value_t* composite_reader(...) {
    /* Parse string as nested EDN */
    edn_result_t nested = edn_parse_with_options(str, len, options);
    /* ... */
}
```

## Conclusion

This design provides a flexible, performant, and backward-compatible reader system for EDN.C. Key benefits:

1. **Zero overhead** when readers are not used
2. **Type-safe** reader API with proper error handling
3. **Flexible** default fallback modes for different use cases
4. **Extensible** for future enhancements
5. **Well-tested** with comprehensive unit and benchmark tests

The implementation follows C11 best practices and EDN.C conventions (arena allocation, zero-copy where possible, clear error messages).
