# EDN.C Design Philosophy and Architecture

**Version**: 1.0.0  
**Status**: Complete

## Overview

EDN.C is a high-performance, zero-copy EDN (Extensible Data Notation) parser written in C11. This document describes the design principles, architecture, and implementation strategies that guide the project.

## Core Principles

### 1. Performance First

**Goal**: Parse EDN data as fast as possible while maintaining correctness.

**Strategies**:
- **SIMD acceleration**: Vectorized operations for hot paths (whitespace, comments, identifiers)
- **Zero-copy**: Minimize allocations by referencing input buffer where safe
- **Lazy decoding**: Defer expensive operations (string unescaping) until accessed
- **Arena allocation**: Single bulk allocation/deallocation eliminates malloc overhead
- **Efficient data structures**: Sorted arrays with binary search for maps/sets

**Results**: 1-30 ns per operation on modern hardware (Apple M1).

### 2. Simplicity and Safety

**Goal**: Easy to use, hard to misuse, memory-safe by default.

**Strategies**:
- **Minimal API surface**: Two main functions (`edn_read()`, `edn_free()`)
- **Opaque types**: Hide implementation details, prevent misuse
- **Single cleanup**: One `edn_free()` call frees entire value tree
- **Clear error messages**: Line/column numbers, descriptive messages
- **No manual memory management**: Arena handles all allocations

**Benefits**:
- No use-after-free bugs (arena lifecycle)
- No memory leaks (single free)
- No double-free bugs (opaque pointers)
- No buffer overruns (length-checked parsing)

### 3. Zero Dependencies

**Goal**: Pure C11 with standard library only.

**Rationale**:
- Easy integration into any project
- No version conflicts with other dependencies
- Predictable behavior across platforms
- Minimal build complexity

**Trade-offs**:
- No external BigInt library (store digit strings instead)
- No external UTF-8 validation library (implement ourselves)
- Acceptable for EDN use case (simple numeric types, UTF-8 is common)

### 4. Spec Compliance

