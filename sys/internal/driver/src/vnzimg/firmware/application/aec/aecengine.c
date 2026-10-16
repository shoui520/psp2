/*
 * The echo canceller's engine (0x8689ec-0x869910). A call takes 512
 * microphone samples and 512 far-end samples, and gives back 512 samples
 * of the microphone with the echo and the noise taken out.
 */
#include "aec.h"

#define AEC_DEFAULT_CURVE	{ 255, 255, 255, 255, 255, 255, 255, 127, \
				  63, 31, 31, 31, 31, 63, 127, 255 }
#define AEC_LIMITER_GAIN	16306		/* the input's, Q12: 3.98 */
#define AEC_FAR_FILL		16
#define AEC_DELAY_MIN		(-54424)	/* below it the far end is silent */
#define AEC_DELAY_RANGE		54425
#define AEC_LEVEL_MAX		0x1fffff
#define AEC_UNITY		0x8000		/* a gain of 1, Q15 */
#define AEC_FRAMES_SETTLED	100

/* the analysis window's first half (Hamming), used mirrored */
static const int16_t analysisWindow[AEC_HALF] = {
	  2621,   2621,   2622,   2623,   2625,   2628,   2631,   2635,   2639,   2644,
	  2649,   2655,   2662,   2669,   2677,   2685,   2694,   2703,   2713,   2723,
	  2735,   2746,   2758,   2771,   2784,   2798,   2813,   2828,   2843,   2859,
	  2876,   2893,   2911,   2929,   2948,   2968,   2988,   3008,   3030,   3051,
	  3074,   3096,   3120,   3144,   3168,   3193,   3219,   3245,   3271,   3298,
	  3326,   3354,   3383,   3413,   3442,   3473,   3504,   3535,   3567,   3600,
	  3633,   3667,   3701,   3735,   3771,   3806,   3843,   3879,   3917,   3954,
	  3993,   4032,   4071,   4111,   4151,   4192,   4233,   4275,   4318,   4361,
	  4404,   4448,   4493,   4537,   4583,   4629,   4675,   4722,   4770,   4817,
	  4866,   4915,   4964,   5014,   5064,   5115,   5166,   5218,   5270,   5323,
	  5376,   5429,   5483,   5538,   5593,   5648,   5704,   5760,   5817,   5874,
	  5932,   5990,   6049,   6108,   6167,   6227,   6287,   6348,   6409,   6471,
	  6533,   6595,   6658,   6721,   6785,   6849,   6914,   6979,   7044,   7110,
	  7176,   7242,   7309,   7377,   7444,   7512,   7581,   7650,   7719,   7788,
	  7858,   7929,   7999,   8070,   8142,   8214,   8286,   8358,   8431,   8504,
	  8578,   8652,   8726,   8801,   8876,   8951,   9026,   9102,   9179,   9255,
	  9332,   9409,   9487,   9564,   9643,   9721,   9800,   9879,   9958,  10038,
	 10118,  10198,  10278,  10359,  10440,  10521,  10603,  10685,  10767,  10849,
	 10932,  11015,  11098,  11181,  11265,  11348,  11433,  11517,  11601,  11686,
	 11771,  11857,  11942,  12028,  12114,  12200,  12286,  12372,  12459,  12546,
	 12633,  12721,  12808,  12896,  12984,  13072,  13160,  13248,  13337,  13425,
	 13514,  13603,  13692,  13782,  13871,  13961,  14051,  14141,  14231,  14321,
	 14411,  14501,  14592,  14683,  14773,  14864,  14955,  15046,  15138,  15229,
	 15320,  15412,  15503,  15595,  15687,  15778,  15870,  15962,  16054,  16146,
	 16238,  16331,  16423,  16515,  16607,  16700,  16792,  16885,  16977,  17069,
	 17162,  17255,  17347,  17440,  17532,  17625,  17717,  17810,  17903,  17995,
	 18088,  18180,  18273,  18365,  18458,  18550,  18643,  18735,  18827,  18920,
	 19012,  19104,  19196,  19288,  19380,  19472,  19564,  19656,  19748,  19839,
	 19931,  20022,  20114,  20205,  20296,  20388,  20479,  20570,  20660,  20751,
	 20842,  20932,  21023,  21113,  21203,  21293,  21383,  21472,  21562,  21651,
	 21741,  21830,  21919,  22007,  22096,  22185,  22273,  22361,  22449,  22537,
	 22624,  22712,  22799,  22886,  22973,  23059,  23146,  23232,  23318,  23404,
	 23489,  23575,  23660,  23745,  23829,  23914,  23998,  24082,  24166,  24249,
	 24332,  24415,  24498,  24581,  24663,  24745,  24826,  24908,  24989,  25070,
	 25151,  25231,  25311,  25391,  25470,  25549,  25628,  25707,  25785,  25863,
	 25941,  26018,  26095,  26172,  26248,  26324,  26400,  26475,  26550,  26625,
	 26699,  26774,  26847,  26921,  26994,  27066,  27139,  27211,  27282,  27354,
	 27424,  27495,  27565,  27635,  27704,  27773,  27842,  27910,  27978,  28046,
	 28113,  28179,  28246,  28312,  28377,  28442,  28507,  28571,  28635,  28699,
	 28762,  28824,  28887,  28948,  29010,  29071,  29131,  29191,  29251,  29310,
	 29369,  29427,  29485,  29543,  29600,  29656,  29712,  29768,  29823,  29878,
	 29932,  29986,  30039,  30092,  30145,  30197,  30248,  30299,  30350,  30400,
	 30449,  30498,  30547,  30595,  30643,  30690,  30736,  30783,  30828,  30873,
	 30918,  30962,  31006,  31049,  31092,  31134,  31176,  31217,  31258,  31298,
	 31337,  31376,  31415,  31453,  31491,  31528,  31564,  31600,  31636,  31671,
	 31705,  31739,  31772,  31805,  31837,  31869,  31900,  31931,  31961,  31991,
	 32020,  32048,  32076,  32104,  32131,  32157,  32183,  32208,  32233,  32257,
	 32281,  32304,  32326,  32348,  32369,  32390,  32411,  32430,  32450,  32468,
	 32486,  32504,  32521,  32537,  32553,  32568,  32583,  32597,  32611,  32624,
	 32636,  32648,  32660,  32670,  32681,  32690,  32699,  32708,  32716,  32723,
	 32730,  32736,  32742,  32747,  32752,  32755,  32759,  32762,  32764,  32766,
	 32767,  32767,
};

