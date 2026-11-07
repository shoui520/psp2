#ifndef PAF_STD_MATH_H
#define PAF_STD_MATH_H

#include <stdint.h>
#include <intrinsics.h>
#include <std/math.h>

static inline int FloatToInt(float value)
{
	union {
		float value;
		uint32_t bits;
	} number;

	number.value = value;
	/* Both conversions saturate identically for these invalid inputs. */
	if ((number.bits & 0x7fffffffU) >= 0x4f000000U &&
	    number.bits != 0xcf000000U)
		return __builtin_vcvt_n_s32_f32(value, 1);
	return (int)value;
}

#endif
