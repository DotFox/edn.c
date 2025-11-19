/**
 * EDN.C - Fast EDN (Extensible Data Notation) parser
 * 
 * A simple and performant EDN parser written in C11 with SIMD acceleration.
 */

#ifndef EDN_H
#define EDN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EDN value types */
typedef enum {
    EDN_TYPE_NIL,
    EDN_TYPE_BOOL,
    EDN_TYPE_INT,
    EDN_TYPE_BIGINT,
    EDN_TYPE_FLOAT,
    EDN_TYPE_BIGDEC,
#ifdef EDN_ENABLE_RATIO
    EDN_TYPE_RATIO,
#endif
    EDN_TYPE_CHARACTER,
    EDN_TYPE_STRING,
    EDN_TYPE_SYMBOL,
    EDN_TYPE_KEYWORD,
    EDN_TYPE_LIST,
    EDN_TYPE_VECTOR,
    EDN_TYPE_MAP,
    EDN_TYPE_SET,
    EDN_TYPE_TAGGED
} edn_type_t;

/* Opaque EDN value structure */
typedef struct edn_value edn_value_t;

/* Error codes */
typedef enum {
    EDN_OK = 0,
    EDN_ERROR_INVALID_SYNTAX,
    EDN_ERROR_UNEXPECTED_EOF,
    EDN_ERROR_OUT_OF_MEMORY,
    EDN_ERROR_INVALID_UTF8,
    EDN_ERROR_INVALID_NUMBER,
    EDN_ERROR_INVALID_STRING,
    EDN_ERROR_INVALID_ESCAPE,
    EDN_ERROR_UNMATCHED_DELIMITER,
    EDN_ERROR_UNKNOWN_TAG,
    EDN_ERROR_DUPLICATE_KEY,
    EDN_ERROR_DUPLICATE_ELEMENT
} edn_error_t;

/* Parse result */
typedef struct {
    edn_value_t* value;        /* Parsed value (NULL on error) */
    edn_error_t error;         /* Error code (EDN_OK on success) */
    size_t error_line;         /* Line number where error occurred (1-indexed) */
    size_t error_column;       /* Column number where error occurred (1-indexed) */
    const char* error_message; /* Human-readable error description */
} edn_result_t;

/**
 * Parse EDN from a UTF-8 string.
 * 
 * @param input UTF-8 encoded string containing EDN data
 * @param length Length of input in bytes (or 0 to use strlen)
 * @return Parse result containing value or error information
 * 
 * The returned value must be freed with edn_free().
 */
edn_result_t edn_parse(const char* input, size_t length);

/**
 * Free an EDN value and all associated memory.
 * 
 * @param value Value to free (may be NULL)
 */
void edn_free(edn_value_t* value);

/**
 * Get the type of an EDN value.
 * 
 * @param value EDN value
 * @return Type of the value
 */
edn_type_t edn_type(const edn_value_t* value);

/**
 * Get the C string value from an EDN string.
 * 
 * This function implements lazy decoding:
 * - For strings without escapes: returns pointer to original input (zero-copy)
 * - For strings with escapes: decodes and caches result on first call
 * 
 * @param value EDN string value
 * @param length Optional output parameter for string length (may be NULL)
 * @return Pointer to UTF-8 string, or NULL if value is not a string
 * 
 * The returned pointer is valid until the value is freed with edn_free().
 * The string is guaranteed to be null-terminated.
 */
const char* edn_string_get(const edn_value_t* value, size_t* length);

/**
 * Get int64_t value from an EDN integer.
 * 
 * @param value EDN integer value
 * @param out Pointer to store the result
 * @return true if value is EDN_TYPE_INT, false otherwise
 */
bool edn_int64_get(const edn_value_t* value, int64_t* out);

/**
 * Get BigInt digit string from an EDN big integer.
 * 
 * Returns the string representation of the big integer for use with
 * external BigInt libraries (GMP, OpenSSL BIGNUM, etc.).
 * 
 * @param value EDN big integer value
 * @param length Pointer to store the digit string length (may be NULL)
 * @param negative Pointer to store the sign (may be NULL)
 * @param radix Pointer to store the number base (may be NULL)
 * @return Pointer to digit string, or NULL if value is not a big integer
 * 
 * The returned pointer is valid until the value is freed with edn_free().
 */
const char* edn_bigint_get(const edn_value_t* value, size_t* length, bool* negative,
                           uint8_t* radix);

/**
 * Get double value from an EDN float.
 * 
 * @param value EDN float value
 * @param out Pointer to store the result
 * @return true if value is EDN_TYPE_FLOAT, false otherwise
 */
