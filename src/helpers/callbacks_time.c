#include "helpers/callbacks_time.h"
#include "helpers/handle_pool.h"
#include "helpers/time_heap.h"
#include "helpers/time_utils.h"
#include <stdint.h>

// CALLBACKS_TIME_CAPACITY comes from the compiler flags, set in Taskfile.yml
#ifndef CALLBACKS_TIME_CAPACITY
#error                                                                                             \
    "CALLBACKS_TIME_CAPACITY is not set: it comes from Taskfile.yml (-DCALLBACKS_TIME_CAPACITY=...)"
#endif

// What to run for one slot: the function, and the context to hand it. The queue never
// looks at the context, only carries it from insert to the call.
typedef struct callback_entry_t {
    callback_time_t fn;
    void *context;
} callback_entry_t;

// The queue: a handle pool to hand out slots, a time heap to order them by deadline, and
// an entry for each slot (indexed by handle_index, like the heap's ids).
typedef struct callbacks_time_t {
    handle_pool_t handles;
    time_heap_t heap;
    callback_entry_t *entries;
} callbacks_time_t;

static callbacks_time_t callbacks_time = {
    .handles = HANDLE_POOL_WITH_CAPACITY(CALLBACKS_TIME_CAPACITY),
    .heap = TIME_HEAP_WITH_CAPACITY(CALLBACKS_TIME_CAPACITY),
    .entries = (callback_entry_t[HANDLE_POOL_CHECKED_SIZE(CALLBACKS_TIME_CAPACITY)]){},
};

void init_callbacks_time() {
    handle_pool_init(&callbacks_time.handles);
    time_heap_init(&callbacks_time.heap);
}

handle_t callbacks_time_insert(callback_time_t fn, void *context, millis_t deadline) {
    if (fn == nullptr) {
        return HANDLE_NONE;
    }

    handle_t handle = handle_pool_alloc(&callbacks_time.handles);
    if (handle == HANDLE_NONE) {
        return HANDLE_NONE;
    }

    uint8_t index = handle_index(handle);

    if (!time_heap_insert(&callbacks_time.heap, index, deadline)) {
        handle_pool_free(&callbacks_time.handles, handle);
        return HANDLE_NONE;
    }

    callbacks_time.entries[index] = (callback_entry_t){.fn = fn, .context = context};

    return handle;
}

void callbacks_time_execute(millis_t now) {
    time_heap_entry_t due;

    uint16_t called = 0;

    while (time_heap_pop_if_expired(&callbacks_time.heap, &due, now)) {
        // Take a copy: the slot is about to be freed, and fn may reuse it
        callback_entry_t entry = callbacks_time.entries[due.id];

        // Free the slot *before* calling fn, so fn can schedule a new callback into it
        handle_t handle = handle_pool_handle_at(&callbacks_time.handles, due.id);
        handle_pool_free(&callbacks_time.handles, handle);
        callbacks_time.entries[due.id] = (callback_entry_t){};

        entry.fn(entry.context); // fn is never null: insert refuses null

        // a bit of an artificial limit, to avoid looping forever
        // when callbacks schedule themselves in the same time slice.
        // At most capacity calls: enough for every slot to fire once.
        if (++called >= callbacks_time.handles.capacity) {
            return;
        }
    }
}

bool callbacks_time_cancel(handle_t handle) {
    if (!handle_pool_is_live(&callbacks_time.handles, handle)) {
        return false; // stale or bogus: rejected here, once
    }
    uint8_t index = handle_index(handle);
    time_heap_remove(&callbacks_time.heap, index); // index is trusted from here on
    handle_pool_free(&callbacks_time.handles, handle);
    callbacks_time.entries[index] = (callback_entry_t){};
    return true;
}
