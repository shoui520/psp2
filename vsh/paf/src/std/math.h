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

#define sce_paf_sinf sce_paf_sinf
#define sce_paf_cosf sce_paf_cosf
#define sce_paf_tanf sce_paf_tanf
#define sce_paf_atanf sce_paf_atanf
#define sce_paf_atan2f sce_paf_atan2f
#define sce_paf_asinf sce_paf_asinf
#define sce_paf_acosf sce_paf_acosf
#define sce_paf_expf sce_paf_expf
#define sce_paf_logf sce_paf_logf
#define sce_paf_powf sce_paf_powf
#define sce_paf_floorf sce_paf_floorf
#define sce_paf_ceilf sce_paf_ceilf
#define sce_paf_roundf sce_paf_roundf
#define sce_paf_floor sce_paf_floor

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
