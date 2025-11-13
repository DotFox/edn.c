/**
 * EDN.C - Value equality and comparison
 * 
 * Deep structural equality, comparison, and hashing for all EDN types.
 */

#include <math.h>
#include <string.h>

#include "edn_internal.h"

/* Maximum recursion depth to prevent stack overflow */
#define MAX_RECURSION_DEPTH 100

/* Forward declarations */
static bool edn_value_equal_internal(const edn_value_t* a, const edn_value_t* b, int depth);
static uint64_t edn_value_hash_internal(const edn_value_t* value);

/**
 * Deep structural equality comparison.
 * 
 * Returns true if two values are equal according to EDN semantics:
 * - nil == nil
 * - Booleans: true == true, false == false
 * - Numbers: Compare by value (NaN == NaN in EDN semantics)
 * - Characters: Compare Unicode codepoints
 * - Strings: Compare raw bytes (zero-copy, no decoding)
 * - Symbols/Keywords: Compare namespace and name
 * - Lists/Vectors: Element-wise comparison in order
 * - Sets/Maps: Order-independent comparison
 * - Tagged: Compare tag and value recursively
 * 
 * Uses cached hashes for fast inequality detection. Prevents stack overflow
 * via MAX_RECURSION_DEPTH limit.
 */
bool edn_value_equal(const edn_value_t* a, const edn_value_t* b) {
    return edn_value_equal_internal(a, b, 0);
}

/**
 * Get or compute hash for a value (with caching).
 * 
 * Hash value 0 is reserved as "not computed", so actual hash of 0 maps to 1.
 */
static inline uint64_t edn_value_get_hash(edn_value_t* value) {
    if (value->cached_hash == 0) {
        uint64_t hash = edn_value_hash_internal(value);
        value->cached_hash = (hash == 0) ? 1 : hash;
    }
    return value->cached_hash;
}

