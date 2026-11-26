#pragma once
/**
 * @file utils.h
 * @brief Small integer math helpers (no floating point: the chip has no FPU).
 */

#include <stdint.h>

/**
 * @brief numerator / divisor, rounded to the nearest whole number.
 *
 * Plain integer division always rounds down; adding half the divisor first
 * rounds to the nearest instead.
 */
uint32_t divide_round(uint32_t numerator, uint32_t divisor);
