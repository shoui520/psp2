/* Wrapping high-word fixed-point multiplication. */

#pragma once

#include <stdint.h>

#define MUL_HIGH(left, right) \
	((uint32_t)((int64_t)(left) * (int64_t)(right) >> 32))

