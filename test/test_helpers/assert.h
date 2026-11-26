#pragma once
/**
 * @file assert.h
 * @brief Checks for the unit tests.
 */

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Stop the test with its file, line and the failed condition if `condition` is false.
 *
 * Wrapped in do { ... } while (0) so it acts like one statement, even after an unbraced if.
 */
#define TEST_ASSERT(condition)                                                                     \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#define TEST_ASSERT_CHAR(actual, expected)                                                         \
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

#define TEST_ASSERT_STRING(actual, expected)                                                       \
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
