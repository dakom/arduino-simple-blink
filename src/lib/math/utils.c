#include "lib/math/utils.h"

uint32_t divide_round(uint32_t numerator, uint32_t divisor) {
    return (numerator + (divisor / 2)) / divisor;
}
