#include "lib/collections/time_heap.h"
#include "test_helpers/assert.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Heaps of several sizes. Each test calls time_heap_init on the one it uses, so they
// start fresh.
static time_heap_t heap1 = TIME_HEAP_WITH_CAPACITY(1);
static time_heap_t heap4 = TIME_HEAP_WITH_CAPACITY(4);
static time_heap_t heap7 = TIME_HEAP_WITH_CAPACITY(7);
static time_heap_t heap16 = TIME_HEAP_WITH_CAPACITY(16);
static time_heap_t heap255 = TIME_HEAP_WITH_CAPACITY(255);

// The heap's order, written out separately: earlier deadline, then lower id.
static bool entry_before(time_heap_entry_t a, time_heap_entry_t b) {
    if (a.deadline != b.deadline) {
        return a.deadline < b.deadline;
    }
    return a.id < b.id;
}

// Check everything the heap promises: items and pos agree, each id appears once, ids not
// in the heap are marked absent, and no child comes before its parent.
static void check_invariants(const time_heap_t *heap) {
    TEST_ASSERT(heap->len <= heap->capacity);
    size_t present = 0;
    for (uint8_t id = 0; id < heap->capacity; id++) {
        uint8_t position = heap->pos[id];
        if (position != UINT8_MAX) {
            TEST_ASSERT(position < heap->len);
            TEST_ASSERT(heap->items[position].id == id);
            present++;
        }
    }
    TEST_ASSERT(present == heap->len);
    for (uint16_t position = 1; position < heap->len; position++) {
        TEST_ASSERT(!entry_before(heap->items[position], heap->items[(position - 1) / 2]));
    }
}

// The most comparisons one operation may make on a heap of n entries: at most one per
// level going up, or two per level going down (which child is first, then child vs
// parent), plus one to pick the direction. log2(n) levels, rounded down.
static unsigned long max_comparisons(uint16_t n) {
    unsigned long levels = 0;
    while (n > 1) {
        n /= 2;
        levels++;
    }
    return 2 * levels + 1;
}

static void test_empty(void) {
    time_heap_t *h = &heap4;
    time_heap_init(h);
    check_invariants(h);
    time_heap_entry_t out = {.deadline = 42, .id = 42};
    TEST_ASSERT(time_heap_len(h) == 0);
    TEST_ASSERT(!time_heap_peek(h, &out));
    TEST_ASSERT(!time_heap_pop(h, &out));
    TEST_ASSERT(out.deadline == 42 && out.id == 42); // left alone
    TEST_ASSERT(!time_heap_remove(h, 0));
    TEST_ASSERT(!time_heap_update(h, 0, 5));
    TEST_ASSERT(!time_heap_contains(h, 0));
}

static void test_single_and_capacity_one(void) {
    time_heap_t *h = &heap1;
    time_heap_init(h);
    time_heap_entry_t out;
    TEST_ASSERT(!time_heap_insert(h, 1, 100)); // out of range
    TEST_ASSERT(time_heap_insert(h, 0, 100));
    TEST_ASSERT(!time_heap_insert(h, 0, 50)); // duplicate, even with a different deadline
    TEST_ASSERT(time_heap_len(h) == 1);
    TEST_ASSERT(time_heap_contains(h, 0));
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 0 && out.deadline == 100);
    TEST_ASSERT(time_heap_update(h, 0, 200));
    TEST_ASSERT(time_heap_peek(h, &out) && out.deadline == 200);
    TEST_ASSERT(time_heap_pop(h, &out) && out.id == 0 && out.deadline == 200);
    TEST_ASSERT(time_heap_len(h) == 0);
    TEST_ASSERT(!time_heap_contains(h, 0));
    check_invariants(h);
}

static void test_rejects_bad_ids(void) {
    time_heap_t *h = &heap4;
    time_heap_init(h);
    TEST_ASSERT(!time_heap_insert(h, 4, 1));
    TEST_ASSERT(!time_heap_insert(h, 255, 1));
    TEST_ASSERT(!time_heap_contains(h, 200));
    TEST_ASSERT(!time_heap_remove(h, 200));
    TEST_ASSERT(!time_heap_update(h, 200, 1));
    TEST_ASSERT(time_heap_len(h) == 0);
}