bool edn_double_get(const edn_value_t* value, double* out);

/**
 * Get BigDecimal string from an EDN big decimal.
 *
 * Returns the string representation of the big decimal for use with
 * external BigDecimal libraries (Java BigDecimal, GMP mpf_t, etc.).
 *
 * @param value EDN big decimal value
 * @param length Pointer to store the string length (may be NULL)
 * @param negative Pointer to store the sign (may be NULL)
 * @return Pointer to decimal string, or NULL if value is not a big decimal
 *
 * The returned pointer is valid until the value is freed with edn_free().
 * The string contains the exact decimal representation (e.g., "3.14159265358979323846").
 */
const char* edn_bigdec_get(const edn_value_t* value, size_t* length, bool* negative);

#ifdef EDN_ENABLE_RATIO
/**
 * Get numerator and denominator from an EDN ratio.
 *
 * @param value EDN ratio value
 * @param numerator Pointer to store the numerator
 * @param denominator Pointer to store the denominator
 * @return true if value is EDN_TYPE_RATIO, false otherwise
 */
bool edn_ratio_get(const edn_value_t* value, int64_t* numerator, int64_t* denominator);
#endif

/**
 * Convert any EDN number type to double.
 * 
 * Automatically converts INT, BIGINT (may lose precision), FLOAT, and BIGDEC to double.
 * 
 * @param value EDN number value (INT, BIGINT, FLOAT, or BIGDEC)
 * @param out Pointer to store the result
 * @return true if value is a number type, false otherwise
 */
bool edn_number_as_double(const edn_value_t* value, double* out);

/**
 * Get Unicode codepoint from an EDN character.
 * 
 * @param value EDN character value
 * @param out Pointer to store the Unicode codepoint
 * @return true if value is EDN_TYPE_CHARACTER, false otherwise
 */
bool edn_character_get(const edn_value_t* value, uint32_t* out);

/**
 * Get symbol name and optional namespace from an EDN symbol.
 * 
 * @param value EDN symbol value
 * @param namespace Optional output for namespace pointer (may be NULL)
 * @param ns_length Optional output for namespace length (may be NULL)
 * @param name Output for name pointer
 * @param name_length Optional output for name length (may be NULL)
 * @return true if value is EDN_TYPE_SYMBOL, false otherwise
 */
bool edn_symbol_get(const edn_value_t* value, const char** namespace, size_t* ns_length,
                    const char** name, size_t* name_length);

/**
 * Get keyword name and optional namespace from an EDN keyword.
 * 
 * @param value EDN keyword value
 * @param namespace Optional output for namespace pointer (may be NULL)
 * @param ns_length Optional output for namespace length (may be NULL)
 * @param name Output for name pointer
 * @param name_length Optional output for name length (may be NULL)
 * @return true if value is EDN_TYPE_KEYWORD, false otherwise
 */
bool edn_keyword_get(const edn_value_t* value, const char** namespace, size_t* ns_length,
                     const char** name, size_t* name_length);

/**
 * List API
 */

/**
 * Get the number of elements in a list.
 * 
 * @param value EDN list value
 * @return Number of elements, or 0 if not a list
 */
size_t edn_list_count(const edn_value_t* value);

/**
 * Get element at index from a list.
 * 
 * @param value EDN list value
 * @param index Element index (0-based)
 * @return Element at index, or NULL if out of bounds or not a list
 */
edn_value_t* edn_list_get(const edn_value_t* value, size_t index);

/**
 * Vector API
 */

/**
 * Get the number of elements in a vector.
 * 
 * @param value EDN vector value
 * @return Number of elements, or 0 if not a vector
 */
size_t edn_vector_count(const edn_value_t* value);

/**
 * Get element at index from a vector.
 * 
 * @param value EDN vector value
 * @param index Element index (0-based)
 * @return Element at index, or NULL if out of bounds or not a vector
 */
edn_value_t* edn_vector_get(const edn_value_t* value, size_t index);

/**
 * Set API
 */

/**
 * Get the number of elements in a set.
 * 
 * @param value EDN set value
 * @return Number of elements, or 0 if not a set
 */
size_t edn_set_count(const edn_value_t* value);

/**
 * Get element at index from a set.
 * 
 * Note: Sets are unordered, this is for iteration only.
 * 
 * @param value EDN set value
 * @param index Element index (0-based)
 * @return Element at index, or NULL if out of bounds or not a set
 */
