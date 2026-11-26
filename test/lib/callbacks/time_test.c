#include "lib/callbacks/time.h"
#include "test_helpers/assert.h"
#include <stdint.h>
#include <string.h>

// The queue is one global. Every test starts by initializing it again (which unlinks
// whatever the last test left pending) and resetting its timed callbacks. They're all
// statics, since the queue holds their addresses.

// What ran, in order: each test callback appends its letter (its context points at it).
static char ran[1024];
static size_t ran_len;

static void record_letter(char letter) {
    TEST_ASSERT(ran_len + 1 < sizeof ran);
    ran[ran_len++] = letter;
    ran[ran_len] = '\0';
}

static bool ran_is(const char *expected) {
    return strcmp(ran, expected) == 0;
}

// context is the char to record
static void record(void *context, [[maybe_unused]] void *data) {
    record_letter(*(char *)context);
}

static char A = 'a', B = 'b', C = 'c', D = 'd';
static timed_callback_t ta, tb, tc, td;

static void start(void) {
    init_callbacks_time();
    ran_len = 0;
    ran[0] = '\0';
    ta = (timed_callback_t){.fn = record, .context = &A};
    tb = (timed_callback_t){.fn = record, .context = &B};
    tc = (timed_callback_t){.fn = record, .context = &C};
    td = (timed_callback_t){.fn = record, .context = &D};
}

static bool none_pending(void) {
    return !callbacks_time_is_pending(&ta) && !callbacks_time_is_pending(&tb) &&
           !callbacks_time_is_pending(&tc) && !callbacks_time_is_pending(&td);
}

static void test_nothing_scheduled(void) {
    start();
    callbacks_time_execute(0);
    callbacks_time_execute(UINT64_MAX);
    TEST_ASSERT(ran_is(""));
}

// Callbacks run once their deadline arrives (exactly at it counts), earliest first, and
// equal deadlines in the order they were scheduled
static void test_runs_due_in_order(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&tc, 300));
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 200));
    TEST_ASSERT(callbacks_time_schedule(&td, 200));
    TEST_ASSERT(callbacks_time_is_pending(&ta) && callbacks_time_is_pending(&tc));

    callbacks_time_execute(99);
    TEST_ASSERT(ran_is(""));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("a"));
    TEST_ASSERT(!callbacks_time_is_pending(&ta));
    callbacks_time_execute(100); // already ran: not again
    TEST_ASSERT(ran_is("a"));
    callbacks_time_execute(250);
    TEST_ASSERT(ran_is("abd"));
    callbacks_time_execute(1000);
    TEST_ASSERT(ran_is("abdc"));
    TEST_ASSERT(none_pending());
}

static void test_rejects_null_callback(void) {
    start();
    timed_callback_t no_fn = {};
    TEST_ASSERT(!callbacks_time_schedule(&no_fn, 100));
    TEST_ASSERT(!callbacks_time_is_pending(&no_fn));
    callbacks_time_execute(1000); // and nothing tries to call it
}

static void test_cancel(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 100));

    TEST_ASSERT(callbacks_time_cancel(&ta));
    TEST_ASSERT(!callbacks_time_is_pending(&ta));
    TEST_ASSERT(!callbacks_time_cancel(&ta)); // already cancelled
    TEST_ASSERT(!callbacks_time_cancel(&tc)); // never scheduled

    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("b"));
    TEST_ASSERT(!callbacks_time_cancel(&tb)); // already ran
}

// Cancelling from the front, middle and back of the queue leaves the rest in order
static void test_cancel_each_position(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 200));
    TEST_ASSERT(callbacks_time_schedule(&tc, 300));
    TEST_ASSERT(callbacks_time_schedule(&td, 400));

    TEST_ASSERT(callbacks_time_cancel(&tb)); // middle
    TEST_ASSERT(callbacks_time_cancel(&ta)); // front
    TEST_ASSERT(callbacks_time_cancel(&td)); // back
    callbacks_time_execute(1000);
    TEST_ASSERT(ran_is("c"));

    // And the queue still works after being emptied that way
    TEST_ASSERT(callbacks_time_schedule(&ta, 2000));
    TEST_ASSERT(callbacks_time_schedule(&tb, 1500));
    callbacks_time_execute(2000);
    TEST_ASSERT(ran_is("cba"));
}

