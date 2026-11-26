#include "lib/callbacks/time.h"

// How it works:
//
// Every pending callback is on one of two doubly linked lists, both kept in deadline
// order (equal deadlines in scheduling order), threaded through the timed_callback_t
// structs themselves, so there's no storage here to size:
//
//   queue     everything waiting for its deadline
//   running   only during callbacks_time_execute: the ones that were due when it started
//
// execute first moves the due run at the front of queue onto running, then calls them one
// by one. Anything scheduled meanwhile goes onto queue, so it can't run until the next
// call; that's what stops a callback that reschedules itself for `now` from looping
// forever. Each timed_callback_t's list pointer says which list it's on (nullptr: none), so
// cancel and reschedule can unlink it from either one in O(1).

// TEST-ONLY: counting schedule's steps, to prove the usual case is O(1).
//
// A schedule that walked the whole list would still put things in the right order, so
// ordinary tests can't catch that. avr-gcc defines __AVR__, so on the chip COUNT_STEP() is
// ((void)0), "do nothing": no counter, no code, no cost. Only Mac builds (the tests) count.
#ifdef __AVR__
#define COUNT_STEP() ((void)0)
#else
unsigned long callbacks_time_schedule_steps;
#define COUNT_STEP() (callbacks_time_schedule_steps++)
#endif

static timed_callback_list_t queue = {};
static timed_callback_list_t running = {};

// Take t off whichever list it's on. It must be on one.
static void unlink(timed_callback_t *t) {
    timed_callback_list_t *list = t->list;
    if (t->prev)
        t->prev->next = t->next;
    else
        list->first = t->next; // it was the first one
    if (t->next)
        t->next->prev = t->prev;
    else
        list->last = t->prev; // it was the last one
    t->next = t->prev = nullptr;
    t->list = nullptr;
}

static void unlink_all(timed_callback_list_t *list) {
    while (list->first != nullptr) {
        unlink(list->first);
    }
}

void init_callbacks_time() {
    unlink_all(&queue);
    unlink_all(&running);
}

bool callbacks_time_schedule(timed_callback_t *t, millis_t deadline) {
    if (t->fn == nullptr) {
        return false;
    }
    if (t->list != nullptr) {
        unlink(t); // already pending: move it rather than adding it twice
    }
    t->deadline = deadline;

    // Find the last one due at or before the new deadline, walking back from the latest.
    // New deadlines are usually the latest yet, so this usually stops straight away. Going
    // after equal deadlines (> rather than >=) keeps them in scheduling order.
    timed_callback_t *before = queue.last;
    while (before != nullptr && before->deadline > deadline) {
        COUNT_STEP();
        before = before->prev;
    }

    // Link t in right after `before` (or at the front, if nothing is due before it)
    t->list = &queue;
    t->prev = before;
    if (before != nullptr) {
        t->next = before->next;
        before->next = t;
    } else {
        t->next = queue.first;
        queue.first = t;
    }
    if (t->next != nullptr)
        t->next->prev = t;
    else
        queue.last = t;

    return true;
}

bool callbacks_time_cancel(timed_callback_t *t) {
    if (t->list == nullptr) {
        return false;
    }
    unlink(t);
    return true;
}

bool callbacks_time_is_pending(const timed_callback_t *t) {
    return t->list != nullptr;
}

void callbacks_time_execute(millis_t now) {
    // The due ones are a run at the front of queue. Move that run onto running as it is
    // (running is always empty here): one splice, plus updating each one's list pointer.
    timed_callback_t *first_due = queue.first;
    timed_callback_t *last_due = nullptr;
    for (timed_callback_t *t = first_due; t != nullptr && t->deadline <= now; t = t->next) {
        t->list = &running;
        last_due = t;
    }
    if (last_due == nullptr) {
        return; // nothing due
    }
    queue.first = last_due->next;
    if (queue.first != nullptr)
        queue.first->prev = nullptr;
    else
        queue.last = nullptr;
    last_due->next = nullptr;
    running.first = first_due;
    running.last = last_due;

    // Run them in order. Each comes off running before its fn runs, so fn can schedule it
    // again; and since a callback may cancel or reschedule later ones, always take
    // whichever is first now, rather than following a saved next pointer.
    while (running.first != nullptr) {
        timed_callback_t *t = running.first;
        unlink(t);
        t->fn(t->context, &now); // fn is never null: schedule refuses null
    }
}
