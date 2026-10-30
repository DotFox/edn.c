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

struct edn_collection_index {
    size_t capacity;
    size_t* slots;
};

static edn_collection_index_t edn_unindexed_collection = {0};

static bool edn_value_can_index(const edn_value_t* value, int depth) {
    if (value == NULL || depth >= MAX_RECURSION_DEPTH) {
        return false;
    }
    switch (value->type) {
        case EDN_TYPE_EXTERNAL:
            return false;
        case EDN_TYPE_LIST:
        case EDN_TYPE_VECTOR:
        case EDN_TYPE_SET:
            for (size_t i = 0; i < value->as.list.count; i++) {
                if (!edn_value_can_index(value->as.list.elements[i], depth + 1)) {
                    return false;
                }
            }
            return true;
        case EDN_TYPE_MAP:
            for (size_t i = 0; i < value->as.map.count; i++) {
                if (!edn_value_can_index(value->as.map.keys[i], depth + 1) ||
                    !edn_value_can_index(value->as.map.values[i], depth + 1)) {
                    return false;
                }
            }
            return true;
        case EDN_TYPE_TAGGED:
            return edn_value_can_index(value->as.tagged.value, depth + 1);
        default:
            return true;
    }
}

static edn_collection_index_t* edn_collection_build_index(const edn_value_t* collection,
                                                          edn_value_t* const* keys, size_t count) {
    if (count > SIZE_MAX / 2) {
        return &edn_unindexed_collection;
    }
    size_t capacity = 32;
    while (capacity < count * 2) {
        if (capacity > SIZE_MAX / 2) {
            return &edn_unindexed_collection;
        }
        capacity *= 2;
    }
    if (capacity > (SIZE_MAX - sizeof(edn_collection_index_t)) / sizeof(size_t)) {
        return &edn_unindexed_collection;
    }
    for (size_t i = 0; i < count; i++) {
        if (!edn_value_can_index(keys[i], 0)) {
            return &edn_unindexed_collection;
        }
    }
    edn_collection_index_t* index =
        edn_arena_alloc(collection->arena, sizeof(*index) + capacity * sizeof(size_t));
    if (index == NULL) {
        return &edn_unindexed_collection;
    }
    index->capacity = capacity;
    index->slots = (size_t*) (index + 1);
    memset(index->slots, 0, capacity * sizeof(size_t));
    for (size_t i = 0; i < count; i++) {
        size_t slot = edn_value_hash(keys[i]) & (capacity - 1);
        while (index->slots[slot] != 0) {
            slot = (slot + 1) & (capacity - 1);
        }
        index->slots[slot] = i + 1;
    }
    return index;
}

