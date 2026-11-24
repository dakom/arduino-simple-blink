#include "blink.h"
#include "helpers/pins.h"
#include "helpers/serial.h"
#include "helpers/time_utils.h"

typedef struct blink_state_t {
    bool active;          ///< active, as of the previous call (to spot changes).
    bool light_on;        ///< Whether the LED is on right now.
    millis_t last_update; ///< When the LED last changed, from time_now().
} blink_state_t;

static blink_state_t state = {};

// Time between toggles, so a full on-off cycle takes twice this.
constexpr millis_t BLINK_SPEED = 250;

// Change the LED and remember when, so the next toggle waits a full BLINK_SPEED.
static void set_light(pin_id pin_led, bool light_on) {
    state.last_update = time_now();
    state.light_on = light_on;
    pin_set_flag(pin_led, light_on ? PIN_FLAG_HIGH : PIN_FLAG_LOW);
}

void blink_update(bool active, pin_id pin_led) {
    bool changed = state.active != active;
    state.active = active;

    if (changed) {
        if (!active) {
            serial_write_string("Stopping blink!\r\n");
        } else {
            serial_write_string("Starting blink!\r\n");
        }
    }

    if (!active) {
        pin_set_flag(pin_led, PIN_FLAG_LOW);
    } else if (changed) {
        // changed always starts on
        set_light(pin_led, true);
    } else if (time_now() - state.last_update >= BLINK_SPEED) {
        // blinking switches back and forth. Subtracting (rather than comparing
        // against a target time) stays correct when the clock wraps after ~49 days.
        set_light(pin_led, !state.light_on);
    }
}
