/* Four-component reciprocal-norm transform; all NEON operations are C intrinsics. */
#include <arm_neon.h>

float *ScePafMisc_28D57B83(float *destination, const float *source)
{
	float32x4_t input = vld1q_f32(source);
	float32x4_t squared = vmulq_f32(input, input);
	float32x2_t pair = vpadd_f32(vget_low_f32(squared), vget_high_f32(squared));
	float32x2_t total = vpadd_f32(pair, pair);
	float32x4_t divisor = vdupq_n_f32(vget_lane_f32(total, 0));
	float32x4_t reciprocal = vrecpeq_f32(divisor);

	reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(reciprocal, divisor));
	reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(reciprocal, divisor));
	vst1q_f32(destination, vnegq_f32(vmulq_f32(input, reciprocal)));
	return destination;
}