edn_value_t* edn_set_get(const edn_value_t* value, size_t index);

/**
 * Check if set contains an element.
 * 
 * @param value EDN set value
 * @param element Element to search for
 * @return true if element is in set, false otherwise
 */
bool edn_set_contains(const edn_value_t* value, const edn_value_t* element);

/**
 * Map API
 */

/**
 * Get the number of key-value pairs in a map.
 * 
 * @param value EDN map value
 * @return Number of pairs, or 0 if not a map
 */
size_t edn_map_count(const edn_value_t* value);

/**
 * Get key at index from a map.
 * 
 * Note: Maps are unordered, this is for iteration only.
 * 
 * @param value EDN map value
 * @param index Pair index (0-based)
 * @return Key at index, or NULL if out of bounds or not a map
 */
edn_value_t* edn_map_get_key(const edn_value_t* value, size_t index);

/**
 * Get value at index from a map.
 * 
 * Note: Maps are unordered, this is for iteration only.
 * 
 * @param value EDN map value
 * @param index Pair index (0-based)
 * @return Value at index, or NULL if out of bounds or not a map
 */
edn_value_t* edn_map_get_value(const edn_value_t* value, size_t index);

/**
 * Look up value by key in a map.
 * 
 * @param value EDN map value
 * @param key Key to search for
 * @return Value associated with key, or NULL if not found or not a map
 */
edn_value_t* edn_map_lookup(const edn_value_t* value, const edn_value_t* key);

/**
 * Check if map contains a key.
 * 
 * @param value EDN map value
 * @param key Key to search for
 * @return true if key is in map, false otherwise
 */
bool edn_map_contains_key(const edn_value_t* value, const edn_value_t* key);

/**
 * Tagged Literal API
 */

/**
 * Get tag and value from a tagged literal.
 * 
 * @param value EDN tagged literal value
 * @param tag Output for tag string pointer
 * @param tag_length Optional output for tag length (may be NULL)
 * @param tagged_value Output for the tagged value
 * @return true if value is EDN_TYPE_TAGGED, false otherwise
 * 
 * The tag string is the raw symbol name (e.g., "inst", "uuid", "myapp/custom").
 */
bool edn_tagged_get(const edn_value_t* value, const char** tag, size_t* tag_length,
                    edn_value_t** tagged_value);

/**
 * Reader API
 */

/* Forward declarations */
typedef struct edn_arena edn_arena_t;
typedef struct edn_reader_registry edn_reader_registry_t;

/**
 * Reader function for tagged literals.
 * 
 * Transforms a tagged literal value into its target representation.
 * 
 * @param value The wrapped EDN value (e.g., string, map, vector)
 * @param arena Arena allocator for creating new values
 * @param error_message Output parameter for error message (set to NULL for success)
 * @return Transformed EDN value, or NULL on error
 * 
 * The returned value must be allocated from the provided arena.
 * On error, set error_message to a static string and return NULL.
 * 
 * Note: The reader is invoked based on the tag it was registered for,
 * so tag information is not needed as a parameter.
 */
typedef edn_value_t* (*edn_reader_fn)(edn_value_t* value, edn_arena_t* arena,
                                      const char** error_message);

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
 * @param reader Reader function
 * @return true on success, false on allocation failure
 */
bool edn_reader_register(edn_reader_registry_t* registry, const char* tag, edn_reader_fn reader);

/**
 * Unregister a reader function for a tag.
 * 
 * @param registry Reader registry
 * @param tag Tag name
 */
void edn_reader_unregister(edn_reader_registry_t* registry, const char* tag);

/**
 * Look up a reader function for a tag.
 * 
 * @param registry Reader registry
 * @param tag Tag name
 * @return Reader function, or NULL if not found
 */
edn_reader_fn edn_reader_lookup(const edn_reader_registry_t* registry, const char* tag);

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
edn_result_t edn_parse_with_options(const char* input, size_t length,
                                    const edn_parse_options_t* options);

/**
 * Metadata API (optional, requires EDN_ENABLE_METADATA)
 */

#ifdef EDN_ENABLE_METADATA
/**
 * Get metadata attached to a value.
 *
 * Metadata is always a map (or NULL if no metadata).
 *
 * @param value EDN value
 * @return Metadata map, or NULL if no metadata attached
 */
edn_value_t* edn_value_meta(const edn_value_t* value);

/**
 * Check if a value has metadata attached.
 *
 * @param value EDN value
 * @return true if value has metadata, false otherwise
 */
bool edn_value_has_meta(const edn_value_t* value);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EDN_H */
