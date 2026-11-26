/*
 * Blinks an LED while a switch is on, and reports each change over serial.
 *
 *   D11 -- 180 ohm -- LED anode, LED cathode -- GND
 *   D9  -- switch -- GND (uses the internal pull-up)
 *
 * Nothing here waits: the millisecond clock and serial output both run on
 * interrupts, so the main loop just checks the switch and updates the blink as
 * fast as it can go round.
 */
#include "buttons.h"
#include "lib/all.h"
#include "lights.h"
#include <avr/interrupt.h>

int main(void) {
    // Library init
    init_timer();
    init_uart();
    init_callbacks_time();

    // Application init
    init_buttons();
    init_lights();

    // Interrupts are off after every reset. Turn them on only once everything
    // they use is set up.
    sei();

    // Handle the case where we start off with the button pressed
    // after this, it's all callback-driven
    if (button_get(BUTTON_ID_LIGHT_SWITCH)->state.pressed) {
        lights_update(true, timer_now());
    }

    while (true) {
        millis_t now = timer_now();
        callbacks_time_execute(now);
        buttons_debounce(now);
    }
}