static void test_pops_in_order(void) {
    time_heap_t *h = &heap7;
    time_heap_init(h);
    const millis_t deadlines[6] = {50, 10, 40, 20, 60, 30};
    for (uint8_t id = 0; id < 6; id++) {
        TEST_ASSERT(time_heap_insert(h, id, deadlines[id]));
        check_invariants(h);
    }
    const uint8_t expected[6] = {1, 3, 5, 2, 0, 4};
    for (size_t index = 0; index < 6; index++) {
        time_heap_entry_t out;
        TEST_ASSERT(time_heap_pop(h, &out));
        TEST_ASSERT(out.id == expected[index]);
        TEST_ASSERT(out.deadline == deadlines[out.id]);
        check_invariants(h);
    }
}

static void test_equal_deadlines_break_ties_by_id(void) {
    time_heap_t *h = &heap7;
    time_heap_init(h);
    const uint8_t order[5] = {3, 0, 4, 1, 2};
    for (size_t index = 0; index < 5; index++) {
        TEST_ASSERT(time_heap_insert(h, order[index], 7));
    }
    for (uint8_t id = 0; id < 5; id++) {
        time_heap_entry_t out;
        TEST_ASSERT(time_heap_pop(h, &out) && out.id == id);
    }
}

// Deadlines use all 64 bits: values above 2^32, and values any distance apart, come out in
// plain number order. (With a 32-bit clock, 0xFFFFFFF0 to 0x100000010 was "the wrap".)
static void test_full_64_bit_range(void) {
    time_heap_t *h = &heap7;
    time_heap_init(h);
    TEST_ASSERT(time_heap_insert(h, 0, UINT64_MAX));
    TEST_ASSERT(time_heap_insert(h, 1, 0x100000010)); // just past the old 32-bit wrap
    TEST_ASSERT(time_heap_insert(h, 2, 0xFFFFFFF0));  // just before it
    TEST_ASSERT(time_heap_insert(h, 3, 0));
    TEST_ASSERT(time_heap_insert(h, 4, UINT64_C(1) << 63)); // far more than 24.8 days apart
    TEST_ASSERT(time_heap_insert(h, 5, 0x80000000));        // was ambiguous against 0
    TEST_ASSERT(time_heap_insert(h, 6, 0x100000000));       // exactly 2^32
    check_invariants(h);
    const uint8_t expected[7] = {3, 5, 2, 6, 1, 4, 0};
    const millis_t expected_deadline[7] = {
        0, 0x80000000, 0xFFFFFFF0, 0x100000000, 0x100000010, UINT64_C(1) << 63, UINT64_MAX,
    };
    for (size_t index = 0; index < 7; index++) {
        time_heap_entry_t out;
        TEST_ASSERT(time_heap_pop(h, &out));
        TEST_ASSERT(out.id == expected[index]);
        TEST_ASSERT(out.deadline == expected_deadline[index]); // all 64 bits came back
        check_invariants(h);
    }
}

// Deadlines that differ only above bit 31 must not be mistaken for equal. If anything cut
// them down to 32 bits, all three would read as 5 and pop in id order (0, 1, 2) instead.
static void test_high_bits_matter(void) {
    time_heap_t *h = &heap4;
    time_heap_init(h);
    TEST_ASSERT(time_heap_insert(h, 0, 0x200000005));
    TEST_ASSERT(time_heap_insert(h, 1, 0x100000005));
    TEST_ASSERT(time_heap_insert(h, 2, 0x000000005));
    time_heap_entry_t out;
    TEST_ASSERT(time_heap_pop(h, &out) && out.id == 2 && out.deadline == 5);
    TEST_ASSERT(time_heap_pop(h, &out) && out.id == 1 && out.deadline == 0x100000005);
    TEST_ASSERT(time_heap_pop(h, &out) && out.id == 0 && out.deadline == 0x200000005);

    // The same through update: moving a deadline across 2^32 in either direction
    TEST_ASSERT(time_heap_insert(h, 0, 0x0FFFFFFFF));
    TEST_ASSERT(time_heap_insert(h, 1, 0x100000000));
    TEST_ASSERT(time_heap_update(h, 0, 0x100000001)); // now later than id 1
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 1);
    TEST_ASSERT(time_heap_update(h, 1, 0x200000000)); // now later than id 0
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 0 && out.deadline == 0x100000001);
    check_invariants(h);
}

