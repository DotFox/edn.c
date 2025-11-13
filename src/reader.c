/**
 * EDN.C - Reader registry for tagged literals
 */

#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

/* Initial number of hash table buckets */
#define INITIAL_BUCKET_COUNT 16

/* Reader registry entry */
typedef struct edn_reader_entry {
    char* tag;                     /* Owned copy of tag name */
    size_t tag_length;             /* Length of tag */
    edn_reader_fn reader;          /* Reader function */
    struct edn_reader_entry* next; /* For hash table chaining */
} edn_reader_entry_t;

/* Reader registry */
struct edn_reader_registry {
    edn_reader_entry_t** buckets; /* Hash table */
    size_t bucket_count;          /* Number of buckets */
    size_t entry_count;           /* Total entries */
};

/* FNV-1a hash function for tag strings */
static uint64_t hash_tag(const char* tag, size_t length) {
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t) tag[i];
        hash *= 1099511628211ULL; /* FNV prime */
    }
    return hash;
}

edn_reader_registry_t* edn_reader_registry_create(void) {
    edn_reader_registry_t* registry = malloc(sizeof(edn_reader_registry_t));
    if (registry == NULL) {
        return NULL;
    }

    registry->buckets = calloc(INITIAL_BUCKET_COUNT, sizeof(edn_reader_entry_t*));
    if (registry->buckets == NULL) {
        free(registry);
        return NULL;
    }

    registry->bucket_count = INITIAL_BUCKET_COUNT;
    registry->entry_count = 0;

    return registry;
}

void edn_reader_registry_destroy(edn_reader_registry_t* registry) {
    if (registry == NULL) {
        return;
    }

    /* Free all entries */
    for (size_t i = 0; i < registry->bucket_count; i++) {
        edn_reader_entry_t* entry = registry->buckets[i];
        while (entry != NULL) {
            edn_reader_entry_t* next = entry->next;
            free(entry->tag);
            free(entry);
            entry = next;
        }
    }

    free(registry->buckets);
    free(registry);
}

bool edn_reader_register(edn_reader_registry_t* registry, const char* tag, edn_reader_fn reader) {
    if (registry == NULL || tag == NULL || reader == NULL) {
        return false;
    }

    size_t tag_length = strlen(tag);

    /* Compute hash and bucket index */
    uint64_t hash = hash_tag(tag, tag_length);
    size_t bucket_idx = hash % registry->bucket_count;

    /* Check if tag already exists (replace if found) */
    edn_reader_entry_t* entry = registry->buckets[bucket_idx];
    while (entry != NULL) {
        if (entry->tag_length == tag_length && memcmp(entry->tag, tag, tag_length) == 0) {
            /* Update existing entry */
            entry->reader = reader;
            return true;
        }
        entry = entry->next;
    }

    /* Create new entry */
    edn_reader_entry_t* new_entry = malloc(sizeof(edn_reader_entry_t));
    if (new_entry == NULL) {
        return false;
    }

    /* Allocate and copy tag string */
    new_entry->tag = malloc(tag_length + 1);
    if (new_entry->tag == NULL) {
        free(new_entry);
        return false;
    }

    memcpy(new_entry->tag, tag, tag_length);
    new_entry->tag[tag_length] = '\0';
    new_entry->tag_length = tag_length;
    new_entry->reader = reader;

    /* Insert at head of bucket chain */
    new_entry->next = registry->buckets[bucket_idx];
    registry->buckets[bucket_idx] = new_entry;
    registry->entry_count++;

    return true;
}

void edn_reader_unregister(edn_reader_registry_t* registry, const char* tag) {
    if (registry == NULL || tag == NULL) {
        return;
    }

    size_t tag_length = strlen(tag);

    /* Compute hash and bucket index */
    uint64_t hash = hash_tag(tag, tag_length);
    size_t bucket_idx = hash % registry->bucket_count;

    /* Find and remove entry */
    edn_reader_entry_t** entry_ptr = &registry->buckets[bucket_idx];
    while (*entry_ptr != NULL) {
        edn_reader_entry_t* entry = *entry_ptr;
        if (entry->tag_length == tag_length && memcmp(entry->tag, tag, tag_length) == 0) {
            /* Remove from chain */
            *entry_ptr = entry->next;
            free(entry->tag);
            free(entry);
            registry->entry_count--;
            return;
        }
        entry_ptr = &entry->next;
    }
}

edn_reader_fn edn_reader_lookup(const edn_reader_registry_t* registry, const char* tag) {
    if (registry == NULL || tag == NULL) {
        return NULL;
    }

    size_t tag_length = strlen(tag);

    /* Compute hash and bucket index */
    uint64_t hash = hash_tag(tag, tag_length);
    size_t bucket_idx = hash % registry->bucket_count;

    /* Search bucket chain */
    edn_reader_entry_t* entry = registry->buckets[bucket_idx];
    while (entry != NULL) {
        if (entry->tag_length == tag_length && memcmp(entry->tag, tag, tag_length) == 0) {
            return entry->reader;
        }
        entry = entry->next;
    }

    return NULL;
}

/* Internal lookup function that takes tag length (for non-null-terminated substrings) */
edn_reader_fn edn_reader_lookup_internal(const edn_reader_registry_t* registry, const char* tag,
                                         size_t tag_length) {
    if (registry == NULL || tag == NULL) {
        return NULL;
    }

    /* Compute hash and bucket index */
    uint64_t hash = hash_tag(tag, tag_length);
    size_t bucket_idx = hash % registry->bucket_count;

    /* Search bucket chain */
    edn_reader_entry_t* entry = registry->buckets[bucket_idx];
    while (entry != NULL) {
        if (entry->tag_length == tag_length && memcmp(entry->tag, tag, tag_length) == 0) {
            return entry->reader;
        }
        entry = entry->next;
    }

    return NULL;
}
