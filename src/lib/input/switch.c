#include "lib/input/switch.h"
#include "lib/callbacks/time.h"
#include <stdint.h>

// How debouncing works, per switch:
//
//   interrupt          on each change of the pin's level: raw.pressed = new level,
//   (any time)         raw.edges++
//   input_switch_      sees raw.edges move: cancels the pending settle callback, and if the
//   update             raw level differs from the accepted one, schedules a new settle
//   (main loop)        callback `debounce` ms from now
//   settle callback    nothing changed for `debounce` ms: accept the raw level as pressed
//
// So a change only counts after the pin has been steady for the whole debounce time.

// Whether the switch is pressed right now (not debounced). Pressed reads LOW with the
// pull-up, where the switch connects the pin to GND, and HIGH with an external pull-down.
static bool read_pressed(const input_switch_t *input_switch) {
    pin_flag pressed_level = input_switch->pull_up ? PIN_FLAG_LOW : PIN_FLAG_HIGH;
    return pin_get_flag(input_switch->pin) == pressed_level;
}

void init_input_switch(input_switch_t *input_switch) {
    // pin_set_config's level for an input picks the pull-up: HIGH turns it on
    pin_set_config(input_switch->pin, PIN_DIR_INPUT,
                   input_switch->pull_up ? PIN_FLAG_HIGH : PIN_FLAG_LOW);
    input_switch->state.pressed = read_pressed(input_switch);
    input_switch->raw.pressed = input_switch->state.pressed;
    pin_interrupt_enable(input_switch->pin);
}

// The settle callback: runs once a switch's pin has had no changes for the debounce time,
// and accepts its level. context is that switch's input_switch_t. (Its handle goes stale
// by itself as it runs, so there is nothing to clear.)
static void input_switch_settle(void *context) {
    input_switch_t *input_switch = context;
    input_switch->state.pressed = input_switch->raw.pressed;
}

void input_switch_update(input_switch_t *input_switch, millis_t now, millis_t debounce) {
    // Read each volatile field once, so both checks below see the same values
    uint8_t raw_edges = input_switch->raw.edges;
    bool raw_pressed = input_switch->raw.pressed;

    // The interrupt saw the pin change since last time: whatever was settling isn't steady
    // any more, so cancel the pending settle callback
    if (raw_edges != input_switch->state.edges) {
        input_switch->state.edges = raw_edges;
        callbacks_time_cancel(input_switch->state.callback_handle); // false if none: fine

        // If the pin now differs from the accepted level, start a fresh debounce wait. (If it
        // bounced back to the accepted level, there's nothing to settle.)
        if (raw_pressed != input_switch->state.pressed) {
            input_switch->state.callback_handle =
                callbacks_time_insert(input_switch_settle, input_switch, now + debounce);
        }
    }
}

void input_switch_handle_interrupt(input_switch_t *input_switch) {
    bool pressed = read_pressed(input_switch);
    if (pressed == input_switch->raw.pressed) {
        return; // not this switch (or it bounced away and back before we got here)
    }

    input_switch->raw.pressed = pressed;

    // This will loop when overflowing, so it's okay to increment indefinitely:
    // it's not meant to track a total count, just a count for comparison against the last update
    input_switch->raw.edges++;
}
