/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * Single-precision fdlibm/newlib math routines statically linked by the codecs
 * (floorf, logf, powf, scalbnf, cosf, ceilf, sqrtf, copysignf, and trig kernels).
 */
#include "audiocodec_internal.h"

typedef union FloatBits {
	float f;
	int32_t i;
	uint32_t u;
} FloatBits;

static inline int32_t getFloatWord(float x)
{
	FloatBits u;
	u.f = x;
	return u.i;
}

static inline float setFloatWord(int32_t i)
{
	FloatBits u;
	u.i = i;
	return u.f;
}

static inline float fabsVal(float x)
{
	return setFloatWord(getFloatWord(x) & 0x7FFFFFFF);
}

static const float kHuge = 1.0e30f;
static const float kTiny = 1.0e-30f;
static const float kTwo25 = 3.3554432000e+07f;
static const float kTwom25 = 2.9802322388e-08f;

__attribute__((noinline)) float audioCopysignf(float x, float y)
{
	int32_t ix = getFloatWord(x);
	int32_t iy = getFloatWord(y);
	return setFloatWord((ix & 0x7FFFFFFF) | (iy & (int32_t)0x80000000U));
}

float audioFloorf(float x)
{
	int32_t i0 = getFloatWord(x);
	int32_t j0 = ((i0 >> 23) & 0xFF) - 0x7F;
	if (j0 < 23) {
		if (j0 < 0) {
			if (kHuge + x > 0.0f) {
				if (i0 >= 0)
					i0 = 0;
				else if ((i0 & 0x7FFFFFFF) != 0)
					i0 = (int32_t)0xBF800000U;
			}
		} else {
			uint32_t i = 0x007FFFFFU >> j0;
			if (((uint32_t)i0 & i) == 0)
				return x;
			if (kHuge + x > 0.0f) {
				if (i0 < 0)
					i0 += 0x00800000 >> j0;
				i0 &= (int32_t)(~i);
			}
		}
	} else {
		if (j0 == 0x80)
			return x + x;
		return x;
	}
	return setFloatWord(i0);
}

float audioCeilf(float x)
{
	int32_t i0 = getFloatWord(x);
	int32_t j0 = ((i0 >> 23) & 0xFF) - 0x7F;
	if (j0 < 23) {
		if (j0 < 0) {
			if (kHuge + x > 0.0f) {
				if (i0 < 0)
					i0 = (int32_t)0x80000000U;
				else if (i0 != 0)
					i0 = 0x3F800000;
			}
		} else {
			uint32_t i = 0x007FFFFFU >> j0;
			if (((uint32_t)i0 & i) == 0)
				return x;
			if (kHuge + x > 0.0f) {
				if (i0 > 0)
					i0 += 0x00800000 >> j0;
				i0 &= (int32_t)(~i);
			}
		}
	} else {
		if (j0 == 0x80)
			return x + x;
		return x;
	}
	return setFloatWord(i0);
}

float audioScalbnf(float x, int n)
{
	int32_t ix = getFloatWord(x);
	int32_t k = (ix & 0x7F800000) >> 23;
	if (k == 0) {
		if ((ix & 0x7FFFFFFF) == 0)
			return x;
		x *= kTwo25;
		ix = getFloatWord(x);
		k = ((ix & 0x7F800000) >> 23) - 25;
		if (n < -50000)
			return kTiny * x;
	}
	if (k == 0xFF)
		return x + x;
	k = k + n;
	if (k > 0xFE)
		return kHuge * audioCopysignf(kHuge, x);
	if (k > 0)
		return setFloatWord((ix & (int32_t)0x807FFFFFU) | (k << 23));
	if (k <= -25) {
		if (n > 50000)
			return kHuge * audioCopysignf(kHuge, x);
		return kTiny * audioCopysignf(kTiny, x);
	}
	k += 25;
	x = setFloatWord((ix & (int32_t)0x807FFFFFU) | (k << 23));
	return x * kTwom25;
}

