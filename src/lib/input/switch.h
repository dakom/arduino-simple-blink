#pragma once
/**
 * @file switch.h
 * @brief A debounced switch or button on any pin, driven by its pin-change interrupt.
 *
 * A change only counts once the pin has been steady for the debounce time, so the
 * contacts bouncing for a millisecond or two after a press can't register as several
 * presses. Each switch is one input_switch_t, which you define and wire up:
 *
 *     static input_switch_t door = {.pin = GPIO_PIN_DIGITAL_9, .pull_up = true,
 *                                   .state = {.callback_handle = HANDLE_NONE}};
 *
 *     init_input_switch(&door);                    // at startup, before sei()
 *
 *     ISR(PCINT0_vect) {                           // the interrupt for the pin's port
 *         input_switch_handle_interrupt(&door);    // (see gpio_interrupt_enable in gpio.h)
 *     }
 *
 *     // in main's loop, once per pass, along with callbacks_time_execute(now):
 *     input_switch_update(&door, now, 20);
 *     if (door.state.pressed) { ... }
 *
 * Several switches on the same port share one ISR: call input_switch_handle_interrupt for
 * each of them there. Each one only counts changes of its own pin.
 *
 * Rules:
 * - The input_switch_t must be static or global: the callback queue keeps its address
 *   while a change is settling.
 * - Uses one callbacks_time slot per switch, only while a change is settling.
 * - input_switch_handle_interrupt is the only function here that may be called from an
 *   interrupt; the others are main loop only.
 */

#include "lib/collections/handle.h"
#include "lib/hardware/gpio.h"
#include "lib/hardware/timer.h"

/** @brief What the main loop knows about a switch. Only touched outside interrupts. */
typedef struct input_switch_state_t {
    bool pressed;             ///< The debounced level: what the rest of the program sees.
    uint8_t edges;            ///< raw.edges as of the last input_switch_update, to spot new ones.
    handle_t callback_handle; ///< The pending settle callback, to cancel on a new change.
} input_switch_state_t;

/**
 * @brief What the interrupt records. A volatile member of input_switch_t: the interrupt
 * changes it behind the main loop's back. Both fields are single bytes, so the main loop
 * can never read one half-written.
 */
typedef struct input_switch_state_raw_t {
    bool pressed;  ///< The pin's level when the interrupt last saw it change (not debounced).
    uint8_t edges; ///< Counts changes. Only compared for "moved?", so wrapping at 255 is fine.
} input_switch_state_raw_t;

/**
 * @brief One switch: where it is, how it's wired, and its state.
 *
 * Set pin and pull_up, and state.callback_handle to HANDLE_NONE; leave the rest zero.
 * Both halves of the state live here, so a pointer to the whole switch is all its settle
 * callback needs. Only raw is volatile, so the main loop's reads of state (the 4-byte
 * handle, say) stay ordinary reads.
 */
typedef struct input_switch_t {
    gpio_pin_t pin; ///< The pin the switch is on.
    /// true: the chip's internal pull-up holds the pin HIGH, and the switch connects it to
    /// GND, so "pressed" reads LOW. false: no internal pull-up; there must be an external
    /// pull-down resistor, and the switch connects the pin to 5 V, so "pressed" reads HIGH.
    bool pull_up;
    input_switch_state_t state;            ///< Main loop only.
    volatile input_switch_state_raw_t raw; ///< Written by the interrupt.
} input_switch_t;

/**
 * @brief Set the pin up as an input and start watching it.
 *
 * Call once at startup, before sei(). Takes the switch's current level as its starting
 * state, so a switch that's already on doesn't count as a change.
 */
void init_input_switch(input_switch_t *input_switch);

/**
 * @brief Notice any changes the interrupt saw, and (re)start the debounce wait.
 *
 * Call every time round the main loop. The new state is accepted later, by a callback, so
 * callbacks_time_execute() must run in the main loop too.
 *
 * @param now       The current time, from timer_now().
 * @param debounce  How long the pin must stay steady before a change counts, in ms.
 */
void input_switch_update(input_switch_t *input_switch, millis_t now, millis_t debounce);

/**
 * @brief Record a change of the pin's level. Call from the ISR for the pin's port.
 *
 * Safe to call when some other pin on the port caused the interrupt: it only counts a
 * change when this switch's own level differs from the last one it saw.
 */
void input_switch_handle_interrupt(input_switch_t *input_switch);
