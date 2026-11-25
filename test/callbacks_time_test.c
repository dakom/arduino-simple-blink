#include "helpers/callbacks_time.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

// The scheduler is one global, sized by CALLBACKS_TIME_CAPACITY from Taskfile.yml (the
// tests get the same value as the chip). Every test starts by initializing it again.
#define CAPACITY CALLBACKS_TIME_CAPACITY
static_assert(CAPACITY >= 4, "these tests need CALLBACKS_TIME_CAPACITY of at least 4");

// What ran, in order: each test callback appends its own letter.
static char ran[1024];
static size_t ran_len;

static void start(void) {
    init_callbacks_time();
    ran_len = 0;
    ran[0] = '\0';
}

static void record(char letter) {
    CHECK(ran_len + 1 < sizeof ran);
    ran[ran_len++] = letter;
    ran[ran_len] = '\0';
}

static bool ran_is(const char *expected) {
    return strcmp(ran, expected) == 0;
}

static void a([[maybe_unused]] void *context) {
    record('a');
}
static void b([[maybe_unused]] void *context) {
    record('b');
}
static void c([[maybe_unused]] void *context) {
    record('c');
}
static void d([[maybe_unused]] void *context) {
    record('d');
}
static void never([[maybe_unused]] void *context) {
    record('!'); // fills a slot for tests that need one; should never be due
}

// A deadline no test ever reaches
#define FAR_FUTURE (UINT64_C(1) << 60)

// Fill every free slot with a callback that never runs; returns how many it filled
static int fill_free_slots(void) {
    int filled = 0;
    while (callbacks_time_insert(never, nullptr, FAR_FUTURE) != HANDLE_NONE) {
        filled++;
    }
    return filled;
}

// For callbacks that act on the scheduler themselves
static handle_t own_handle;    // the handle of the callback that's about to run
static handle_t other_handle;  // a different pending callback
static millis_t reschedule_at; // when a callback schedules its follow-up

static void test_nothing_scheduled(void) {
    start();
    callbacks_time_execute(0);
    callbacks_time_execute(UINT64_MAX);
    CHECK(ran_is(""));
    CHECK(fill_free_slots() == CAPACITY); // every slot is free
}

// Callbacks run once their deadline arrives (exactly at it counts), earliest first, and
// equal deadlines in the order they were scheduled (slot order)
static void test_runs_due_in_order(void) {
    start();
    CHECK(callbacks_time_insert(c, nullptr, 300) != HANDLE_NONE);
    CHECK(callbacks_time_insert(a, nullptr, 100) != HANDLE_NONE);
    CHECK(callbacks_time_insert(b, nullptr, 200) != HANDLE_NONE);
    CHECK(callbacks_time_insert(d, nullptr, 200) != HANDLE_NONE);

    callbacks_time_execute(99);
    CHECK(ran_is(""));
    callbacks_time_execute(100);
    CHECK(ran_is("a"));
    callbacks_time_execute(100); // already ran: not again
    CHECK(ran_is("a"));
    callbacks_time_execute(250);
    CHECK(ran_is("abd"));
    callbacks_time_execute(1000);
    CHECK(ran_is("abdc"));

    // Every slot was given back
    CHECK(fill_free_slots() == CAPACITY);
}

static void test_rejects_null_callback(void) {
    start();
    CHECK(callbacks_time_insert(nullptr, nullptr, 100) == HANDLE_NONE);
    CHECK(fill_free_slots() == CAPACITY); // it didn't use up a slot
}

static void test_full(void) {
    start();
    CHECK(callbacks_time_insert(a, nullptr, 100) != HANDLE_NONE);
    CHECK(fill_free_slots() == CAPACITY - 1);
    CHECK(callbacks_time_insert(b, nullptr, 50) == HANDLE_NONE); // no room

    // Once one has run, its slot can be used again
    callbacks_time_execute(100);
    CHECK(ran_is("a"));
    CHECK(callbacks_time_insert(b, nullptr, 150) != HANDLE_NONE);
    callbacks_time_execute(1000);
    CHECK(ran_is("ab"));
}

