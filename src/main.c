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
#include "helpers/callbacks_time.h"
#include "helpers/serial.h"
#include "helpers/time_utils.h"
#include "lights.h"
#include <avr/interrupt.h>

int main(void) {
    init_buttons();
    init_lights();
    init_time();
    init_serial();
    init_callbacks_time();

    // Interrupts are off after every reset. Turn them on only once everything
    // they use is set up.
    sei();

    while (true) {
        millis_t now = time_now();
        callbacks_time_execute(now);
        buttons_update(now);
        lights_update(button_is_pressed(BUTTON_ID_LIGHT_SWITCH), now);
    }
}