float audioSqrtf(float x)
{
	int32_t sign = (int32_t)0x80000000U;
	int32_t ix = getFloatWord(x);

	if ((ix & 0x7F800000) == 0x7F800000)
		return x * x + x;
	if (ix <= 0) {
		if ((ix & ~sign) == 0)
			return x;
		if (ix < 0)
			return (x - x) / (x - x);
	}
	int32_t m = ix >> 23;
	if (m == 0) {
		int32_t i;
		for (i = 0; (ix & 0x00800000) == 0; ++i)
			ix <<= 1;
		m -= i - 1;
	}
	m -= 127;
	ix = (ix & 0x007FFFFF) | 0x00800000;
	if (m & 1)
		ix += ix;
	m >>= 1;

	ix += ix;
	int32_t q = 0;
	int32_t s = 0;
	int32_t r = 0x01000000;

	while (r != 0) {
		int32_t t = s + r;
		if (t <= ix) {
			s = t + r;
			ix -= t;
			q += r;
		}
		ix += ix;
		r >>= 1;
	}

	if (ix != 0) {
		float z = 1.0f - kTiny;
		if (z >= 1.0f) {
			z = 1.0f + kTiny;
			if (z > 1.0f)
				q += 2;
			else
				q += (q & 1);
		}
	}
	ix = (q >> 1) + 0x3F000000;
	ix += (m << 23);
	return setFloatWord(ix);
}

float audioLogf(float x)
{
	static const float ln2Hi = 6.9313812256e-01f;
	static const float ln2Lo = 9.0580006145e-06f;
	static const float lg1 = 6.6666668653e-01f;
	static const float lg2 = 4.0000000596e-01f;
	static const float lg3 = 2.8571429849e-01f;
	static const float lg4 = 2.2222198546e-01f;
	static const float lg5 = 1.8183572590e-01f;
	static const float lg6 = 1.5313838422e-01f;
	static const float lg7 = 1.4798198640e-01f;

	int32_t ix = getFloatWord(x);
	int32_t k = 0;
	if (ix < 0x00800000) {
		if ((ix & 0x7FFFFFFF) == 0)
			return -kTwo25 / 0.0f;
		if (ix < 0)
			return (x - x) / 0.0f;
		k -= 25;
		x *= kTwo25;
		ix = getFloatWord(x);
	}
	if (ix >= 0x7F800000)
		return x + x;
	k += (ix >> 23) - 127;
	ix &= 0x007FFFFF;
	int32_t i = (ix + (0x95F64 << 3)) & 0x800000;
	x = setFloatWord(ix | (i ^ 0x3F800000));
	k += (i >> 23);
	float f = x - 1.0f;
	if ((0x007FFFFF & (15 + ix)) < 16) {
		if (f == 0.0f) {
			if (k == 0)
				return 0.0f;
			float dk = (float)k;
			return dk * ln2Hi + dk * ln2Lo;
		}
		float R = f * f * (0.5f - 0.33333333333333333f * f);
		if (k == 0)
			return f - R;
		float dk = (float)k;
		return dk * ln2Hi - ((R - dk * ln2Lo) - f);
	}
	float s = f / (2.0f + f);
	float dk = (float)k;
	float z = s * s;
	i = ix - (0x6147A << 3);
	float w = z * z;
	int32_t j = (0x6B851 << 3) - ix;
	float t1 = w * (lg2 + w * (lg4 + w * lg6));
	float t2 = z * (lg1 + w * (lg3 + w * (lg5 + w * lg7)));
	i |= j;
	float R = t2 + t1;
	if (i > 0) {
		float hfsq = 0.5f * f * f;
		if (k == 0)
			return f - (hfsq - s * (hfsq + R));
		return dk * ln2Hi - ((hfsq - (s * (hfsq + R) + dk * ln2Lo)) - f);
	}
	if (k == 0)
		return f - s * (f - R);
	return dk * ln2Hi - ((s * (f - R) - dk * ln2Lo) - f);
}

