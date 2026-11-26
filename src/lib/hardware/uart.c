#include "lib/hardware/uart.h"
#include "lib/collections/ring_buffer.h"
#include "lib/math/utils.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

constexpr uint16_t SERIAL_BUFFER_SIZE = 128;

// Bytes waiting to be sent. Written by serial_write_string(), read by the ISR.
static ring_buffer_t serial_buffer = {.size = SERIAL_BUFFER_SIZE,
                                      .queue = (uint8_t[SERIAL_BUFFER_SIZE]){}};

void init_serial() {

    // The baud rate register divides the clock: UBRR0 = F_CPU / (16 * baud) - 1.
    // The 16 is because the USART splits each bit into 16 time slices. Serial has
    // no clock wire, so the receiver times every bit from the start of each byte
    // and samples it in the middle, 8 slices in, the furthest from either edge.
    // The math is 32-bit: 16 * 9600 doesn't fit in a 16-bit int.
    // The result (103 at 9600 baud) fits easily in the 16-bit register, hence the cast.
    uint16_t register_rate =
        (uint16_t)(divide_round((uint32_t)F_CPU, (16 * (uint32_t)BAUD_RATE)) - 1);

    // Set the USART Baud Rate Register 0 to our desired rate
    UBRR0 = register_rate;

    // Set the Transmitter Enable bit for USART 0 (TXEN0)
    // on USART Control and Status Register B
    UCSR0B |= (1 << TXEN0);
}

void serial_write_string(const char *string) {
    // The ISR modifies serial_buffer and the register, and could run halfway
    // through either change, so keep interrupts off until both are done.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ring_buffer_write_string(&serial_buffer, string);

        // there is now data to send, so we need to interrupt
        UCSR0B |= (1 << UDRIE0);
    }
}

// Runs whenever the data register is empty and UDRIE0 is set: the interrupt
// version of polling `while (!(UCSR0A & (1 << UDRE0))) {}`.
// It keeps firing for as long as the register is empty, so once there's nothing
// left to send it must switch itself off, or the main loop would never run again.
ISR(USART_UDRE_vect) {
    auto result = ring_buffer_read_byte(&serial_buffer);

    if (!result.exists) {
        // no more data to send, so no need to interrupt
        UCSR0B &= (uint8_t)~(1 << UDRIE0);
    } else {
        // Send the byte
        UDR0 = result.byte;
    }
}
