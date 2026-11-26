/*
 * Implementation of gpio.h.
 *
 * Each port x (B, C, D) has three 8-bit registers, with one bit per pin:
 *   DDRx   direction: 1 = output, 0 = input
 *   PORTx  output level, or pull-up enable for an input
 *   PINx   level currently on the pin
 *
 * See "I/O-Ports" in the ATmega328P datasheet.
 */
#include "lib/hardware/gpio.h"
#include <avr/io.h>

typedef enum gpio_port_t : uint8_t {
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D
} gpio_port_t;

typedef struct gpio_pin_info_t {
    gpio_port_t port;
    uint8_t bit;
} gpio_pin_info_t;

// Board label -> port and bit, from the Uno R3 pinout diagram.
static const gpio_pin_info_t pin_infos[] = {
    [GPIO_PIN_DIGITAL_0] = {.port = GPIO_PORT_D, .bit = 0},
    [GPIO_PIN_DIGITAL_1] = {.port = GPIO_PORT_D, .bit = 1},
    [GPIO_PIN_DIGITAL_2] = {.port = GPIO_PORT_D, .bit = 2},
    [GPIO_PIN_DIGITAL_3] = {.port = GPIO_PORT_D, .bit = 3},
    [GPIO_PIN_DIGITAL_4] = {.port = GPIO_PORT_D, .bit = 4},
    [GPIO_PIN_DIGITAL_5] = {.port = GPIO_PORT_D, .bit = 5},
    [GPIO_PIN_DIGITAL_6] = {.port = GPIO_PORT_D, .bit = 6},
    [GPIO_PIN_DIGITAL_7] = {.port = GPIO_PORT_D, .bit = 7},
    [GPIO_PIN_DIGITAL_8] = {.port = GPIO_PORT_B, .bit = 0},
    [GPIO_PIN_DIGITAL_9] = {.port = GPIO_PORT_B, .bit = 1},
    [GPIO_PIN_DIGITAL_10] = {.port = GPIO_PORT_B, .bit = 2},
    [GPIO_PIN_DIGITAL_11] = {.port = GPIO_PORT_B, .bit = 3},
    [GPIO_PIN_DIGITAL_12] = {.port = GPIO_PORT_B, .bit = 4},
    [GPIO_PIN_DIGITAL_13] = {.port = GPIO_PORT_B, .bit = 5},
    [GPIO_PIN_ANALOG_0] = {.port = GPIO_PORT_C, .bit = 0},
    [GPIO_PIN_ANALOG_1] = {.port = GPIO_PORT_C, .bit = 1},
    [GPIO_PIN_ANALOG_2] = {.port = GPIO_PORT_C, .bit = 2},
    [GPIO_PIN_ANALOG_3] = {.port = GPIO_PORT_C, .bit = 3},
    [GPIO_PIN_ANALOG_4] = {.port = GPIO_PORT_C, .bit = 4},
    [GPIO_PIN_ANALOG_5] = {.port = GPIO_PORT_C, .bit = 5},
};

// Port -> register address. The registers are volatile because reading or
// writing them has effects the compiler can't see.
static volatile uint8_t *port_direction_register[] = {
    [GPIO_PORT_B] = &DDRB,
    [GPIO_PORT_C] = &DDRC,
    [GPIO_PORT_D] = &DDRD,
};

static volatile uint8_t *port_write_register[] = {
    [GPIO_PORT_B] = &PORTB,
    [GPIO_PORT_C] = &PORTC,
    [GPIO_PORT_D] = &PORTD,
};

static volatile uint8_t const *port_read_register[] = {
    [GPIO_PORT_B] = &PINB,
    [GPIO_PORT_C] = &PINC,
    [GPIO_PORT_D] = &PIND,
};

typedef struct port_interrupt_info_t {
    volatile uint8_t *mask_register; // the register for watching pin changes, per-port
    uint8_t control_bit;             // the bit representing the port, always set on PCICR
} port_interrupt_info_t;

static port_interrupt_info_t const port_interrupt_info[] = {
    [GPIO_PORT_B] = {.mask_register = &PCMSK0, .control_bit = PCIE0},
    [GPIO_PORT_C] = {.mask_register = &PCMSK1, .control_bit = PCIE1},
    [GPIO_PORT_D] = {.mask_register = &PCMSK2, .control_bit = PCIE2},
};

// The byte with only this bit set, e.g. bit 3 -> 0b00001000. `1 << bit` alone is an int (16
// bits here), and the registers are 8 bits: this keeps it 8 bits, so -Wconversion is happy.
// ~bit_mask(...) is every bit except this one, for clearing it.
static uint8_t bit_mask(uint8_t bit) {
    return (uint8_t)(1u << bit);
}

void gpio_set_config(gpio_pin_t id, gpio_dir_t dir, gpio_flag_t initial_flag) {
    gpio_pin_info_t const *info = &pin_infos[id];

    volatile uint8_t *dir_reg = port_direction_register[info->port];

    if (dir == GPIO_DIR_INPUT) {
        // Stop driving before setting the pull-up, so that a pin becoming an
        // input is never briefly a hard 5 V (a short if it's wired to GND).
        *dir_reg &= (uint8_t)~bit_mask(info->bit);
        gpio_set_flag(id, initial_flag);
    } else {
        // Write the level before the direction, so that a pin becoming an output
        // never drives a stale level.
        gpio_set_flag(id, initial_flag);
        *dir_reg |= bit_mask(info->bit);
    }
}

void gpio_set_flag(gpio_pin_t id, gpio_flag_t flag) {
    gpio_pin_info_t const *info = &pin_infos[id];

    volatile uint8_t *reg = port_write_register[info->port];

    if (flag == GPIO_FLAG_HIGH) {
        *reg |= bit_mask(info->bit);
    } else {
        *reg &= (uint8_t)~bit_mask(info->bit);
    }
}

gpio_flag_t gpio_get_flag(gpio_pin_t id) {
    gpio_pin_info_t const *info = &pin_infos[id];

    volatile uint8_t const *reg = port_read_register[info->port];

    return (*reg & bit_mask(info->bit)) != 0 ? GPIO_FLAG_HIGH : GPIO_FLAG_LOW;
}

void gpio_interrupt_enable(gpio_pin_t id) {
    gpio_pin_info_t const *pin_info = &pin_infos[id];
    port_interrupt_info_t const *port_info = &port_interrupt_info[pin_info->port];

    // Enable the port as a whole
    PCICR |= bit_mask(port_info->control_bit);

    // Enable the bit within the port
    *port_info->mask_register |= bit_mask(pin_info->bit);
}