float audioPowf(float x, float y)
{
	static const float bp[2] = { 1.0f, 1.5f };
	static const float dpH[2] = { 0.0f, 5.84960938e-01f };
	static const float dpL[2] = { 0.0f, 1.56322085e-06f };
	static const float zero = 0.0f;
	static const float one = 1.0f;
	static const float two = 2.0f;
	static const float two24 = 16777216.0f;
	static const float l1 = 6.0000002384e-01f;
	static const float l2 = 4.2857143283e-01f;
	static const float l3 = 3.3333334327e-01f;
	static const float l4 = 2.7272811532e-01f;
	static const float l5 = 2.3066075146e-01f;
	static const float l6 = 2.0697501302e-01f;
	static const float p1 = 1.6666667163e-01f;
	static const float p2 = -2.7777778450e-03f;
	static const float p3 = 6.6137559770e-05f;
	static const float p4 = -1.6533901999e-06f;
	static const float p5 = 4.1381369442e-08f;
	static const float lg2 = 6.9314718246e-01f;
	static const float lg2H = 6.93145752e-01f;
	static const float lg2L = 1.42860654e-06f;
	static const float ovt = 4.2995665694e-08f;
	static const float cp = 9.6179670095e-01f;
	static const float cpH = 9.6179199219e-01f;
	static const float cpL = 4.7017383622e-06f;
	static const float ivln2 = 1.4426950216e+00f;
	static const float ivln2H = 1.4426879883e+00f;
	static const float ivln2L = 7.0526075433e-06f;

	int32_t hx = getFloatWord(x);
	int32_t hy = getFloatWord(y);
	int32_t ix = hx & 0x7FFFFFFF;
	int32_t iy = hy & 0x7FFFFFFF;

	if (iy == 0)
		return one;
	if (ix > 0x7F800000 || iy > 0x7F800000)
		return x + y;

	int32_t yisint = 0;
	if (hx < 0) {
		if (iy >= 0x4B800000) {
			yisint = 2;
		} else if (iy >= 0x3F800000) {
			int32_t k = (iy >> 23) - 0x7F;
			int32_t j = iy >> (23 - k);
			if ((j << (23 - k)) == iy)
				yisint = 2 - (j & 1);
		}
	}

	if (iy == 0x7F800000) {
		if (ix == 0x3F800000)
			return y - y;
		if (ix > 0x3F800000)
			return (hy >= 0) ? y : zero;
		return (hy < 0) ? -y : zero;
	}
	if (iy == 0x3F800000) {
		if (hy < 0)
			return one / x;
		return x;
	}
	if (hy == 0x40000000)
		return x * x;
	if (hy == 0x3F000000 && hx >= 0)
		return audioSqrtf(x);

	float ax = fabsVal(x);
	if (ix == 0x7F800000 || ix == 0 || ix == 0x3F800000) {
		float z = ax;
		if (hy < 0)
			z = one / z;
		if (hx < 0) {
			if (((ix - 0x3F800000) | yisint) == 0)
				z = (z - z) / (z - z);
			else if (yisint == 1)
				z = -z;
		}
		return z;
	}

	float s = one;
	if (hx < 0) {
		if (yisint == 0)
			return (x - x) / (x - x);
		if (yisint == 1)
			s = -one;
	}

	if (iy > 0x4D000000) {
		if (iy > 0x4F000000) {
			if (ix <= 0x3F7FFFFF)
				return (hy < 0) ? kHuge * kHuge : kTiny * kTiny;
			if (ix >= 0x3F800000)
				return (hy > 0) ? kHuge * kHuge : kTiny * kTiny;
		}
		if (ix < 0x3F7FFFFF)
			return (hy < 0) ? s * kHuge * kHuge : s * kTiny * kTiny;
		if (ix > 0x3F800000)
			return (hy > 0) ? s * kHuge * kHuge : s * kTiny * kTiny;
		float t = ax - one;
		float w = (t * t) * (0.5f - t * (0.333333333333f - t * 0.25f));
		float u = ivln2H * t;
		float v = t * ivln2L - w * ivln2;
		float t1 = u + v;
		int32_t is = getFloatWord(t1);
		t1 = setFloatWord(is & (int32_t)0xFFFFF000U);
		float t2 = v - (t1 - u);
		float pH = t1 * y;
		float pL = t2 * y;
		float z = pL + pH;
		int32_t j = getFloatWord(z);
		if (j >= 0x43000000) {
			if (j != 0x43000000)
				return s * kHuge * kHuge;
			if (pL + ovt > z - pH)
				return s * kHuge * kHuge;
		} else if ((j & 0x7FFFFFFF) >= 0x43160000) {
			if ((uint32_t)j != 0xC3160000U)
				return s * kTiny * kTiny;
			if (pL <= z - pH)
				return s * kTiny * kTiny;
		}
		int32_t i = j & 0x7FFFFFFF;
		int32_t k = (i >> 23) - 0x7F;
		int32_t n = 0;
		if (i > 0x3F000000) {
			n = j + (0x00800000 >> (k + 1));
			k = ((n & 0x7FFFFFFF) >> 23) - 0x7F;
			t = setFloatWord(n & ~(0x007FFFFF >> k));
			n = ((n & 0x007FFFFF) | 0x00800000) >> (23 - k);
			if (j < 0)
				n = -n;
			pH -= t;
		}
		t = pL + pH;
		is = getFloatWord(t);
		t = setFloatWord(is & (int32_t)0xFFFF8000U);
		u = t * lg2H;
		v = (pL - (t - pH)) * lg2 + t * lg2L;
		z = u + v;
		w = v - (z - u);
		t = z * z;
		t1 = z - t * (p1 + t * (p2 + t * (p3 + t * (p4 + t * p5))));
		float r = (z * t1) / (t1 - two) - (w + z * w);
		z = one - (r - z);
		j = getFloatWord(z);
		j += (n << 23);
		if ((j >> 23) <= 0)
			z = audioScalbnf(z, n);
		else
			z = setFloatWord(j);
		return s * z;
	}

	float t1, t2;
	int32_t n = 0;
	if (ix < 0x00800000) {
		ax *= two24;
		n -= 24;
		ix = getFloatWord(ax);
	}
	n += ((ix) >> 23) - 0x7F;
	int32_t j = ix & 0x007FFFFF;
	ix = j | 0x3F800000;
	int32_t k;
	if (j <= 0x1CC471) {
		k = 0;
	} else if (j < 0x5DB3D7) {
		k = 1;
	} else {
		k = 0;
		n += 1;
		ix -= 0x00800000;
	}
	ax = setFloatWord(ix);
	float u = ax - bp[k];
	float v = one / (ax + bp[k]);
	float ss = u * v;
	float s2 = ss * ss;

	float sH = setFloatWord(getFloatWord(ss) & (int32_t)0xFFFFF000U);
	float tH = setFloatWord(((ix >> 1) | 0x20000000) + 0x00400000 + (k << 21));
	float tL = ax - (tH - bp[k]);
	float sL = v * ((u - sH * tH) - sH * tL);
	float r = s2 * s2 * (l1 + s2 * (l2 + s2 * (l3 + s2 * (l4 + s2 * (l5 + s2 * l6)))));
	r += sL * (sH + ss);
	s2 = sH * sH;
	tH = 3.0f + s2 + r;
	tH = setFloatWord(getFloatWord(tH) & (int32_t)0xFFFFF000U);
	tL = r - ((tH - 3.0f) - s2);
	u = sH * tH;
	v = sL * tH + tL * ss;
	float pH = u + v;
	pH = setFloatWord(getFloatWord(pH) & (int32_t)0xFFFFF000U);
	float pL = v - (pH - u);
	float zH = cpH * pH;
	float zL = cpL * pH + pL * cp + dpL[k];
	float t = (float)n;
	t1 = (((zH + zL) + dpH[k]) + t);
	t1 = setFloatWord(getFloatWord(t1) & (int32_t)0xFFFFF000U);
	t2 = zL - (((t1 - t) - dpH[k]) - zH);

	float y1 = setFloatWord(hy & (int32_t)0xFFFFF000U);
	pL = (y - y1) * t1 + y * t2;
	pH = y1 * t1;
	float z = pL + pH;
	j = getFloatWord(z);
	if (j >= 0x43000000) {
		if (j != 0x43000000)
			return s * kHuge * kHuge;
		if (pL + ovt > z - pH)
			return s * kHuge * kHuge;
	} else if ((j & 0x7FFFFFFF) >= 0x43160000) {
		if ((uint32_t)j != 0xC3160000U)
			return s * kTiny * kTiny;
		if (pL <= z - pH)
			return s * kTiny * kTiny;
	}

	int32_t i = j & 0x7FFFFFFF;
	k = (i >> 23) - 0x7F;
	n = 0;
	if (i > 0x3F000000) {
		n = j + (0x00800000 >> (k + 1));
		k = ((n & 0x7FFFFFFF) >> 23) - 0x7F;
		t = setFloatWord(n & ~(0x007FFFFF >> k));
		n = ((n & 0x007FFFFF) | 0x00800000) >> (23 - k);
		if (j < 0)
			n = -n;
		pH -= t;
	}
	t = pL + pH;
	t = setFloatWord(getFloatWord(t) & (int32_t)0xFFFF8000U);
	u = t * lg2H;
	v = (pL - (t - pH)) * lg2 + t * lg2L;
	z = u + v;
	float w = v - (z - u);
	t = z * z;
	t1 = z - t * (p1 + t * (p2 + t * (p3 + t * (p4 + t * p5))));
	r = (z * t1) / (t1 - two) - (w + z * w);
	z = one - (r - z);
	j = getFloatWord(z);
	j += (n << 23);
	if ((j >> 23) <= 0)
		z = audioScalbnf(z, n);
	else
		z = setFloatWord(j);
	return s * z;
}

