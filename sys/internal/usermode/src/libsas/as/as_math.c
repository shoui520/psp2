/*
 * Single-precision math for the AS runtime: the arc functions by a continued fraction of
 * the arc tangent, square roots by Newton's method, sine and cosine by their Taylor series
 * after reducing the angle to [-pi, pi].
 */

#include "as_math.h"

/* 10^(k / 2560): the gain of k / 128 dB */
const float sceSasAsDbFractionTable[128] = {
	1.0f, 1.00089991f, 1.00180054f, 1.002702f,
	1.00360429f, 1.00450742f, 1.00541127f, 1.00631595f,
	1.00722158f, 1.00812793f, 1.00903499f, 1.00994301f,
	1.01085186f, 1.01176143f, 1.01267183f, 1.01358318f,
	1.01449525f, 1.01540816f, 1.01632178f, 1.01723635f,
	1.01815176f, 1.01906788f, 1.01998496f, 1.02090275f,
	1.02182138f, 1.02274096f, 1.02366126f, 1.02458239f,
	1.02550435f, 1.02642715f, 1.02735078f, 1.02827525f,
	1.02920055f, 1.03012669f, 1.03105366f, 1.03198147f,
	1.03290999f, 1.03383946f, 1.03476977f, 1.03570092f,
	1.0366329f, 1.03756571f, 1.03849936f, 1.03943384f,
	1.04036927f, 1.04130542f, 1.04224241f, 1.04318035f,
	1.044119f, 1.04505861f, 1.04599893f, 1.04694021f,
	1.04788232f, 1.04882526f, 1.04976904f, 1.05071366f,
	1.05165911f, 1.05260551f, 1.05355263f, 1.0545007f,
	1.0554496f, 1.05639935f, 1.05734992f, 1.05830145f,
	1.05925369f, 1.06020689f, 1.06116092f, 1.06211579f,
	1.06307161f, 1.06402814f, 1.06498563f, 1.06594396f,
	1.06690311f, 1.06786323f, 1.06882417f, 1.06978595f,
	1.07074857f, 1.07171214f, 1.07267642f, 1.07364166f,
	1.07460785f, 1.07557487f, 1.07654262f, 1.07751143f,
	1.07848096f, 1.07945144f, 1.08042288f, 1.08139503f,
	1.08236814f, 1.08334208f, 1.08431697f, 1.0852927f,
	1.08626926f, 1.08724678f, 1.08822513f, 1.08920431f,
	1.09018445f, 1.09116554f, 1.09214735f, 1.09313011f,
	1.09411383f, 1.09509838f, 1.09608376f, 1.0970701f,
	1.09805727f, 1.0990454f, 1.10003436f, 1.10102427f,
	1.10201502f, 1.1030066f, 1.10399914f, 1.10499263f,
	1.10598695f, 1.10698211f, 1.10797834f, 1.10897529f,
	1.10997319f, 1.11097205f, 1.11197174f, 1.11297238f,
	1.11397386f, 1.11497629f, 1.11597955f, 1.11698377f,
	1.11798894f, 1.11899495f, 1.12000191f, 1.12100971f,
};

