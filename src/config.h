#pragma once
#include "lib/all.h"

///////// App-specific config /////////////
[[maybe_unused]] constexpr pin_id PIN_LED = PIN_DIGITAL_11;
[[maybe_unused]] constexpr pin_id PIN_SWITCH = PIN_DIGITAL_9;
[[maybe_unused]] constexpr millis_t BUTTON_SWITCH_DEBOUNCE_THRESHOLD_MS = 20;

// Time between toggles, so a full on-off cycle takes twice this.
[[maybe_unused]] constexpr millis_t BLINK_SPEED = 250;

///////// Library config comes from compiler flags set in Taskfile.yml ///////////
// That is:
//
// F_CPU
// BAUD_RATE
// CALLBACKS_TIME_CAPACITY
