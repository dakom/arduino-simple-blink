/**
 * @file time_utils.h
 * @brief A millisecond clock, counted by Timer0 interrupts.
 *
 * Uses Timer0 and its compare-match A interrupt, so don't use Timer0 elsewhere.
 */
#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>

/** @brief Milliseconds since init_time(). Wraps back to 0 after about 49.7 days. */
typedef uint32_t millis_t;

/**
 * @brief Start the clock: one interrupt per millisecond.
 *
 * Call once at startup, then enable interrupts with sei().
 */
void init_time();

/**
 * @brief Milliseconds since init_time().
 *
 * To measure a duration, subtract: `time_now() - start >= interval`. That stays
 * correct across the wrap; comparing against a target time does not.
 */
millis_t time_now();

#endif
