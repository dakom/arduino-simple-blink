#pragma once
/**
 * @file buttons.h
 * @brief The app's buttons and switches, each debounced by lib/input/switch.
 *
 * Each one has a button_id_t, and its wiring is one entry in the table at the top of
 * buttons.c. A press or release only counts once the pin has been steady for
 * BUTTON_SWITCH_DEBOUNCE_THRESHOLD_MS (config.h). Each uses one callbacks_time slot while a
 * change is settling.
 *
 * To add one: add its id below (before BUTTON_ID_COUNT), add its entry to the table in
 * buttons.c, and make sure the pin-change interrupt for its port runs (see the ISR there).
 */

#include "lib/all.h"

/** @brief Names each button, and is its index in the table in buttons.c. */
typedef enum button_id_t {
    BUTTON_ID_LIGHT_SWITCH, ///< The switch that turns the blink on and off (PIN_SWITCH).
    BUTTON_ID_COUNT,        ///< How many there are. Keep it last; not a real button.
} button_id_t;

/**
 * @brief Set every button's pin up as an input and start watching it.
 *
 * Call once at startup, before sei(). Each button's current level is its starting state,
 * so a switch that's already on doesn't count as a change.
 */
void init_buttons();

/**
 * @brief Notice any changes the interrupts saw, and (re)start each one's debounce wait.
 *
 * Call every time round the main loop. New states are accepted later, by callbacks, so
 * callbacks_time_execute() must run in the main loop too.
 *
 * @param now  The current time, from timer_now().
 */
void buttons_update(millis_t now);

/**
 * @brief A button's debounced state: true while it's pressed (or the switch is on).
 *
 * @param button_id  Which button. Must be a real one, not BUTTON_ID_COUNT.
 */
bool button_is_pressed(button_id_t button_id);
