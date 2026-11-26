#include "lib/collections/handle.h"
#include "test_helpers/assert.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Pools of several sizes. Each test calls handle_pool_init on the one it uses, so they
// start fresh.
static handle_pool_t pool1 = HANDLE_POOL_WITH_CAPACITY(1);
static handle_pool_t pool2 = HANDLE_POOL_WITH_CAPACITY(2);
static handle_pool_t pool2_other = HANDLE_POOL_WITH_CAPACITY(2);
static handle_pool_t pool3 = HANDLE_POOL_WITH_CAPACITY(3);
static handle_pool_t pool16 = HANDLE_POOL_WITH_CAPACITY(16);
static handle_pool_t pool255 = HANDLE_POOL_WITH_CAPACITY(255);

// A pool inside another struct, the way the timer scheduler will hold one
static struct {
    handle_pool_t pool;
    int other_field;
} owner = {.pool = HANDLE_POOL_WITH_CAPACITY(4), .other_field = 7};

// Build a handle by hand, for testing handles the pool never issued
static handle_t handle_of(uint32_t generation, uint8_t index) {
    return (generation << 8) | index;
}

static uint32_t generation_of(handle_t handle) {
    return handle >> 8;
}

// A slot in use points to itself (see handle_pool_slot_t)
static bool in_use(const handle_pool_t *pool, uint8_t index) {
    return pool->slots[index].next_free == index;
}

// Walk the free list and check it against the slots: every free slot appears exactly once,
// no slot in use appears at all, the tail is the last one, and in use + free adds up to
// the capacity.
static void check_invariants(const handle_pool_t *pool) {
    bool seen[HANDLE_POOL_MAX_CAPACITY] = {};
    size_t free_count = 0;
    uint8_t last = UINT8_MAX;
    for (uint8_t index = pool->free_head; index != UINT8_MAX;
         index = pool->slots[index].next_free) {
        TEST_ASSERT(index < pool->capacity);
        TEST_ASSERT(!seen[index]);
        TEST_ASSERT(!in_use(pool, index));
        seen[index] = true;
        free_count++;
        last = index;
    }
    TEST_ASSERT(pool->free_tail == last);

    size_t occupied_count = 0;
    for (uint8_t index = 0; index < pool->capacity; index++) {
        TEST_ASSERT(pool->slots[index].generation >= 1);
        TEST_ASSERT(pool->slots[index].generation <= HANDLE_GENERATION_MAX);
        if (in_use(pool, index)) {
            occupied_count++;
        } else {
            TEST_ASSERT(seen[index]);
        }
    }

    TEST_ASSERT(occupied_count == pool->count);
    TEST_ASSERT(occupied_count + free_count == pool->capacity);
}

static void test_sizes_and_nesting(void) {
    handle_pool_init(&pool3);
    handle_pool_init(&pool255);
    handle_pool_init(&owner.pool);
    TEST_ASSERT(pool3.capacity == 3);
    TEST_ASSERT(pool255.capacity == 255);
    TEST_ASSERT(owner.pool.capacity == 4);
    TEST_ASSERT(owner.other_field == 7);
    check_invariants(&pool3);
    check_invariants(&pool255);
    check_invariants(&owner.pool);
    // Every pool got its own slots
    TEST_ASSERT(pool3.slots != pool255.slots && pool3.slots != owner.pool.slots);
}

static void test_fill_to_capacity(void) {
    handle_pool_t *pool = &pool3;
    handle_pool_init(pool);
    TEST_ASSERT(handle_pool_count(pool) == 0);

    // The first allocs get the low indices, in order
    for (uint8_t index = 0; index < 3; index++) {
        handle_t handle = handle_pool_alloc(pool);
        TEST_ASSERT(handle != HANDLE_NONE);
        TEST_ASSERT(handle_index(handle) == index);
        TEST_ASSERT(generation_of(handle) == 1);
        TEST_ASSERT(handle_pool_is_live(pool, handle));
        check_invariants(pool);
    }
    TEST_ASSERT(handle_pool_count(pool) == 3);

    // Full: the extra alloc fails without changing anything
    handle_pool_t before = *pool;
    handle_pool_slot_t slots_before[3] = {pool->slots[0], pool->slots[1], pool->slots[2]};
    TEST_ASSERT(handle_pool_alloc(pool) == HANDLE_NONE);
    TEST_ASSERT(pool->count == before.count && pool->free_head == before.free_head &&
                pool->free_tail == before.free_tail);
    for (size_t index = 0; index < 3; index++) {
        TEST_ASSERT(pool->slots[index].generation == slots_before[index].generation);
        TEST_ASSERT(pool->slots[index].next_free == slots_before[index].next_free);
    }
}

