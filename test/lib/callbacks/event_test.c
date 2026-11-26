#include "lib/callbacks/event.h"
#include "test_helpers/assert.h"
#include <stdint.h>
#include <string.h>

// Events and listeners are plain structs, so every test makes its own fresh ones (zeroed,
// as a static or global would be). Locals are fine here: nothing outlives the test.

// What ran, in order: each test callback appends its letter (from its context).
static char ran[1024];
static size_t ran_len;

// What the most recent callback was handed
static void *last_context;
static void *last_data;

static void start(void) {
    ran_len = 0;
    ran[0] = '\0';
    last_context = nullptr;
    last_data = nullptr;
}

static bool ran_is(const char *expected) {
    return strcmp(ran, expected) == 0;
}

// context points at the char to record
static void record(void *context, void *data) {
    TEST_ASSERT(ran_len + 1 < sizeof ran);
    ran[ran_len++] = *(char *)context;
    ran[ran_len] = '\0';
    last_context = context;
    last_data = data;
}

static char A = 'a', B = 'b', C = 'c', D = 'd';

#define LISTENER(letter) ((listener_t){.fn = record, .context = &(letter)})

static void test_no_listeners(void) {
    start();
    event_t event = {};
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is(""));
}

// Every execute calls the listener, with its context and that execute's data
static void test_runs_with_context_and_data(void) {
    start();
    event_t event = {};
    listener_t a = LISTENER(A);
    int data1 = 0, data2 = 0;
    event_subscribe(&event, &a);

    event_execute(&event, &data1);
    TEST_ASSERT(last_context == &A && last_data == &data1);
    event_execute(&event, &data2);
    TEST_ASSERT(last_context == &A && last_data == &data2);
    TEST_ASSERT(ran_is("aa"));
}

// All listeners run, newest subscriber first
static void test_many_listeners(void) {
    start();
    event_t event = {};
    listener_t a = LISTENER(A), b = LISTENER(B), c = LISTENER(C);
    event_subscribe(&event, &a);
    event_subscribe(&event, &b);
    event_subscribe(&event, &c);
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("cba"));
}

// Each event only runs its own listeners
static void test_events_are_separate(void) {
    start();
    event_t first = {}, second = {};
    listener_t a = LISTENER(A), b = LISTENER(B);
    event_subscribe(&first, &a);
    event_subscribe(&second, &b);
    event_execute(&second, nullptr);
    event_execute(&first, nullptr);
    TEST_ASSERT(ran_is("ba"));
}

// Unsubscribing works from the front, middle and back of the list, and leaves the rest linked
static void test_unsubscribe_each_position(void) {
    start();
    event_t event = {};
    listener_t a = LISTENER(A), b = LISTENER(B), c = LISTENER(C), d = LISTENER(D);
    event_subscribe(&event, &a);
    event_subscribe(&event, &b);
    event_subscribe(&event, &c);
    event_subscribe(&event, &d); // list: d c b a

    event_unsubscribe(&c); // middle
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("dba"));

    start();
    event_unsubscribe(&d); // front
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("ba"));

    start();
    event_unsubscribe(&a); // back
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("b"));

    start();
    event_unsubscribe(&b); // the only one
    TEST_ASSERT(event.first == nullptr);
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is(""));
}

// Subscribing twice is refused (it would corrupt the list), and so is joining a second event
static void test_subscribe_twice_does_nothing(void) {
    start();
    event_t event = {}, other = {};
    listener_t a = LISTENER(A), b = LISTENER(B);
    event_subscribe(&event, &a);
    event_subscribe(&event, &b);
    event_subscribe(&event, &a); // again
    event_subscribe(&other, &a); // on another event while still on the first

    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("ba"));
    start();
    event_execute(&other, nullptr);
    TEST_ASSERT(ran_is(""));
}

// Unsubscribing something that isn't subscribed does nothing
static void test_unsubscribe_twice_does_nothing(void) {
    start();
    event_t event = {};
    listener_t a = LISTENER(A), b = LISTENER(B), never = LISTENER(C);
    event_subscribe(&event, &a);
    event_subscribe(&event, &b);

    event_unsubscribe(&never);
    event_unsubscribe(&a);
    event_unsubscribe(&a);
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("b"));
}

// After unsubscribing, a listener can join the same or another event again
static void test_resubscribe(void) {
    start();
    event_t event = {}, other = {};
    listener_t a = LISTENER(A);
    event_subscribe(&event, &a);
    event_unsubscribe(&a);
    event_subscribe(&other, &a);

    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is(""));
    event_execute(&other, nullptr);
    TEST_ASSERT(ran_is("a"));

    event_unsubscribe(&a);
    event_subscribe(&event, &a);
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("aa"));
}

// A listener can unsubscribe itself while it runs; the ones after it still run this time
static listener_t self_removing;
static void unsubscribe_self(void *context, void *data) {
    record(context, data);
    event_unsubscribe(&self_removing);
}

static void test_listener_unsubscribes_itself(void) {
    start();
    event_t event = {};
    listener_t a = LISTENER(A), c = LISTENER(C);
    self_removing = (listener_t){.fn = unsubscribe_self, .context = &B};
    event_subscribe(&event, &a);
    event_subscribe(&event, &self_removing);
    event_subscribe(&event, &c); // list: c b a

    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("cba"));
    start();
    event_execute(&event, nullptr);
    TEST_ASSERT(ran_is("ca"));
}

int main(void) {
    test_no_listeners();
    test_runs_with_context_and_data();
    test_many_listeners();
    test_events_are_separate();
    test_unsubscribe_each_position();
    test_subscribe_twice_does_nothing();
    test_unsubscribe_twice_does_nothing();
    test_resubscribe();
    test_listener_unsubscribes_itself();
    return 0;
}