float audioKernelCosf(float x, float y)
{
	static const float one = 1.0000000000e+00f;
	static const float c1 = 4.1666667908e-02f;
	static const float c2 = -1.3888889225e-03f;
	static const float c3 = 2.4801587642e-05f;
	static const float c4 = -2.7557314297e-07f;
	static const float c5 = 2.0875723372e-09f;
	static const float c6 = -1.1359647598e-11f;

	int32_t ix = getFloatWord(x) & 0x7FFFFFFF;
	if (ix < 0x32000000) {
		if ((int)x == 0)
			return one;
	}
	float z = x * x;
	float r = z * (c1 + z * (c2 + z * (c3 + z * (c4 + z * (c5 + z * c6)))));
	if (ix < 0x3E99999A) {
		return one - (0.5f * z - (z * r - x * y));
	}
	float qx;
	if (ix > 0x3F480000)
		qx = 0.28125f;
	else
		qx = setFloatWord(ix - 0x01000000);
	float hz = 0.5f * z - qx;
	float a = one - qx;
	return a - (hz - (z * r - x * y));
}

float audioKernelSinf(float x, float y, int iy)
{
	static const float half = 5.0000000000e-01f;
	static const float s1 = -1.6666667163e-01f;
	static const float s2 = 8.3333337680e-03f;
	static const float s3 = -1.9841270114e-04f;
	static const float s4 = 2.7557314297e-06f;
	static const float s5 = -2.5050759689e-08f;
	static const float s6 = 1.5896910177e-10f;

	int32_t ix = getFloatWord(x) & 0x7FFFFFFF;
	if (ix < 0x32000000) {
		if ((int)x == 0)
			return x;
	}
	float z = x * x;
	float v = z * x;
	float r = s2 + z * (s3 + z * (s4 + z * (s5 + z * s6)));
	if (iy == 0)
		return x + v * (s1 + z * r);
	return x - ((z * (half * y - v * r) - y) - v * s1);
}

