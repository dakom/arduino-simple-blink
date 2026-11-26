#include "lib/collections/ring_buffer.h"

// Move a cursor one slot forward, wrapping from the last slot back to slot 0.
static size_t next_slot(const ring_buffer_t *ring_buffer, size_t cursor) {
    cursor++;
    return cursor >= ring_buffer->size ? 0 : cursor;
}

void ring_buffer_write_bytes(ring_buffer_t *ring_buffer, const uint8_t *data, size_t len) {
    for (size_t count = 0; count < len; count++) {
        if (ring_buffer->queue_len == ring_buffer->size) {
            // Full: the write and read cursors point at the same slot, which holds
            // the oldest unread byte, and this write is about to destroy it. Move the
            // read cursor on to the next slot, the oldest byte that survives, so that
            // reading still goes from oldest to newest. The length stays the same:
            // one byte lost, one byte added.
            ring_buffer->read_cursor = next_slot(ring_buffer, ring_buffer->read_cursor);
        } else {
            ring_buffer->queue_len++;
        }

        ring_buffer->queue[ring_buffer->write_cursor] = data[count];
        ring_buffer->write_cursor = next_slot(ring_buffer, ring_buffer->write_cursor);
    }
}

ring_buffer_result_t ring_buffer_read_byte(ring_buffer_t *ring_buffer) {
    ring_buffer_result_t result = {.exists = false};

    if (ring_buffer->queue_len > 0) {
        result.exists = true;
        result.byte = ring_buffer->queue[ring_buffer->read_cursor];
        ring_buffer->read_cursor = next_slot(ring_buffer, ring_buffer->read_cursor);
        ring_buffer->queue_len--;
    }

    return result;
}

void ring_buffer_write_string(ring_buffer_t *ring_buffer, const char *data) {
    size_t len = 0;
    while (data[len++] != '\0') {
        ;
    }
    ring_buffer_write_bytes(ring_buffer, (const uint8_t *)data, len - 1);
}