static void test_cancel(void) {
    start();
    handle_t ha = callbacks_time_insert(a, nullptr, 100);
    handle_t hb = callbacks_time_insert(b, nullptr, 100);

    CHECK(callbacks_time_cancel(ha));
    CHECK(!callbacks_time_cancel(ha)); // already cancelled
    CHECK(!callbacks_time_cancel(HANDLE_NONE));

    callbacks_time_execute(100);
    CHECK(ran_is("b"));
    CHECK(!callbacks_time_cancel(hb));    // already ran
    CHECK(fill_free_slots() == CAPACITY); // cancelling gave the slot back too
}

// A handle from a callback that already ran can't cancel the next one in its slot
static void test_stale_handle_cannot_cancel_replacement(void) {
    start();
    handle_t old = callbacks_time_insert(a, nullptr, 100);
    fill_free_slots(); // so the replacement has to reuse old's slot
    callbacks_time_execute(100);

    handle_t replacement = callbacks_time_insert(b, nullptr, 200);
    CHECK(handle_index(replacement) == handle_index(old)); // the same slot
    CHECK(replacement != old);

    CHECK(!callbacks_time_cancel(old));
    callbacks_time_execute(200);
    CHECK(ran_is("ab"));
}

// While a callback runs its own handle is already stale, so it can't cancel itself (and
// can't accidentally cancel whatever reuses its slot later)
static bool cancel_self_result;
static void cancel_self([[maybe_unused]] void *context) {
    record('s');
    cancel_self_result = callbacks_time_cancel(own_handle);
}

static void test_handle_is_stale_while_running(void) {
    start();
    own_handle = callbacks_time_insert(cancel_self, nullptr, 100);
    cancel_self_result = true;
    callbacks_time_execute(100);
    CHECK(ran_is("s"));
    CHECK(!cancel_self_result);
    CHECK(fill_free_slots() == CAPACITY);
}

// A callback can cancel another pending callback, even one due in the same call
static void cancel_other([[maybe_unused]] void *context) {
    record('x');
    CHECK(callbacks_time_cancel(other_handle));
}

static void test_callback_cancels_another(void) {
    start();
    CHECK(callbacks_time_insert(cancel_other, nullptr, 100) != HANDLE_NONE);
    other_handle = callbacks_time_insert(b, nullptr, 100);
    CHECK(callbacks_time_insert(c, nullptr, 100) != HANDLE_NONE);
    callbacks_time_execute(100);
    CHECK(ran_is("xc"));
    CHECK(fill_free_slots() == CAPACITY);
}

// A callback can schedule a follow-up into the slot it just freed: with every other slot
// taken, that's the only place it can go
static void schedule_follow_up([[maybe_unused]] void *context) {
    record('f');
    CHECK(callbacks_time_insert(b, nullptr, reschedule_at) != HANDLE_NONE);
}

static void test_callback_schedules_into_its_own_slot(void) {
    start();
    CHECK(callbacks_time_insert(schedule_follow_up, nullptr, 100) != HANDLE_NONE);
    fill_free_slots();
    reschedule_at = 200;
    callbacks_time_execute(100);
    CHECK(ran_is("f"));
    callbacks_time_execute(199);
    CHECK(ran_is("f"));
    callbacks_time_execute(200);
    CHECK(ran_is("fb"));
}

// A follow-up due right away runs in the same call (the call limit allows it)
static void test_follow_up_due_now_runs_in_same_call(void) {
    start();
    CHECK(callbacks_time_insert(schedule_follow_up, nullptr, 100) != HANDLE_NONE);
    reschedule_at = 100;
    callbacks_time_execute(100);
    CHECK(ran_is("fb"));
}

// A callback that keeps rescheduling itself for the same time would loop forever; each
// call stops after CAPACITY callbacks instead, and the next call carries on.
static int forever_calls;
static void forever([[maybe_unused]] void *context) {
    forever_calls++;
    CHECK(callbacks_time_insert(forever, nullptr, reschedule_at) != HANDLE_NONE);
}

static int counted_calls;
static void counted([[maybe_unused]] void *context) {
    counted_calls++;
}