static bool edn_value_equal_internal(const edn_value_t* a, const edn_value_t* b, int depth) {
    if (a == b) {
        return true;
    }

    if (a == NULL || b == NULL) {
        return false;
    }

    if (depth >= MAX_RECURSION_DEPTH) {
        return false;
    }

    if (a->type != b->type) {
        return false;
    }

    /* Cast away const for hash caching (logically const operation) */
    if (a->cached_hash != 0 && b->cached_hash != 0) {
        if (a->cached_hash != b->cached_hash) {
            return false;
        }
    }

    switch (a->type) {
        case EDN_TYPE_NIL:
            return true;

        case EDN_TYPE_BOOL:
            return a->as.boolean == b->as.boolean;

        case EDN_TYPE_INT:
            return a->as.integer == b->as.integer;

        case EDN_TYPE_BIGINT:
            if (a->as.bigint.radix != b->as.bigint.radix) {
                return false;
            }
            if (a->as.bigint.negative != b->as.bigint.negative) {
                return false;
            }
            if (a->as.bigint.length != b->as.bigint.length) {
                return false;
            }
            return memcmp(a->as.bigint.digits, b->as.bigint.digits, a->as.bigint.length) == 0;

        case EDN_TYPE_FLOAT:
            if (isnan(a->as.floating) && isnan(b->as.floating)) {
                return true;
            }
            return a->as.floating == b->as.floating;

        case EDN_TYPE_CHARACTER:
            return a->as.character == b->as.character;

        case EDN_TYPE_STRING: {
            if (a->as.string.has_escapes != b->as.string.has_escapes) {
                return false;
            }

            if (a->as.string.length != b->as.string.length) {
                return false;
            }

            return memcmp(a->as.string.data, b->as.string.data, a->as.string.length) == 0;
        }

        case EDN_TYPE_SYMBOL:
        case EDN_TYPE_KEYWORD: {
            const char *ns_a, *name_a, *ns_b, *name_b;
            size_t ns_len_a, name_len_a, ns_len_b, name_len_b;

            if (a->type == EDN_TYPE_SYMBOL) {
                edn_symbol_get(a, &ns_a, &ns_len_a, &name_a, &name_len_a);
                edn_symbol_get(b, &ns_b, &ns_len_b, &name_b, &name_len_b);
            } else {
                edn_keyword_get(a, &ns_a, &ns_len_a, &name_a, &name_len_a);
                edn_keyword_get(b, &ns_b, &ns_len_b, &name_b, &name_len_b);
            }

            if (ns_len_a != ns_len_b) {
                return false;
            }

            if (ns_len_a > 0) {
                if (memcmp(ns_a, ns_b, ns_len_a) != 0) {
                    return false;
                }
            }

            if (name_len_a != name_len_b) {
                return false;
            }

            return memcmp(name_a, name_b, name_len_a) == 0;
        }

        case EDN_TYPE_LIST:
        case EDN_TYPE_VECTOR: {
            size_t count_a = a->as.list.count;
            size_t count_b = b->as.list.count;

            if (count_a != count_b) {
                return false;
            }

            edn_value_t** elements_a = a->as.list.elements;
            edn_value_t** elements_b = b->as.list.elements;

            for (size_t i = 0; i < count_a; i++) {
                if (!edn_value_equal_internal(elements_a[i], elements_b[i], depth + 1)) {
                    return false;
                }
            }

            return true;
        }

        case EDN_TYPE_SET: {
            if (a->as.set.count != b->as.set.count) {
                return false;
            }

            size_t count = a->as.set.count;

            for (size_t i = 0; i < count; i++) {
                edn_value_t* elem_a = a->as.set.elements[i];

                bool found = false;
                for (size_t j = 0; j < count; j++) {
                    if (edn_value_equal_internal(elem_a, b->as.set.elements[j], depth + 1)) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return false;
                }
            }

            return true;
        }

        case EDN_TYPE_MAP: {
            if (a->as.map.count != b->as.map.count) {
                return false;
            }

            size_t count = a->as.map.count;

            for (size_t i = 0; i < count; i++) {
                edn_value_t* key_a = a->as.map.entries[i].key;
                edn_value_t* val_a = a->as.map.entries[i].value;

                bool found = false;
                for (size_t j = 0; j < count; j++) {
                    if (edn_value_equal_internal(key_a, b->as.map.entries[j].key, depth + 1)) {
                        if (!edn_value_equal_internal(val_a, b->as.map.entries[j].value,
                                                      depth + 1)) {
                            return false;
                        }
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return false;
                }
            }

            return true;
        }

        case EDN_TYPE_TAGGED:
            if (a->as.tagged.tag_length != b->as.tagged.tag_length) {
                return false;
            }
            if (memcmp(a->as.tagged.tag, b->as.tagged.tag, a->as.tagged.tag_length) != 0) {
                return false;
            }
            return edn_value_equal_internal(a->as.tagged.value, b->as.tagged.value, depth + 1);

        default:
            return false;
    }
}

/**
 * Comparison function for qsort (total ordering).
 * 
 * Defines total ordering for all EDN types. Order by type first, then
 * type-specific comparison. For collections and tagged values, uses
 * pointer comparison (full recursive comparison would be expensive).
 * Used by sorted uniqueness checking algorithm.
 */
int edn_value_compare(const void* a_ptr, const void* b_ptr) {
    const edn_value_t* a = *(const edn_value_t**) a_ptr;
    const edn_value_t* b = *(const edn_value_t**) b_ptr;

    if (a == b)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;

    if (a->type != b->type) {
        return (int) a->type - (int) b->type;
    }

    switch (a->type) {
        case EDN_TYPE_NIL:
            return 0;

        case EDN_TYPE_BOOL:
            return (int) a->as.boolean - (int) b->as.boolean;

        case EDN_TYPE_INT:
            if (a->as.integer < b->as.integer)
                return -1;
            if (a->as.integer > b->as.integer)
                return 1;
            return 0;

        case EDN_TYPE_BIGINT: {
            if (a->as.bigint.radix != b->as.bigint.radix) {
                return (int) a->as.bigint.radix - (int) b->as.bigint.radix;
            }
            if (a->as.bigint.negative != b->as.bigint.negative) {
                return a->as.bigint.negative ? -1 : 1;
            }
            if (a->as.bigint.length != b->as.bigint.length) {
                return (int) (a->as.bigint.length - b->as.bigint.length);
            }
            return memcmp(a->as.bigint.digits, b->as.bigint.digits, a->as.bigint.length);
        }

        case EDN_TYPE_FLOAT:
            if (isnan(a->as.floating) && isnan(b->as.floating))
                return 0;
            if (isnan(a->as.floating))
                return 1;
            if (isnan(b->as.floating))
                return -1;

            if (a->as.floating < b->as.floating)
                return -1;
            if (a->as.floating > b->as.floating)
                return 1;
            return 0;

        case EDN_TYPE_CHARACTER:
            if (a->as.character < b->as.character)
                return -1;
            if (a->as.character > b->as.character)
                return 1;
            return 0;

        case EDN_TYPE_STRING: {
            if (a->as.string.has_escapes != b->as.string.has_escapes) {
                return a->as.string.has_escapes ? 1 : -1;
            }

            if (a->as.string.length != b->as.string.length) {
                return (int) (a->as.string.length - b->as.string.length);
            }

            return memcmp(a->as.string.data, b->as.string.data, a->as.string.length);
        }

        case EDN_TYPE_SYMBOL:
        case EDN_TYPE_KEYWORD: {
            const char *ns_a, *name_a, *ns_b, *name_b;
            size_t ns_len_a, name_len_a, ns_len_b, name_len_b;

            if (a->type == EDN_TYPE_SYMBOL) {
                edn_symbol_get(a, &ns_a, &ns_len_a, &name_a, &name_len_a);
                edn_symbol_get(b, &ns_b, &ns_len_b, &name_b, &name_len_b);
            } else {
                edn_keyword_get(a, &ns_a, &ns_len_a, &name_a, &name_len_a);
                edn_keyword_get(b, &ns_b, &ns_len_b, &name_b, &name_len_b);
            }

            if (ns_len_a != ns_len_b) {
                return (int) (ns_len_a - ns_len_b);
            }
            if (ns_len_a > 0) {
                int cmp = memcmp(ns_a, ns_b, ns_len_a);
                if (cmp != 0)
                    return cmp;
            }

            if (name_len_a != name_len_b) {
                return (int) (name_len_a - name_len_b);
            }
            return memcmp(name_a, name_b, name_len_a);
        }

        default:
            if (a < b)
                return -1;
            if (a > b)
                return 1;
            return 0;
    }
}

/**
 * Compute FNV-1a hash for an EDN value (internal, uncached).
 * 
 * Returns deterministic 64-bit hash. Order-independent for sets/maps via XOR.
 * Normalizes NaN floats to canonical representation for consistent hashing.
 */
static uint64_t edn_value_hash_internal(const edn_value_t* value) {
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    if (value == NULL) {
        return FNV_OFFSET_BASIS;
    }

    uint64_t hash = FNV_OFFSET_BASIS;

    hash ^= (uint64_t) value->type;
    hash *= FNV_PRIME;

    switch (value->type) {
        case EDN_TYPE_NIL:
            break;

        case EDN_TYPE_BOOL:
            hash ^= value->as.boolean ? 1 : 0;
            hash *= FNV_PRIME;
            break;

        case EDN_TYPE_INT: {
            int64_t val = value->as.integer;
            for (size_t i = 0; i < sizeof(int64_t); i++) {
                hash ^= (val >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_BIGINT:
            hash ^= value->as.bigint.radix;
            hash *= FNV_PRIME;
            hash ^= value->as.bigint.negative ? 1 : 0;
            hash *= FNV_PRIME;
            for (size_t i = 0; i < value->as.bigint.length; i++) {
                hash ^= (uint8_t) value->as.bigint.digits[i];
                hash *= FNV_PRIME;
            }
            break;

        case EDN_TYPE_FLOAT: {
            union {
                double d;
                uint64_t u;
            } val;
            val.d = value->as.floating;

            if (isnan(val.d)) {
                val.u = 0x7FF8000000000000ULL;
            }

            for (size_t i = 0; i < sizeof(uint64_t); i++) {
                hash ^= (val.u >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_CHARACTER:
            hash ^= value->as.character;
            hash *= FNV_PRIME;
            break;

        case EDN_TYPE_STRING: {
            for (size_t i = 0; i < value->as.string.length; i++) {
                hash ^= (uint8_t) value->as.string.data[i];
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_SYMBOL:
        case EDN_TYPE_KEYWORD: {
            const char *ns, *name;
            size_t ns_len, name_len;

            if (value->type == EDN_TYPE_SYMBOL) {
                edn_symbol_get(value, &ns, &ns_len, &name, &name_len);
            } else {
                edn_keyword_get(value, &ns, &ns_len, &name, &name_len);
            }

            for (size_t i = 0; i < ns_len; i++) {
                hash ^= (uint8_t) ns[i];
                hash *= FNV_PRIME;
            }

            for (size_t i = 0; i < name_len; i++) {
                hash ^= (uint8_t) name[i];
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_LIST:
        case EDN_TYPE_VECTOR: {
            size_t count = value->as.list.count;
            edn_value_t** elements = value->as.list.elements;

            for (size_t i = 0; i < count; i++) {
                uint64_t elem_hash = edn_value_hash_internal(elements[i]);
                hash ^= elem_hash;
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_SET: {
            uint64_t set_hash = 0;
            size_t count = value->as.set.count;

            for (size_t i = 0; i < count; i++) {
                uint64_t elem_hash = edn_value_hash_internal(value->as.set.elements[i]);
                set_hash ^= elem_hash;
            }
            hash ^= set_hash;
            hash *= FNV_PRIME;
            break;
        }

        case EDN_TYPE_MAP: {
            uint64_t map_hash = 0;
            for (size_t i = 0; i < value->as.map.count; i++) {
                uint64_t key_hash = edn_value_hash_internal(value->as.map.entries[i].key);
                uint64_t val_hash = edn_value_hash_internal(value->as.map.entries[i].value);
                uint64_t pair_hash = key_hash ^ (val_hash * FNV_PRIME);
                map_hash ^= pair_hash;
            }
            hash ^= map_hash;
            hash *= FNV_PRIME;
            break;
        }

        case EDN_TYPE_TAGGED:
            for (size_t i = 0; i < value->as.tagged.tag_length; i++) {
                hash ^= (uint8_t) value->as.tagged.tag[i];
                hash *= FNV_PRIME;
            }
            hash ^= edn_value_hash_internal(value->as.tagged.value);
            hash *= FNV_PRIME;
            break;

        default:
            break;
    }

    return hash;
}

/**
 * Get or compute hash for a value (public API with caching).
 * 
 * Returns cached hash if available, otherwise computes and caches it.
 * Casting away const is safe because caching is logically const.
 */
uint64_t edn_value_hash(const edn_value_t* value) {
    if (value == NULL) {
        return 14695981039346656037ULL;
    }

    edn_value_t* mutable_value = (edn_value_t*) value;
    return edn_value_get_hash(mutable_value);
}
