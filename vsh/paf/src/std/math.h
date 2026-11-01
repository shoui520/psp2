#ifndef PAF_STD_MATH_H
#define PAF_STD_MATH_H

#include <stdint.h>
#include <intrinsics.h>

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


#ifdef __cplusplus
extern "C" {
#endif

float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_tanf(float x);
float sce_paf_atanf(float x);
float sce_paf_atan2f(float x, float y);
float sce_paf_asinf(float x);
float sce_paf_acosf(float x);
float sce_paf_expf(float x);
float sce_paf_logf(float x);
float sce_paf_powf(float base, float exponent);
float sce_paf_floorf(float x);
float sce_paf_ceilf(float x);
float sce_paf_roundf(float x);
double sce_paf_floor(double x);
double sce_paf_fmod(double x, double y);

#ifdef __cplusplus
}
#endif

#endif