/* the synthesis window's first half, Q11, used mirrored */
static const int16_t synthesisWindow[AEC_HALF] = {
	 25600,  25597,  25588,  25575,  25555,  25530,  25500,  25464,  25423,  25377,
	 25325,  25268,  25206,  25139,  25067,  24990,  24908,  24822,  24731,  24636,
	 24536,  24433,  24325,  24213,  24097,  23977,  23854,  23728,  23598,  23465,
	 23329,  23190,  23048,  22904,  22757,  22607,  22456,  22302,  22147,  21989,
	 21830,  21670,  21507,  21344,  21179,  21014,  20847,  20679,  20511,  20342,
	 20173,  20003,  19833,  19662,  19491,  19321,  19150,  18980,  18809,  18639,
	 18469,  18300,  18131,  17963,  17795,  17628,  17462,  17297,  17132,  16968,
	 16805,  16643,  16482,  16323,  16164,  16006,  15850,  15694,  15540,  15387,
	 15235,  15085,  14936,  14788,  14641,  14496,  14352,  14210,  14068,  13929,
	 13790,  13653,  13517,  13383,  13250,  13119,  12989,  12860,  12732,  12607,
	 12482,  12359,  12237,  12117,  11997,  11880,  11763,  11648,  11535,  11422,
	 11311,  11202,  11093,  10986,  10880,  10775,  10672,  10570,  10469,  10370,
	 10271,  10174,  10078,   9983,   9889,   9797,   9705,   9615,   9526,   9438,
	  9351,   9265,   9180,   9097,   9014,   8932,   8851,   8772,   8693,   8615,
	  8539,   8463,   8388,   8314,   8241,   8169,   8098,   8028,   7959,   7890,
	  7822,   7756,   7690,   7624,   7560,   7497,   7434,   7372,   7311,   7250,
	  7190,   7131,   7073,   7016,   6959,   6903,   6847,   6792,   6738,   6685,
	  6632,   6580,   6528,   6478,   6427,   6378,   6329,   6280,   6232,   6185,
	  6138,   6092,   6046,   6001,   5957,   5913,   5869,   5826,   5784,   5742,
	  5700,   5659,   5619,   5579,   5539,   5500,   5462,   5423,   5386,   5348,
	  5311,   5275,   5239,   5203,   5168,   5133,   5099,   5065,   5031,   4998,
	  4965,   4933,   4900,   4869,   4837,   4806,   4776,   4745,   4715,   4685,
	  4656,   4627,   4598,   4570,   4542,   4514,   4487,   4459,   4433,   4406,
	  4380,   4354,   4328,   4303,   4277,   4253,   4228,   4204,   4180,   4156,
	  4132,   4109,   4086,   4063,   4040,   4018,   3996,   3974,   3952,   3931,
	  3910,   3889,   3868,   3847,   3827,   3807,   3787,   3767,   3748,   3729,
	  3710,   3691,   3672,   3654,   3635,   3617,   3599,   3581,   3564,   3546,
	  3529,   3512,   3495,   3479,   3462,   3446,   3430,   3414,   3398,   3382,
	  3366,   3351,   3336,   3321,   3306,   3291,   3276,   3262,   3248,   3233,
	  3219,   3205,   3192,   3178,   3165,   3151,   3138,   3125,   3112,   3099,
	  3086,   3074,   3061,   3049,   3037,   3024,   3012,   3001,   2989,   2977,
	  2966,   2954,   2943,   2932,   2921,   2910,   2899,   2888,   2877,   2867,
	  2856,   2846,   2836,   2826,   2816,   2806,   2796,   2786,   2776,   2767,
	  2757,   2748,   2739,   2730,   2720,   2711,   2703,   2694,   2685,   2676,
	  2668,   2659,   2651,   2643,   2634,   2626,   2618,   2610,   2602,   2594,
	  2586,   2579,   2571,   2564,   2556,   2549,   2541,   2534,   2527,   2520,
	  2513,   2506,   2499,   2492,   2486,   2479,   2472,   2466,   2459,   2453,
	  2447,   2440,   2434,   2428,   2422,   2416,   2410,   2404,   2398,   2392,
	  2387,   2381,   2375,   2370,   2364,   2359,   2354,   2348,   2343,   2338,
	  2333,   2328,   2323,   2318,   2313,   2308,   2303,   2298,   2294,   2289,
	  2284,   2280,   2275,   2271,   2267,   2262,   2258,   2254,   2250,   2246,
	  2241,   2237,   2233,   2230,   2226,   2222,   2218,   2214,   2211,   2207,
	  2203,   2200,   2196,   2193,   2190,   2186,   2183,   2180,   2176,   2173,
	  2170,   2167,   2164,   2161,   2158,   2155,   2152,   2149,   2146,   2144,
	  2141,   2138,   2136,   2133,   2131,   2128,   2126,   2123,   2121,   2118,
	  2116,   2114,   2112,   2109,   2107,   2105,   2103,   2101,   2099,   2097,
	  2095,   2093,   2092,   2090,   2088,   2086,   2085,   2083,   2081,   2080,
	  2078,   2077,   2075,   2074,   2073,   2071,   2070,   2069,   2068,   2066,
	  2065,   2064,   2063,   2062,   2061,   2060,   2059,   2058,   2057,   2057,
	  2056,   2055,   2054,   2054,   2053,   2052,   2052,   2051,   2051,   2050,
	  2050,   2049,   2049,   2049,   2048,   2048,   2048,   2048,   2048,   2048,
	  2048,   2048,
};