static void test_free_and_reuse(void) {
    handle_pool_t *pool = &pool3;
    handle_pool_init(pool);

    handle_t a = handle_pool_alloc(pool);
    handle_t b = handle_pool_alloc(pool);
    handle_t c = handle_pool_alloc(pool);

    TEST_ASSERT(handle_pool_free(pool, b));
    check_invariants(pool);
    TEST_ASSERT(!handle_pool_is_live(pool, b));
    TEST_ASSERT(handle_pool_is_live(pool, a) && handle_pool_is_live(pool, c));
    TEST_ASSERT(handle_pool_count(pool) == 2);

    // Double free fails
    TEST_ASSERT(!handle_pool_free(pool, b));
    TEST_ASSERT(handle_pool_count(pool) == 2);

    // The freed slot is reused, with a new generation, so the old handle stays stale
    handle_t d = handle_pool_alloc(pool);
    TEST_ASSERT(handle_index(d) == handle_index(b));
    TEST_ASSERT(generation_of(d) == 2);
    TEST_ASSERT(d != b);
    TEST_ASSERT(handle_pool_is_live(pool, d));
    TEST_ASSERT(!handle_pool_is_live(pool, b));
    TEST_ASSERT(!handle_pool_free(pool, b)); // the stale handle can't free its replacement
    TEST_ASSERT(handle_pool_is_live(pool, d));
    check_invariants(pool);

    // Freed slots are reused oldest-freed first
    TEST_ASSERT(handle_pool_free(pool, a));
    TEST_ASSERT(handle_pool_free(pool, c));
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == handle_index(a));
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == handle_index(c));
    check_invariants(pool);
}

// Freeing into a partly free list goes to the back, behind slots that were never used
static void test_freed_slot_goes_to_back(void) {
    handle_pool_t *pool = &pool3;
    handle_pool_init(pool);
    handle_t a = handle_pool_alloc(pool); // index 0; 1 and 2 still free
    TEST_ASSERT(handle_pool_free(pool, a));
    check_invariants(pool);
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 1);
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 2);
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 0);
    check_invariants(pool);
}

static void test_rejects_bad_handles(void) {
    handle_pool_t *pool = &pool2;
    handle_pool_init(pool);
    handle_t a = handle_pool_alloc(pool);

    TEST_ASSERT(!handle_pool_is_live(pool, HANDLE_NONE));
    TEST_ASSERT(!handle_pool_free(pool, HANDLE_NONE));
    // Index out of range, even with a plausible generation
    TEST_ASSERT(!handle_pool_is_live(pool, handle_of(1, 2)));
    TEST_ASSERT(!handle_pool_is_live(pool, handle_of(1, 255)));
    TEST_ASSERT(!handle_pool_free(pool, handle_of(1, 255)));
    // Right index, wrong generation (0 is never valid)
    TEST_ASSERT(!handle_pool_is_live(pool, handle_of(0, handle_index(a))));
    TEST_ASSERT(!handle_pool_is_live(pool, handle_of(2, handle_index(a))));
    // A free slot, even with its current generation
    TEST_ASSERT(!handle_pool_is_live(pool, handle_of(1, 1)));
    TEST_ASSERT(handle_pool_count(pool) == 1);
    check_invariants(pool);
}