static size_t edn_collection_find_internal(const edn_value_t* collection, const edn_value_t* key,
                                           int depth) {
    bool is_map = collection->type == EDN_TYPE_MAP;
    size_t count = is_map ? collection->as.map.count : collection->as.set.count;
    edn_value_t* const* keys = is_map ? collection->as.map.keys : collection->as.set.elements;
    if (count > 16 && collection->arena != NULL) {
        edn_value_t* mutable_collection = (edn_value_t*) collection;
        edn_collection_index_t** cache =
            is_map ? &mutable_collection->as.map.index : &mutable_collection->as.set.index;
        if (*cache == NULL) {
            *cache = edn_collection_build_index(collection, keys, count);
        }
        edn_collection_index_t* index = *cache;
        if (index->capacity != 0 && edn_value_can_index(key, 0)) {
            uint64_t hash = edn_value_hash(key);
            size_t slot = hash & (index->capacity - 1);
            while (index->slots[slot] != 0) {
                size_t i = index->slots[slot] - 1;
                if (edn_value_hash(keys[i]) == hash &&
                    edn_value_equal_internal(keys[i], key, depth)) {
                    return i;
                }
                slot = (slot + 1) & (index->capacity - 1);
            }
            return SIZE_MAX;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (edn_value_equal_internal(keys[i], key, depth)) {
            return i;
        }
    }
    return SIZE_MAX;
}

size_t edn_collection_find(const edn_value_t* collection, const edn_value_t* key) {
    return edn_collection_find_internal(collection, key, 0);
}

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
        bool a_is_seq = (a->type == EDN_TYPE_LIST || a->type == EDN_TYPE_VECTOR);
        bool b_is_seq = (b->type == EDN_TYPE_LIST || b->type == EDN_TYPE_VECTOR);
        if (!(a_is_seq && b_is_seq)) {
            return false;
        }
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

        case EDN_TYPE_BIGINT: {
            if (a->as.bigint.radix != b->as.bigint.radix) {
                return false;
            }
            if (a->as.bigint.negative != b->as.bigint.negative) {
                return false;
            }

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            size_t len_a, len_b;
            uint8_t radix_a, radix_b;
            bool neg_a, neg_b;
            const char* digits_a = edn_bigint_get(a, &len_a, &neg_a, &radix_a);
            const char* digits_b = edn_bigint_get(b, &len_b, &neg_b, &radix_b);

            if (len_a != len_b) {
                return false;
            }
            return memcmp(digits_a, digits_b, len_a) == 0;
#else
            if (a->as.bigint.length != b->as.bigint.length) {
                return false;
            }
            return memcmp(a->as.bigint.digits, b->as.bigint.digits, a->as.bigint.length) == 0;
#endif
        }

        case EDN_TYPE_FLOAT:
            if (isnan(a->as.floating) && isnan(b->as.floating)) {
                return true;
            }
            /* IEEE 754 already treats +0.0 == -0.0 — consistent with hashing
             * which normalizes zero (see edn_value_hash_internal). */
            return a->as.floating == b->as.floating;

        case EDN_TYPE_BIGDEC: {
            if (a->as.bigdec.negative != b->as.bigdec.negative) {
                return false;
            }

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            size_t len_a, len_b;
            bool neg_a, neg_b;
            const char* decimal_a = edn_bigdec_get(a, &len_a, &neg_a);
            const char* decimal_b = edn_bigdec_get(b, &len_b, &neg_b);

            if (len_a != len_b) {
                return false;
            }
            return memcmp(decimal_a, decimal_b, len_a) == 0;
#else
            if (a->as.bigdec.length != b->as.bigdec.length) {
                return false;
            }
            return memcmp(a->as.bigdec.decimal, b->as.bigdec.decimal, a->as.bigdec.length) == 0;
#endif
        }

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        case EDN_TYPE_RATIO:
            return a->as.ratio.numerator == b->as.ratio.numerator &&
                   a->as.ratio.denominator == b->as.ratio.denominator;

        case EDN_TYPE_BIGRATIO: {
            if (a->as.bigratio.numer_negative != b->as.bigratio.numer_negative) {
                return false;
            }
            if (a->as.bigratio.numer_length != b->as.bigratio.numer_length) {
                return false;
            }
            if (a->as.bigratio.denom_length != b->as.bigratio.denom_length) {
                return false;
            }
            if (memcmp(a->as.bigratio.numerator, b->as.bigratio.numerator,
                       a->as.bigratio.numer_length) != 0) {
                return false;
            }
            return memcmp(a->as.bigratio.denominator, b->as.bigratio.denominator,
                          a->as.bigratio.denom_length) == 0;
        }
#endif

        case EDN_TYPE_CHARACTER:
            return a->as.character == b->as.character;

        case EDN_TYPE_STRING: {
            size_t len_a = edn_string_get_length(a);
            size_t len_b = edn_string_get_length(b);

            if (edn_string_has_escapes(a) != edn_string_has_escapes(b)) {
                return false;
            }

            if (len_a != len_b) {
                return false;
            }

            return memcmp(a->as.string.data, b->as.string.data, len_a) == 0;
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

                if (edn_collection_find_internal(b, elem_a, depth + 1) == SIZE_MAX) {
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
                edn_value_t* key_a = a->as.map.keys[i];
                edn_value_t* val_a = a->as.map.values[i];

                size_t j = edn_collection_find_internal(b, key_a, depth + 1);
                if (j == SIZE_MAX ||
                    !edn_value_equal_internal(val_a, b->as.map.values[j], depth + 1)) {
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

        case EDN_TYPE_EXTERNAL: {
            if (a->as.external.type_id != b->as.external.type_id) {
                return false;
            }

            edn_external_equal_fn equal_fn = edn_external_lookup_equal(a->as.external.type_id);
            if (equal_fn) {
                return equal_fn(a->as.external.data, b->as.external.data);
            }

            /* Fallback: pointer equality */
            return a->as.external.data == b->as.external.data;
        }

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

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            size_t len_a, len_b;
            bool neg_a, neg_b;
            uint8_t radix_a, radix_b;
            const char* digits_a = edn_bigint_get(a, &len_a, &neg_a, &radix_a);
            const char* digits_b = edn_bigint_get(b, &len_b, &neg_b, &radix_b);

            if (len_a != len_b) {
                return (int) (len_a - len_b);
            }
            return memcmp(digits_a, digits_b, len_a);
#else
            if (a->as.bigint.length != b->as.bigint.length) {
                return (int) (a->as.bigint.length - b->as.bigint.length);
            }
            return memcmp(a->as.bigint.digits, b->as.bigint.digits, a->as.bigint.length);
#endif
        }

        case EDN_TYPE_BIGDEC: {
            if (a->as.bigdec.negative != b->as.bigdec.negative) {
                return a->as.bigdec.negative ? -1 : 1;
            }

#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            size_t len_a, len_b;
            bool neg_a, neg_b;
            const char* decimal_a = edn_bigdec_get(a, &len_a, &neg_a);
            const char* decimal_b = edn_bigdec_get(b, &len_b, &neg_b);

            if (len_a != len_b) {
                return (int) (len_a - len_b);
            }
            return memcmp(decimal_a, decimal_b, len_a);
#else
            if (a->as.bigdec.length != b->as.bigdec.length) {
                return (int) (a->as.bigdec.length - b->as.bigdec.length);
            }
            return memcmp(a->as.bigdec.decimal, b->as.bigdec.decimal, a->as.bigdec.length);
#endif
        }

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        case EDN_TYPE_RATIO: {
            /* Lexicographic on (sign-of-numerator, numerator, denominator).
             * NOT a numeric total order — matches BIGINT/BIGDEC compare semantics. */
            bool neg_a = a->as.ratio.numerator < 0;
            bool neg_b = b->as.ratio.numerator < 0;
            if (neg_a != neg_b) {
                return neg_a ? -1 : 1;
            }
            if (a->as.ratio.numerator != b->as.ratio.numerator) {
                return (a->as.ratio.numerator < b->as.ratio.numerator) ? -1 : 1;
            }
            if (a->as.ratio.denominator != b->as.ratio.denominator) {
                return (a->as.ratio.denominator < b->as.ratio.denominator) ? -1 : 1;
            }
            return 0;
        }

        case EDN_TYPE_BIGRATIO: {
            /* Lex order on (numer_negative, numer_length, numerator bytes,
             * denom_length, denominator bytes). Mirrors BIGINT compare. */
            if (a->as.bigratio.numer_negative != b->as.bigratio.numer_negative) {
                return a->as.bigratio.numer_negative ? -1 : 1;
            }
            if (a->as.bigratio.numer_length != b->as.bigratio.numer_length) {
                return (int) (a->as.bigratio.numer_length - b->as.bigratio.numer_length);
            }
            int cmp = memcmp(a->as.bigratio.numerator, b->as.bigratio.numerator,
                             a->as.bigratio.numer_length);
            if (cmp != 0) {
                return cmp;
            }
            if (a->as.bigratio.denom_length != b->as.bigratio.denom_length) {
                return (int) (a->as.bigratio.denom_length - b->as.bigratio.denom_length);
            }
            return memcmp(a->as.bigratio.denominator, b->as.bigratio.denominator,
                          a->as.bigratio.denom_length);
        }
#endif

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
            bool has_esc_a = edn_string_has_escapes(a);
            bool has_esc_b = edn_string_has_escapes(b);
            size_t len_a = edn_string_get_length(a);
            size_t len_b = edn_string_get_length(b);

            if (has_esc_a != has_esc_b) {
                return has_esc_a ? 1 : -1;
            }

            if (len_a != len_b) {
                return (int) (len_a - len_b);
            }

            return memcmp(a->as.string.data, b->as.string.data, len_a);
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

    edn_type_t hash_type = value->type == EDN_TYPE_VECTOR ? EDN_TYPE_LIST : value->type;
    hash ^= (uint64_t) hash_type;
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

        case EDN_TYPE_BIGINT: {
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            /* Use cleaned digits for hashing */
            size_t len;
            bool neg;
            uint8_t radix;
            const char* digits = edn_bigint_get(value, &len, &neg, &radix);
#else
            size_t len = value->as.bigint.length;
            bool neg = value->as.bigint.negative;
            uint8_t radix = value->as.bigint.radix;
            const char* digits = value->as.bigint.digits;
#endif

            hash ^= radix;
            hash *= FNV_PRIME;
            hash ^= neg ? 1 : 0;
            hash *= FNV_PRIME;
            for (size_t i = 0; i < len; i++) {
                hash ^= (uint8_t) digits[i];
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_FLOAT: {
            union {
                double d;
                uint64_t u;
            } val;
            val.d = value->as.floating;

            if (isnan(val.d)) {
                val.u = 0x7FF8000000000000ULL;
            } else if (val.d == 0.0) {
                /* Normalize +0.0 and -0.0 to the same bit pattern so that
                 * equal values (per IEEE 754 ==) always hash identically. */
                val.u = 0;
            }

            for (size_t i = 0; i < sizeof(uint64_t); i++) {
                hash ^= (val.u >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_BIGDEC: {
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
            /* Use cleaned decimal for hashing */
            size_t len;
            bool neg;
            const char* decimal = edn_bigdec_get(value, &len, &neg);
#else
            size_t len = value->as.bigdec.length;
            bool neg = value->as.bigdec.negative;
            const char* decimal = value->as.bigdec.decimal;
#endif

            hash ^= neg ? 1 : 0;
            hash *= FNV_PRIME;
            for (size_t i = 0; i < len; i++) {
                hash ^= (uint8_t) decimal[i];
                hash *= FNV_PRIME;
            }
            break;
        }

#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        case EDN_TYPE_RATIO: {
            for (size_t i = 0; i < sizeof(int64_t); i++) {
                hash ^= (value->as.ratio.numerator >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
            for (size_t i = 0; i < sizeof(int64_t); i++) {
                hash ^= (value->as.ratio.denominator >> (i * 8)) & 0xFF;
                hash *= FNV_PRIME;
            }
            break;
        }

        case EDN_TYPE_BIGRATIO: {
            hash ^= value->as.bigratio.numer_negative ? 1 : 0;
            hash *= FNV_PRIME;
            for (size_t i = 0; i < value->as.bigratio.numer_length; i++) {
                hash ^= (uint8_t) value->as.bigratio.numerator[i];
                hash *= FNV_PRIME;
            }
            for (size_t i = 0; i < value->as.bigratio.denom_length; i++) {
                hash ^= (uint8_t) value->as.bigratio.denominator[i];
                hash *= FNV_PRIME;
            }
            break;
        }
#endif

        case EDN_TYPE_CHARACTER:
            hash ^= value->as.character;
            hash *= FNV_PRIME;
            break;

        case EDN_TYPE_STRING: {
            size_t len = edn_string_get_length(value);
            for (size_t i = 0; i < len; i++) {
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
                uint64_t key_hash = edn_value_hash_internal(value->as.map.keys[i]);
                uint64_t val_hash = edn_value_hash_internal(value->as.map.values[i]);
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

        case EDN_TYPE_EXTERNAL: {
            hash ^= value->as.external.type_id;
            hash *= FNV_PRIME;

            edn_external_hash_fn hash_fn = edn_external_lookup_hash(value->as.external.type_id);
            if (hash_fn) {
                hash ^= hash_fn(value->as.external.data);
                hash *= FNV_PRIME;
            } else {
                uintptr_t ptr = (uintptr_t) value->as.external.data;
                for (size_t i = 0; i < sizeof(uintptr_t); i++) {
                    hash ^= (ptr >> (i * 8)) & 0xFF;
                    hash *= FNV_PRIME;
                }
            }
            break;
        }

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