static inline int32_t clamp16(int32_t value)
{
	if (value > 32767)
		return 32767;
	if (value < -32768)
		return -32768;
	return value;
}

/*
 * The microphone's gain by bin from 16 points, one every 32 bins, each
 * (x + 1) / 256, interpolated linearly in Q4.
 */
void aecSetCurve(AecWork *work, const uint8_t *points)
{
	int32_t i, j, low, high, step, value;

	for (i = 0; i < AEC_CURVE_POINTS; i++) {
		low = (points[i] + 1) << 4;
		if (i == AEC_CURVE_POINTS - 1)
			high = low;
		else
			high = (points[i + 1] + 1) << 4;
		step = (high - low) >> 5;
		for (j = 0; j < 32; j++) {
			low += step;
			value = low >> 4;
			work->curve[i * 32 + j] = value ? value - 1 : 0;
		}
	}
}

void aecApplyCurve(const uint8_t *curve, AecComplex *spectrum)
{
	int32_t k;

	for (k = 0; k < AEC_BINS; k++) {
		spectrum[k].re = ((int64_t)spectrum[k].re * (curve[k] + 1)) >> 8;
		spectrum[k].im = ((int64_t)spectrum[k].im * (curve[k] + 1)) >> 8;
	}
}

/* the far end's delay, in samples, into the bins' log units */
void aecSetDelay(AecWork *work, int16_t delay, uint32_t flags)
{
	int32_t scaled = (delay * 21771) >> 13;

	if (scaled != work->delay)
		work->delay = scaled;
	if (flags != work->flags) {
		work->flags = flags;
		work->frames = 0;
	}
}