/* 10^((k - 144) / 20): the gain of k - 144 dB */
const float sceSasAsDbTable[289] = {
	6.30960031e-08f, 7.07949965e-08f, 7.94330006e-08f, 8.91250025e-08f,
	1.00000001e-07f, 1.12202002e-07f, 1.25892996e-07f, 1.41254006e-07f,
	1.58489001e-07f, 1.77827999e-07f, 1.99525999e-07f, 2.23871993e-07f,
	2.51188993e-07f, 2.81837998e-07f, 3.16228011e-07f, 3.54813011e-07f,
	3.98107005e-07f, 4.46683998e-07f, 5.01187003e-07f, 5.62340972e-07f,
	6.30957004e-07f, 7.07946015e-07f, 7.94328002e-07f, 8.91250977e-07f,
	9.99999997e-07f, 1.12201803e-06f, 1.25892495e-06f, 1.41253804e-06f,
	1.58489297e-06f, 1.77827906e-06f, 1.99526198e-06f, 2.23872098e-06f,
	2.5118859e-06f, 2.81838311e-06f, 3.16227806e-06f, 3.54813392e-06f,
	3.98107204e-06f, 4.46683589e-06f, 5.01187196e-06f, 5.62341302e-06f,
	6.30957311e-06f, 7.0794581e-06f, 7.94328207e-06f, 8.91250875e-06f,
	9.99999975e-06f, 1.12201851e-05f, 1.25892539e-05f, 1.41253749e-05f,
	1.58489329e-05f, 1.77827933e-05f, 1.99526221e-05f, 2.23872103e-05f,
	2.51188649e-05f, 2.81838293e-05f, 3.16227779e-05f, 3.54813383e-05f,
	3.98107186e-05f, 4.46683589e-05f, 5.01187242e-05f, 5.62341338e-05f,
	6.30957366e-05f, 7.07945801e-05f, 7.94328225e-05f, 8.91250966e-05f,
	9.99999975e-05f, 0.000112201844f, 0.000125892548f, 0.000141253753f,
	0.000158489318f, 0.00017782794f, 0.000199526228f, 0.000223872121f,
	0.000251188641f, 0.000281838293f, 0.000316227757f, 0.000354813383f,
	0.000398107164f, 0.000446683582f, 0.000501187227f, 0.000562341302f,
	0.000630957366f, 0.000707945786f, 0.000794328225f, 0.000891250966f,
	0.00100000005f, 0.00112201844f, 0.00125892542f, 0.00141253753f,
	0.00158489321f, 0.00177827943f, 0.00199526222f, 0.00223872112f,
	0.00251188641f, 0.00281838304f, 0.00316227763f, 0.00354813389f,
	0.00398107152f, 0.00446683588f, 0.00501187239f, 0.00562341325f,
	0.00630957354f, 0.00707945786f, 0.0079432819f, 0.00891250931f,
	0.00999999978f, 0.0112201842f, 0.0125892544f, 0.0141253751f,
	0.0158489328f, 0.0177827943f, 0.0199526232f, 0.0223872121f,
	0.0251188651f, 0.028183829f, 0.0316227749f, 0.0354813375f,
	0.0398107171f, 0.0446683578f, 0.050118722f, 0.0562341325f,
	0.0630957335f, 0.0707945749f, 0.0794328228f, 0.0891250968f,
	0.100000001f, 0.112201847f, 0.125892535f, 0.141253754f,
	0.158489317f, 0.177827939f, 0.199526235f, 0.22387211f,
	0.251188636f, 0.281838298f, 0.316227764f, 0.354813397f,
	0.398107171f, 0.446683586f, 0.501187205f, 0.562341332f,
	0.630957365f, 0.707945764f, 0.794328213f, 0.891250908f,
	1.0f, 1.12201846f, 1.25892544f, 1.41253757f,
	1.58489323f, 1.77827942f, 1.99526227f, 2.23872113f,
	2.51188636f, 2.81838298f, 3.1622777f, 3.54813385f,
	3.98107171f, 4.46683598f, 5.01187229f, 5.62341309f,
	6.30957365f, 7.07945776f, 7.94328213f, 8.91250896f,
	10.0f, 11.2201843f, 12.5892544f, 14.1253757f,
	15.8489323f, 17.782795f, 19.9526234f, 22.3872108f,
	25.1188641f, 28.1838303f, 31.622776f, 35.4813385f,
	39.8107185f, 44.6683578f, 50.1187248f, 56.2341309f,
	63.0957336f, 70.7945786f, 79.4328232f, 89.1250916f,
	100.0f, 112.201843f, 125.89254f, 141.253754f,
	158.489319f, 177.827942f, 199.52623f, 223.872116f,
	251.188644f, 281.838287f, 316.227753f, 354.813385f,
	398.107178f, 446.683594f, 501.187225f, 562.341309f,
	630.957336f, 707.945801f, 794.328247f, 891.250916f,
	1000.0f, 1122.01843f, 1258.92542f, 1412.5376f,
	1584.89319f, 1778.27942f, 1995.26233f, 2238.72119f,
	2511.88647f, 2818.38281f, 3162.27759f, 3548.13379f,
	3981.07178f, 4466.83594f, 5011.87256f, 5623.41309f,
	6309.57324f, 7079.45801f, 7943.28223f, 8912.50977f,
	10000.0f, 11220.1846f, 12589.2539f, 14125.375f,
	15848.9316f, 17782.7949f, 19952.623f, 22387.2109f,
	25118.8652f, 28183.8301f, 31622.7773f, 35481.3398f,
	39810.7188f, 44668.3594f, 50118.7227f, 56234.1328f,
	63095.7344f, 70794.5781f, 79432.8203f, 89125.0938f,
	100000.0f, 112201.844f, 125892.539f, 141253.75f,
	158489.312f, 177827.938f, 199526.234f, 223872.109f,
	251188.641f, 281838.281f, 316227.781f, 354813.375f,
	398107.156f, 446683.594f, 501187.219f, 562341.312f,
	630957.375f, 707945.812f, 794328.25f, 891250.938f,
	1000000.0f, 1122018.5f, 1258925.38f, 1412537.5f,
	1584893.25f, 1778279.38f, 1995262.38f, 2238721.25f,
	2511886.5f, 2818383.0f, 3162277.75f, 3548134.0f,
	3981071.75f, 4466836.0f, 5011872.5f, 5623413.5f,
	6309573.5f, 7079458.0f, 7943282.5f, 8912509.0f,
	10000000.0f, 11220185.0f, 12589254.0f, 14125375.0f,
	15848932.0f,
};

