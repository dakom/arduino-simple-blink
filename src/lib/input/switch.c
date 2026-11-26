#include "lib/input/switch.h"
#include "lib/callbacks/event.h"
#include "lib/callbacks/time.h"
#include <stdint.h>

// How debouncing works, per switch:
//
//   interrupt          on each change of the pin's level: raw.pressed = new level,
//   (any time)         raw.edges++
//   input_switch_      sees raw.edges move: cancels the pending settle callback, and if the
//   debounce           raw level differs from the accepted one, schedules a new settle
//   (main loop)        callback `debounce` ms from now
//   settle callback    nothing changed for `debounce` ms: accept the raw level as pressed,
//                      then execute on_press or on_release
//
// So a change only counts after the pin has been steady for the whole debounce time.

// Whether the switch is pressed right now (not debounced). Pressed reads LOW with the
// pull-up, where the switch connects the pin to GND, and HIGH with an external pull-down.
static bool read_pressed(const input_switch_t *input_switch) {
    gpio_flag_t pressed_level = input_switch->pull_up ? GPIO_FLAG_LOW : GPIO_FLAG_HIGH;
    return gpio_get_flag(input_switch->pin) == pressed_level;
}

// The settle callback: runs once a switch's pin has had no changes for the debounce time,
// accepts its level, and tells the listeners. context is that switch's input_switch_t (set
// by init_input_switch).
static void input_switch_settle(void *context, [[maybe_unused]] void *data) {
    input_switch_t *input_switch = context;
    input_switch->state.pressed = input_switch->raw.pressed;

    if (input_switch->state.pressed) {
        event_execute(&input_switch->on_press, input_switch);
    } else {
        event_execute(&input_switch->on_release, input_switch);
    }
}

void init_input_switch(input_switch_t *input_switch) {
    // gpio_set_config's level for an input picks the pull-up: HIGH turns it on
    gpio_set_config(input_switch->pin, GPIO_DIR_INPUT,
                    input_switch->pull_up ? GPIO_FLAG_HIGH : GPIO_FLAG_LOW);
    input_switch->state.pressed = read_pressed(input_switch);
    input_switch->raw.pressed = input_switch->state.pressed;
    input_switch->state.settle =
        (timed_callback_t){.fn = input_switch_settle, .context = input_switch};
    gpio_interrupt_enable(input_switch->pin);
}

void input_switch_debounce(input_switch_t *input_switch, millis_t now, millis_t debounce) {
    // Read each volatile field once, so both checks below see the same values
    uint8_t raw_edges = input_switch->raw.edges;
    bool raw_pressed = input_switch->raw.pressed;

    // The interrupt saw the pin change since last time: whatever was settling isn't steady
    // any more, so the settle callback has to start over or stop
    if (raw_edges != input_switch->state.edges) {
        input_switch->state.edges = raw_edges;

        // If the pin now differs from the accepted level, start a fresh debounce wait
        // (schedule moves the settle callback if it was already pending). If it bounced back
        // to the accepted level, there's nothing to settle.
        if (raw_pressed != input_switch->state.pressed) {
            callbacks_time_schedule(&input_switch->state.settle, now + debounce);
        } else {
            callbacks_time_cancel(&input_switch->state.settle); // false if none: fine
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
