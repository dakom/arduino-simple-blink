#pragma once
/**
 * @file gpio.h
 * @brief Digital I/O for the Arduino Uno (ATmega328P).
 *
 * A pin's level sets its voltage; the connected circuit determines the current.
 * Design for less than 20 mA per pin (40 mA is the absolute maximum), and see
 * the datasheet for the limits on groups of pins and on the whole chip.
 *
 * These functions don't validate pin IDs and aren't interrupt-safe.
 */

#include <stdint.h>

/** @brief A pin, named by its label on the Uno board. */
typedef enum gpio_pin_t : uint8_t {
    GPIO_PIN_DIGITAL_0, ///< Also serial RX.
    GPIO_PIN_DIGITAL_1, ///< Also serial TX.
    GPIO_PIN_DIGITAL_2,
    GPIO_PIN_DIGITAL_3,
    GPIO_PIN_DIGITAL_4,
    GPIO_PIN_DIGITAL_5,
    GPIO_PIN_DIGITAL_6,
    GPIO_PIN_DIGITAL_7,
    GPIO_PIN_DIGITAL_8,
    GPIO_PIN_DIGITAL_9,
    GPIO_PIN_DIGITAL_10,
    GPIO_PIN_DIGITAL_11,
    GPIO_PIN_DIGITAL_12,
    GPIO_PIN_DIGITAL_13, ///< Also drives the built-in "L" LED.
    GPIO_PIN_ANALOG_0,
    GPIO_PIN_ANALOG_1,
    GPIO_PIN_ANALOG_2,
    GPIO_PIN_ANALOG_3,
    GPIO_PIN_ANALOG_4,
    GPIO_PIN_ANALOG_5,
} gpio_pin_t;

/** @brief Whether a pin senses a voltage or drives one. */
typedef enum gpio_dir_t : uint8_t {
    GPIO_DIR_INPUT,  ///< Senses the voltage on the pin.
    GPIO_DIR_OUTPUT, ///< Drives the pin to 5 V or 0 V.
} gpio_dir_t;

/**
 * @brief A logic level.
 *
 * When written to an input, the level selects the internal pull-up instead.
 */
typedef enum gpio_flag_t : uint8_t {
    GPIO_FLAG_LOW,  ///< Output: 0 V. Input: pull-up off.
    GPIO_FLAG_HIGH, ///< Output: 5 V. Input: pull-up on.
} gpio_flag_t;

/**
 * @brief Configure a pin's direction and initial level, without glitches.
 *
 * @param id            The pin to configure.
 * @param dir           The new direction.
 * @param initial_flag  For an output, the starting level.
 *                      For an input, HIGH enables the pull-up (20-50 kOhm).
 */
void gpio_set_config(gpio_pin_t id, gpio_dir_t dir, gpio_flag_t initial_flag);

/**
 * @brief Set an output's level, or switch an input's pull-up on or off.
 *
 * @param id    The pin to write.
 * @param flag  The level to write; see ::gpio_flag_t.
 */
void gpio_set_flag(gpio_pin_t id, gpio_flag_t flag);

/**
 * @brief Read the level that is physically present on a pin.
 *
 * An input with no pull-up, pull-down or driver floats and reads unpredictably.
 *
 * @param id  The pin to read.
 * @return    ::GPIO_FLAG_HIGH or ::GPIO_FLAG_LOW.
 */
gpio_flag_t gpio_get_flag(gpio_pin_t id);

/**
 * @brief Run an interrupt every time a pin's level changes, in either direction.
 *
 * The chip has one pin-change interrupt per port, shared by every pin on it, so define
 * the ISR for the pin's port:
 *
 * | Pins       | Port | ISR                 |
 * |------------|------|---------------------|
 * | D8 - D13   | B    | `ISR(PCINT0_vect)`  |
 * | A0 - A5    | C    | `ISR(PCINT1_vect)`  |
 * | D0 - D7    | D    | `ISR(PCINT2_vect)`  |
 *
 * If two enabled pins share a port, the same ISR runs for both, and it can't tell which
 * one changed: read each pin and compare with its last level.
 *
 * Takes effect once interrupts are enabled with sei(). Without an ISR for the port, the
 * chip resets when the interrupt fires.
 *
 * @param id  The pin to watch.
 */
void gpio_interrupt_enable(gpio_pin_t id);