// A handle that was never set (zeroed memory) is the same as none, and matches nothing
static void test_zeroed_handle_is_none(void) {
    static handle_t forgotten;
    handle_pool_t *pool = &pool1;
    handle_pool_init(pool);
    handle_t a = handle_pool_alloc(pool); // occupies index 0
    TEST_ASSERT(forgotten == HANDLE_NONE);
    TEST_ASSERT(handle_index(forgotten) == handle_index(a));
    TEST_ASSERT(!handle_pool_is_live(pool, forgotten));
    TEST_ASSERT(!handle_pool_free(pool, forgotten));
    TEST_ASSERT(handle_pool_is_live(pool, a));
}

static void test_handle_at(void) {
    handle_pool_t *pool = &pool2;
    handle_pool_init(pool);
    handle_t a = handle_pool_alloc(pool);
    TEST_ASSERT(handle_pool_free(pool, a));
    handle_pool_alloc(pool);              // takes index 1
    handle_t b = handle_pool_alloc(pool); // takes index 0 again, generation 2

    TEST_ASSERT(handle_pool_handle_at(pool, handle_index(b)) == b);
    TEST_ASSERT(handle_pool_handle_at(pool, 2) == HANDLE_NONE);   // out of range
    TEST_ASSERT(handle_pool_handle_at(pool, 255) == HANDLE_NONE); // way out of range
    TEST_ASSERT(handle_pool_free(pool, b));
    TEST_ASSERT(handle_pool_handle_at(pool, 0) == HANDLE_NONE); // free
}

// Every one of the 255 slots can be used
static void test_largest_capacity(void) {
    handle_pool_t *pool = &pool255;
    handle_pool_init(pool);
    handle_t last = HANDLE_NONE;
    for (size_t count = 0; count < 255; count++) {
        last = handle_pool_alloc(pool);
        TEST_ASSERT(last != HANDLE_NONE);
    }
    TEST_ASSERT(handle_index(last) == 254);
    TEST_ASSERT(handle_pool_is_live(pool, last));
    TEST_ASSERT(handle_pool_count(pool) == 255);
    TEST_ASSERT(handle_pool_alloc(pool) == HANDLE_NONE);
    check_invariants(pool);

    // The highest index with the highest generation still works. (Reaching it for real
    // takes 16.7 million reuses, so set the generation directly.)
    TEST_ASSERT(handle_pool_free(pool, last));
    pool->slots[254].generation = HANDLE_GENERATION_MAX;
    last = handle_pool_alloc(pool);
    TEST_ASSERT(last == 0xFFFFFFFE);
    TEST_ASSERT(handle_pool_is_live(pool, last));
}

// Documented behaviour: after HANDLE_GENERATION_MAX the generation wraps back to 1,
// skipping 0, and a handle from the very first use of the slot looks valid again.
static void test_generation_wraps(void) {
    handle_pool_t *pool = &pool1;
    handle_pool_init(pool);

    handle_t first = handle_pool_alloc(pool);
    TEST_ASSERT(generation_of(first) == 1);
    TEST_ASSERT(handle_pool_free(pool, first));

    // Skip ahead rather than reusing the slot 16.7 million times
    pool->slots[0].generation = HANDLE_GENERATION_MAX - 1;
    handle_t handle = handle_pool_alloc(pool);
    TEST_ASSERT(generation_of(handle) == HANDLE_GENERATION_MAX - 1);
    TEST_ASSERT(handle_pool_free(pool, handle));
    handle = handle_pool_alloc(pool);
    TEST_ASSERT(generation_of(handle) == HANDLE_GENERATION_MAX);
    TEST_ASSERT(!handle_pool_is_live(pool, first));

    TEST_ASSERT(handle_pool_free(pool, handle));
    handle = handle_pool_alloc(pool);
    TEST_ASSERT(generation_of(handle) == 1); // not 0
    TEST_ASSERT(handle != HANDLE_NONE);
    TEST_ASSERT(handle_pool_is_live(pool, first));
}