uint32_t aecSetFarFill(AecWork *work, uint32_t fill)
{
	if (fill > AEC_HALF)
		fill = AEC_HALF;
	work->farFill = fill;
	return fill;
}

static void binsReset(AecWork *work)
{
	AecBin *bin;
	int32_t k;

	for (k = 0; k < AEC_BINS; k++) {
		bin = &work->bins[k];
		bin->farLevel = 100;
		bin->farPower = 100 << 9;
		bin->nearLevel = 1000000;
		bin->nearPower = 15625 << 15;
		bin->noise = 15625 << 13;
		bin->delayMean = 6803;
		bin->delayMoment = 0x1a930000;
		bin->delaySquare = 0x0301c09a;
		bin->noiseGain = 0;
		bin->reserved = 100;
		bin->delaySpread = 0;
		bin->delayAverage = 6803;
	}
}

static void aecDefaults(AecWork *work)
{
	work->delay = 0;
	work->reserved2 = 1;
	work->highPass = 1;
	work->noiseScale = 16;
	work->echoScale = 1;
	work->agcEnabled = 1;
	work->noiseSuppression = 1;
	work->echoSuppression = 1;
	work->agcGain = 0;
	work->flags = 0;
	binsReset(work);
}

static void historiesClear(AecWork *work)
{
	memset(work->farHistory, 0, sizeof(work->farHistory));
	memset(work->farPrevious, 0, sizeof(work->farPrevious));
	memset(work->nearPrevious, 0, sizeof(work->nearPrevious));
	memset(work->outputPrevious, 0, sizeof(work->outputPrevious));
}

static void agcInit(AecAgc *agc)
{
	agc->nearPeak = 1200;
	agc->inputPeak = 1200;
	agc->farPeak = 30;
	agc->level = 30 << 7;
	agc->gain = AEC_UNITY << 4;
}

