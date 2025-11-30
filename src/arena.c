#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

edn_arena_t* edn_arena_create(void) {
    edn_arena_t* arena = malloc(sizeof(edn_arena_t));
    if (!arena) {
        return NULL;
    }

    /* Start with small block for small documents */
    arena_block_t* block = malloc(sizeof(arena_block_t) + ARENA_INITIAL_SIZE);
    if (!block) {
        free(arena);
        return NULL;
    }

    block->next = NULL;
    block->used = 0;
    block->capacity = ARENA_INITIAL_SIZE;

    arena->current = block;
    arena->first = block;
    arena->next_block_size = ARENA_MEDIUM_SIZE; /* Grow to medium on next allocation */
    arena->total_allocated = ARENA_INITIAL_SIZE;

    return arena;
}

void edn_arena_destroy(edn_arena_t* arena) {
    if (!arena) {
        return;
    }

    arena_block_t* block = arena->first;
    while (block) {
        arena_block_t* next = block->next;
        free(block);
        block = next;
    }

    free(arena);
}

static void* edn_arena_alloc_slow(edn_arena_t* arena, size_t size) {
    arena_block_t* block = arena->current;

    /* Use adaptive block size - either the next planned size or the requested size (whichever is larger) */
    size_t block_size = (size > arena->next_block_size) ? size : arena->next_block_size;

    arena_block_t* new_block = malloc(sizeof(arena_block_t) + block_size);
    if (!new_block) {
        return NULL;
    }

    new_block->next = NULL;
    new_block->used = 0;
    new_block->capacity = block_size;

    block->next = new_block;
    arena->current = new_block;
    block = new_block;

    /* Adaptive growth: double size up to LARGE limit */
    if (arena->next_block_size < ARENA_LARGE_SIZE) {
        arena->next_block_size *= 2;
        if (arena->next_block_size > ARENA_LARGE_SIZE) {
            arena->next_block_size = ARENA_LARGE_SIZE;
        }
    }

    arena->total_allocated += block_size;

    void* ptr = block->data + block->used;
    block->used += size;

    return ptr;
}

void* edn_arena_alloc(edn_arena_t* arena, size_t size) {
    if (!arena) {
        return NULL;
    }

    size = (size + 7) & ~7;

    arena_block_t* block = arena->current;

    if (block->used + size <= block->capacity) {
        void* ptr = block->data + block->used;
        block->used += size;
        return ptr;
    }

    return edn_arena_alloc_slow(arena, size);
}
