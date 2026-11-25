/**
 * @file serial.h
 * @brief Interrupt-driven serial output on USART0 (pin D1, through the Uno's USB port).
 *
 * Writing only queues the text and returns at once; an interrupt sends it a byte
 * at a time in the background. Format is 8N1 (8 data bits, no parity, 1 stop bit).
 */
#ifndef SERIAL_H
#define SERIAL_H

/**
 * @brief Set the speed and switch the transmitter on.
 *
 * Call once at startup, then enable interrupts with sei(), or nothing is sent.
 * Pin D1 belongs to the serial port from then on.
 *
 */
void init_serial();

/**
 * @brief Queue a string to send, without its terminating '\0'.
 *
 * Doesn't wait. The queue holds 128 bytes: if more is written before it's sent,
 * the oldest unsent bytes are dropped. Use "\r\n" to end lines in a terminal.
 */
void serial_write_string(const char *string);

#endif