// Initializing again throws everything away: every slot free and at generation 1, and the
// free list back in order, however messy the pool was.
static void test_reinit_resets_everything(void) {
    handle_pool_t *pool = &pool3;
    handle_pool_init(pool);
    handle_t a = handle_pool_alloc(pool);
    handle_t b = handle_pool_alloc(pool);
    handle_pool_alloc(pool);
    TEST_ASSERT(handle_pool_free(pool, b));
    TEST_ASSERT(handle_pool_free(pool, a));
    handle_t c = handle_pool_alloc(pool); // generation 2 somewhere
    TEST_ASSERT(generation_of(c) == 2);

    handle_pool_init(pool);
    check_invariants(pool);
    TEST_ASSERT(handle_pool_count(pool) == 0);
    TEST_ASSERT(pool->capacity == 3);
    for (uint8_t index = 0; index < 3; index++) {
        TEST_ASSERT(!in_use(pool, index));
        TEST_ASSERT(pool->slots[index].generation == 1);
    }
    TEST_ASSERT(!handle_pool_is_live(pool, c));
    for (uint8_t index = 0; index < 3; index++) {
        handle_t handle = handle_pool_alloc(pool);
        TEST_ASSERT(handle_index(handle) == index && generation_of(handle) == 1);
    }
}

// Two pools don't share anything
static void test_separate_pools(void) {
    handle_pool_init(&pool2);
    handle_pool_init(&pool2_other);
    handle_t handle = handle_pool_alloc(&pool2);
    TEST_ASSERT(handle_pool_count(&pool2_other) == 0);
    TEST_ASSERT(!handle_pool_is_live(&pool2_other, handle));
    TEST_ASSERT(handle_pool_count(&pool2) == 1);
}

// The "in use" marker is a slot pointing to itself. These check the cases where it could
// be confused with a free-list link: the end of the list (255), a single free slot, the
// highest index, and a slot freed behind the current tail.
static void test_in_use_marker(void) {
    // Fresh pool: no slot points to itself, including the last one (which points to 255)
    handle_pool_init(&pool255);
    for (uint16_t index = 0; index < 255; index++) {
        TEST_ASSERT(!in_use(&pool255, (uint8_t)index));
        TEST_ASSERT(handle_pool_handle_at(&pool255, (uint8_t)index) == HANDLE_NONE);
    }
    TEST_ASSERT(pool255.slots[254].next_free == UINT8_MAX);

    // Capacity 1: the only slot is both the head and the end of the free list
    handle_pool_t *pool = &pool1;
    handle_pool_init(pool);
    TEST_ASSERT(pool->slots[0].next_free == UINT8_MAX);
    TEST_ASSERT(!in_use(pool, 0));
    handle_t a = handle_pool_alloc(pool);
    TEST_ASSERT(pool->slots[0].next_free == 0);
    TEST_ASSERT(in_use(pool, 0));
    TEST_ASSERT(handle_pool_free(pool, a));
    TEST_ASSERT(!in_use(pool, 0)); // the only free slot, so it's the end of the list (255)
    TEST_ASSERT(pool->slots[0].next_free == UINT8_MAX);
    TEST_ASSERT(!handle_pool_is_live(pool, a));
    check_invariants(pool);

    // Free slots link to each other in any order without ever looking in use
    pool = &pool3;
    handle_pool_init(pool);
    handle_t h0 = handle_pool_alloc(pool);
    handle_t h1 = handle_pool_alloc(pool);
    handle_t h2 = handle_pool_alloc(pool);
    TEST_ASSERT(in_use(pool, 0) && in_use(pool, 1) && in_use(pool, 2));
    TEST_ASSERT(handle_pool_free(pool, h2)); // list: 2
    TEST_ASSERT(handle_pool_free(pool, h0)); // list: 2 -> 0, so slot 2 now points to 0
    TEST_ASSERT(pool->slots[2].next_free == 0);
    TEST_ASSERT(!in_use(pool, 2) && !in_use(pool, 0) && in_use(pool, 1));
    TEST_ASSERT(handle_pool_free(pool, h1)); // list: 2 -> 0 -> 1
    TEST_ASSERT(pool->slots[0].next_free == 1);
    TEST_ASSERT(!in_use(pool, 0) && !in_use(pool, 1) && !in_use(pool, 2));
    check_invariants(pool);
    // Allocating takes them back in that order, and each one becomes in use
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 2 && in_use(pool, 2));
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 0 && in_use(pool, 0));
    TEST_ASSERT(handle_index(handle_pool_alloc(pool)) == 1 && in_use(pool, 1));
    check_invariants(pool);

    // The highest index: in use it points to 254, freed (as the only free slot) to 255
    pool = &pool255;
    handle_pool_init(pool);
    handle_t last = HANDLE_NONE;
    for (uint16_t count = 0; count < 255; count++) {
        last = handle_pool_alloc(pool);
    }
    TEST_ASSERT(handle_index(last) == 254);
    TEST_ASSERT(pool->slots[254].next_free == 254);
    TEST_ASSERT(handle_pool_handle_at(pool, 254) == last);
    TEST_ASSERT(handle_pool_free(pool, last));
    TEST_ASSERT(pool->slots[254].next_free == UINT8_MAX);
    TEST_ASSERT(!in_use(pool, 254));
    TEST_ASSERT(handle_pool_handle_at(pool, 254) == HANDLE_NONE);
    check_invariants(pool);
}