// Builds this exact heap (by inserting ids in order, since it's already valid):
//
//   position:  0   1   2   3   4   5   6
//   id:        0   1   2   3   4   5   6
//   deadline:  1  10   2  11  12   3   4
static void make_seven(time_heap_t *h) {
    time_heap_init(h);
    const millis_t deadlines[7] = {1, 10, 2, 11, 12, 3, 4};
    for (uint8_t id = 0; id < 7; id++) {
        TEST_ASSERT(time_heap_insert(h, id, deadlines[id]));
    }
    for (uint8_t id = 0; id < 7; id++) {
        TEST_ASSERT(h->items[id].id == id);
    }
}

static void test_remove_positions(void) {
    time_heap_t *h = &heap7;
    time_heap_entry_t out;

    // Removing the root
    make_seven(h);
    TEST_ASSERT(time_heap_remove(h, 0));
    check_invariants(h);
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 2);

    // Removing the last leaf: nothing else moves
    make_seven(h);
    TEST_ASSERT(time_heap_remove(h, 6));
    check_invariants(h);
    for (uint8_t id = 0; id < 6; id++) {
        TEST_ASSERT(h->pos[id] == id);
    }

    // Removing id 3 (under deadline 10): the last leaf, id 6 (deadline 4), fills the gap
    // and has to move UP past its new parent
    make_seven(h);
    TEST_ASSERT(time_heap_remove(h, 3));
    check_invariants(h);
    TEST_ASSERT(h->pos[6] == 1);
    TEST_ASSERT(h->pos[1] == 3);

    // Removing id 1 (internal): id 6 (deadline 4) fills the gap and still beats its new
    // children (11 and 12), so it stays. Then a later deadline makes it sift down.
    make_seven(h);
    TEST_ASSERT(time_heap_remove(h, 1));
    check_invariants(h);
    TEST_ASSERT(h->pos[6] == 1);
    TEST_ASSERT(time_heap_update(h, 6, 100));
    check_invariants(h);
    TEST_ASSERT(h->pos[6] >= 3); // moved down to a leaf
}

static void test_update_both_ways(void) {
    time_heap_t *h = &heap7;
    time_heap_entry_t out;
    make_seven(h);

    // A leaf becomes the earliest
    TEST_ASSERT(time_heap_update(h, 4, 0));
    check_invariants(h);
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 4 && out.deadline == 0);

    // The root becomes the latest
    TEST_ASSERT(time_heap_update(h, 4, 1000));
    check_invariants(h);
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 0);
    TEST_ASSERT(h->items[h->pos[4]].deadline == 1000);

    // The same deadline again is fine too
    TEST_ASSERT(time_heap_update(h, 2, 2));
    check_invariants(h);
}

// Every one of the 255 ids can be used, including id 254
static void test_largest_capacity(void) {
    time_heap_t *h = &heap255;
    time_heap_init(h);
    // Insert with ever-earlier deadlines, so every insert climbs all the way to the root
    for (uint16_t id = 0; id < 255; id++) {
        TEST_ASSERT(time_heap_insert(h, (uint8_t)id, (millis_t)(255 - id)));
    }
    check_invariants(h);
    TEST_ASSERT(time_heap_len(h) == 255);
    time_heap_entry_t out;
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == 254);
    for (uint16_t expected = 255; expected-- > 0;) {
        TEST_ASSERT(time_heap_pop(h, &out) && out.id == expected);
    }
    check_invariants(h);
}

