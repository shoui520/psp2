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

#define sce_paf_sinf ScePafStdc_C3C6F821
#define sce_paf_cosf ScePafStdc_99E15A83
#define sce_paf_tanf ScePafStdc_3C250862
#define sce_paf_atanf ScePafStdc_A74ACFE5
#define sce_paf_atan2f ScePafStdc_30C3A75E
#define sce_paf_asinf ScePafStdc_A75AAF07
#define sce_paf_acosf ScePafStdc_C6A059A7
#define sce_paf_expf ScePafStdc_28E1E0ED
#define sce_paf_powf ScePafStdc_992DFB85
#define sce_paf_floorf ScePafStdc_148C1463
#define sce_paf_ceilf ScePafStdc_21241EE8
#define sce_paf_roundf ScePafStdc_65B8E8A9
#define sce_paf_floor ScePafStdc_1D16BA78

float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_tanf(float x);
float sce_paf_atanf(float x);
float sce_paf_atan2f(float x, float y);
float sce_paf_asinf(float x);
float sce_paf_acosf(float x);
float sce_paf_expf(float x);
float sce_paf_powf(float base, float exponent);
float sce_paf_floorf(float x);
float sce_paf_ceilf(float x);
float sce_paf_roundf(float x);
double sce_paf_floor(double x);

#endif
