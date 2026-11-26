#include "lights.h"
#include "config.h"
#include "lib/all.h"

// The blink is a chain of timed callbacks: each toggle_light flips the LED and schedules
// the next one, BLINK_SPEED later. Stopping cancels whichever toggle is pending, which ends
// the chain. The blink owns its one timed_callback_t, so it can never run out of room.

typedef struct blink_state_t {
    bool active;             ///< active, as of the previous call (to spot changes).
    bool light_on;           ///< Whether the LED is on right now.
    timed_callback_t toggle; ///< The next toggle. Its deadline is when it's (or was last) due.
} blink_state_t;

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
// It isn't pending while it runs, so it can schedule itself again; toggle.deadline still
// holds the deadline it just ran for.
//
// context is the blink_state_t it belongs to (&state), so this function doesn't depend on
// there being only one blink: a second LED would be a second blink_state_t (plus a pin
// field), with no new functions.
static void toggle_light(void *context, [[maybe_unused]] void *data) {
    blink_state_t *blink = context;
    set_light(blink, !blink->light_on);
    callbacks_time_schedule(&blink->toggle, blink->toggle.deadline + BLINK_SPEED);
}

// toggle's context points back at the state it belongs to
static blink_state_t state = {.toggle = {.fn = toggle_light, .context = &state}};

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
            callbacks_time_cancel(&state.toggle); // ends the chain
        } else {
            uart_write_string("Starting blink!\r\n");
            // On straight away, then start the chain: first toggle (to off) BLINK_SPEED later
            set_light(&state, true);
            callbacks_time_schedule(&state.toggle, now + BLINK_SPEED);
        }
    }
}
