/*
 * erfcf function: the complementary error function in float. Below 0.4769
 * it is 1 - erff(x); up to 1.523 a ratio of polynomials in x - 0.75 or
 * x - 1.28125; beyond, up to the underflow, a ratio of polynomials in 1/x
 * times exp(-x^2), with x^2 split by _FDint so that no bits of it are lost.
 */
#include <math.h>

#include "xmath.h"

static const float x0 = 0.75F;	/* the origins of the polynomials in x below 1.523 */
static const float x1 = 1.0F;	/* where the second takes over */
static const float x2 = 1.28125F;

static const float numa[] = {
	0.020652281F, -0.15091428F, 0.42620614F, -0.55738676F, 0.2855036F,
};
static const float dena[] = {
	0.052445814F, 0.06903691F, 0.42737156F, 0.2704161F, 0.988434F,
};
static const float numb[] = {
	0.017105736F, -0.0839632F, 0.17227347F, -0.17205718F, 0.06967652F,
};
static const float denb[] = {
	0.04467895F, 0.15276381F, 0.50822675F, 0.64992857F, 0.9954897F,
};
static const float num1[] = {
	-0.022398684F, 0.04179726F, -0.04815653F, 0.028948374F, 0.030646054F, -0.14707626F,
	0.40698788F, 0.30173224F,
};
static const float den1[] = {
	0.069268085F, 1.0F,
};
static const float num2[] = {
	-0.0084462855F, 0.03128812F, -0.07400039F, 0.14095789F, 0.7004227F, 0.8193149F,
	0.26279184F,
};
static const float den2[] = {
	0.935345F, 1.5179788F, 0.9986785F,
};
static const float num3[] = {
	0.19987153F, -0.27972746F, 0.18733451F, 0.0889886F, -0.5291699F, 1.08968F, 1.0596254F,
	0.2159547F,
};
static const float den3[] = {
	2.774346F, 1.0F,
};
static const float num4[] = {
	0.17821649F, -0.21015696F, 0.105550304F, 0.08748043F, -0.3116615F, 0.51809525F,
	0.7383285F, 0.17314993F,
};
static const float den4[] = {
	1.4131174F, 1.0F,
};
static const float num5[] = {
	0.25174305F, -0.23277406F, 0.060718246F, 0.12543769F, -0.23503435F, 0.23909761F,
	0.6255418F, 0.14035712F,
};
static const float den5[] = {
	0.7802651F, 1.0F,
};
static const float num6[] = {
	0.43460816F, -0.30091992F, -0.027489357F, 0.22247604F, -0.15637404F, -0.12651052F,
	0.53530633F, 0.11402114F,
};
static const float den6[] = {
	0.033005707F, 1.0F,
};
static const float num7[] = {
	-1.9548408F, 5.830823F, 7.140281F, 7.473683F, 1.564537F, 0.08596663F,
};
static const float den7[] = {
	17.4414F, 13.239546F, 11.826776F, 0.9940051F,
};
static const float num8[] = {
	0.2185443F, -0.20824075F, -0.2869901F, 1.1248646F, 0.65499604F, 0.61943614F,
	0.062726445F,
};
static const float den8[] = {
	2.6221535F, 1.0463544F, 0.99977475F,
};

static const struct {	/* the ranges of 1/x above 1.523, each with its approximation */
	float xmax;	/* the range ends below xmax */
	float xoff;	/* the middle of 1/x in the range */
	const float *num;
	int nnum;	/* the numerator's degree */
	const float *den;
	int nden;	/* the denominator's */
} range[] = {
	{1.75F, 0.6140137F, num1, 7, den1, 1},
	{2.15F, 0.5182724F, num2, 6, den2, 2},
	{2.8F, 0.41112956F, num3, 7, den3, 1},
	{3.5F, 0.32142857F, num4, 7, den4, 1},
	{4.4F, 0.25649348F, num5, 7, den5, 1},
	{5.4F, 0.20622894F, num6, 7, den6, 1},
	{8.0F, 0.15509257F, num7, 5, den7, 3},
	{10.123F, 0.11189246F, num8, 6, den8, 2},
};

static const float xsmall = 0.47693628F;	/* erfc(x) is 1 - erf(x) below */

float (erfcf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case 0:
		return 1.0F;
	case _INFCODE:
		return FSIGN(x) ? 2.0F : 0.0F;
	default:
		if (x < xsmall) {
			return 1.0F - erff(x);
		} else if (x < 1.523F) {
			float y;

			if (x < x1) {
				y = x - x0;
				return _FPoly(y, numa, 4) / _FPoly(y, dena, 4);
			} else {
				y = x - x2;
				return _FPoly(y, numb, 4) / _FPoly(y, denb, 4);
			}
		} else {
			int i;

			for (i = 0; i < sizeof(range) / sizeof(range[0]); ++i) {
				if (x < range[i].xmax) {
					float y = 1.0F / x - range[i].xoff;
					float z;
					float t;

					z = _FPoly(y, range[i].num, range[i].nnum)
					    / _FPoly(y, range[i].den, range[i].nden);
					t = x;
					_FDint(&t, 8);	/* x^2 = t^2 + (x - t)(x + t) */
					z *= expf((t - x) * (x + t));
					t = -t * t;
					_FExp(&t, z, 0);
					return t;
				}
			}
			_Feraise(_FE_UNDERFLOW);
			return 0.0F;
		}
	}
}
