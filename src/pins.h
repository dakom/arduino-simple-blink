/**
 * @file pins.h
 * @brief Digital I/O for the Arduino Uno (ATmega328P).
 *
 * A pin's level sets its voltage; the connected circuit determines the current.
 * Design for less than 20 mA per pin (40 mA is the absolute maximum), and see
 * the datasheet for the limits on groups of pins and on the whole chip.
 *
 * These functions don't validate pin IDs and aren't interrupt-safe.
 */
#ifndef PINS_H
#define PINS_H

#include <stdint.h>

/** @brief A pin, named by its label on the Uno board. */
typedef enum pin_id : uint8_t {
    PIN_DIGITAL_0, ///< Also serial RX.
    PIN_DIGITAL_1, ///< Also serial TX.
    PIN_DIGITAL_2,
    PIN_DIGITAL_3,
    PIN_DIGITAL_4,
    PIN_DIGITAL_5,
    PIN_DIGITAL_6,
    PIN_DIGITAL_7,
    PIN_DIGITAL_8,
    PIN_DIGITAL_9,
    PIN_DIGITAL_10,
    PIN_DIGITAL_11,
    PIN_DIGITAL_12,
    PIN_DIGITAL_13, ///< Also drives the built-in "L" LED.
    PIN_ANALOG_0,
    PIN_ANALOG_1,
    PIN_ANALOG_2,
    PIN_ANALOG_3,
    PIN_ANALOG_4,
    PIN_ANALOG_5,
} pin_id;

/** @brief Whether a pin senses a voltage or drives one. */
typedef enum pin_dir : uint8_t {
    PIN_DIR_INPUT,  ///< Senses the voltage on the pin.
    PIN_DIR_OUTPUT, ///< Drives the pin to 5 V or 0 V.
} pin_dir;

/**
 * @brief A logic level.
 *
 * When written to an input, the level selects the internal pull-up instead.
 */
typedef enum pin_flag : uint8_t {
    PIN_FLAG_LOW,  ///< Output: 0 V. Input: pull-up off.
    PIN_FLAG_HIGH, ///< Output: 5 V. Input: pull-up on.
} pin_flag;

/**
 * @brief Configure a pin's direction and initial level, without glitches.
 *
 * @param id            The pin to configure.
 * @param dir           The new direction.
 * @param initial_flag  For an output, the starting level.
 *                      For an input, HIGH enables the pull-up (20-50 kOhm).
 */
void pin_set_config(pin_id id, pin_dir dir, pin_flag initial_flag);

/**
 * @brief Set an output's level, or switch an input's pull-up on or off.
 *
 * @param id    The pin to write.
 * @param flag  The level to write; see ::pin_flag.
 */
void pin_set_flag(pin_id id, pin_flag flag);

/**
 * @brief Read the level that is physically present on a pin.
 *
 * An input with no pull-up, pull-down or driver floats and reads unpredictably.
 *
 * @param id  The pin to read.
 * @return    ::PIN_FLAG_HIGH or ::PIN_FLAG_LOW.
 */
pin_flag pin_get_flag(pin_id id);

#endif /* PINS_H */
