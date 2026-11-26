#pragma once
/**
 * @file time.h
 * @brief Run a function once at a set time, with no fixed limit on how many are pending.
 *
 * Each timed callback is a timed_callback_t that you own; the library only links the
 * pending ones together, in deadline order:
 *
 *     static void led_off(void *context, void *data) {
 *         led_t *led = context;              // the pointer stored in the timed_callback_t
 *         millis_t now = *(millis_t *)data;  // the time execute was called with
 *         ...
 *     }
 *
 *     static timed_callback_t red_off = {.fn = led_off, .context = &red_led};
 *
 *     callbacks_time_schedule(&red_off, timer_now() + 500);
 *     ...
 *     callbacks_time_cancel(&red_off);       // changed your mind (false if it already ran)
 *
 *     // in main's loop, once per pass:
 *     callbacks_time_execute(timer_now());
 *
 * Each schedule runs the callback once. For a repeating one, schedule it again from
 * inside the callback: it's no longer pending while it runs, so that's allowed.
 *
 * There's no capacity to configure and no "queue full": the most callbacks that can be
 * pending is the number of timed_callback_t variables in the program. The catch is that
 * each one costs n steps to schedule in the worst case (n = how many are pending), see
 * callbacks_time_schedule.
 *
 * Rules:
 * - Every timed_callback_t must be static or global: the queue holds its address while
 *   it's pending.
 * - Set fn (and context, if you want one) and leave the rest zero. Don't change any field
 *   while it's pending; read deadline any time.
 * - Main loop only: none of these may be called from an interrupt.
 */

#include "lib/callbacks/shared.h"
#include "lib/hardware/timer.h"

struct timed_callback_list_t;

/** @brief One timed callback. Set fn and context; the library owns the rest. */
typedef struct timed_callback_t {
    callback_t fn; ///< Called as fn(context, &now) when due. Must not be null.
    void *context; ///< Handed to fn; may be nullptr. Must still exist when fn runs.
    /// When it's due (pending), or was last due. Set by callbacks_time_schedule; kept after
    /// it runs, so a repeating callback can schedule its next run from this one.
    millis_t deadline;
    struct timed_callback_t *next; ///< Library only: the next one due (later or equal).
    struct timed_callback_t *prev; ///< Library only: the previous one, for O(1) cancel.
    struct timed_callback_list_t *list; ///< Library only: the list it's on; nullptr = not pending.
} timed_callback_t;

/** @brief Library only: a list of pending callbacks in deadline order. */
typedef struct timed_callback_list_t {
    timed_callback_t *first; ///< Due soonest, or nullptr.
    timed_callback_t *last;  ///< Due latest. Kept so the usual schedule (latest yet) is O(1).
} timed_callback_list_t;

/**
 * @brief Forget every pending callback: none of them will run, and each is no longer
 * pending (so it can be scheduled again).
 *
 * Call once at startup, before any other function here. Not from inside a callback.
 *
 * O(n), n = how many are pending.
 */
void init_callbacks_time();

/**
 * @brief Run t->fn once, at or after deadline (an absolute time, e.g. timer_now() + 500).
 *
 * If t is already pending, it moves to the new deadline instead (and still runs once).
 * Callbacks with equal deadlines run in the order they were scheduled.
 *
 * O(1) when deadline is at or after every other pending one, which is the usual case
 * (a blink, a debounce, a timeout: "some time from now"). Otherwise O(n), n = how many are
 * pending: it walks back from the latest to find its place.
 *
 * @return false, changing nothing, if t->fn is null.
 */
bool callbacks_time_schedule(timed_callback_t *t, millis_t deadline);

/**
 * @brief Stop a pending callback from running. It can be scheduled again any time.
 *
 * O(1).
 *
 * @return false, changing nothing, if t isn't pending: it already ran (or is running right
 *         now), was cancelled, or was never scheduled.
 */
bool callbacks_time_cancel(timed_callback_t *t);

/**
 * @brief Whether t is waiting to run. False while its own fn is running.
 *
 * O(1).
 */
bool callbacks_time_is_pending(const timed_callback_t *t);

/**
 * @brief Run every callback that was due (deadline at or before now) when this call
 * started, earliest first, each with data = &now.
 *
 * Callbacks scheduled while this runs wait for the next call, even if they're already due,
 * so a callback that keeps rescheduling itself for `now` can't loop forever: it runs once
 * per call. Each one stops being pending just before it runs, so it may schedule itself
 * again, and may cancel or reschedule any other callback, including ones due in this call.
 *
 * O(d), d = how many were due. Callbacks that aren't due are never visited.
 */
void callbacks_time_execute(millis_t now);

#ifndef __AVR__
/**
 * @brief TEST-ONLY, Mac builds only: how many pending callbacks schedule has stepped past
 * to find a place, ever. Lets the tests check the O(1) case really is O(1).
 */
extern unsigned long callbacks_time_schedule_steps;
#endif
