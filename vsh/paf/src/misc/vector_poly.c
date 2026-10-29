/* Six Euler rotation conventions, evaluated using Sony's NEON sine/cosine polynomial.
 * The fourth input lane is reduced in place but is not used by the quaternion.
 */
#include <arm_neon.h>
#include <stdint.h>

static float32x4_t PafCoefficient(uint32_t bits)
{
	return vreinterpretq_f32_u32(vdupq_n_u32(bits));
}

static void PafSinCosHalfAngles(float *halfAngles, float *sine, float *cosine)
{
	float32x4_t x = vmulq_n_f32(vld1q_f32(halfAngles), 0.5f);
	uint32x4_t originalBits = vreinterpretq_u32_f32(x);
	uint32x4_t signs = vandq_u32(originalBits, vdupq_n_u32(0x80000000U));
	float32x4_t half = vreinterpretq_f32_u32(
		vorrq_u32(signs, vdupq_n_u32(0x3f000000U)));
	int32x4_t quadrant = vcvtq_s32_f32(vaddq_f32(
		vmulq_f32(x, PafCoefficient(0x3f22f983U)), half));
	float32x4_t quadrantFloat = vcvtq_f32_s32(quadrant);
	float32x4_t angle = vmlsq_f32(x, quadrantFloat,
				     PafCoefficient(0x3fc90fdaU));
	float32x4_t square;
	float32x4_t cube;
	float32x4_t sinPoly;
	float32x4_t cosPoly;
	float32x4_t sinBase;
	float32x4_t cosBase;
	float32x4_t s;
	float32x4_t c;
	uint32x4_t sinEven, cosEven, sinPositive, cosPositive, infinite;

	angle = vmlsq_f32(angle, quadrantFloat, PafCoefficient(0x33a22168U));
	square = vmulq_f32(angle, angle);
	cube = vmulq_f32(square, angle);
	sinPoly = vmlaq_f32(PafCoefficient(0x3c088342U),
			   PafCoefficient(0xb94c8c6eU), square);
	sinPoly = vmlaq_f32(PafCoefficient(0xbe2aaaa1U), sinPoly, square);
	cosPoly = vmlaq_f32(PafCoefficient(0x3d2aa036U),
			   PafCoefficient(0xbab24993U), square);
	cosPoly = vmlaq_f32(PafCoefficient(0xbeffffdfU), cosPoly, square);
	sinBase = vmlaq_f32(angle, sinPoly, cube);
	cosBase = vmlaq_f32(vdupq_n_f32(1.0f), cosPoly, square);

	quadrant = vandq_s32(quadrant, vdupq_n_s32(3));
	{
		int32x4_t next = vaddq_s32(vdupq_n_s32(1), quadrant);

		sinEven = vceqq_s32(vandq_s32(quadrant, vdupq_n_s32(1)),
				    vdupq_n_s32(0));
		cosEven = vceqq_s32(vandq_s32(next, vdupq_n_s32(1)),
				    vdupq_n_s32(0));
		sinPositive = vceqq_s32(vandq_s32(quadrant, vdupq_n_s32(2)),
					vdupq_n_s32(0));
		cosPositive = vceqq_s32(vandq_s32(next, vdupq_n_s32(2)),
					vdupq_n_s32(0));
	}

	s = vbslq_f32(sinEven, sinBase, cosBase);
	c = vbslq_f32(cosEven, sinBase, cosBase);
	s = vbslq_f32(sinPositive, s, vnegq_f32(s));
	c = vbslq_f32(cosPositive, c, vnegq_f32(c));
	s = vbslq_f32(vceqq_f32(x, vdupq_n_f32(0.0f)), x, s);
	infinite = vceqq_u32(vandq_u32(originalBits,
					vdupq_n_u32(0x7fffffffU)),
			      vdupq_n_u32(0x7f800000U));
	s = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(s), infinite));
	c = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(c), infinite));
	vst1q_f32(halfAngles, x);
	vst1q_f32(sine, s);
	vst1q_f32(cosine, c);
}

static void PafEulerQuaternion(float *output, float *angles, unsigned order)
{
	float s[4], c[4];
	float s0, s1, s2, c0, c1, c2;

	PafSinCosHalfAngles(angles, s, c);
	s0 = s[0]; s1 = s[1]; s2 = s[2];
	c0 = c[0]; c1 = c[1]; c2 = c[2];

	switch (order) {
	case 0:
		output[0] = c0 * s1 * s2 + c1 * c2 * s0;
		output[1] = c0 * c2 * s1 - c1 * s0 * s2;
		output[2] = c0 * c1 * s2 + c2 * s0 * s1;
		output[3] = c0 * c1 * c2 - s0 * s1 * s2;
		break;
	case 1:
		output[0] = c0 * s1 * s2 - c1 * c2 * s0;
		output[1] = c0 * c2 * s1 + c1 * s0 * s2;
		output[2] = -c0 * c1 * s2 + c2 * s0 * s1;
		output[3] = c0 * c1 * c2 + s0 * s1 * s2;
		break;
	case 2:
		output[0] = c0 * s1 * s2 - c1 * c2 * s0;
		output[1] = c0 * c2 * s1 + c1 * s0 * s2;
		output[2] = c0 * c1 * s2 + c2 * s0 * s1;
		output[3] = -(c0 * c1 * c2) + s0 * s1 * s2;
		break;
	case 3:
		output[0] = c0 * s1 * s2 + c1 * c2 * s0;
		output[1] = c0 * c2 * s1 + c1 * s0 * s2;
		output[2] = c0 * c1 * s2 - c2 * s0 * s1;
		output[3] = c0 * c1 * c2 - s0 * s1 * s2;
		break;
	case 4:
		output[0] = c0 * s1 * s2 + c1 * c2 * s0;
		output[1] = c0 * c2 * s1 - c1 * s0 * s2;
		output[2] = c0 * c1 * s2 - c2 * s0 * s1;
		output[3] = c0 * c1 * c2 + s0 * s1 * s2;
		break;
	default:
		output[0] = -(c0 * s1 * s2) + c1 * c2 * s0;
		output[1] = c0 * c2 * s1 - c1 * s0 * s2;
		output[2] = c0 * c1 * s2 + c2 * s0 * s1;
		output[3] = c0 * c1 * c2 + s0 * s1 * s2;
		break;
	}
}

float *ScePafMisc_78190BD6(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 0);
	return output;
}

float *ScePafMisc_22CB84B0(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 1);
	return output;
}

float *ScePafMisc_D7E919C5(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 2);
	return output;
}

float *ScePafMisc_315CCB24(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 3);
	return output;
}

float *ScePafMisc_39B88D48(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 4);
	return output;
}

float *ScePafMisc_195D4D04(float *output, float *angles)
{
	PafEulerQuaternion(output, angles, 5);
	return output;
}
