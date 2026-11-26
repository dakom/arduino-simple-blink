#include "lib/collections/ring_buffer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_CHAR(actual, expected)                                                              \
    do {                                                                                           \
        if (!actual.exists) {                                                                      \
            fprintf(stderr, "%s:%d: failed: !exists\n", __FILE__, __LINE__);                       \
            exit(1);                                                                               \
        }                                                                                          \
        if ((char)actual.byte != expected) {                                                       \
            fprintf(stderr, "%s:%d: failed: got %c, expected %c\n", __FILE__, __LINE__,            \
                    (char)actual.byte, expected);                                                  \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#define ASSERT_STRING(actual, expected)                                                            \
    do {                                                                                           \
        if (!actual.exists) {                                                                      \
            fprintf(stderr, "%s:%d: failed: !exists\n", __FILE__, __LINE__);                       \
            exit(1);                                                                               \
        }                                                                                          \
        if (strlen(actual.data) != strlen(expected)) {                                             \
            fprintf(stderr,                                                                        \
                    "%s:%d: failed: different length, got %zu (\"%s\") expected %zu (\"%s\")\n",   \
                    __FILE__, __LINE__, strlen(actual.data), actual.data, strlen(expected),        \
                    expected);                                                                     \
            exit(1);                                                                               \
        }                                                                                          \
        for (size_t index = 0; index < strlen(expected); index++) {                                \
            if (expected[index] != actual.data[index]) {                                           \
                fprintf(stderr,                                                                    \
                        "%s:%d: failed at index %zu: got %c, expected %c (\"%s\" vs \"%s\")\n",    \
                        __FILE__, __LINE__, index, actual.data[index], expected[index],            \
                        actual.data, expected);                                                    \
                exit(1);                                                                           \
            }                                                                                      \
        }                                                                                          \
    } while (0)

static ring_buffer_t make_test_buffer(size_t size) {
    ring_buffer_t ring_buffer = {.size = size, .queue = malloc(size)};

    return ring_buffer;
}

static void free_test_buffer(ring_buffer_t *ring_buffer) {
    free(ring_buffer->queue);
}

typedef struct ring_buffer_result_string_t {
    bool exists;
    char *data;
} ring_buffer_result_string_t;

static ring_buffer_result_string_t ring_buffer_read_all_as_string(ring_buffer_t *ring_buffer) {
    ring_buffer_result_string_t result = {.exists = false,
                                          .data = malloc(ring_buffer->queue_len + 1)};

    size_t index = 0;

    while (ring_buffer->queue_len) {
        auto byte_result = ring_buffer_read_byte(ring_buffer);
        if (!byte_result.exists) {
            return result;
        }

        *(result.data + index++) = (char)byte_result.byte;
    }

    *(result.data + index) = '\0';

    result.exists = true;
    return result;
}

void test_ring_buffer_char() {
    ring_buffer_t ring_buffer = make_test_buffer(1024);

    ring_buffer_write_string(&ring_buffer, "Hello World");

    auto result = ring_buffer_read_byte(&ring_buffer);

    ASSERT_CHAR(result, 'H');

    free_test_buffer(&ring_buffer);
}

void test_ring_buffer_string() {
    ring_buffer_t ring_buffer = make_test_buffer(1024);

    ring_buffer_write_string(&ring_buffer, "Hello World");

    auto result = ring_buffer_read_all_as_string(&ring_buffer);

    ASSERT_STRING(result, "Hello World");

    free(result.data);
    free_test_buffer(&ring_buffer);
}

// Writing more than fits must not corrupt what's queued. In a 4-byte buffer:
//   write "ABCD"  ->  A B C D           (full)
//   read 2        ->  "AB"; C and D still queued, 2 slots free
//   write "EFG"   ->  only 2 slots free, so something has to give
// Policy: overwrite the oldest. C is overwritten to make room for G, so what's
// left to read is "DEFG": never more than 4 bytes, and never C after it was lost.
void test_ring_buffer_overfill() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    ring_buffer_write_string(&ring_buffer, "ABCD"); // memory: ABCD
    ring_buffer_read_byte(&ring_buffer);            // A -> memory: !BCD, read order: BCD!
    ring_buffer_read_byte(&ring_buffer);            // B -> memory: !!CD, read order: CD!!
    ring_buffer_write_string(&ring_buffer, "EFG");  // memory: EFGD, read order: GDEF

    auto result = ring_buffer_read_all_as_string(&ring_buffer);

    ASSERT_STRING(result, "DEFG");

    free(result.data);
    free_test_buffer(&ring_buffer);
}

// Reading an empty buffer must say so, both before anything is written and
// after everything written has been read.
void test_ring_buffer_empty() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    auto before = ring_buffer_read_byte(&ring_buffer);
    if (before.exists) {
        fprintf(stderr, "%s:%d: failed: read a byte from a new, empty buffer\n", __FILE__,
                __LINE__);
        exit(1);
    }

    ring_buffer_write_string(&ring_buffer, "AB");
    ring_buffer_read_byte(&ring_buffer);
    ring_buffer_read_byte(&ring_buffer);

    auto after = ring_buffer_read_byte(&ring_buffer);
    if (after.exists) {
        fprintf(stderr, "%s:%d: failed: read a byte after everything was read\n", __FILE__,
                __LINE__);
        exit(1);
    }

    free_test_buffer(&ring_buffer);
}