static uint16_t peak(const int16_t *samples)
{
	uint16_t max = 0, value;
	int32_t i;

	for (i = 0; i < AEC_HALF; i++) {
		value = abs(samples[i]);
		if (value > max)
			max = value;
	}
	return max;
}

/*
 * The output's automatic gain: full while the output is loud enough,
 * down to nothing when it is quiet, and lowered while the far end talks
 * or the microphone is much louder than the output.
 */
static uint32_t agc(AecAgc *agc, const int16_t *far, int16_t *output,
		    const int16_t *input)
{
	uint32_t average, level, rise, target, peak0, gain, scale;
	int32_t i;

	average = (agc->farPeak + peak(far)) >> 1;
	agc->farPeak = average;
	level = agc->level;
	rise = (level + (level >> 3)) >> 7;
	if (rise < average && (level >> 7) >= 31)
		target = rise;
	else if ((level >> 7) + 30 < average)
		target = (level >> 7) + 30;
	else
		target = average;
	level = level - (level >> 7) + target;
	if (level < 6400)
		level = 6400;
	else if (level > 128000)
		level = 128000;
	agc->level = level;

	agc->nearPeak = agc->nearPeak - agc->nearPeak / 2 + peak(output);
	agc->inputPeak = agc->inputPeak - agc->inputPeak / 2 + peak(input);

	peak0 = agc->nearPeak >> 2;
	if (peak0 >= 161)
		gain = AEC_UNITY;
	else if (peak0 < 32)
		gain = 0;
	else
		gain = (uint16_t)((((peak0 << 15) >> 5) - AEC_UNITY) >> 2);
	if (((level >> 7) << 4) < average) {
		scale = ((peak0 << 15) >> 5) >> 2;
		if (scale < gain)
			gain = scale;
	}
	if (agc->nearPeak < agc->inputPeak >> 2)
		gain = 8192;
	if (agc->nearPeak < agc->inputPeak >> 3)
		gain = 4096;
	if (agc->nearPeak < agc->inputPeak >> 4)
		gain = 0;
	if (gain > AEC_UNITY)
		gain = AEC_UNITY;

	scale = agc->gain;
	for (i = 0; i < AEC_HALF; i++) {
		if (gain < (scale >> 13))
			scale = scale - (scale >> 13) + gain;
		else
			scale = (gain << 6) + scale - (scale >> 7);
		output[i] = (int16_t)((output[i] * (int32_t)(scale >> 13)) >> 15);
	}
	agc->gain = scale;
	return gain;
}

/* the first half by the window, the second by its mirror */
static void window(const int16_t *src, int16_t *dst)
{
	int32_t k;

	for (k = 0; k < AEC_HALF; k++) {
		dst[k] = (src[k] * analysisWindow[k]) >> 15;
		dst[AEC_HALF + k] = (src[AEC_HALF + k] *
				     analysisWindow[AEC_HALF - 1 - k]) >> 15;
	}
}

static void synthesis(const int16_t *src, int16_t *dst)
{
	int32_t k;

	for (k = 0; k < AEC_HALF; k++) {
		dst[k] = clamp16((src[k] * synthesisWindow[k]) >> 11);
		dst[AEC_HALF + k] = clamp16((src[AEC_HALF + k] *
				    synthesisWindow[AEC_HALF - 1 - k]) >> 11);
	}
}

/* a level's rise limited to an eighth over its average, at least 10 */
static int32_t limitRise(int32_t level, int32_t average)
{
	int32_t rise = average + (average >> 3);

	if (rise < level) {
		level = rise;
		if (average < 8)
			level = average + 1;
	}
	if (level < 10)
		level = 10;
	return level;
}

static int32_t magnitude(int32_t re, int32_t im)
{
	uint64_t power = (int64_t)re * re + (int64_t)im * im;

	return aecExp2(aecLog2Long(&power) >> 1);
}

