#pragma once
/**
 * @file time_heap.h
 * @brief A queue of small ids ordered by deadline, that can also remove or reschedule any id.
 *
 * Each id (0 to capacity - 1, at most once) is stored with a deadline, and the heap can
 * always say which comes first. Typically an id is a handle_index from a handle_pool, so
 * your own data for it lives in an array at that index.
 *
 *     static time_heap_t queue = TIME_HEAP_WITH_CAPACITY(20);
 *
 *     time_heap_init(&queue);
 *     time_heap_insert(&queue, 3, timer_now() + 500);
 *     time_heap_entry_t first;
 *     if (time_heap_peek(&queue, &first)) { ... first.id, first.deadline ... }
 *
 * How it works: `items` is a binary min-heap stored in an array. Position 0 is the
 * earliest entry; the children of position p are at 2p + 1 and 2p + 2, and no child comes
 * before its parent. Adding or removing an entry swaps it up or down one level at a time,
 * so it only touches about log2(N) positions, never the whole array.
 *
 * A plain heap can only remove the first entry quickly. To also remove any id (say, a
 * cancelled timer), `pos` records where each id currently sits in `items`, so there is
 * nothing to search for. Every move updates both arrays together.
 *
 * | Operation                       | Cost           |
 * |---------------------------------|----------------|
 * | peek, contains, len             | O(1)           |
 * | insert, remove, pop, update     | O(log N)       |
 *
 * Order: earlier deadline first; equal deadlines go to the lower id, so the order is
 * always the same for the same entries. Deadlines are 64-bit millis_t, which never wraps
 * in practice, so any two deadlines can be compared, however far apart.
 *
 * Rules:
 * - Capacity is 1 to 255 (checked when the program is compiled).
 * - Not interrupt-safe: use it from the main loop only.
 *
 * Pure C with no hardware access, so it can be unit tested on the Mac.
 */

#include "lib/hardware/timer.h"
#include <stdint.h>

/** @brief The most ids a heap can hold. Position 255 is kept to mean "not in the heap". */
#define TIME_HEAP_MAX_CAPACITY 255

/** @brief One id and its deadline. */
typedef struct time_heap_entry_t {
    millis_t deadline;
    uint8_t id;
} time_heap_entry_t;

/** @brief A time heap. Create it with TIME_HEAP_WITH_CAPACITY, then time_heap_init. */
typedef struct time_heap_t {
    time_heap_entry_t *items; ///< The heap order: items[0] is the earliest.
    uint8_t *pos;             ///< pos[id] = where id is in items, or 255 when it isn't.
    uint8_t capacity;         ///< Length of items and pos, and 1 + the largest id.
    uint8_t len;              ///< Number of entries in the heap.
} time_heap_t;

// Array size for slot_count entries, or -1 (a compile error, "array size is negative")
// when slot_count is out of range.
#define TIME_HEAP_CHECKED_SIZE(slot_count)                                                         \
    ((slot_count) >= 1 && (slot_count) <= TIME_HEAP_MAX_CAPACITY ? (slot_count) : -1)

/**
 * @brief Initializer for a heap with room for ids 0 to `slot_count` - 1, storage included.
 *
 *     static time_heap_t queue = TIME_HEAP_WITH_CAPACITY(20);
 *
 * Use it for a heap outside any function, or inside a struct that is: the storage then
 * lasts as long as the program. A capacity outside 1 to 255 fails to compile with
 * "array size is negative".
 */
#define TIME_HEAP_WITH_CAPACITY(slot_count)                                                        \
    {                                                                                              \
        .items = (time_heap_entry_t[TIME_HEAP_CHECKED_SIZE(slot_count)]){},                        \
        .pos = (uint8_t[TIME_HEAP_CHECKED_SIZE(slot_count)]){},                                    \
        .capacity = (slot_count),                                                                  \
    }

/** @brief Empty the heap. Call before use; takes O(capacity). */
void time_heap_init(time_heap_t *heap);

/** @brief Number of entries in the heap. */
uint8_t time_heap_len(const time_heap_t *heap);

/** @brief True if id is in the heap. False for ids out of range. */
bool time_heap_contains(const time_heap_t *heap, uint8_t id);

/**
 * @brief Look at the earliest entry without removing it.
 *
 * @return false (leaving *out alone) when the heap is empty.
 */
bool time_heap_peek(const time_heap_t *heap, time_heap_entry_t *out);

/**
 * @brief Add an id with its deadline.
 *
 * @return false, changing nothing, if id is out of range or already in the heap.
 */
bool time_heap_insert(time_heap_t *heap, uint8_t id, millis_t deadline);

/**
 * @brief Remove any id, wherever it is in the heap.
 *
 * @return false, changing nothing, if id isn't in the heap.
 */
bool time_heap_remove(time_heap_t *heap, uint8_t id);

/**
 * @brief Remove the earliest entry and hand it back.
 *
 * @return false (leaving *out alone) when the heap is empty.
 */
bool time_heap_pop(time_heap_t *heap, time_heap_entry_t *out);

/**
 * @brief Remove the earliest entry if its deadline is at or before now, and hand it back.
 *
 * @return false (leaving *out alone) when the heap is empty or nothing is due yet.
 */
bool time_heap_pop_if_expired(time_heap_t *heap, time_heap_entry_t *out, millis_t now);

/**
 * @brief Give an id already in the heap a new deadline (to reschedule a timer).
 *
 * @return false, changing nothing, if id isn't in the heap.
 */
bool time_heap_update(time_heap_t *heap, uint8_t id, millis_t deadline);

#ifndef __AVR__
/**
 * @brief TEST-ONLY, Mac builds only: how many times two entries have been compared, ever.
 *
 * The tests read it before and after an operation to check the heap stays fast (about
 * log2(N) comparisons, never a scan of every entry). It doesn't exist in the chip build,
 * so firmware code can't use it. See COUNT_COMPARISON in time_heap.c.
 */
extern unsigned long time_heap_comparisons;
#endif
