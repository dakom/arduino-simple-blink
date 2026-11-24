/**
 * @file ring_buffer.h
 * @brief A fixed-size first-in, first-out byte queue that wraps around its array.
 *
 * When the buffer is full, writing overwrites the oldest unread byte, so the
 * newest `size` bytes are always kept, and reading still goes oldest to newest.
 *
 * Pure C with no hardware access, so it can be unit tested on the Mac.
 * Not interrupt-safe on its own.
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief A ring buffer over caller-provided storage.
 *
 * Start with both cursors and queue_len at 0, size set to the length of queue.
 */
typedef struct ring_buffer_t {
    size_t read_cursor;  ///< Slot of the oldest unread byte.
    size_t write_cursor; ///< Slot the next written byte goes into.
    size_t size;         ///< Number of slots in queue; must be at least 1.
    size_t queue_len;    ///< Number of unread bytes, from 0 to size.
    uint8_t *queue;      ///< The storage, size bytes long.
} ring_buffer_t;

/** @brief The result of a read: a byte, if there was one. */
typedef struct ring_buffer_result_t {
    uint8_t byte; ///< The byte read; 0 when exists is false.
    bool exists;  ///< False when the buffer was empty.
} ring_buffer_result_t;

/**
 * @brief Queue len bytes, overwriting the oldest unread bytes if there's no room.
 */
void ring_buffer_write_bytes(ring_buffer_t *ring_buffer, const uint8_t *data, size_t len);

/**
 * @brief Queue the characters of a string, without its terminating '\0'.
 */
void ring_buffer_write_string(ring_buffer_t *ring_buffer, const char *data);

/**
 * @brief Take the oldest unread byte out of the buffer.
 */
ring_buffer_result_t ring_buffer_read_byte(ring_buffer_t *ring_buffer);

#endif
