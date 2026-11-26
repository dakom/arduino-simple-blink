#include "buttons.h"
#include "config.h"
#include "lib/all.h"
#include "lib/callbacks/event.h"
#include "lib/input/switch.h"
#include "lights.h"
#include <avr/interrupt.h>
#include <stddef.h>

// Every button, indexed by its button_id_t: one entry per id in buttons.h.
// Static, since the callback queue keeps each one's address while a change is settling.
//
// Give every id an entry: one left out in the middle would be all zeros, which means pin
// D0 (the serial port's RX pin) with no pull-up. The static_assert below catches one left
// off the end.
static input_switch_t buttons[] = {
    // The switch on PIN_SWITCH (see config.h): wired to GND, with the chip's pull-up.
    [BUTTON_ID_LIGHT_SWITCH] =
        {
            .pin = PIN_SWITCH,
            .pull_up = true,
        },
};

// The table is sized by its entries, not by BUTTON_ID_COUNT, so that this can catch a missing one
static_assert(sizeof buttons / sizeof buttons[0] == BUTTON_ID_COUNT,
              "buttons[] needs exactly one entry per button_id_t (see buttons.h)");

static void on_light_button_press([[maybe_unused]] void *context, [[maybe_unused]] void *event_data) {
    lights_update(true, timer_now());
}

static void on_light_button_release([[maybe_unused]] void *context, [[maybe_unused]] void *event_data) {
    lights_update(false, timer_now());
}

// Listeners are static, since each event keeps their addresses
static listener_t light_press_listener = {.fn = on_light_button_press};
static listener_t light_release_listener = {.fn = on_light_button_release};

void init_buttons() {
    for (size_t i = 0; i < BUTTON_ID_COUNT; i++) {
        init_input_switch(&buttons[i]);
    }

    event_subscribe(&buttons[BUTTON_ID_LIGHT_SWITCH].on_press, &light_press_listener);
    event_subscribe(&buttons[BUTTON_ID_LIGHT_SWITCH].on_release, &light_release_listener);
}

void buttons_debounce(millis_t now) {
    for (size_t i = 0; i < BUTTON_ID_COUNT; i++) {
        input_switch_debounce(&buttons[i], now, BUTTON_SWITCH_DEBOUNCE_THRESHOLD_MS);
    }
}

input_switch_t *button_get(button_id_t button_id) {
    return &buttons[button_id];
}

// The pin-change interrupt for port B (PCINT0_vect): pins D8-D13, where the light switch
// is (D9). It fires when a pin changes; nothing reads the pins every loop turn.
//
// It can't tell which pin changed, so it gives every button the chance to check its own:
// input_switch_handle_interrupt only counts a change of that button's own level, so
// calling it for a button that didn't change (or isn't on port B at all) does nothing.
//
// Buttons on other ports need their port's interrupt too (see gpio_interrupt_enable in
// lib/hardware/gpio.h): ISR(PCINT1_vect) for A0-A5, ISR(PCINT2_vect) for D0-D7, with the same loop.
// Without one, the chip resets the first time such a button changes.
ISR(PCINT0_vect) {
    for (size_t i = 0; i < BUTTON_ID_COUNT; i++) {
        input_switch_handle_interrupt(&buttons[i]);
    }
}
