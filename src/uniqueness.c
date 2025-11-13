/**
 * EDN.C - Uniqueness checking
 */

#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

#define LINEAR_THRESHOLD 16
#define SORTED_THRESHOLD 1000

static bool edn_has_duplicates_linear(edn_value_t** elements, size_t count) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (edn_value_equal(elements[i], elements[j])) {
                return true;
            }
        }
    }
    return false;
}

static bool edn_has_duplicates_sorted(edn_value_t** elements, size_t count) {
    edn_value_t** temp = malloc(count * sizeof(edn_value_t*));
    if (temp == NULL) {
        return edn_has_duplicates_linear(elements, count);
    }

    memcpy(temp, elements, count * sizeof(edn_value_t*));
    qsort(temp, count, sizeof(edn_value_t*), edn_value_compare);

    bool has_dups = false;
    for (size_t i = 0; i < count - 1; i++) {
        if (edn_value_equal(temp[i], temp[i + 1])) {
            has_dups = true;
            break;
        }
    }

    free(temp);
    return has_dups;
}

/**
 * Hash table entry for duplicate detection.
 * Uses tombstone pattern: NULL = empty, element = occupied, (void*)1 = deleted
 */
typedef struct {
    edn_value_t* element;
    uint64_t hash;
} hash_entry_t;

static bool edn_has_duplicates_hash(edn_value_t** elements, size_t count) {
    /* Use hash table with open addressing (linear probing) for O(n) detection.
     * Load factor target: 0.7 (30% empty slots for good performance) */
    size_t table_size = (count * 10) / 7; /* 1.43x count for ~70% load factor */

    /* Round up to next power of 2 for faster modulo via bitwise AND */
    size_t size = 16;
    while (size < table_size) {
        size <<= 1;
    }
    size_t mask = size - 1;

    /* Allocate hash table (calloc zeros the memory, NULL = empty slot) */
    hash_entry_t* table = calloc(size, sizeof(hash_entry_t));
    if (table == NULL) {
        /* Memory allocation failed, fall back to sorted algorithm */
        return edn_has_duplicates_sorted(elements, count);
    }

    bool has_dups = false;

    for (size_t i = 0; i < count; i++) {
        edn_value_t* elem = elements[i];
        uint64_t hash = edn_value_hash(elem);
        size_t index = hash & mask;

        /* Linear probing to find slot */
        size_t probes = 0;
        while (table[index].element != NULL) {
            /* Check if this is the same element (duplicate found) */
            if (table[index].hash == hash && edn_value_equal(table[index].element, elem)) {
                has_dups = true;
                goto cleanup;
            }

            /* Move to next slot (linear probing) */
            index = (index + 1) & mask;
            probes++;

            /* Safety check: if we've probed the entire table, something is very wrong.
             * This should never happen with 70% load factor, but guard against it. */
            if (probes >= size) {
                /* Table full or infinite loop - fall back to sorted */
                free(table);
                return edn_has_duplicates_sorted(elements, count);
            }
        }

        /* Insert element into empty slot */
        table[index].element = elem;
        table[index].hash = hash;
    }

cleanup:
    free(table);
    return has_dups;
}

bool edn_has_duplicates(edn_value_t** elements, size_t count) {
    if (count <= 1) {
        return false;
    }

    if (count <= LINEAR_THRESHOLD) {
        return edn_has_duplicates_linear(elements, count);
    } else if (count <= SORTED_THRESHOLD) {
        return edn_has_duplicates_sorted(elements, count);
    } else {
        return edn_has_duplicates_hash(elements, count);
    }
}