static void test_call_limit(void) {
    start();
    forever_calls = 0;
    reschedule_at = 100;
    CHECK(callbacks_time_insert(forever, nullptr, 100) != HANDLE_NONE);
    callbacks_time_execute(100);
    CHECK(forever_calls == CAPACITY);
    callbacks_time_execute(100);
    CHECK(forever_calls == 2 * CAPACITY);

    // The limit never holds back ordinary work: every slot full, all due, all run
    start();
    counted_calls = 0;
    for (int count = 0; count < CAPACITY; count++) {
        CHECK(callbacks_time_insert(counted, nullptr, 100) != HANDLE_NONE);
    }
    callbacks_time_execute(100);
    CHECK(counted_calls == CAPACITY);
}

// A repeating timer, built by rescheduling for one period later each time
static void every_100([[maybe_unused]] void *context) {
    record('r');
    reschedule_at += 100;
    CHECK(callbacks_time_insert(every_100, nullptr, reschedule_at) != HANDLE_NONE);
}

static void test_repeating(void) {
    start();
    reschedule_at = 100;
    CHECK(callbacks_time_insert(every_100, nullptr, reschedule_at) != HANDLE_NONE);
    for (millis_t now = 0; now <= 1000; now += 10) {
        callbacks_time_execute(now);
    }
    CHECK(ran_is("rrrrrrrrrr"));              // at 100, 200, ... 1000
    CHECK(fill_free_slots() == CAPACITY - 1); // the next repeat is still pending
}

// Initializing again forgets everything: pending callbacks never run, old handles are dead
static void test_reinit(void) {
    start();
    handle_t ha = callbacks_time_insert(a, nullptr, 100);
    CHECK(callbacks_time_insert(b, nullptr, 200) != HANDLE_NONE);
    start();
    callbacks_time_execute(1000);
    CHECK(ran_is(""));
    CHECK(!callbacks_time_cancel(ha));
    CHECK(fill_free_slots() == CAPACITY);
}

// The context given to insert is handed to the callback exactly as given: one function can
// serve several things, nullptr is fine, and a reused slot never hands over an old context.
static int counters[3];
static void *last_context;
static int null_context_calls;
static void count_into(void *context) {
    last_context = context;
    if (context == nullptr) {
        null_context_calls++;
    } else {
        int *counter = context;
        (*counter)++;
    }
}

static void test_context(void) {
    start();
    counters[0] = counters[1] = counters[2] = 0;
    null_context_calls = 0;

    // One function, three different contexts, one of them cancelled
    CHECK(callbacks_time_insert(count_into, &counters[0], 100) != HANDLE_NONE);
    CHECK(callbacks_time_insert(count_into, &counters[1], 100) != HANDLE_NONE);
    handle_t cancelled = callbacks_time_insert(count_into, &counters[2], 100);
    CHECK(callbacks_time_cancel(cancelled));
    callbacks_time_execute(100);
    CHECK(counters[0] == 1 && counters[1] == 1 && counters[2] == 0);

    // nullptr is a valid context
    CHECK(callbacks_time_insert(count_into, nullptr, 200) != HANDLE_NONE);
    callbacks_time_execute(200);
    CHECK(null_context_calls == 1);
    CHECK(last_context == nullptr);

    // A slot freed by a callback with a context, then reused with nullptr: the new callback
    // gets nullptr, not the old pointer
    start();
    null_context_calls = 0;
    handle_t first = callbacks_time_insert(count_into, &counters[0], 100);
    fill_free_slots(); // so the next insert has to reuse first's slot
    callbacks_time_execute(100);
    handle_t reused = callbacks_time_insert(count_into, nullptr, 200);
    CHECK(handle_index(reused) == handle_index(first));
    callbacks_time_execute(200);
    CHECK(null_context_calls == 1);
    CHECK(last_context == nullptr);
}

int main(void) {
    test_nothing_scheduled();
    test_runs_due_in_order();
    test_rejects_null_callback();
    test_full();
    test_cancel();
    test_stale_handle_cannot_cancel_replacement();
    test_handle_is_stale_while_running();
    test_callback_cancels_another();
    test_callback_schedules_into_its_own_slot();
    test_follow_up_due_now_runs_in_same_call();
    test_call_limit();
    test_repeating();
    test_reinit();
    test_context();
    return 0;
}