// A small, repeatable pseudo-random generator (xorshift), so a failure can be re-run.
static uint32_t random_state = 12345;
static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

// Random allocs and frees, compared against a simple list of live handles. After every
// step, every slot's in-use state must match the list exactly.
static void test_random_against_model(handle_pool_t *pool, int steps) {
    handle_pool_init(pool);
    const uint8_t capacity = pool->capacity;

    handle_t live[HANDLE_POOL_MAX_CAPACITY];
    size_t live_count = 0;
    handle_t dead[64];
    size_t dead_count = 0;

    for (int step = 0; step < steps; step++) {
        if (next_random() % 2 == 0) {
            handle_t handle = handle_pool_alloc(pool);
            if (live_count == capacity) {
                TEST_ASSERT(handle == HANDLE_NONE);
            } else {
                TEST_ASSERT(handle != HANDLE_NONE);
                for (size_t index = 0; index < live_count; index++) {
                    TEST_ASSERT(handle_index(live[index]) != handle_index(handle));
                }
                live[live_count++] = handle;
            }
        } else if (live_count > 0) {
            size_t pick = next_random() % live_count;
            handle_t handle = live[pick];
            TEST_ASSERT(handle_pool_free(pool, handle));
            live[pick] = live[--live_count];
            dead[dead_count++ % 64] = handle;
        }

        check_invariants(pool);
        TEST_ASSERT(handle_pool_count(pool) == live_count);

        // Exactly the live handles' slots are in use, and handle_at gives them back
        bool expected_in_use[HANDLE_POOL_MAX_CAPACITY] = {};
        for (size_t index = 0; index < live_count; index++) {
            TEST_ASSERT(handle_pool_is_live(pool, live[index]));
            expected_in_use[handle_index(live[index])] = true;
            TEST_ASSERT(handle_pool_handle_at(pool, handle_index(live[index])) == live[index]);
        }
        for (uint8_t index = 0; index < capacity; index++) {
            TEST_ASSERT(in_use(pool, index) == expected_in_use[index]);
        }

        // Recently freed handles stay stale even after their slot is reused
        for (size_t index = 0; index < (dead_count < 64 ? dead_count : 64); index++) {
            TEST_ASSERT(!handle_pool_is_live(pool, dead[index]));
        }
    }
}

int main(void) {
    test_sizes_and_nesting();
    test_fill_to_capacity();
    test_free_and_reuse();
    test_freed_slot_goes_to_back();
    test_rejects_bad_handles();
    test_zeroed_handle_is_none();
    test_handle_at();
    test_largest_capacity();
    test_generation_wraps();
    test_separate_pools();
    test_reinit_resets_everything();
    test_in_use_marker();
    test_random_against_model(&pool1, 2000);
    test_random_against_model(&pool3, 20000);
    test_random_against_model(&pool16, 100000);
    test_random_against_model(&pool255, 100000);
    return 0;
}
