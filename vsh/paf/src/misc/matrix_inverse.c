/* Inverse of the 3-by-3 portion of a column-major 4-by-4 matrix. */
#include <arm_neon.h>

float *ScePafMisc_62EB3B09(float *output, const float *input)
{
	float a = input[0], b = input[1], c = input[2];
	float d = input[4], e = input[5], f = input[6];
	float g = input[8], h = input[9], i = input[10];
	float determinant =
		((a * e * i + b * f * g) + c * d * h) -
		((a * f * h + b * d * i) + c * e * g);
	float32x2_t divisor = vdup_n_f32(determinant);
	float32x2_t reciprocal = vrecpe_f32(divisor);
	float scale;

	reciprocal = vmul_f32(reciprocal, vrecps_f32(reciprocal, divisor));
	reciprocal = vmul_f32(reciprocal, vrecps_f32(reciprocal, divisor));
	scale = vget_lane_f32(reciprocal, 0);

	output[0] = (e * i - f * h) * scale;
	output[1] = (c * h - b * i) * scale;
	output[2] = (b * f - c * e) * scale;
	output[3] = 0.0f;
	output[4] = (f * g - d * i) * scale;
	output[5] = (a * i - c * g) * scale;
	output[6] = (c * d - a * f) * scale;
	output[7] = 0.0f;
	output[8] = (d * h - e * g) * scale;
	output[9] = (b * g - a * h) * scale;
	output[10] = (a * e - b * d) * scale;
	output[11] = 0.0f;
	output[12] = 0.0f;
	output[13] = 0.0f;
	output[14] = 0.0f;
	output[15] = 1.0f;

	/* The original VSTMIA advances r0 to the final matrix column. */
	return output + 12;
}
