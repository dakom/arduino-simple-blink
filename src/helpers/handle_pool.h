/**
 * @file handle_pool.h
 * @brief Hands out handles: small ids that can tell when the thing they named is gone.
 *
 * A handle pool answers one question: "is the thing this handle named still alive?"
 * It stores no data. Each live handle owns one numbered slot, from 0 to capacity - 1,
 * and handle_index gives you that number, so you can keep your own data in an array of
 * the same length:
 *
 *     static handle_pool_t pool = HANDLE_POOL_WITH_CAPACITY(20);
 *     static timer_t timers[20];          // your data, same length
 *
 *     handle_pool_init(&pool);
 *     handle_t h = handle_pool_alloc(&pool);
 *     if (h != HANDLE_NONE) {
 *         timers[handle_index(h)] = (timer_t){...};   // fill it in straight away
 *     }
 *     ...
 *     if (handle_pool_is_live(&pool, h)) {
 *         timers[handle_index(h)].period = 100;       // only touch it after checking
 *     }
 *     handle_pool_free(&pool, h);         // now h, and every copy of it, is stale
 *
 * Why a generation? A freed slot can be handed out again by a later alloc. An old handle
 * still has the same index, so the index alone can't tell "my timer" from "the new timer
 * that took its slot". Each slot also counts how many times it has been freed (its
 * generation); a handle remembers the count from when it was created, so a handle whose
 * count no longer matches is stale and is rejected.
 *
 * A handle is one uint32_t holding both numbers:
 *
 *     bit:   31 ............. 8   7 ..... 0
 *            [   generation    ] [  index  ]
 *
 * Generation 0 is never used, so 0 is never a valid handle: HANDLE_NONE is 0, and a
 * handle you forgot to set (zeroed memory) can't match anything.
 *
 * Every operation is O(1): free slots are kept in a linked list threaded through the
 * slot array itself, so alloc takes the first one without searching.
 *
 * Limits and rules:
 * - Capacity is 1 to 255 (checked when the program is compiled).
 * - Generations wrap from HANDLE_GENERATION_MAX (about 16.7 million) back to 1. So a
 *   handle kept while its slot is freed and reused that many times would look valid
 *   again. Freed slots go to the back of the queue, so reuse is spread over every free
 *   slot.
 * - A handle only means something to the pool that issued it, and only until that pool
 *   is initialized again.
 * - Not interrupt-safe: use it from the main loop only.
 *
 * Pure C with no hardware access, so it can be unit tested on the Mac.
 */
#ifndef HANDLE_POOL_H
#define HANDLE_POOL_H

#include <stdint.h>

/** @brief Names one slot, as it was when the handle was created. 0 means none. */
typedef uint32_t handle_t;

/** @brief A handle that never names anything. Alloc returns it when the pool is full. */
#define HANDLE_NONE ((handle_t)0)

/** @brief The largest generation (24 bits), after which it wraps back to 1. */
#define HANDLE_GENERATION_MAX ((uint32_t)0xFFFFFF)

/** @brief The most slots a pool can have. Index 255 is kept to mean "no slot". */
#define HANDLE_POOL_MAX_CAPACITY 255

/** @brief Which slot a handle names: the index into your own data array. */
[[maybe_unused]] static inline uint8_t handle_index(handle_t handle) {
    return (uint8_t)(handle & 0xFF);
}

/** @brief Bookkeeping for one slot. Created by HANDLE_POOL_WITH_CAPACITY; don't touch. */
typedef struct handle_pool_slot_t {
    uint32_t generation; ///< Bumped every time the slot is freed. 1 to HANDLE_GENERATION_MAX.
    /// While free: the next free slot, or 255 for none. While in use: its own index.
    /// (A free slot can never point to itself, since the free list never loops, so this
    /// doubles as the "in use" flag without a separate field.)
    uint8_t next_free;
} handle_pool_slot_t;

/** @brief A handle pool. Create it with HANDLE_POOL_WITH_CAPACITY, then handle_pool_init. */
typedef struct handle_pool_t {
    handle_pool_slot_t *slots; ///< capacity slots, made by HANDLE_POOL_WITH_CAPACITY.
    uint8_t capacity;          ///< Number of slots, 1 to HANDLE_POOL_MAX_CAPACITY.
    uint8_t count;             ///< Number of slots in use.
    uint8_t free_head;         ///< The free slot alloc takes next, or 255 when full.
    uint8_t free_tail;         ///< The free slot a freed one is queued after, or 255 when full.
} handle_pool_t;

// Array size for slot_count slots, or -1 (a compile error, "array size is negative") when
// slot_count is out of range.
#define HANDLE_POOL_CHECKED_SIZE(slot_count)                                                       \
    ((slot_count) >= 1 && (slot_count) <= HANDLE_POOL_MAX_CAPACITY ? (slot_count) : -1)

/**
 * @brief Initializer for a pool with room for `slot_count` handles, its slots included.
 *
 *     static handle_pool_t pool = HANDLE_POOL_WITH_CAPACITY(20);
 *
 * Use it for a pool outside any function, or inside a struct that is: the slots then
 * last as long as the program. A capacity outside 1 to 255 fails to compile with
 * "array size is negative".
 */
#define HANDLE_POOL_WITH_CAPACITY(slot_count)                                                      \
    {                                                                                              \
        .slots = (handle_pool_slot_t[HANDLE_POOL_CHECKED_SIZE(slot_count)]){},                     \
        .capacity = (slot_count),                                                                  \
    }

/**
 * @brief Make every slot free. Call before use; takes O(capacity).
 *
 * Every handle issued before is forgotten (see the rules above).
 */
void handle_pool_init(handle_pool_t *pool);

/**
 * @brief Claim a free slot.
 *
 * Fill in your own data at handle_index(handle) straight away.
 *
 * @return A handle to the slot, or HANDLE_NONE (changing nothing) when full.
 */
handle_t handle_pool_alloc(handle_pool_t *pool);

/**
 * @brief Free the slot a handle names. Every copy of the handle becomes stale.
 *
 * @return false, changing nothing, if the handle is stale, out of range or none.
 */
bool handle_pool_free(handle_pool_t *pool, handle_t handle);

/** @brief True if the handle names a slot that is in use and hasn't been reused. */
bool handle_pool_is_live(const handle_pool_t *pool, handle_t handle);

/**
 * @brief The current handle for a slot in use, found by its index.
 *
 * For code that already holds an index from a trusted place (such as a heap of indices)
 * and needs the handle back. Not a way to validate handles: use handle_pool_is_live.
 *
 * @return HANDLE_NONE if the index is out of range or the slot is free.
 */
handle_t handle_pool_handle_at(const handle_pool_t *pool, uint8_t index);

/** @brief Number of slots in use. */
uint8_t handle_pool_count(const handle_pool_t *pool);

#endif
