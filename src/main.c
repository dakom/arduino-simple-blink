/*
 * Lights an LED while a switch is on.
 *
 *   D11 -- 180 ohm -- LED anode, LED cathode -- GND
 *   D9  -- switch -- GND (uses the internal pull-up)
 */
#include "pins.h"

constexpr pin_id PIN_LED = PIN_DIGITAL_11;
constexpr pin_id PIN_SWITCH = PIN_DIGITAL_9;

int main(void) {
    pin_set_config(PIN_LED, PIN_DIR_OUTPUT, PIN_FLAG_LOW);
    pin_set_config(PIN_SWITCH, PIN_DIR_INPUT, PIN_FLAG_HIGH);

    while (true) {
        // Active-low: the pull-up weakly holds the pin at 5 V, and a closed
        // switch overpowers it with a direct path to GND, so "on" reads LOW.
        bool switch_is_on = pin_get_flag(PIN_SWITCH) == PIN_FLAG_LOW;

        pin_set_flag(PIN_LED, switch_is_on ? PIN_FLAG_HIGH : PIN_FLAG_LOW);
    }
}