// Scheduling one that's already pending moves it (later or earlier); it still runs once
static void test_schedule_again_moves_it(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 200));
    TEST_ASSERT(callbacks_time_schedule(&ta, 300)); // later
    TEST_ASSERT(ta.deadline == 300);
    callbacks_time_execute(250);
    TEST_ASSERT(ran_is("b"));
    callbacks_time_execute(300);
    TEST_ASSERT(ran_is("ba"));

    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 500));
    TEST_ASSERT(callbacks_time_schedule(&tb, 50)); // earlier
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("ba"));
    callbacks_time_execute(1000);
    TEST_ASSERT(ran_is("ba")); // not again at its old deadline
}

// A cancelled or finished callback can be scheduled again
static void test_reuse(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_cancel(&ta));
    TEST_ASSERT(callbacks_time_schedule(&ta, 200));
    callbacks_time_execute(200);
    TEST_ASSERT(ran_is("a"));
    TEST_ASSERT(callbacks_time_schedule(&ta, 300));
    callbacks_time_execute(300);
    TEST_ASSERT(ran_is("aa"));
}

// While a callback runs it isn't pending, so cancelling itself does nothing
static bool cancel_self_result;
static bool pending_while_running;
static void cancel_self(void *context, void *data) {
    record(context, data);
    pending_while_running = callbacks_time_is_pending(&ta);
    cancel_self_result = callbacks_time_cancel(&ta);
}

static void test_not_pending_while_running(void) {
    start();
    ta.fn = cancel_self;
    cancel_self_result = pending_while_running = true;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("a"));
    TEST_ASSERT(!pending_while_running);
    TEST_ASSERT(!cancel_self_result);
}

// A callback can cancel another one, even one due in the same call
static void cancel_b(void *context, void *data) {
    record(context, data);
    TEST_ASSERT(callbacks_time_cancel(&tb));
}

static void test_callback_cancels_another(void) {
    start();
    ta.fn = cancel_b;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 100));
    TEST_ASSERT(callbacks_time_schedule(&tc, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("ac"));
    TEST_ASSERT(none_pending());
}

// A callback can move another one that's due in the same call; it then waits for its new time
static void move_b_later(void *context, void *data) {
    record(context, data);
    TEST_ASSERT(callbacks_time_schedule(&tb, 200));
}

static void test_callback_moves_another(void) {
    start();
    ta.fn = move_b_later;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 100));
    TEST_ASSERT(callbacks_time_schedule(&tc, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("ac"));
    TEST_ASSERT(callbacks_time_is_pending(&tb));
    callbacks_time_execute(200);
    TEST_ASSERT(ran_is("acb"));
}

// Anything scheduled while execute runs waits for the next call, even if it's already due
static millis_t reschedule_at;
static void schedule_b(void *context, void *data) {
    record(context, data);
    TEST_ASSERT(callbacks_time_schedule(&tb, reschedule_at));
}

static void test_follow_up_waits_for_next_call(void) {
    start();
    ta.fn = schedule_b;
    reschedule_at = 100; // due straight away
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("a"));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("ab"));

    // A follow-up due earlier than what's still waiting to run in this call doesn't jump in
    start();
    ta.fn = schedule_b;
    reschedule_at = 50;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tc, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("ac"));
    callbacks_time_execute(100);
    TEST_ASSERT(ran_is("acb"));
}

// So a callback that keeps rescheduling itself for `now` runs once per call, not forever
static int forever_calls;
static void forever([[maybe_unused]] void *context, void *data) {
    forever_calls++;
    TEST_ASSERT(callbacks_time_schedule(&ta, *(millis_t *)data));
}

static void test_self_reschedule_runs_once_per_call(void) {
    start();
    ta.fn = forever;
    forever_calls = 0;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    callbacks_time_execute(100);
    TEST_ASSERT(forever_calls == 1);
    callbacks_time_execute(100);
    TEST_ASSERT(forever_calls == 2);
}

// A repeating callback: each run schedules the next, one period after its own deadline
static void every_100(void *context, void *data) {
    record(context, data);
    TEST_ASSERT(callbacks_time_schedule(&ta, ta.deadline + 100));
}

static void test_repeating(void) {
    start();
    ta.fn = every_100;
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    for (millis_t now = 0; now <= 1000; now += 10) {
        callbacks_time_execute(now);
    }
    TEST_ASSERT(ran_is("aaaaaaaaaa")); // at 100, 200, ... 1000
    TEST_ASSERT(callbacks_time_is_pending(&ta) && ta.deadline == 1100);

    // Late calls don't make it drift: one call long after the deadline runs it once, and
    // the next deadline still follows on from the old one
    callbacks_time_execute(1150);
    TEST_ASSERT(ta.deadline == 1200);
}

