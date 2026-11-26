#pragma once
/**
 * @file event.h
 * @brief Events that any number of listeners can subscribe to, with no fixed capacity.
 *
 * An event and its listeners are ordinary variables that you own; the library only links
 * them together:
 *
 *     static event_t door_opened = {};                        // no listeners yet
 *     static listener_t log_door = {.fn = on_door_opened, .context = &log};
 *
 *     event_subscribe(&door_opened, &log_door);
 *     event_execute(&door_opened, &door);   // runs on_door_opened(&log, &door) right now
 *     event_unsubscribe(&log_door);
 *
 * Every operation is O(1), except event_execute, which visits that event's listeners once
 * each. Nothing is queued: execute calls every listener before it returns, newest
 * subscriber first.
 *
 * Rules:
 * - Events and listeners must be static or global: the lists hold their addresses.
 * - A listener is on at most one event at a time. Subscribing it again while it's on one
 *   does nothing; unsubscribe it first.
 * - A listener may unsubscribe *itself* while it runs, but not another listener of the same
 *   event: the loop may already have saved that one as the next to call.
 * - data is only valid while the listener runs: copy what you need, never keep the pointer.
 * - Main loop only: none of these may be called from an interrupt.
 */

#include "lib/callbacks/shared.h"

/**
 * @brief One subscription. Set fn (and context, if you want one); leave the rest zero.
 */
typedef struct listener_t {
    callback_t fn;           ///< Called as fn(context, data) on each execute. Must not be null.
    void *context;           ///< Handed to fn every time; may be nullptr.
    struct listener_t *next; ///< Library only: the next listener on the same event.
    struct listener_t *prev; ///< Library only: the previous one, for O(1) unsubscribe.
    struct event_t *event;   ///< Library only: the event it's on, or nullptr if none.
} listener_t;

/** @brief Something that can happen. Start it zeroed (`= {}`): no listeners. */
typedef struct event_t {
    listener_t *first; ///< Library only: the most recent subscriber, or nullptr.
} event_t;

/**
 * @brief Have l called on every execute of event. Does nothing if l is already on one.
 *
 * O(1): l goes on the front of the list.
 */
void event_subscribe(event_t *event, listener_t *l);

/**
 * @brief Stop calling l. Does nothing if it isn't subscribed.
 *
 * O(1): l's prev and event pointers say where to unlink it, so there's no search.
 */
void event_unsubscribe(listener_t *l);

/**
 * @brief Call every listener of event now, each with its own context and this data.
 *
 * O(k), where k is the number of listeners on this event: each is called once. Listeners
 * of other events are never visited.
 */
void event_execute(event_t *event, void *data);