static inline float sceSasAsFabsfInline(float x)
{
	union {
		float value;
		SceUInt32 bits;
	} u;

	u.value = x;
	u.bits &= ~0x80000000u;
	return u.value;
}

float sceSasAsFabsf(float x)
{
	return sceSasAsFabsfInline(x);
}

/* the arc tangent of |x| <= 1: ten terms of its continued fraction */
static inline float sceSasAsAtanfFraction(float x)
{
	float x2 = x * x;

	return x / (1.0f + x2 / (3.0f + x2 * 4.0f / (5.0f + x2 * 9.0f / (7.0f + x2 * 16.0f /
		(9.0f + x2 * 25.0f / (11.0f + x2 * 36.0f / (13.0f + x2 * 49.0f / (15.0f +
		x2 * 64.0f / (17.0f + x2 * 81.0f / (19.0f + x2 * 100.0f / 21.0f))))))))));
}

float sceSasAsAtanf(float x)
{
	if (x > 1.0f)
		return SCE_SAS_AS_HALF_PI - sceSasAsAtanfFraction(1.0f / x);
	if (x < -1.0f)
		return -SCE_SAS_AS_HALF_PI - sceSasAsAtanfFraction(1.0f / x);
	return sceSasAsAtanfFraction(x);
}

/* the square root of x >= 0, from half its exponent, to a relative 1e-5 of that guess */
static inline float sceSasAsSqrtfInline(float x)
{
	union {
		float value;
		SceInt32 bits;
	} u;
	float root;
	float next;
	float error;
	float epsilon;

	if (x != 0.0f) {
		u.value = x;
		u.bits = ((u.bits - 0x3f800000) >> 1) + 0x3f800000;
		root = u.value;
		epsilon = root * 1e-05f;
		error = root;
		while (epsilon < sceSasAsFabsfInline(error)) {
			next = root - (root - x / root) * 0.5f;
			error = next - root;
			root = next;
		}
		return root;
	}
	return 0.0f;
}

float sceSasAsAsinf(float x)
{
	float y = 1.0f - x * x;

	if (y < 0.0f)
		return 0.0f;
	if (y == 0.0f)
		return x < 0.0f ? -SCE_SAS_AS_HALF_PI : SCE_SAS_AS_HALF_PI;
	return sceSasAsAtanf(x / sceSasAsSqrtfInline(y));
}

float sceSasAsAcosf(float x)
{
	float y = 1.0f - x * x;

	if (y < 0.0f)
		return 0.0f;
	if (y == 0.0f)
		return x < 0.0f ? SCE_SAS_AS_PI : 0.0f;
	return SCE_SAS_AS_HALF_PI - sceSasAsAtanf(x / sceSasAsSqrtfInline(y));
}