static const int32_t kInitJk[3] = { 4, 7, 9 };
static const float kPIo2[8] = {
	1.5703125000e+00f, 4.5776367188e-04f, 2.5987625122e-05f, 7.5437128544e-08f,
	6.0026650317e-11f, 7.3896444519e-13f, 5.3845816694e-15f, 5.6378512969e-18f,
};

int32_t audioKernelRemPio2f(float *x, float *y, int32_t e0, int32_t nx, int32_t prec,
			    const int32_t *ipio2)
{
	static const float zero = 0.0f;
	static const float one = 1.0f;
	static const float two8 = 2.5600000000e+02f;
	static const float twon8 = 3.9062500000e-03f;

	int32_t jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
	float z, fw, f[20], fq[20], q[20];

	jk = kInitJk[prec];
	jp = jk;
	jx = nx - 1;
	jv = (e0 - 3) / 8;
	if (jv < 0)
		jv = 0;
	q0 = e0 - 8 * (jv + 1);
	j = jv - jx;
	m = jx + jk;
	for (i = 0; i <= m; ++i, ++j)
		f[i] = (j < 0) ? zero : (float)ipio2[j];

	for (i = 0; i <= jk; ++i) {
		for (j = 0, fw = 0.0f; j <= jx; ++j)
			fw += x[j] * f[jx + i - j];
		q[i] = fw;
	}

	jz = jk;
	for (;;) {
		for (i = 0, j = jz, z = q[jz]; j > 0; ++i, --j) {
			fw = (float)((int32_t)(twon8 * z));
			iq[i] = (int32_t)(z - two8 * fw);
			z = q[j - 1] + fw;
		}
		z = audioScalbnf(z, (int)q0);
		z -= 8.0f * audioFloorf(z * 0.125f);
		n = (int32_t)z;
		z -= (float)n;
		ih = 0;
		if (q0 > 0) {
			i = (iq[jz - 1] >> (8 - q0));
			n += i;
			iq[jz - 1] -= i << (8 - q0);
			ih = iq[jz - 1] >> (7 - q0);
		} else if (q0 == 0) {
			ih = iq[jz - 1] >> 7;
		} else if (z >= 0.5f) {
			ih = 2;
		}

		if (ih > 0) {
			n += 1;
			carry = 0;
			for (i = 0; i < jz; ++i) {
				j = iq[i];
				if (carry == 0) {
					if (j != 0) {
						carry = 1;
						iq[i] = 0x100 - j;
					}
				} else {
					iq[i] = 0xFF - j;
				}
			}
			if (q0 > 0) {
				if (q0 == 1)
					iq[jz - 1] &= 0x7F;
				else if (q0 == 2)
					iq[jz - 1] &= 0x3F;
			}
			if (ih == 2) {
				z = one - z;
				if (carry != 0)
					z -= audioScalbnf(one, (int)q0);
			}
		}

		if (z == zero) {
			j = 0;
			for (i = jz - 1; i >= jk; --i)
				j |= iq[i];
			if (j == 0) {
				for (k = 1; iq[jk - k] == 0; ++k)
					;
				for (i = jz + 1; i <= jz + k; ++i) {
					f[jx + i] = (float)ipio2[jv + i];
					for (j = 0, fw = 0.0f; j <= jx; ++j)
						fw += x[j] * f[jx + i - j];
					q[i] = fw;
				}
				jz += k;
				continue;
			}
		}
		break;
	}

	if (z == 0.0f) {
		jz -= 1;
		q0 -= 8;
		while (iq[jz] == 0) {
			--jz;
			q0 -= 8;
		}
	} else {
		z = audioScalbnf(z, (int)(-q0));
		if (z >= two8) {
			fw = (float)((int32_t)(twon8 * z));
			iq[jz] = (int32_t)(z - two8 * fw);
			jz += 1;
			q0 += 8;
			iq[jz] = (int32_t)fw;
		} else {
			iq[jz] = (int32_t)z;
		}
	}

	fw = audioScalbnf(one, (int)q0);
	for (i = jz; i >= 0; --i) {
		q[i] = fw * (float)iq[i];
		fw *= twon8;
	}

	for (i = jz; i >= 0; --i) {
		for (fw = 0.0f, k = 0; k <= jp && k <= jz - i; ++k)
			fw += kPIo2[k] * q[i + k];
		fq[jz - i] = fw;
	}

	switch (prec) {
	case 0:
		fw = 0.0f;
		for (i = jz; i >= 0; --i)
			fw += fq[i];
		y[0] = (ih == 0) ? fw : -fw;
		break;
	case 1:
	case 2:
		fw = 0.0f;
		for (i = jz; i >= 0; --i)
			fw += fq[i];
		y[0] = (ih == 0) ? fw : -fw;
		fw = fq[0] - fw;
		for (i = 1; i <= jz; ++i)
			fw += fq[i];
		y[1] = (ih == 0) ? fw : -fw;
		break;
	default:
		for (i = jz; i > 0; --i) {
			fw = fq[i - 1] + fq[i];
			fq[i] += fq[i - 1] - fw;
			fq[i - 1] = fw;
		}
		for (i = jz; i > 1; --i) {
			fw = fq[i - 1] + fq[i];
			fq[i] += fq[i - 1] - fw;
			fq[i - 1] = fw;
		}
		for (fw = 0.0f, i = jz; i >= 2; --i)
			fw += fq[i];
		if (ih == 0) {
			y[0] = fq[0];
			y[1] = fq[1];
			y[2] = fw;
		} else {
			y[0] = -fq[0];
			y[1] = -fq[1];
			y[2] = -fw;
		}
		break;
	}
	return n & 7;
}

