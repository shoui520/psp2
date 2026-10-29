/* Orthonormalize a frame using the original NEON reciprocal estimates. */
#include <arm_neon.h>
#include <stdint.h>

static float32x4_t NormalizePafVector(float32x4_t value)
{
	float32x4_t squared = vmulq_f32(value, value);
	float32x2_t pair = vpadd_f32(vget_low_f32(squared), vget_high_f32(squared));
	float32x2_t sum = vpadd_f32(pair, pair);
	float32x2_t threshold = vdup_n_f32(0x1p-126f);
	uint32x2_t active = vcgt_f32(sum, threshold);
	float32x2_t guarded = vbsl_f32(active, sum, vdup_n_f32(1.0f));
	float32x2_t rsqrt = vrsqrte_f32(guarded);
	float32x2_t root;
	float32x2_t reciprocal;
	uint32x2_t invalid;

	rsqrt = vmul_f32(rsqrt,
			vrsqrts_f32(vmul_f32(rsqrt, rsqrt), guarded));
	rsqrt = vmul_f32(rsqrt,
			vrsqrts_f32(vmul_f32(rsqrt, rsqrt), guarded));
	root = vmul_f32(guarded, rsqrt);
	invalid = vorr_u32(vcle_f32(guarded, vdup_n_f32(0.0f)),
			   vceq_u32(vreinterpret_u32_f32(guarded),
				    vdup_n_u32(0x7f800000U)));
	root = vbsl_f32(invalid, guarded, root);
	root = vreinterpret_f32_u32(vand_u32(vreinterpret_u32_f32(root), active));
	reciprocal = vrecpe_f32(root);
	reciprocal = vmul_f32(reciprocal, vrecps_f32(reciprocal, root));
	reciprocal = vmul_f32(reciprocal, vrecps_f32(reciprocal, root));
	return vmulq_n_f32(value, vget_lane_f32(reciprocal, 0));
}

static void PafCross4(float *result, const float *left, const float *right)
{
	float first = left[1] * right[2] - left[2] * right[1];
	float second = left[2] * right[0] - left[0] * right[2];
	float third = left[0] * right[1] - left[1] * right[0];

	result[0] = first;
	result[1] = second;
	result[2] = third;
	/* The native lane permutation repeats the first component. */
	result[3] = first;
}

float *ScePafMisc_077B31E6(float *output, const float *input)
{
	float first[4];
	float middle[4];
	float last[4];
	float32x4_t normalizedFirst = NormalizePafVector(vld1q_f32(input));
	float32x4_t normalizedLast;
	unsigned i;

	vst1q_f32(first, normalizedFirst);
	PafCross4(last, first, input + 4);
	normalizedLast = NormalizePafVector(vld1q_f32(last));
	vst1q_f32(last, normalizedLast);
	PafCross4(middle, last, first);
	for (i = 0; i < 4; i++) {
		output[i] = first[i];
		output[i + 4] = middle[i];
		output[i + 8] = last[i];
		output[i + 12] = input[i + 12];
	}
	return output;
}