// Initializing again forgets everything: nothing pending runs, and all can be reused
static void test_reinit(void) {
    start();
    TEST_ASSERT(callbacks_time_schedule(&ta, 100));
    TEST_ASSERT(callbacks_time_schedule(&tb, 200));
    init_callbacks_time();
    TEST_ASSERT(none_pending());
    callbacks_time_execute(1000);
    TEST_ASSERT(ran_is(""));
    TEST_ASSERT(!callbacks_time_cancel(&ta));

    TEST_ASSERT(callbacks_time_schedule(&tb, 1100));
    callbacks_time_execute(1100);
    TEST_ASSERT(ran_is("b"));
}

// The context is handed over exactly as stored (nullptr included), and data points at the
// time execute was called with, not the deadline
static void *last_context;
static millis_t last_now;
static void remember(void *context, void *data) {
    last_context = context;
    last_now = *(millis_t *)data;
}

static void test_context_and_data(void) {
    start();
    static int thing;
    timed_callback_t with = {.fn = remember, .context = &thing};
    timed_callback_t without = {.fn = remember};
    // (Locals are fine here only because neither is still pending when the test ends)

    TEST_ASSERT(callbacks_time_schedule(&with, 100));
    callbacks_time_execute(123);
    TEST_ASSERT(last_context == &thing);
    TEST_ASSERT(last_now == 123);

    TEST_ASSERT(callbacks_time_schedule(&without, 200));
    callbacks_time_execute(200);
    TEST_ASSERT(last_context == nullptr);
    TEST_ASSERT(last_now == 200);
}

// No capacity: lots pending at once, scheduled out of order with many equal deadlines, all
// run in deadline order, and equal deadlines in scheduling order
#define MANY 500
static timed_callback_t many[MANY];
static int many_order[MANY];
static int many_ran;

// context is the callback itself, so its index in many[] says which one ran
static void record_index(void *context, [[maybe_unused]] void *data) {
    TEST_ASSERT(many_ran < MANY);
    many_order[many_ran++] = (int)((timed_callback_t *)context - many);
}

static void test_many_pending(void) {
    start();
    many_ran = 0;
    uint32_t seed = 12345;
    for (int i = 0; i < MANY; i++) {
        seed = seed * 1103515245u + 12345u; // a fixed pseudo-random sequence
        many[i] = (timed_callback_t){.fn = record_index, .context = &many[i]};
        TEST_ASSERT(callbacks_time_schedule(&many[i], (seed >> 16) % 40)); // 40 deadlines
    }

    callbacks_time_execute(1000);
    TEST_ASSERT(many_ran == MANY);
    for (int i = 1; i < MANY; i++) {
        timed_callback_t *before = &many[many_order[i - 1]];
        timed_callback_t *after = &many[many_order[i]];
        TEST_ASSERT(before->deadline <= after->deadline);
        if (before->deadline == after->deadline) {
            TEST_ASSERT(many_order[i - 1] < many_order[i]); // scheduled first, ran first
        }
    }
}

// The usual case is O(1): a deadline at or after every pending one never walks the queue.
// A deadline before every pending one walks all of them: that's the O(n) worst case.
static void test_schedule_cost(void) {
    start();
    unsigned long before = callbacks_time_schedule_steps;
    for (int i = 0; i < MANY; i++) {
        many[i] = (timed_callback_t){.fn = record_index, .context = &many[i]};
        TEST_ASSERT(callbacks_time_schedule(&many[i], (millis_t)(1000 + i / 2))); // rising, with ties
    }
    TEST_ASSERT(callbacks_time_schedule_steps == before); // not a single step

    // Earliest yet: steps past every pending one, and no more
    before = callbacks_time_schedule_steps;
    TEST_ASSERT(callbacks_time_schedule(&ta, 0));
    TEST_ASSERT(callbacks_time_schedule_steps - before == MANY);

    init_callbacks_time(); // leave nothing pending for the next test
}

int main(void) {
    test_nothing_scheduled();
    test_runs_due_in_order();
    test_rejects_null_callback();
    test_cancel();
    test_cancel_each_position();
    test_schedule_again_moves_it();
    test_reuse();
    test_not_pending_while_running();
    test_callback_cancels_another();
    test_callback_moves_another();
    test_follow_up_waits_for_next_call();
    test_self_reschedule_runs_once_per_call();
    test_repeating();
    test_reinit();
    test_context_and_data();
    test_many_pending();
    test_schedule_cost();
    return 0;
}