static const int32_t kNpio2Hw[32] = {
	0x3FC90F00, 0x40490F00, 0x4096CB00, 0x40C90F00, 0x40FB5300, 0x4116CB00,
	0x412FED00, 0x41490F00, 0x41623100, 0x417B5300, 0x418A3A00, 0x4196CB00,
	0x41A35C00, 0x41AFED00, 0x41BC7E00, 0x41C90F00, 0x41D5A000, 0x41E23100,
	0x41EEC200, 0x41FB5300, 0x4203F200, 0x420A3A00, 0x42108300, 0x4216CB00,
	0x421D1400, 0x42235C00, 0x4229A500, 0x422FED00, 0x42363600, 0x423C7E00,
	0x4242C700, 0x42490F00,
};

static const int32_t kTwoOverPi[198] = {
	0xA2, 0xF9, 0x83, 0x6E, 0x4E, 0x44, 0x15, 0x29, 0xFC, 0x27, 0x57, 0xD1,
	0xF5, 0x34, 0xDD, 0xC0, 0xDB, 0x62, 0x95, 0x99, 0x3C, 0x43, 0x90, 0x41,
	0xFE, 0x51, 0x63, 0xAB, 0xDE, 0xBB, 0xC5, 0x61, 0xB7, 0x24, 0x6E, 0x3A,
	0x42, 0x4D, 0xD2, 0xE0, 0x06, 0x49, 0x2E, 0xEA, 0x09, 0xD1, 0x92, 0x1C,
	0xFE, 0x1D, 0xEB, 0x1C, 0xB1, 0x29, 0xA7, 0x3E, 0xE8, 0x82, 0x35, 0xF5,
	0x2E, 0xBB, 0x44, 0x84, 0xE9, 0x9C, 0x70, 0x26, 0xB4, 0x5F, 0x7E, 0x41,
	0x39, 0x91, 0xD6, 0x39, 0x83, 0x53, 0x39, 0xF4, 0x9C, 0x84, 0x5F, 0x8B,
	0xBD, 0xF9, 0x28, 0x3B, 0x1F, 0xF8, 0x97, 0xFF, 0xDE, 0x05, 0x98, 0x0F,
	0xEF, 0x2F, 0x11, 0x8B, 0x5A, 0x0A, 0x6D, 0x1F, 0x6D, 0x36, 0x7E, 0xCF,
	0x27, 0xCB, 0x09, 0xB7, 0x4F, 0x46, 0x3F, 0x66, 0x9E, 0x5F, 0xEA, 0x2D,
	0x75, 0x27, 0xBA, 0xC7, 0xEB, 0xE5, 0xF1, 0x7B, 0x3D, 0x07, 0x39, 0xF7,
	0x8A, 0x52, 0x92, 0xEA, 0x6B, 0xFB, 0x5F, 0xB1, 0x1F, 0x8D, 0x5D, 0x08,
	0x56, 0x03, 0x30, 0x46, 0xFC, 0x7B, 0x6B, 0xAB, 0xF0, 0xCF, 0xBC, 0x20,
	0x9A, 0xF4, 0x36, 0x1D, 0xA9, 0xE3, 0x91, 0x61, 0x5E, 0xE6, 0x1B, 0x08,
	0x65, 0x99, 0x85, 0x5F, 0x14, 0xA0, 0x68, 0x40, 0x8D, 0xFF, 0xD8, 0x80,
	0x4D, 0x73, 0x27, 0x31, 0x06, 0x06, 0x15, 0x56, 0xCA, 0x73, 0xA8, 0xC9,
	0x60, 0xE2, 0x7B, 0xC0, 0x8C, 0x6B,
};