float sceSasAsAtan2f(float y, float x)
{
	if (x == 0.0f)
		return y < 0.0f ? -SCE_SAS_AS_HALF_PI : SCE_SAS_AS_HALF_PI;
	if (!(x < 0.0f))
		return sceSasAsAtanf(y / x);
	if (y < 0.0f)
		return sceSasAsAtanf(y / x) - SCE_SAS_AS_PI;
	return sceSasAsAtanf(y / x) + SCE_SAS_AS_PI;
}

/* x rounded down, where it fits an int */
static inline float sceSasAsFloorfInline(float x)
{
	float truncated;

	if (x < -2147483648.0f || x > 2147483648.0f)
		return x;
	truncated = (float)(SceInt32)x;
	if (x < 0.0f && x - truncated < 0.0f)
		return truncated - 1.0f;
	return truncated;
}

/* x in [-pi, pi] */
static inline float sceSasAsReduceAngle(float x)
{
	float turns = sceSasAsFloorfInline(x / SCE_SAS_AS_TWO_PI);

	x -= turns * 2.0f * SCE_SAS_AS_PI;
	if (x > SCE_SAS_AS_PI)
		x -= SCE_SAS_AS_TWO_PI;
	if (x < -SCE_SAS_AS_PI)
		x += SCE_SAS_AS_TWO_PI;
	return x;
}

/* the sine of |x| <= pi/2 to the 11th power */
static inline float sceSasAsSinfSeries(float x)
{
	float x2 = x * x;

	return x * (1.0f + x2 * (x2 * (0.00833333377f + x2 * (x2 * (2.75573188e-06f +
		x2 * -2.50521079e-08f) - 0.000198412701f)) - 0.166666672f));
}

float sceSasAsSinf(float x)
{
	x = sceSasAsReduceAngle(x);
	if (x == 0.0f)
		return 0.0f;
	if (x == SCE_SAS_AS_HALF_PI)
		return 1.0f;
	if (x == -SCE_SAS_AS_HALF_PI)
		return -1.0f;
	if (sceSasAsFabsfInline(x) < SCE_SAS_AS_HALF_PI)
		return sceSasAsSinfSeries(x);
	if (x < 0.0f)
		return sceSasAsSinfSeries(-SCE_SAS_AS_PI - x);
	return sceSasAsSinfSeries(SCE_SAS_AS_PI - x);
}

float sceSasAsCosf(float x)
{
	x = sceSasAsReduceAngle(x);
	if (x == 0.0f)
		return 1.0f;
	if (x == SCE_SAS_AS_HALF_PI || x == -SCE_SAS_AS_HALF_PI)
		return 0.0f;
	if (x < 0.0f)
		return sceSasAsSinfSeries(x + SCE_SAS_AS_HALF_PI);
	return sceSasAsSinfSeries(SCE_SAS_AS_HALF_PI - x);
}

float sceSasAsTanf(float x)
{
	SceInt32 quadrant = (SceInt32)(x / SCE_SAS_AS_HALF_PI + (x >= 0.0f ? 0.5f : -0.5f));
	float r = x - (float)quadrant * SCE_SAS_AS_HALF_PI;
	float r2 = r * r;
	float t = r / (1.0f - r2 / (3.0f - r2 / (5.0f - r2 / (7.0f - r2 / 9.0f))));

	if (quadrant & 1) {
		if (t == 0.0f)
			return 3.40282347e+38f;
		return -1.0f / t;
	}
	return t;
}

float sceSasAsSqrtf(float x)
{
	return sceSasAsSqrtfInline(x);
}

/*
 * The natural logarithm from the exponent (times ln 2 in 8.24 fixed point) and the series
 * of 2 atanh(z) for the mantissa taken to [sqrt(2) / 2, sqrt(2)]; -inf for 0, NaN below.
 */
