#include "time_heap.h"

// In pos: "this id is not in the heap". Never a real position, since there are at most 255.
#define NOT_IN_HEAP UINT8_MAX

// TEST-ONLY: counting comparisons, to prove the heap is fast.
//
// Every heap operation should compare only about log2(N) entries, never all N. A heap
// that scanned every entry would still give the right answers, so ordinary tests can't
// catch that. Instead, entry_before calls COUNT_COMPARISON() each time it runs, and the
// tests check the count goes up by at most 2 * log2(N) + 1 per operation (15 with 255
// entries, where a scan would need up to 254).
//
// avr-gcc defines __AVR__, so on the chip the macro is ((void)0), "do nothing": no
// counter, no code, no cost. Only Mac builds (the tests) actually count.
#ifdef __AVR__
#define COUNT_COMPARISON() ((void)0)
#else
unsigned long time_heap_comparisons;
#define COUNT_COMPARISON() (time_heap_comparisons++)
#endif

// Positions are worked out in uint16_t: with up to 255 entries, 2 * position + 2 can reach
// 510, which doesn't fit in a uint8_t. Every position actually stored is below capacity,
// so it fits back into a uint8_t.

// The heap's order: earlier deadline first, then lower id.
static bool entry_before(time_heap_entry_t a, time_heap_entry_t b) {
    COUNT_COMPARISON();
    if (a.deadline != b.deadline) {
        return a.deadline < b.deadline;
    }
    return a.id < b.id;
}

// Put an entry at a position, and record that position for its id. Every move goes
// through here so that items and pos can never disagree.
static void place(time_heap_t *heap, uint16_t position, time_heap_entry_t entry) {
    heap->items[position] = entry;
    heap->pos[entry.id] = (uint8_t)position;
}

// The entry at position moves up past every parent that it comes before.
//
// Rather than swapping at each step, hold the moving entry aside, shift each parent it
// beats down one level into the gap, then drop the entry into the final gap. Same result
// as swapping, with half the writes.
static void sift_up(time_heap_t *heap, uint16_t position) {
    time_heap_entry_t entry = heap->items[position];
    while (position > 0) {
        uint16_t parent = (uint16_t)((position - 1) / 2);
        if (!entry_before(entry, heap->items[parent])) {
            break;
        }
        place(heap, position, heap->items[parent]);
        position = parent;
    }
    place(heap, position, entry);
}

// The entry at position moves down past every child that comes before it, the same way.
static void sift_down(time_heap_t *heap, uint16_t position) {
    time_heap_entry_t entry = heap->items[position];
    for (;;) {
        uint16_t child = (uint16_t)(2 * position + 1);
        if (child >= heap->len) {
            break;
        }
        // Follow whichever child comes first, so it can be the parent of the other
        uint16_t right = (uint16_t)(child + 1);
        if (right < heap->len && entry_before(heap->items[right], heap->items[child])) {
            child = right;
        }
        if (!entry_before(heap->items[child], entry)) {
            break;
        }
        place(heap, position, heap->items[child]);
        position = child;
    }
    place(heap, position, entry);
}

// The entry at position may be out of order in either direction: if it comes before its
// parent it can only need to go up, otherwise it can only need to go down.
static void repair(time_heap_t *heap, uint16_t position) {
    if (position > 0 && entry_before(heap->items[position], heap->items[(position - 1) / 2])) {
        sift_up(heap, position);
    } else {
        sift_down(heap, position);
    }
}

void time_heap_init(time_heap_t *heap) {
    for (uint8_t id = 0; id < heap->capacity; id++) {
        heap->pos[id] = NOT_IN_HEAP;
    }
    heap->len = 0;
}

uint8_t time_heap_len(const time_heap_t *heap) {
    return heap->len;
}

bool time_heap_contains(const time_heap_t *heap, uint8_t id) {
    // Range check first: pos only has capacity entries
    return id < heap->capacity && heap->pos[id] != NOT_IN_HEAP;
}

bool time_heap_peek(const time_heap_t *heap, time_heap_entry_t *out) {
    if (heap->len == 0) {
        return false;
    }
    *out = heap->items[0];
    return true;
}

bool time_heap_insert(time_heap_t *heap, uint8_t id, millis_t deadline) {
    // Each id is unique and below capacity, so if it isn't already here there is room
    if (id >= heap->capacity || heap->pos[id] != NOT_IN_HEAP) {
        return false;
    }

    // Add it as the last leaf, then let it climb to where it belongs
    uint16_t position = heap->len;
    heap->len++;
    place(heap, position, (time_heap_entry_t){.deadline = deadline, .id = id});
    sift_up(heap, position);
    return true;
}

bool time_heap_remove(time_heap_t *heap, uint8_t id) {
    if (!time_heap_contains(heap, id)) {
        return false;
    }

    uint16_t position = heap->pos[id];
    heap->pos[id] = NOT_IN_HEAP;
    heap->len--;

    // The last leaf fills the gap (unless the removed id *was* the last leaf). It came from
    // elsewhere in the heap, so it may belong higher or lower than the gap.
    if (position != heap->len) {
        place(heap, position, heap->items[heap->len]);
        repair(heap, position);
    }
    return true;
}

bool time_heap_pop(time_heap_t *heap, time_heap_entry_t *out) {
    time_heap_entry_t first;
    if (!time_heap_peek(heap, &first)) {
        return false;
    }
    time_heap_remove(heap, first.id);
    *out = first;
    return true;
}

bool time_heap_update(time_heap_t *heap, uint8_t id, millis_t deadline) {
    if (!time_heap_contains(heap, id)) {
        return false;
    }
    uint16_t position = heap->pos[id];
    heap->items[position].deadline = deadline;
    repair(heap, position);
    return true;
}

bool time_heap_pop_if_expired(time_heap_t *heap, time_heap_entry_t *out, millis_t now) {
    time_heap_entry_t first;
    if (!time_heap_peek(heap, &first)) {
        return false;
    }

    if (now < first.deadline) {
        return false; // not due yet
    }
    time_heap_remove(heap, first.id);
    *out = first;
    return true;
}