// No operation scans the heap: on a full heap of 255, each one makes at most 15
// comparisons (2 per level for 7 levels, plus 1), where a scan would make up to 254.
static void test_operations_stay_logarithmic(void) {
    time_heap_t *h = &heap255;
    const unsigned long allowed = max_comparisons(255);
    TEST_ASSERT(allowed == 15);
    time_heap_entry_t out;
    unsigned long before;

    // Worst case for insert: each new id comes first, so it climbs to the root
    time_heap_init(h);
    for (uint16_t id = 0; id < 255; id++) {
        before = time_heap_comparisons;
        TEST_ASSERT(time_heap_insert(h, (uint8_t)id, (millis_t)(1000 - id)));
        TEST_ASSERT(time_heap_comparisons - before <= allowed);
    }

    // Worst case for update: the first entry becomes the last, so it sinks to a leaf; then
    // a leaf becomes the first, so it climbs back
    TEST_ASSERT(time_heap_peek(h, &out));
    before = time_heap_comparisons;
    TEST_ASSERT(time_heap_update(h, out.id, 5000));
    TEST_ASSERT(time_heap_comparisons - before <= allowed);
    uint8_t leaf = h->items[254].id;
    before = time_heap_comparisons;
    TEST_ASSERT(time_heap_update(h, leaf, 0));
    TEST_ASSERT(time_heap_comparisons - before <= allowed);
    TEST_ASSERT(time_heap_peek(h, &out) && out.id == leaf);

    // Removing from the middle, and popping everything
    before = time_heap_comparisons;
    TEST_ASSERT(time_heap_remove(h, h->items[100].id));
    TEST_ASSERT(time_heap_comparisons - before <= allowed);
    while (time_heap_len(h) > 0) {
        before = time_heap_comparisons;
        TEST_ASSERT(time_heap_pop(h, &out));
        TEST_ASSERT(time_heap_comparisons - before <= allowed);
    }
    check_invariants(h);
}

// pop_if_expired only takes the earliest entry, and only once its deadline has arrived
static void test_pop_if_expired(void) {
    time_heap_t *h = &heap7;
    time_heap_entry_t out = {.deadline = 42, .id = 42};

    // Empty: nothing to pop, whatever the time
    time_heap_init(h);
    TEST_ASSERT(!time_heap_pop_if_expired(h, &out, UINT64_MAX));
    TEST_ASSERT(out.deadline == 42 && out.id == 42); // left alone

    TEST_ASSERT(time_heap_insert(h, 0, 300));
    TEST_ASSERT(time_heap_insert(h, 1, 100));
    TEST_ASSERT(time_heap_insert(h, 2, 200));
    TEST_ASSERT(time_heap_insert(h, 3, 200));

    // Not due yet: nothing is removed
    TEST_ASSERT(!time_heap_pop_if_expired(h, &out, 0));
    TEST_ASSERT(!time_heap_pop_if_expired(h, &out, 99));
    TEST_ASSERT(time_heap_len(h) == 4);
    TEST_ASSERT(out.id == 42);

    // Exactly at the deadline counts as due
    TEST_ASSERT(time_heap_pop_if_expired(h, &out, 100));
    TEST_ASSERT(out.id == 1 && out.deadline == 100);
    TEST_ASSERT(!time_heap_pop_if_expired(h, &out, 100)); // the next one is at 200
    check_invariants(h);

    // Well past several deadlines: they come out one per call, earliest first, ties by id
    TEST_ASSERT(time_heap_pop_if_expired(h, &out, 250) && out.id == 2 && out.deadline == 200);
    TEST_ASSERT(time_heap_pop_if_expired(h, &out, 250) && out.id == 3 && out.deadline == 200);
    TEST_ASSERT(!time_heap_pop_if_expired(h, &out, 250)); // id 0 is at 300
    TEST_ASSERT(time_heap_len(h) == 1 && time_heap_contains(h, 0));
    check_invariants(h);

    // Far in the future, with 64-bit times
    TEST_ASSERT(time_heap_insert(h, 4, UINT64_C(1) << 40));
    TEST_ASSERT(time_heap_pop_if_expired(h, &out, UINT64_C(1) << 40) && out.id == 0);
    TEST_ASSERT(time_heap_pop_if_expired(h, &out, UINT64_C(1) << 40) && out.id == 4);
    TEST_ASSERT(time_heap_len(h) == 0);
}