static inline float sceSasAsLogfInline(float x)
{
	union {
		float value;
		SceInt32 bits;
	} u;
	SceInt32 mantissa;
	SceInt32 numerator;
	SceInt32 denominator;
	float z;
	float z2;

	u.value = x;
	if (u.bits <= 0) {
		if (u.bits & 0x7fffffff)
			return -__builtin_nanf("");
		return -__builtin_inff();
	}
	mantissa = u.bits & 0x7fffff;
	numerator = mantissa - 0x800000;
	denominator = mantissa;
	if (mantissa <= 0x3504f3) {
		numerator = mantissa;
		denominator = mantissa - 0x800000;
		u.bits -= 0x800000;
	}
	denominator += 0x1800000;
	z = (float)numerator / (float)denominator;
	z2 = z * z;
	return (float)(((u.bits >> 23) - 126) * 0xb17218) * (1.0f / 16777216.0f) +
		(2.00000048f + (0.666477323f + z2 * 0.413974732f) * z2) * z;
}

float sceSasAsLogf(float x)
{
	return sceSasAsLogfInline(x);
}

float sceSasAsLog10f(float x)
{
	return sceSasAsLogfInline(x) / 2.30258512f;
}

/* e^x for 0 <= x < 1 to the 14th power */
static inline float sceSasAsExpfSeries(float x)
{
	float x2 = x * x;
	float x3 = x * x2;
	float x4 = x * x3;
	float x5 = x * x4;
	float x6 = x * x5;
	float x7 = x * x6;
	float x8 = x * x7;
	float x9 = x * x8;
	float x10 = x * x9;
	float x11 = x * x10;
	float x12 = x * x11;
	float x13 = x * x12;
	float x14 = x * x13;

	return x + 1.0f + x2 * 0.5f + x3 * 0.166666672f + x4 * 0.0416666679f +
		x5 * 0.00833333377f + x6 * 0.00138888892f + x7 * 0.000198412701f +
		x8 * 2.48015876e-05f + x9 * 2.75573188e-06f + x10 * 2.755732e-07f +
		x11 * 2.50521079e-08f + x12 * 2.08767559e-09f + x13 * 1.60590444e-10f +
		x14 * 1.14707462e-11f;
}

float sceSasAsExpf(float x)
{
	SceInt32 whole;
	SceInt32 i;
	float scale;

	if (x < 0.0f)
		return 1.0f / sceSasAsExpfSeries(-x);
	if (x < 1.0f)
		return sceSasAsExpfSeries(x);
	whole = (SceInt32)x;
	scale = 1.0f;
	for (i = 1; i <= whole; i++)
		scale *= 2.71828175f;
	return sceSasAsExpfSeries(x - (float)whole) * scale;
}

float sceSasAsCeilf(float x)
{
	float truncated;

	if (x < -2147483648.0f || x > 2147483648.0f)
		return x;
	truncated = (float)(SceInt32)x;
	if (x >= 0.0f && x - truncated > 0.0f)
		return truncated + 1.0f;
	return truncated;
}

float sceSasAsFloorf(float x)
{
	return sceSasAsFloorfInline(x);
}

float sceSasAsPowf(float x, float y)
{
	SceInt32 whole = (SceInt32)y;
	SceInt32 i;
	float result;

	if (y < 0.0f) {
		if (x < 0.0f && y - (float)whole > 0.0f)
			return 0.0f;
		return 1.0f / sceSasAsPowf(x, -y);
	}
	if (y == 0.0f)
		return 1.0f;
	if (sceSasAsFabsfInline(y - (float)whole) <= 1e-06f) {
		result = 1.0f;
		for (i = 0; i < whole; i++)
			result *= x;
		return result;
	}
	if (x == 0.0f)
		return 0.0f;
	return sceSasAsExpf(y * sceSasAsLogfInline(x));
}

/* the gain of db, 0 at or below minimum, from the tables in 1/128 dB steps over +-144 dB */
float sceSasAsDbToLinear(float db, float minimum)
{
	SceInt32 whole;
	SceInt32 fraction;
	float offset;

	if (db == 0.0f)
		return 1.0f;
	if (db <= minimum)
		return 0.0f;
	if (!(db > -144.0f)) {
		whole = 0;
		fraction = 0;
	} else if (db > 144.0f) {
		whole = 288;
		fraction = 0;
	} else {
		offset = db + 144.0f;
		whole = (SceInt32)offset;
		fraction = (SceInt32)((offset - (float)whole) * 128.0f);
	}
	return sceSasAsDbTable[whole] * sceSasAsDbFractionTable[fraction];
}
