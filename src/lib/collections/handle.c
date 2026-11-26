#include "lib/collections/handle.h"

// "No slot", for the free list only. Never a real index, since there are at most 255.
#define NO_SLOT UINT8_MAX

// Pack a generation and an index into a handle: generation in the high 24 bits, index in
// the low 8.
static handle_t make_handle(uint32_t generation, uint8_t index) {
    return (generation << 8) | index;
}

static uint32_t handle_generation(handle_t handle) {
    return handle >> 8;
}

// A slot in use points to itself; a free slot points to the next free one, or NO_SLOT.
static bool in_use(const handle_pool_t *pool, uint8_t index) {
    return pool->slots[index].next_free == index;
}

// The generation a slot gets after being freed. Wraps from the largest one that fits in
// a handle back to 1: 0 is skipped so that HANDLE_NONE (0) never matches a real slot.
static uint32_t next_generation(uint32_t generation) {
    return generation >= HANDLE_GENERATION_MAX ? 1 : generation + 1;
}

void handle_pool_init(handle_pool_t *pool) {
    uint8_t capacity = pool->capacity;

    // Chain every slot into the free list in order: 0 -> 1 -> ... -> last,
    // so the first allocs get the low indices.
    for (uint8_t index = 0; index < capacity; index++) {
        pool->slots[index] = (handle_pool_slot_t){
            .generation = 1,
            .next_free = index + 1 < capacity ? (uint8_t)(index + 1) : NO_SLOT,
        };
    }

    pool->count = 0;
    pool->free_head = 0;
    pool->free_tail = (uint8_t)(capacity - 1);
}

handle_t handle_pool_alloc(handle_pool_t *pool) {
    if (pool->free_head == NO_SLOT) {
        return HANDLE_NONE;
    }

    // Take the slot at the front of the free list
    uint8_t index = pool->free_head;
    handle_pool_slot_t *slot = &pool->slots[index];
    pool->free_head = slot->next_free;
    if (pool->free_head == NO_SLOT) {
        pool->free_tail = NO_SLOT; // that was the last free slot
    }
    slot->next_free = index; // marks it in use
    pool->count++;

    return make_handle(slot->generation, index);
}

bool handle_pool_free(handle_pool_t *pool, handle_t handle) {
    if (!handle_pool_is_live(pool, handle)) {
        return false;
    }

    // Bumping the generation is what makes every existing copy of the handle stale.
    uint8_t index = handle_index(handle);
    handle_pool_slot_t *slot = &pool->slots[index];
    slot->generation = next_generation(slot->generation);

    // Then add the slot to the BACK of the free list, so it's reused last. If one slot
    // were reused over and over, its generation would wrap round much sooner.
    slot->next_free = NO_SLOT;
    if (pool->free_tail == NO_SLOT) {
        pool->free_head = index;
    } else {
        pool->slots[pool->free_tail].next_free = index;
    }
    pool->free_tail = index;
    pool->count--;
    return true;
}

bool handle_pool_is_live(const handle_pool_t *pool, handle_t handle) {
    if (handle == HANDLE_NONE) {
        return false;
    }
    // Check the index first: it must be in range before we look at slots[index]
    uint8_t index = handle_index(handle);
    if (index >= pool->capacity) {
        return false;
    }
    const handle_pool_slot_t *slot = &pool->slots[index];
    return in_use(pool, index) && slot->generation == handle_generation(handle);
}

handle_t handle_pool_handle_at(const handle_pool_t *pool, uint8_t index) {
    if (index >= pool->capacity || !in_use(pool, index)) {
        return HANDLE_NONE;
    }
    return make_handle(pool->slots[index].generation, index);
}

uint8_t handle_pool_count(const handle_pool_t *pool) {
    return pool->count;
}
