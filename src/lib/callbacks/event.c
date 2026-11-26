#include "lib/callbacks/event.h"

// Each event's listeners are a doubly linked list threaded through the listeners
// themselves, so there's no table to size: every listener brings its own links.
// listener->event is the event it's on, or nullptr while it isn't subscribed.

void event_subscribe(event_t *event, listener_t *l) {
    if (l->event != nullptr)
        return; // already on a list: subscribing twice would corrupt it
    l->event = event;
    l->prev = nullptr;
    l->next = event->first;
    if (event->first)
        event->first->prev = l;
    event->first = l;
}

void event_unsubscribe(listener_t *l) {
    if (l->event == nullptr)
        return; // not subscribed: nothing to do
    if (l->prev)
        l->prev->next = l->next;
    else
        l->event->first = l->next; // it was the first one
    if (l->next)
        l->next->prev = l->prev;
    l->event = nullptr;
    l->next = l->prev = nullptr;
}

void event_execute(event_t *event, void *data) {
    for (listener_t *l = event->first; l != nullptr;) {
        listener_t *next = l->next; // read first, so fn can unsubscribe itself
        l->fn(l->context, data);
        l = next;
    }
}
