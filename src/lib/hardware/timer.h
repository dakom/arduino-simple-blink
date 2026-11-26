#pragma once
/**
 * @file timer.h
 * @brief A millisecond clock, counted by Timer0 interrupts.
 *
 * Uses Timer0 and its compare-match A interrupt, so don't use Timer0 elsewhere.
 */

#include <stdint.h>

/**
 * @brief Milliseconds since init_timer().
 *
 * 64 bits, so it would take about 584 million years to wrap back to 0: in practice it
 * never does, and times can be compared with a plain `<`. (A 32-bit count would wrap
 * after about 49.7 days, and every comparison would have to allow for that.)
 */
typedef uint64_t millis_t;

/**
 * @brief Start the clock: one interrupt per millisecond.
 *
 * Call once at startup, then enable interrupts with sei().
 */
void init_timer();

/**
 * @brief Milliseconds since init_timer().
 *
 * To measure a duration, subtract: `timer_now() - start >= interval`. To check a deadline,
 * compare: `timer_now() >= deadline`.
 */
millis_t timer_now();
