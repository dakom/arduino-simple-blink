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

typedef enum pin_group : uint8_t {
    PIN_GROUP_B,
    PIN_GROUP_C,
    PIN_GROUP_D
} pin_group;

typedef struct pin_info_t {
    pin_group group;
    uint8_t bit;
} pin_info_t;

// Board label -> port and bit, from the Uno R3 pinout diagram.
static const pin_info_t pin_infos[] = {
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

typedef struct pin_group_interrupt_info_t {
    volatile uint8_t *mask_register; // the register for watching pin changes, per-group
    uint8_t control_bit;             // the bit representing the group, always set on PCICR
} pin_group_interrupt_info_t;

static pin_group_interrupt_info_t const pin_group_interrupt_info[] = {
    [PIN_GROUP_B] = {.mask_register = &PCMSK0, .control_bit = PCIE0},
    [PIN_GROUP_C] = {.mask_register = &PCMSK1, .control_bit = PCIE1},
    [PIN_GROUP_D] = {.mask_register = &PCMSK2, .control_bit = PCIE2},
};

// The byte with only this bit set, e.g. bit 3 -> 0b00001000. `1 << bit` alone is an int (16
// bits here), and the registers are 8 bits: this keeps it 8 bits, so -Wconversion is happy.
// ~bit_mask(...) is every bit except this one, for clearing it.
static uint8_t bit_mask(uint8_t bit) {
    return (uint8_t)(1u << bit);
}

void pin_set_config(pin_id id, pin_dir dir, pin_flag initial_flag) {
    pin_info_t const *info = &pin_infos[id];

    volatile uint8_t *dir_reg = pin_group_direction_register[info->group];

    if (dir == PIN_DIR_INPUT) {
        // Stop driving before setting the pull-up, so that a pin becoming an
        // input is never briefly a hard 5 V (a short if it's wired to GND).
        *dir_reg &= (uint8_t)~bit_mask(info->bit);
        pin_set_flag(id, initial_flag);
    } else {
        // Write the level before the direction, so that a pin becoming an output
        // never drives a stale level.
        pin_set_flag(id, initial_flag);
        *dir_reg |= bit_mask(info->bit);
    }
}

void pin_set_flag(pin_id id, pin_flag flag) {
    pin_info_t const *info = &pin_infos[id];

    volatile uint8_t *reg = pin_group_write_register[info->group];

    if (flag == PIN_FLAG_HIGH) {
        *reg |= bit_mask(info->bit);
    } else {
        *reg &= (uint8_t)~bit_mask(info->bit);
    }
}

pin_flag pin_get_flag(pin_id id) {
    pin_info_t const *info = &pin_infos[id];

    volatile uint8_t const *reg = pin_group_read_register[info->group];

    return (*reg & bit_mask(info->bit)) != 0 ? PIN_FLAG_HIGH : PIN_FLAG_LOW;
}

void pin_interrupt_enable(pin_id id) {
    pin_info_t const *pin_info = &pin_infos[id];
    pin_group_interrupt_info_t const *group_info = &pin_group_interrupt_info[pin_info->group];

    // Enable the group as a whole
    PCICR |= bit_mask(group_info->control_bit);

    // Enable the bit within the group
    *group_info->mask_register |= bit_mask(pin_info->bit);
}
