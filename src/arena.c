#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

edn_arena_t* edn_arena_create(void) {
    edn_arena_t* arena = malloc(sizeof(edn_arena_t));
    if (!arena) {
        return NULL;
    }

    arena_block_t* block = malloc(sizeof(arena_block_t) + ARENA_BLOCK_SIZE);
    if (!block) {
        free(arena);
        return NULL;
    }

    block->next = NULL;
    block->used = 0;
    block->capacity = ARENA_BLOCK_SIZE;

    arena->current = block;
    arena->first = block;

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

void* edn_arena_alloc_slow(edn_arena_t* arena, size_t size) {
    if (!arena) {
        return NULL;
    }

    size = (size + 7) & ~7;

    arena_block_t* block = arena->current;

    size_t block_size = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
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

    void* ptr = block->data + block->used;
    block->used += size;

    return ptr;
}
