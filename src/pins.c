/*
 * Implementation of pins.h.
 *
 * Each port x (B, C, D) has three 8-bit registers, with one bit per pin:
 *   DDRx   direction: 1 = output, 0 = input
 *   PORTx  output level, or pull-up enable for an input
 *   PINx   level currently on the pin
 *
 * See "I/O-Ports" in the ATmega328P datasheet.
 */
#include "pins.h"
#include <avr/io.h>
#include <stdint.h>

typedef enum pin_group : uint8_t {
    PIN_GROUP_B,
    PIN_GROUP_C,
    PIN_GROUP_D
} pin_group;

typedef struct pin_info {
    pin_group group;
    uint8_t bit;
} pin_info;

// Board label -> port and bit, from the Uno R3 pinout diagram.
static const pin_info pin_infos[] = {
    [PIN_DIGITAL_0] = {.group = PIN_GROUP_D, .bit = 0},
    [PIN_DIGITAL_1] = {.group = PIN_GROUP_D, .bit = 1},
    [PIN_DIGITAL_2] = {.group = PIN_GROUP_D, .bit = 2},
    [PIN_DIGITAL_3] = {.group = PIN_GROUP_D, .bit = 3},
    [PIN_DIGITAL_4] = {.group = PIN_GROUP_D, .bit = 4},
    [PIN_DIGITAL_5] = {.group = PIN_GROUP_D, .bit = 5},
    [PIN_DIGITAL_6] = {.group = PIN_GROUP_D, .bit = 6},
    [PIN_DIGITAL_7] = {.group = PIN_GROUP_D, .bit = 7},
    [PIN_DIGITAL_8] = {.group = PIN_GROUP_B, .bit = 0},
    [PIN_DIGITAL_9] = {.group = PIN_GROUP_B, .bit = 1},
    [PIN_DIGITAL_10] = {.group = PIN_GROUP_B, .bit = 2},
    [PIN_DIGITAL_11] = {.group = PIN_GROUP_B, .bit = 3},
    [PIN_DIGITAL_12] = {.group = PIN_GROUP_B, .bit = 4},
    [PIN_DIGITAL_13] = {.group = PIN_GROUP_B, .bit = 5},
    [PIN_ANALOG_0] = {.group = PIN_GROUP_C, .bit = 0},
    [PIN_ANALOG_1] = {.group = PIN_GROUP_C, .bit = 1},
    [PIN_ANALOG_2] = {.group = PIN_GROUP_C, .bit = 2},
    [PIN_ANALOG_3] = {.group = PIN_GROUP_C, .bit = 3},
    [PIN_ANALOG_4] = {.group = PIN_GROUP_C, .bit = 4},
    [PIN_ANALOG_5] = {.group = PIN_GROUP_C, .bit = 5},
};

// Port -> register address. The registers are volatile because reading or
// writing them has effects the compiler can't see.
static volatile uint8_t *pin_group_direction_register[] = {
    [PIN_GROUP_B] = &DDRB,
    [PIN_GROUP_C] = &DDRC,
    [PIN_GROUP_D] = &DDRD,
};

static volatile uint8_t *pin_group_write_register[] = {
    [PIN_GROUP_B] = &PORTB,
    [PIN_GROUP_C] = &PORTC,
    [PIN_GROUP_D] = &PORTD,
};

static volatile uint8_t const *pin_group_read_register[] = {
    [PIN_GROUP_B] = &PINB,
    [PIN_GROUP_C] = &PINC,
    [PIN_GROUP_D] = &PIND,
};

void pin_set_config(pin_id id, pin_dir dir, pin_flag initial_flag) {
    pin_info const *info = &pin_infos[id];

    volatile uint8_t *dir_reg = pin_group_direction_register[info->group];

    if (dir == PIN_DIR_INPUT) {
        // Stop driving before setting the pull-up, so that a pin becoming an
        // input is never briefly a hard 5 V (a short if it's wired to GND).
        *dir_reg &= ~(1 << info->bit);
        pin_set_flag(id, initial_flag);
    } else {
        // Write the level before the direction, so that a pin becoming an output
        // never drives a stale level.
        pin_set_flag(id, initial_flag);
        *dir_reg |= (1 << info->bit);
    }
}

void pin_set_flag(pin_id id, pin_flag flag) {
    pin_info const *info = &pin_infos[id];

    volatile uint8_t *reg = pin_group_write_register[info->group];

    if (flag == PIN_FLAG_HIGH) {
        *reg |= (1 << info->bit);
    } else {
        *reg &= ~(1 << info->bit);
    }
}

pin_flag pin_get_flag(pin_id id) {
    pin_info const *info = &pin_infos[id];

    volatile uint8_t const *reg = pin_group_read_register[info->group];

    return (*reg & (1 << info->bit)) != 0 ? PIN_FLAG_HIGH : PIN_FLAG_LOW;
}