static uint32_t random_state = 98765;
static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint64_t next_random64(void) {
    uint64_t high = next_random();
    return (high << 32) | next_random();
}

// Random inserts, removes, updates and pops, checked against a plain list of which ids are
// in and their deadlines, where the earliest is found the slow way by looking at every one.
//
// Deadlines are start + a random offset below spread, or any 64-bit value at all when
// spread is 0, so the runs below cover small and huge spreads and values far above 2^32.
static void test_random_against_model(time_heap_t *heap, millis_t start, millis_t spread) {
    time_heap_init(heap);
    const uint8_t capacity = heap->capacity;
    bool in_model[TIME_HEAP_MAX_CAPACITY] = {};
    millis_t deadline[TIME_HEAP_MAX_CAPACITY] = {};
    size_t model_len = 0;

    for (int step = 0; step < 50000; step++) {
        uint8_t id = (uint8_t)(next_random() % capacity);
        millis_t new_deadline = spread == 0 ? next_random64() : start + next_random64() % spread;
        const unsigned long allowed = max_comparisons((uint16_t)(heap->len + 1));
        const unsigned long before = time_heap_comparisons;

        switch (next_random() % 4) {
        case 0: // insert
            TEST_ASSERT(time_heap_insert(heap, id, new_deadline) == !in_model[id]);
            if (!in_model[id]) {
                in_model[id] = true;
                deadline[id] = new_deadline;
                model_len++;
            }
            break;
        case 1: // remove
            TEST_ASSERT(time_heap_remove(heap, id) == in_model[id]);
            if (in_model[id]) {
                in_model[id] = false;
                model_len--;
            }
            break;
        case 2: // reschedule
            TEST_ASSERT(time_heap_update(heap, id, new_deadline) == in_model[id]);
            if (in_model[id]) {
                deadline[id] = new_deadline;
            }
            break;
        default: { // pop
            bool found = false;
            uint8_t expected = 0;
            for (uint8_t candidate = 0; candidate < capacity; candidate++) {
                if (in_model[candidate] &&
                    (!found || deadline[candidate] < deadline[expected] ||
                     (deadline[candidate] == deadline[expected] && candidate < expected))) {
                    expected = candidate;
                    found = true;
                }
            }
            time_heap_entry_t out;
            TEST_ASSERT(time_heap_pop(heap, &out) == found);
            if (found) {
                TEST_ASSERT(out.id == expected);
                TEST_ASSERT(out.deadline == deadline[expected]);
                in_model[expected] = false;
                model_len--;
            }
            break;
        }
        }

        TEST_ASSERT(time_heap_comparisons - before <= allowed);
        check_invariants(heap);
        TEST_ASSERT(heap->len == model_len);
        for (uint8_t candidate = 0; candidate < capacity; candidate++) {
            TEST_ASSERT(time_heap_contains(heap, candidate) == in_model[candidate]);
            if (in_model[candidate]) {
                TEST_ASSERT(heap->items[heap->pos[candidate]].deadline == deadline[candidate]);
            }
        }
    }
}

int main(void) {
    test_empty();
    test_single_and_capacity_one();
    test_rejects_bad_ids();
    test_pops_in_order();
    test_equal_deadlines_break_ties_by_id();
    test_full_64_bit_range();
    test_high_bits_matter();
    test_remove_positions();
    test_update_both_ways();
    test_largest_capacity();
    test_operations_stay_logarithmic();
    test_pop_if_expired();

    test_random_against_model(&heap16, 0, 1000);
    test_random_against_model(&heap16, 0, 4); // lots of equal deadlines
    // Around and far above the old 32-bit limit, up to the top of the range, and anywhere
    test_random_against_model(&heap16, 0xFFFFFF00, 1000);
    test_random_against_model(&heap255, UINT64_C(1) << 40, 0x20000);
    test_random_against_model(&heap16, UINT64_MAX - 1000, 1000);
    test_random_against_model(&heap255, 0, 0);
    test_random_against_model(&heap1, 0, 0);
    return 0;
}
