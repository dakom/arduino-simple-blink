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
#include "blink.h"
#include "helpers/pins.h"
#include "helpers/serial.h"
#include "helpers/time_utils.h"
#include <avr/interrupt.h>

constexpr pin_id PIN_LED = PIN_DIGITAL_11;
constexpr pin_id PIN_SWITCH = PIN_DIGITAL_9;

// Must match BAUD in Taskfile.yml, which the serial monitors use.
constexpr uint16_t baud_rate = 9600;

int main(void) {
    pin_set_config(PIN_LED, PIN_DIR_OUTPUT, PIN_FLAG_LOW);
    pin_set_config(PIN_SWITCH, PIN_DIR_INPUT, PIN_FLAG_HIGH);

    init_time();
    init_serial(baud_rate);

    // Interrupts are off after every reset. Turn them on only once everything
    // they use is set up.
    sei();

    while (true) {
        // Active-low: the pull-up weakly holds the pin at 5 V, and a closed
        // switch overpowers it with a direct path to GND, so "on" reads LOW.
        bool switch_is_on = pin_get_flag(PIN_SWITCH) == PIN_FLAG_LOW;

        blink_update(switch_is_on, PIN_LED);
    }
}
