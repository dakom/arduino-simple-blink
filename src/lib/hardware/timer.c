#include "lib/hardware/timer.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

// volatile: the ISR changes it behind the back of any code reading it, so every
// read must come from memory rather than a copy the compiler kept in a register.
static volatile millis_t ms_since_start = 0;

void init_time() {

    // the CPU ticks at F_CPU per second.
    // To get ticks per millisecond, divide by 1000
    // but the number needs to fit in a 8-bit register (max value 255) and ideally be whole
    // so, we pick 64 as our "prescaler": 16 MHz / 64 / 1000 = exactly 250 ticks.
    // The timer counts from 0, so 250 ticks means counting up to 249.
    uint8_t register_rate = (F_CPU / 64 / 1000) - 1;

    // Set the tick rate in the register
    OCR0A = register_rate;

    // Set CTC mode ("Clear Timer on Compare"): back to 0 after reaching OCR0A
    TCCR0A |= (1 << WGM01);

    // Compare-match interrupt
    TIMSK0 |= (1 << OCIE0A);

    // Set the prescaler to 64. This starts the timer, so it comes last.
    TCCR0B |= (1 << CS00) | (1 << CS01);
}

millis_t time_now() {
    millis_t value;

    // millis_t is 8 bytes, we need to avoid a tear: the CPU reads it one byte at a
    // time, and the ISR could change it between those reads.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        value = ms_since_start;
    }

    return value;
}

// Install the ISR for the timer: runs once per millisecond. A 64-bit increment takes about
// 50 cycles here, out of the 16,000 the CPU runs each millisecond.
ISR(TIMER0_COMPA_vect) {
    ms_since_start++;
}
