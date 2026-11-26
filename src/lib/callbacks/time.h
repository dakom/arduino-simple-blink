#pragma once
/**
 * @file time.h
 * @brief Run a function once at a set time: one shared queue of timed callbacks.
 *
 * Any module can schedule a callback, and the main loop runs the ones that are due:
 *
 *     static void led_off(void *context) {
 *         led_t *led = context;              // the pointer given to insert; no cast needed
 *         ...
 *     }
 *
 *     handle_t h = callbacks_time_insert(led_off, &red_led, time_now() + 500);
 *     ...
 *     callbacks_time_cancel(h);              // changed your mind (false if it already ran)
 *
 *     // in main's loop, once per pass:
 *     callbacks_time_execute(time_now());
 *
 * Each callback runs once. For a repeating one, schedule the next run from inside it.
 *
 * The context lets one function serve many things (several LEDs, say): it's handed back to
 * the function exactly as given, so it must point at the type that function expects (the
 * compiler can't check), and at something that still exists when the callback runs: a
 * static or global, never a local variable. Pass nullptr if there's nothing to pass.
 *
 * There is one queue for the whole program, with room for CALLBACKS_TIME_CAPACITY
 * callbacks pending at once (set in Taskfile.yml), shared by every module that uses it.
 * Inserting into a full queue returns HANDLE_NONE.
 *
 * Main loop only: none of these may be called from an interrupt.
 */

#include "lib/collections/handle.h"
#include "lib/hardware/timer.h"

/** @brief A function to call later, with the context it was scheduled with. */
typedef void (*callback_time_t)(void *context);

/**
 * @brief Empty the queue. Call once at startup, before any other function here.
 *
 * Calling it again throws away every pending callback, and their handles go stale.
 */
void init_callbacks_time();

/**
 * @brief Schedule fn(context) to run once, at or after deadline (an absolute time, e.g.
 * time_now() + 500).
 *
 * @param fn        The function to call. Must not be null.
 * @param context   Handed to fn when it runs; may be nullptr. See the notes above.
 * @param deadline  When to run it.
 * @return A handle for callbacks_time_cancel, or HANDLE_NONE if fn is null or the queue
 *         is full.
 */
handle_t callbacks_time_insert(callback_time_t fn, void *context, millis_t deadline);

/**
 * @brief Run every callback whose deadline is at or before now, earliest first.
 *
 * Each one's slot is freed before it runs, so its handle is already stale and it can
 * schedule a new callback. Runs at most CALLBACKS_TIME_CAPACITY callbacks per call, so
 * callbacks that keep rescheduling themselves for `now` can't loop forever; the rest wait
 * for the next call.
 */
void callbacks_time_execute(millis_t now);

/**
 * @brief Stop a pending callback from running.
 *
 * @return false, changing nothing, if the handle is stale (the callback already ran or was
 *         cancelled) or HANDLE_NONE.
 */
bool callbacks_time_cancel(handle_t handle);
