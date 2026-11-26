#include "lights.h"
#include "config.h"
#include "lib/all.h"

// The blink is a chain of timed callbacks: each toggle_light flips the LED and schedules
// the next one, BLINK_SPEED later. Stopping cancels whichever toggle is pending, which ends
// the chain. There is only ever one pending toggle, so it uses one callbacks_time slot.

typedef struct blink_state_t {
    bool active;          ///< active, as of the previous call (to spot changes).
    bool light_on;        ///< Whether the LED is on right now.
    millis_t next_toggle; ///< When the pending toggle is due.
    handle_t callback;    ///< The pending toggle, for cancelling it when the blink stops.
} blink_state_t;

static blink_state_t state = {};

// Turn the LED on or off, and remember which.
static void set_light(blink_state_t *blink, bool light_on) {
    blink->light_on = light_on;
    gpio_set_flag(PIN_LED, light_on ? GPIO_FLAG_HIGH : GPIO_FLAG_LOW);
}

// Runs every BLINK_SPEED while blinking: flip the LED, then schedule the next flip.
//
// The next deadline is the previous one + BLINK_SPEED, not timer_now() + BLINK_SPEED: the
// callback runs a little after its deadline (whenever the main loop gets to it), and
// counting from the deadline stops that delay adding up, so the blink keeps a steady rhythm.
//
// If the callbacks_time queue were ever full, the insert would return HANDLE_NONE and the
// blink would stop where it is. CALLBACKS_TIME_CAPACITY (Taskfile.yml) leaves room for it.
//
// context is the blink_state_t it was scheduled with (&state), so this function doesn't
// depend on there being only one blink: a second LED would be a second blink_state_t
// (plus a pin field), with no new functions.
static void toggle_light(void *context) {
    blink_state_t *blink = context;
    set_light(blink, !blink->light_on);
    blink->next_toggle += BLINK_SPEED;
    blink->callback = callbacks_time_insert(toggle_light, blink, blink->next_toggle);
}

void init_lights() {
    gpio_set_config(PIN_LED, GPIO_DIR_OUTPUT, GPIO_FLAG_LOW);
}

void lights_update(bool active, millis_t now) {
    // Nothing to do unless active changed: while blinking, the callbacks do the toggling
    if (state.active != active) {
        state.active = active;
        if (!active) {
            uart_write_string("Stopping blink!\r\n");
            set_light(&state, false);
            callbacks_time_cancel(state.callback); // ends the chain
        } else {
            uart_write_string("Starting blink!\r\n");
            // On straight away, then start the chain: first toggle (to off) BLINK_SPEED later
            set_light(&state, true);
            state.next_toggle = now + BLINK_SPEED;
            state.callback = callbacks_time_insert(toggle_light, &state, state.next_toggle);
        }
    }
}