static int32_t levelLog(int32_t level)
{
	if (level < 1)
		level = 1;
	else if (level > AEC_LEVEL_MAX)
		level = AEC_LEVEL_MAX;
	return aecLog2(level);
}

/* a slow average that falls by the step's quarter and rises by its 512th */
static int32_t powerUpdate(int32_t power, int32_t level)
{
	if (level < (power >> 9))
		return (level << 2) + power - (power >> 7);
	return power - (power >> 9) + level;
}

/* the bin's levels from this frame's far and near values */
static void binUpdate(AecBin *bin, const AecComplex *far,
		      const AecComplex *near, int32_t *farLog, int32_t *nearLog,
		      int32_t *farLevel, int32_t *nearMagnitude,
		      int32_t *farPower, int32_t *nearPower)
{
	int32_t level, nearLevel, power, noise, floor;

	level = (magnitude(far->re, far->im) + bin->farLevel) >> 1;
	*farLog = levelLog(level);
	bin->farPower = powerUpdate(bin->farPower,
				    limitRise(level, bin->farPower >> 9));

	*nearMagnitude = magnitude(near->re, near->im);
	if (*nearMagnitude < 1)
		*nearMagnitude = bin->nearLevel;
	nearLevel = (*nearMagnitude + bin->nearLevel) >> 1;
	*nearLog = levelLog(nearLevel);
	power = powerUpdate(bin->nearPower,
			    limitRise(nearLevel, bin->nearPower >> 9));

	noise = bin->noise;
	floor = limitRise(nearLevel, noise >> 7);
	if (floor > 1000000)
		floor = 1000000;
	if ((noise >> 7) < floor)
		noise = noise - (noise >> 7) + floor;
	else
		noise = (noise >> 1) + (floor << 6);
	if (((noise >> 7) << 1) < (power >> 9))
		power = noise << 3;

	*farLevel = level;
	*farPower = bin->farPower >> 9;
	*nearPower = power >> 9;
	bin->noise = noise;
	bin->nearPower = power;
	bin->nearLevel = nearLevel;
	bin->farLevel = level;
}

/*
 * The echo's gain: the near level over the delayed far level, tracked
 * as a mean and a spread while the far end is well over its floor; the
 * more the near level exceeds what the echo explains, the less is taken.
 */
static int32_t echoGain(AecBin *bin, int32_t scale, int32_t farLog,
			int32_t nearLog, int32_t farLevel, int32_t farPower)
{
	int32_t delay = nearLog - farLog, mean = bin->delayMean;
	int32_t moment = bin->delayMoment, average = bin->delayAverage;
	int32_t spread = bin->delaySpread, deviation, exponent, value;
	int32_t keep, weight, gain;
	uint32_t square = bin->delaySquare, high;

	if (5 * farPower + 5 < farLevel && delay >= -AEC_DELAY_RANGE &&
	    delay < AEC_DELAY_RANGE + 1) {
		moment = (moment >> 1) + (delay << 15);
		square = (square + (uint32_t)(delay * delay)) >> 1;
		high = ((int64_t)moment * moment) >> 32;
		deviation = high < square ? aecSqrt(square - high) : 0;
		value = delay;
		if (value > average + 100)
			value = average + 100;
		average = (average + value) >> 1;
		exponent = 0x1f000 - aecLog2((deviation * 0xe28c + 0x25a0f5b) >> 4);
		if (delay >= (moment >> 16))
			exponent -= 12288;
		value = 0xf000 - aecExp2(exponent);
		if (value > 0xffff)
			value = 0xffff;
		keep = aecExp2(value);
		weight = AEC_UNITY - keep;
		if (deviation < 2042) {
			spread = (spread * keep + weight * deviation) >> 15;
			if (spread > 2041)
				spread = 2041;
		}
		mean = (mean * keep + weight * delay) >> 15;
	}

	exponent = farLog + mean + scale * spread - nearLog + 0xf000;
	if (exponent < 0)
		exponent = 0;
	else if (exponent > 0xffff)
		exponent = 0xffff;
	gain = aecExp2(exponent);
	if (gain < 0)
		gain = 0;
	else if (gain > AEC_UNITY)
		gain = AEC_UNITY;

	bin->delay = delay;
	bin->delayAverage = average;
	bin->delaySquare = square;
	bin->delayMean = mean;
	bin->delayMoment = moment;
	bin->delaySpread = spread;
	bin->echoGain = gain;
	return gain;
}

