#pragma once
/**
 * @file lights.h
 * @brief A non-blocking blink, driven by timed callbacks: call lights_update() every time round
 * the main loop.
 *
 * Needs init_time(), init_serial() and init_callbacks_time() to have run, and interrupts
 * enabled.
 */

#include "lib/all.h"

/** @brief Set the LED pin up as an output, off. Call once at startup. */
void init_lights();

/**
 * @brief Blink the LED while active, keep it off otherwise.
 *
 * Only acts when active changes, and says so over serial. Starting turns the LED on
 * straight away and schedules a toggle every BLINK_SPEED (in config.h) through
 * callbacks_time; stopping cancels it and turns the LED off. So the blink needs
 * callbacks_time_execute() in the main loop too, and one free callbacks_time slot.
 *
 * @param active  Whether the LED should be blinking.
 * @param now     The current time, from time_now().
 */
void lights_update(bool active, millis_t now);