**Goal**: Fully compliant with [EDN specification](https://github.com/edn-format/edn).

**Coverage**:
- ✅ All primitive types (nil, bool, int, float, char, string, symbol, keyword)
- ✅ All collection types (lists, vectors, maps, sets)
- ✅ Tagged literals with custom reader support
- ✅ Comments (`;` line comments)
- ✅ Discard forms (`#_`)
- ✅ Equality semantics (order-independent maps/sets)
- ✅ Duplicate detection (maps/sets reject duplicates)

**Testing**: 340+ tests across 24 test suites ensure compliance.

## Architecture

### Memory Management: Arena Allocation

**Problem**: EDN values form a tree. Individual `malloc`/`free` is expensive and error-prone.

**Solution**: Arena allocator (bump pointer allocation).

```c
typedef struct edn_arena {
    void* buffer;        // Current allocation block
    size_t used;         // Bytes used in current block
    size_t capacity;     // Total block capacity
    void* first_block;   // First block in chain (for cleanup)
} edn_arena_t;
```

**Benefits**:
- **Fast allocation**: Bump pointer, no free list traversal
- **Simple cleanup**: Free entire arena in one call
- **Spatial locality**: Related data allocated contiguously
- **No fragmentation**: Values are never individually freed

**Trade-offs**:
- Cannot free individual values (acceptable for parse trees)
- Memory held until entire tree freed (acceptable for EDN use case)

**Implementation**: `src/arena.c`

### Parsing Strategy: Recursive Descent

**Approach**: Hand-written recursive descent parser.

**Why not LALR/Bison?**
- EDN grammar is simple (no conflicts, no ambiguity)
- Recursive descent is faster (no state machine, direct function calls)
- Better error messages (direct control over error reporting)
- Easier to optimize (inline hot paths, SIMD insertion points)

**Structure**:
```c
edn_value_t* edn_parse_value(edn_parser_t* parser) {
    skip_whitespace_and_comments(parser);  // SIMD accelerated
    
    char c = peek(parser);
    switch (c) {
        case 'n': return edn_parse_nil(parser);
        case 't':
        case 'f': return edn_parse_bool(parser);
        case '"': return edn_parse_string(parser);  // Zero-copy
        case '\\': return edn_parse_character(parser);
        case ':': return edn_parse_keyword(parser);
        case '(': return edn_read_list(parser);
        case '[': return edn_read_vector(parser);
        case '{': return edn_parse_map(parser);
        case '#': return edn_parse_dispatch(parser);  // Sets, tags, discard
        default: 
            if (is_digit(c) || c == '-' || c == '+')
                return edn_parse_number(parser);
            return edn_parse_symbol(parser);
    }
}
```

**Recursion limit**: 100 levels (prevents stack overflow).

**Implementation**: `src/edn.c`

### Zero-Copy Strings

**Problem**: Allocating and copying strings is expensive.

**Solution**: Point directly into input buffer when safe.

```c
typedef struct {
    const char* data;      // Points into input buffer or allocated memory
    size_t length;         // Byte length
    bool has_escapes;      // True if string contains \n, \t, etc.
    char* decoded;         // Cached decoded result (lazy)
} edn_string_t;
```

**Fast path** (no escapes):
- `data` points into input buffer
- `edn_string_get()` returns `data` directly
- Zero allocation, zero copy

**Slow path** (has escapes):
- First call to `edn_string_get()` decodes escapes
- Result cached in `decoded` field
- Subsequent calls return cached value

**Safety**: Input buffer must remain valid for lifetime of value (documented in API).

**Implementation**: `src/string.c`

### Number Parsing: Three-Tier Strategy

**Tier 1: Fast path** (80%+ of numbers)
- Simple 1-3 digit decimals: `42`, `0`, `-5`
- No overflow check needed
- Direct computation: `value = value * 10 + digit`
- **Performance**: 10-15 ns

**Tier 2: SWAR path** (long decimals)
- 8-digit chunks: `parse_eight_digits_unrolled()`
- Technique from [simdjson](https://github.com/simdjson/simdjson)
- 5x faster than scalar loop
- **Performance**: 20-30 ns

**Tier 3: General path** (all bases, overflow detection)
- Hex: `0xDEADBEEF`
- Octal: `0777`
- Radix: `36rZZ`
- Overflow → promote to BigInt
- **Performance**: 30-50 ns

**Floating point**: Clinger's algorithm (1990)
- Fast path: exponent in [-22, 22], mantissa < 2^53 (90% of floats)
- Precomputed powers of 10 (exact IEEE 754)
- Fallback to `strtod()` for complex cases
- **Performance**: 15-30 ns (fast path)

**Reference**: "How to Read Floating Point Numbers Accurately" - William D. Clinger

**Implementation**: `src/number.c`

### SIMD Acceleration

**Targets**:
- **ARM64**: NEON (default on Apple Silicon, AWS Graviton)
- **x86_64**: SSE4.2 (baseline), AVX2 (optional)

**Hot paths**:
1. **Whitespace skipping** (`src/simd.c`):
   - Scan 16 bytes at once
   - Check for ` `, `\t`, `\n`, `\r`, `,`
   - **Speedup**: 3-5x vs scalar

2. **Comment skipping** (`src/simd.c`):
   - Scan for newline in 16-byte chunks
   - **Speedup**: 4-6x vs scalar

3. **Identifier parsing** (`src/identifier.c`):
   - Validate symbol/keyword characters
   - Check for delimiters (space, brackets, quotes)
   - **Speedup**: 2-3x vs scalar

**Pattern**: Always provide scalar fallback in `#else` branch.

**Platform detection**:
```c
#if defined(__x86_64__)
    #include <immintrin.h>  // SSE4.2, AVX2
    #define SIMD_ENABLED 1
#elif defined(__aarch64__)
    #include <arm_neon.h>   // NEON
    #define SIMD_ENABLED 1
#else
    #define SIMD_ENABLED 0  // Scalar fallback
#endif
```

**Implementation**: `src/simd.c`, `src/identifier.c`

### Collections: Efficient Storage

**Lists**: Linked list (intrusive)
```c
typedef struct edn_list_node {
    edn_value_t* value;
    struct edn_list_node* next;
} edn_list_node_t;
```
- Natural for Lisp-style processing
- O(1) prepend
- O(n) random access (acceptable for EDN)

**Vectors**: Dynamic array
```c
typedef struct {
    edn_value_t** elements;
    size_t count;
    size_t capacity;
} edn_vector_t;
```
- O(1) random access
- Growth strategy: double capacity when full
- Arena-allocated elements array

**Maps and Sets**: Sorted array + binary search
```c
typedef struct {
    edn_value_t** keys;
    edn_value_t** values;
    size_t count;
    size_t capacity;
} edn_map_t;
```
- O(log n) lookup via binary search
- Elements sorted during parsing (ensures uniqueness)
- Duplicate detection: O(1) comparison with previous element
- Trade-off: O(n) insertion during parsing, but EDN is parse-once

**Why not hash tables?**
- EDN maps are typically small (< 20 entries)
- Binary search is fast for small n
- No hash function overhead
- Simpler implementation
- Order-independent equality is easier (no hash collisions)

**Implementation**: `src/collection.c`

### Equality: Deep Structural Comparison

**Requirements** (from EDN spec):
- Value-based, not reference-based
- Order-independent for maps and sets
- Type-strict for numbers (no coercion)
- NaN == NaN (EDN semantics, not IEEE 754)

**Implementation**:
```c
bool edn_value_equal(const edn_value_t* a, const edn_value_t* b) {
    // Fast path: same pointer or cached hash mismatch
    if (a == b) return true;
    if (a->cached_hash != 0 && b->cached_hash != 0 &&
        a->cached_hash != b->cached_hash) return false;
    
    // Type must match
    if (a->type != b->type) return false;
    
    // Type-specific comparison
    switch (a->type) {
        case EDN_TYPE_MAP:
            return edn_map_equal(a, b);  // Order-independent
        case EDN_TYPE_SET:
            return edn_set_equal(a, b);  // Order-independent
        case EDN_TYPE_VECTOR:
        case EDN_TYPE_LIST:
            return edn_sequence_equal(a, b);  // Order-dependent
        // ... other types
    }
}
```

**Hash function**: FNV-1a with XOR for commutative collections
- Memoized in `cached_hash` field
- Guarantees: `equal(a, b) => hash(a) == hash(b)`

**Recursion limit**: 100 levels (prevents stack overflow)

**Implementation**: `src/equality.c`

### Tagged Literals: Custom Readers

**Problem**: EDN is extensible via tagged literals (`#tag value`).

**Solution**: Registry of reader functions that transform values during parsing.

**Architecture**:
```c
typedef edn_value_t* (*edn_reader_fn)(
    edn_value_t* value,
    edn_arena_t* arena,
    const char** error_message
);

typedef struct edn_reader_registry edn_reader_registry_t;
```

**Modes**:
1. **No registry**: All tags return `EDN_TYPE_TAGGED` (default, zero overhead)
2. **With registry**: Look up reader function, transform value
3. **Fallback modes**:
   - `PASSTHROUGH`: Return `EDN_TYPE_TAGGED` (current behavior)
   - `UNWRAP`: Return wrapped value, discard tag
   - `ERROR`: Fail with `EDN_ERROR_UNKNOWN_TAG`

**Example**:
```c
// Reader that converts #inst strings to timestamps
edn_value_t* inst_reader(edn_value_t* value, edn_arena_t* arena,
                         const char** error_message) {
    if (edn_type(value) != EDN_TYPE_STRING) {
        *error_message = "#inst requires string";
        return NULL;
    }
    // Parse ISO8601, return new integer value
}

edn_reader_registry_t* registry = edn_reader_registry_create();
edn_reader_register(registry, "inst", inst_reader);

edn_parse_options_t opts = {.reader_registry = registry};
edn_result_t r = edn_read_with_options("#inst \"2024-01-01T00:00:00Z\"", 0, &opts);
// r.value is now EDN_TYPE_INT, not EDN_TYPE_TAGGED
```

**Performance**: Hash table lookup (~20-50 CPU cycles, acceptable overhead)

**Implementation**: `src/reader.c`, `src/tagged.c`

## File Organization

### Public API
- **`include/edn.h`**: Complete public API
  - Types: `edn_value_t`, `edn_type_t`, `edn_error_t`, `edn_result_t`
  - Core: `edn_read()`, `edn_free()`, `edn_type()`
  - Accessors: `edn_string_get()`, `edn_int64_get()`, etc.
  - Collections: `edn_list_count()`, `edn_vector_get()`, `edn_map_lookup()`, etc.
  - Readers: `edn_reader_registry_create()`, `edn_read_with_options()`, etc.

### Implementation
- **`src/edn_internal.h`**: Internal types and APIs
  - `edn_value_t` structure definition
  - `edn_parser_t` state
  - Internal utilities
- **`src/edn.c`**: Main parser, dispatcher, public API
- **`src/arena.c`**: Arena allocator
- **`src/string.c`**: String parsing, lazy decoding
- **`src/number.c`**: Number parsing (int64, BigInt, double)
- **`src/character.c`**: Character parsing (`\newline`, `\u03B1`)
- **`src/identifier.c`**: Symbol/keyword parsing with SIMD
- **`src/symbolic.c`**: Symbol value creation
- **`src/collection.c`**: List, vector, map, set
- **`src/tagged.c`**: Tagged literal parsing
- **`src/reader.c`**: Reader registry and lookup
- **`src/discard.c`**: Discard form (`#_`) handling
- **`src/equality.c`**: Deep structural equality
- **`src/uniqueness.c`**: Duplicate detection for maps/sets
- **`src/simd.c`**: SIMD acceleration (whitespace, comments)

### Testing
- **`test/*.c`**: 24 test files, 340+ tests
- **`test/test_framework.h`**: Custom test framework
  - Assertions: `assert()`, `assert_int_eq()`, `assert_str_eq()`, etc.
  - Macros: `TEST()`, `RUN_TEST()`, `TEST_SUMMARY()`

### Benchmarks
- **`bench/*.c`**: Microbenchmarks for hot paths
- **`bench/bench_framework.h`**: Timing utilities
- **`bench/data/`**: Test data files

### Documentation
- **`docs/DESIGN.md`**: This file
- **`docs/READER_DESIGN.md`**: Custom reader system

### Examples
- **`examples/reader.c`**: Custom reader demonstrations

## Performance Characteristics

### Parsing Throughput
**Typical performance on Apple M1 (from microbenchmarks)**:

| Operation | Time | Notes |
|-----------|------|-------|
| Whitespace skip | 1-5 ns | SIMD accelerated |
| Number parse | 10-30 ns | Fast path for simple decimals |
| String parse | 15-50 ns | Zero-copy, no escapes |
| Character parse | 8-15 ns | Named chars cached |
| Identifier parse | 10-25 ns | SIMD validation |
| Collection overhead | 5-10 ns | Per element |

**Real-world**: Small EDN documents (< 10 KB) parse in **< 50 µs**.

### Memory Usage
- **Arena overhead**: ~1-2% of total allocations
- **Value size**: 64 bytes per value (includes type tag, data, metadata)
- **Collections**: 8 bytes per element (pointer array)
- **Strings**: Zero-copy (no allocation) or decoded (cached)

**Example**: Parse `{:a 1 :b 2 :c 3}`
- 1 map value: 64 bytes
- 6 values (3 keywords + 3 ints): 384 bytes
- Arrays (keys + values): 48 bytes
- **Total**: ~500 bytes (reasonable overhead)

### Optimization Priorities
1. **Hot paths first**: Whitespace, numbers, strings (80% of parsing time)
2. **SIMD where it helps**: 16-byte operations on repetitive tasks
3. **Avoid allocation**: Zero-copy, lazy decoding, arena allocation
4. **Profile before optimizing**: Use `bench/` tools to measure

## Trade-offs and Rationale

### 1. No BigInt Library
**Decision**: Store digit strings instead of computing arbitrary precision.

**Rationale**:
- Zero-dependency requirement
- BigInt operations (add, multiply) not needed for parsing
- Callers can use GMP, OpenSSL BIGNUM, or other libraries with digit string
- Acceptable trade-off for EDN use case (rare)

### 2. No UTF-8 Library
**Decision**: Implement UTF-8 validation ourselves.

**Rationale**:
- UTF-8 validation is straightforward (state machine)
- Zero-dependency requirement
- Full control over error messages
- ~100 lines of code (reasonable)

### 3. Sorted Arrays for Maps/Sets
**Decision**: Use sorted arrays instead of hash tables.

**Rationale**:
- EDN maps/sets are typically small (< 20 entries)
- Binary search is O(log n), very fast for small n
- Simpler than hash tables (no collisions, no resizing)
- Order-independent equality is easier
- Duplicate detection during parsing is O(1)

**When to reconsider**: If profiling shows map lookup is bottleneck (unlikely).

### 4. O(n²) Equality for Maps/Sets
**Decision**: Nested loop for order-independent comparison.

**Rationale**:
- Correct and simple
- EDN maps/sets are typically small (< 20 entries)
- O(n²) is acceptable for n < 20 (~400 comparisons)
- Alternative (sort by hash) is more complex, not proven faster

**When to reconsider**: If profiling shows equality is bottleneck (unlikely).

### 5. Single Arena (No Pooling)
**Decision**: One arena per parse, freed together.

**Rationale**:
- EDN parse trees are typically short-lived (parse → use → free)
- Single `edn_free()` call is simple and safe
- No need for complex pooling or reference counting
- Acceptable trade-off for EDN use case

**When to reconsider**: If profiling shows allocation is bottleneck (unlikely with arena).

## Future Enhancements

### 1. Streaming Parser
**Goal**: Parse large EDN files without loading entire file into memory.

**Approach**:
- Incremental parsing with `edn_parse_stream(FILE* f)`
- Yield values as they're parsed
- Requires careful buffering and state management

**Use case**: Large data dumps, log files

### 2. Pretty Printer
**Goal**: Convert EDN values back to text with formatting.

**API**:
```c
char* edn_print(const edn_value_t* value, edn_print_options_t* opts);
```

**Options**: Indentation, compact mode, line width

**Use case**: Serialization, debugging, round-trip testing

### 3. Standard Readers Library
**Goal**: Provide common readers (#inst, #uuid) as optional module.

**API**:
```c
void edn_reader_register_standard(edn_reader_registry_t* registry);
```

**Use case**: Quick setup for common EDN extensions

### 4. CMake Build System
**Goal**: Support projects using CMake instead of Make.

**Files**: `CMakeLists.txt` (root), `test/CMakeLists.txt`, `bench/CMakeLists.txt`

**Use case**: Cross-platform builds, integration with CMake projects

### 5. Windows MSVC Support
**Goal**: Compile on Windows with Visual Studio.

**Challenges**: 
- SIMD intrinsics differ (AVX vs NEON)
- C11 support varies by MSVC version
- Path separators, line endings

**Use case**: Windows developers

## Testing Strategy

### Unit Tests (340+ tests, 24 files)
- **Coverage**: All data types, edge cases, error conditions
- **Framework**: Custom (`test/test_framework.h`)
- **Run**: `make test` (all tests), `make test/test_numbers` (single file)
- **Debug**: `make DEBUG=1` enables ASAN/UBSAN

### Test Categories
1. **Type parsing**: Numbers, strings, characters, identifiers, collections
2. **Edge cases**: Empty collections, deeply nested, Unicode, escapes
3. **Errors**: Invalid syntax, unmatched delimiters, bad UTF-8
4. **Equality**: All type combinations, order independence
5. **Duplicates**: Map/set validation
6. **Readers**: Registry operations, tag transformation, fallback modes
7. **Discard**: `#_` forms in various contexts
8. **Memory**: ASAN/UBSAN catch leaks, overflows, use-after-free

### Benchmarks
- **Purpose**: Measure hot path performance, detect regressions
- **Run**: `make bench` (quick), `make bench-all` (comprehensive)
- **Results**: Nanoseconds per operation, throughput

### Continuous Testing
- **Pre-commit**: Run `make test` before committing
- **CI/CD**: GitHub Actions (TODO)
- **Platforms**: Test on ARM64 (M1) and x86_64 (Linux/WSL)

## Code Style

See `.clang-format` for complete specification. Key points:

- **Standard**: C11 (no GNU extensions)
- **Indentation**: 4 spaces, no tabs
- **Braces**: K&R style (opening brace on same line)
- **Line length**: 100 characters max
- **Pointers**: `char *ptr` (star with type, not variable)
- **Comments**: `/* C89 style */` only (no `//`)
- **Naming**:
  - Public API: `edn_function_name()`, `edn_type_t`
  - Internal: `edn_internal_function()`
  - SIMD: `edn_simd_function()`
  - Constants: `UPPER_CASE`
  - Enums: `EDN_TYPE_NIL`

**Formatting**: Run `make format` before committing (uses clang-format).

## References

### EDN Specification
- https://github.com/edn-format/edn

### Parsing Techniques
- "How to Read Floating Point Numbers Accurately" - William D. Clinger (1990)
- simdjson: https://github.com/simdjson/simdjson (SWAR digit parsing)
- yyjson: https://github.com/ibireme/yyjson (SIMD patterns)

### C11 Best Practices
- "Modern C" - Jens Gustedt (2019)
- "21st Century C" - Ben Klemens (2014)

### SIMD Programming
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
- ARM NEON Guide: https://developer.arm.com/architectures/instruction-sets/simd-isas/neon

## Conclusion

EDN.C achieves high performance through careful design:
- **SIMD acceleration** for hot paths
- **Zero-copy strings** and lazy decoding
- **Arena allocation** for fast cleanup
- **Efficient data structures** (sorted arrays, binary search)
- **Spec compliance** with comprehensive testing

The codebase is **production-ready** (1.0.0) with:
- ✅ Full EDN specification support
- ✅ 340+ tests passing
- ✅ Memory safety verified (ASAN/UBSAN)
- ✅ Performance benchmarked
- ✅ Clean API and documentation

Future enhancements (streaming, pretty-printer, CMake) are optional and don't block release.