/* the noise's gain: the floor over the near level, raised by the scale */
static int32_t noiseGain(AecBin *bin, int32_t scale, int32_t nearPower)
{
	int32_t exponent, gain;

	exponent = (scale << 12) + aecLog2(nearPower) - aecLog2(bin->nearLevel);
	if (exponent >= 0x10000)
		gain = AEC_UNITY;
	else if (exponent < 0)
		gain = 0;
	else
		gain = aecExp2Fraction(exponent);
	bin->noiseGain = gain << 2;
	return gain;
}

static void binApply(int32_t echo, int32_t noise, const AecComplex *near,
		     AecComplex *output)
{
	int32_t gain = AEC_UNITY - echo - noise;

	if (gain < 0)
		gain = 0;
	output->re = ((int64_t)near->re * gain) >> 15;
	output->im = ((int64_t)near->im * gain) >> 15;
}

static void bins(AecWork *work, const AecComplex *far, const AecComplex *near,
		 AecComplex *output)
{
	int32_t k, farLog, nearLog, farLevel, nearMagnitude, farPower;
	int32_t nearPower, echo, noise;
	AecBin *bin;

	for (k = 0; k < AEC_BINS; k++) {
		bin = &work->bins[k];
		binUpdate(bin, &far[k], &near[k], &farLog, &nearLog, &farLevel,
			  &nearMagnitude, &farPower, &nearPower);
		echo = echoGain(bin, work->echoScale, work->delay + farLog,
				nearLog, farLevel, farPower);
		noise = noiseGain(bin, work->noiseScale, nearPower);
		if (!work->echoSuppression)
			echo = 0;
		if (!work->noiseSuppression)
			noise = 0;
		binApply(echo, noise, &near[k], &output[k]);
	}
}

static void limiterInit(AecLimiter *limiter, uint16_t gain)
{
	int32_t i;

	limiter->envelope = 0x01000000;
	limiter->gain = gain;
	limiter->hold = 0;
	limiter->peak = 0;
	limiter->count = AEC_HALF;
	for (i = 0; i < AEC_LIMITER_HISTORY; i++)
		limiter->history[i] = 0;
	limiter->index = 0;
}

/*
 * A look-ahead limiter: the input is amplified, delayed by 32 samples,
 * and scaled by an envelope that falls fast ahead of a peak over 20000
 * and recovers slowly once 32 samples have passed it.
 */
static void limiter(AecLimiter *limiter, int16_t *samples)
{
	int32_t i, value, level, target, envelope;

	for (i = 0; i < limiter->count; i++) {
		value = (limiter->gain * samples[i] + 2048) >> 12;
		limiter->history[limiter->index] = value;
		level = abs(value);
		if (level >= 20001)
			limiter->hold = -AEC_LIMITER_HISTORY;
		if (limiter->hold < 0) {
			limiter->hold++;
			if (level > limiter->peak)
				limiter->peak = level;
		} else {
			limiter->peak = level;
		}
		if (limiter->peak >= 20001)
			target = (uint16_t)(0x27100000 / limiter->peak);
		else
			target = AEC_UNITY;
		envelope = limiter->envelope;
		if (envelope < target << 9)
			envelope = envelope - (envelope >> 9) + target;
		else
			envelope = (target << 6) + envelope - (envelope >> 3);
		limiter->envelope = envelope;
		limiter->index = (limiter->index + 1) & (AEC_LIMITER_HISTORY - 1);
		samples[i] = clamp16((limiter->history[limiter->index] *
				      (envelope >> 9)) / 32768);
	}
}

