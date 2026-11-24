/**
 * @file blink.h
 * @brief A non-blocking blink: call blink_update() every time round the main loop.
 *
 * Needs init_time() and init_serial() to have run, and interrupts enabled.
 */
#ifndef BLINK_H
#define BLINK_H

#include "helpers/pins.h"

/**
 * @brief Blink the LED while active, keep it off otherwise.
 *
 * Returns at once: it toggles the LED only when enough time has passed since the
 * last change. When active changes, it says so over serial and, when starting,
 * turns the LED on straight away.
 *
 * @param active   Whether the LED should be blinking.
 * @param pin_led  The pin driving the LED, already configured as an output.
 */
void blink_update(bool active, pin_id pin_led);

#endif