// Exactly filling the buffer is not an overfill: nothing may be lost.
void test_ring_buffer_exactly_full() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    ring_buffer_write_string(&ring_buffer, "ABCD");

    auto result = ring_buffer_read_all_as_string(&ring_buffer);
    ASSERT_STRING(result, "ABCD");

    free(result.data);
    free_test_buffer(&ring_buffer);
}

// Wrapping around the end of the array without overfilling: read 2, then write 4,
// so the data runs from slot 2 to the end and continues from slot 0.
void test_ring_buffer_wrap() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    ring_buffer_write_string(&ring_buffer, "AB");
    ring_buffer_read_byte(&ring_buffer);
    ring_buffer_read_byte(&ring_buffer);
    ring_buffer_write_string(&ring_buffer, "CDEF"); // memory: EFCD, read order: CDEF

    auto result = ring_buffer_read_all_as_string(&ring_buffer);
    ASSERT_STRING(result, "CDEF");

    free(result.data);
    free_test_buffer(&ring_buffer);
}

// Overfilling by a lot: the read cursor has to wrap around too, possibly more than
// once. Only the last 4 bytes written survive.
void test_ring_buffer_overfill_many() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    ring_buffer_write_string(&ring_buffer, "ABCDEFGHIJ");

    auto result = ring_buffer_read_all_as_string(&ring_buffer);
    ASSERT_STRING(result, "GHIJ");

    free(result.data);
    free_test_buffer(&ring_buffer);
}

// A long run of small writes and reads, like a real serial log, checked byte by byte.
// The cursors wrap around many times.
void test_ring_buffer_long_run() {
    ring_buffer_t ring_buffer = make_test_buffer(4);

    for (int round = 0; round < 1000; round++) {
        uint8_t first = (uint8_t)round;
        uint8_t second = (uint8_t)(round + 1);
        ring_buffer_write_bytes(&ring_buffer, (uint8_t[]){first, second}, 2);

        auto a = ring_buffer_read_byte(&ring_buffer);
        auto b = ring_buffer_read_byte(&ring_buffer);
        if (!a.exists || !b.exists || a.byte != first || b.byte != second) {
            fprintf(stderr, "%s:%d: failed in round %d: expected %d %d\n", __FILE__, __LINE__,
                    round, first, second);
            exit(1);
        }
    }

    free_test_buffer(&ring_buffer);
}

int main() {
    test_ring_buffer_char();
    test_ring_buffer_string();
    test_ring_buffer_overfill();
    test_ring_buffer_empty();
    test_ring_buffer_exactly_full();
    test_ring_buffer_wrap();
    test_ring_buffer_overfill_many();
    test_ring_buffer_long_run();

    return 0;
}