void aecReset(AecWork *work)
{
	static const uint8_t points[AEC_CURVE_POINTS] = AEC_DEFAULT_CURVE;

	work->frames = 0;
	aecDefaults(work);
	historiesClear(work);
	agcInit(&work->agc);
	work->highPassState[2] = 0;
	work->highPassState[1] = 0;
	work->highPassState[0] = 0;
	work->farFill = AEC_FAR_FILL;
	limiterInit(&work->limiter, AEC_LIMITER_GAIN);
	aecSetCurve(work, points);
}

/* a second-order high pass on the microphone, in place */
static void highPass(AecWork *work, int16_t *samples)
{
	int32_t *state = work->highPassState, i, input;

	for (i = 0; i < AEC_HALF; i++) {
		input = samples[i] - (state[2] * 217) / 256 -
			(state[1] * -470) / 256;
		samples[i] = clamp16(input - 2 * state[1] + state[2]);
		state[0] = input;
		state[2] = state[1];
		state[1] = input;
	}
}

int32_t aecProcess(AecWork *work, int16_t *near, const int16_t *far)
{
	int16_t frame[AEC_FRAME * 2 + AEC_HALF];
	int16_t *frameNear = frame + AEC_FRAME;
	int16_t *input = frame + AEC_FRAME * 2;
	int32_t i, n;

	if (work->flags & AEC_FLAG_CLEAR) {
		work->flags ^= AEC_FLAG_CLEAR;
		historiesClear(work);
	}
	if (work->flags & AEC_FLAG_BYPASS) {
		limiter(&work->limiter, near);
		return 0;
	}

	/* the far end, delayed by what is kept from the last call */
	for (i = 0; i < (int32_t)work->farFill; i++)
		work->farHistory[i] = work->farHistory[AEC_HALF + i];
	if (work->delay < AEC_DELAY_MIN)
		memset(work->farHistory + work->farFill, 0, AEC_HALF * 2);
	else
		memcpy(work->farHistory + work->farFill, far, AEC_HALF * 2);
	for (i = 0; i < AEC_HALF; i++) {
		frame[i] = work->farPrevious[i];
		frame[AEC_HALF + i] = work->farHistory[i];
		work->farPrevious[i] = work->farHistory[i];
	}
	window(frame, frame);

	if (work->highPass)
		highPass(work, near);
	for (i = 0; i < AEC_HALF; i++) {
		frameNear[i] = work->nearPrevious[i];
		input[i] = work->nearPrevious[i];
		frameNear[AEC_HALF + i] = near[i];
		work->nearPrevious[i] = near[i];
	}
	window(frameNear, frameNear);

	aecTransform(2, frame, frameNear, work->far, work->near);
	if (work->frames == 0)
		binsReset(work);
	if (work->frames < AEC_FRAMES_SETTLED)
		work->frames++;
	aecApplyCurve(work->curve, work->near);
	bins(work, work->far, work->near, work->output);
	aecInverse(2, work->output, frame, 1);
	synthesis(frame, frame);

	/* overlap-add with the last call's second half */
	for (n = 0; n < AEC_HALF; n++)
		frame[n] = clamp16((work->outputPrevious[n] * (AEC_HALF - n) +
				    frame[n] * n) / AEC_HALF);
	for (n = 0; n < AEC_HALF; n++)
		work->outputPrevious[n] = frame[AEC_HALF + n];

	if (work->agcEnabled) {
		work->agcGain = agc(&work->agc, work->farHistory, frame, input);
	} else {
		memcpy(frameNear, frame, AEC_HALF * 2);
		work->agcGain = agc(&work->agc, work->farHistory, frameNear,
				    input);
	}
	limiter(&work->limiter, frame);
	memcpy(near, frame, AEC_HALF * 2);
	return 0;
}