int32_t audioRemPio2f(float x, float *y)
{
	static const float invpio2 = 6.3661980629e-01f;
	static const float pio2_1 = 1.5707855225e+00f;
	static const float pio2_1t = 1.0804334124e-05f;
	static const float pio2_2 = 1.0804273188e-05f;
	static const float pio2_2t = 6.0770999344e-11f;
	static const float pio2_3 = 6.0770943833e-11f;
	static const float pio2_3t = 5.5511151231e-17f;

	int32_t hx = getFloatWord(x);
	int32_t ix = hx & 0x7FFFFFFF;
	if (ix <= 0x3F490FD8) {
		y[0] = x;
		y[1] = 0.0f;
		return 0;
	}
	if (ix < 0x4016CBE4) {
		if (hx > 0) {
			float z = x - pio2_1;
			if ((ix & 0xFFFFFFF0) != 0x3FC90FD0) {
				y[0] = z - pio2_1t;
				y[1] = (z - y[0]) - pio2_1t;
			} else {
				z -= pio2_2;
				y[0] = z - pio2_2t;
				y[1] = (z - y[0]) - pio2_2t;
			}
			return 1;
		}
		float z = x + pio2_1;
		if ((ix & 0xFFFFFFF0) != 0x3FC90FD0) {
			y[0] = z + pio2_1t;
			y[1] = (z - y[0]) + pio2_1t;
		} else {
			z += pio2_2;
			y[0] = z + pio2_2t;
			y[1] = (z - y[0]) + pio2_2t;
		}
		return -1;
	}
	if (ix <= 0x43490F80) {
		float t = fabsVal(x);
		int32_t n = (int32_t)(t * invpio2 + 0.5f);
		float fn = (float)n;
		float r = t - fn * pio2_1;
		float w = fn * pio2_1t;
		if (n < 32 && ix != kNpio2Hw[n - 1]) {
			y[0] = r - w;
		} else {
			y[0] = r - w;
			int32_t j = ix >> 23;
			int32_t i = j - ((getFloatWord(y[0]) >> 23) & 0xFF);
			if (i > 8) {
				t = r;
				w = fn * pio2_2;
				r = t - w;
				w = fn * pio2_2t - ((t - r) - w);
				y[0] = r - w;
				i = j - ((getFloatWord(y[0]) >> 23) & 0xFF);
				if (i > 25) {
					t = r;
					w = fn * pio2_3;
					r = t - w;
					w = fn * pio2_3t - ((t - r) - w);
					y[0] = r - w;
				}
			}
		}
		y[1] = (r - y[0]) - w;
		if (hx < 0) {
			y[0] = -y[0];
			y[1] = -y[1];
			return -n;
		}
		return n;
	}
	if (ix >= 0x7F800000) {
		y[0] = y[1] = x - x;
		return 0;
	}
	int32_t e0 = (ix >> 23) - 134;
	float z = setFloatWord(ix - (e0 << 23));
	float tx[3];
	for (int32_t i = 0; i < 2; ++i) {
		tx[i] = (float)((int32_t)z);
		z = (z - tx[i]) * 256.0f;
	}
	tx[2] = z;
	int32_t nx = 3;
	while (tx[nx - 1] == 0.0f)
		--nx;
	int32_t n = audioKernelRemPio2f(tx, y, e0, nx, 2, kTwoOverPi);
	if (hx < 0) {
		y[0] = -y[0];
		y[1] = -y[1];
		return -n;
	}
	return n;
}

float audioSinf(float x)
{
	float y[2];
	int32_t ix = getFloatWord(x) & 0x7FFFFFFF;
	if (ix <= 0x3F490FD8)
		return audioKernelSinf(x, 0.0f, 0);
	if (ix >= 0x7F800000)
		return x - x;
	int32_t n = audioRemPio2f(x, y);
	switch (n & 3) {
	case 0:
		return audioKernelSinf(y[0], y[1], 1);
	case 1:
		return audioKernelCosf(y[0], y[1]);
	case 2:
		return -audioKernelSinf(y[0], y[1], 1);
	default:
		return -audioKernelCosf(y[0], y[1]);
	}
}
