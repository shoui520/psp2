/*
 * The filter (SCE_NGS_FILTER_ID): a biquad per channel, its coefficients
 * designed from a mode, frequency, resonance and gain (the cookbook
 * formulas, in 8.24), or given directly. Filters that pass everything are
 * copies.
 */
#include "ngs.h"

#define SCE_NGS_FILTER_LOWPASS_RESONANT			1
#define SCE_NGS_FILTER_HIGHPASS_RESONANT		2
#define SCE_NGS_FILTER_BANDPASS_PEAK			3
#define SCE_NGS_FILTER_BANDPASS_ZERO			4
#define SCE_NGS_FILTER_NOTCH				5
#define SCE_NGS_FILTER_PEAK				6
#define SCE_NGS_FILTER_HIGHSHELF			7
#define SCE_NGS_FILTER_LOWSHELF				8
#define SCE_NGS_FILTER_LOWPASS_ONEPOLE			9
#define SCE_NGS_FILTER_HIGHPASS_ONEPOLE			10
#define SCE_NGS_FILTER_ALLPASS				11
#define SCE_NGS_FILTER_LOWPASS_RESONANT_NORMALIZED	12

#define TWO_PI			0x6487ed8	/* 8.24 */
#define E			0x2b7e150
#define SQRT_2			0x16a09e6
#define TEN			(10 * NGS_ONE)
#define TWENTIETH		0xccccc		/* 0.05 */
#define FORTIETH		0x66666		/* 0.025 */
#define NORMAL_Q		0xb33334	/* 0.7 */
#define SMALL_A0		0x20001		/* below, divide by a0 directly */

/* the unnormalized coefficients */
typedef struct Coefficients {
	int32_t b0, b1, b2;
	int32_t a0, a1, a2;
} Coefficients;

static int32_t half(int32_t x)
{
	return x / 2;
}

/* b / a0 and -a / a0; a large a0 through its inverse */
static void biquadSet(NgsBiquad *filter, const Coefficients *k)
{
	int32_t inverse;

	if (k->a0 < SMALL_A0) {
		filter->a1 = -ngsDivide(k->a1, k->a0);
		filter->a2 = -ngsDivide(k->a2, k->a0);
		filter->b0 = ngsDivide(k->b0, k->a0);
		filter->b1 = ngsDivide(k->b1, k->a0);
		filter->b2 = ngsDivide(k->b2, k->a0);
		return;
	}
	inverse = ngsDivide(NGS_ONE, k->a0);
	filter->a1 = -ngsMultiply(k->a1, inverse);
	filter->a2 = -ngsMultiply(k->a2, inverse);
	filter->b0 = ngsMultiply(k->b0, inverse);
	filter->b1 = ngsMultiply(k->b1, inverse);
	filter->b2 = ngsMultiply(k->b2, inverse);
}

/* a resonant pair: 1 +- alpha over 1 - 2c z^-1 */
static void resonant(Coefficients *k, int32_t alpha, int32_t cosine)
{
	k->a0 = alpha + NGS_ONE;
	k->a1 = -cosine << 1;
	k->a2 = NGS_ONE - alpha;
}

/* a shelf: amplitude A = 10^(dB / 40), 2 sqrt(A) alpha with slope 1 */
static void shelf(Coefficients *k, int32_t omega, int32_t level, int32_t high)
{
	int32_t cosine, sine = ngsSinCos(omega, &cosine);
	int32_t amplitude = ngsPower(TEN, level), root = ngsSqrt(amplitude);
	int32_t plus = amplitude + NGS_ONE, minus = amplitude - NGS_ONE;
	int32_t plusCos = ngsMultiply(plus, cosine), minusCos = ngsMultiply(minus, cosine);
	int32_t beta = ngsMultiply(root, half(ngsMultiply(sine, SQRT_2))) << 1;

	if (high) {
		k->b1 = ngsMultiply(-amplitude << 1, minus + plusCos);
		k->b0 = ngsMultiply(amplitude, plus + beta + minusCos);
		k->b2 = ngsMultiply(amplitude, plus - beta + minusCos);
		k->a0 = plus + beta - minusCos;
		k->a1 = (minus - plusCos) << 1;
		k->a2 = plus - beta - minusCos;
	} else {
		k->b1 = ngsMultiply(amplitude << 1, minus - plusCos);
		k->b0 = ngsMultiply(amplitude, plus + beta - minusCos);
		k->b2 = ngsMultiply(amplitude, plus - beta - minusCos);
		k->a0 = plus + beta + minusCos;
		k->a1 = -((minus + plusCos) << 1);
		k->a2 = plus - beta + minusCos;
	}
}

/* 0x87ac68: -1 for an unknown mode; out of range or off, a filter that passes all */
int32_t ngsFilterDesign(float frequency, float resonance, uint32_t mode, float gain,
	NgsBiquad *filter, uint32_t sampleRate)
{
	int32_t decibels = (int32_t)(gain * 16777216.0f);
	int32_t q = (int32_t)(resonance * 16777216.0f);
	int32_t w = ((int32_t)(frequency * 65536.0f) / (int32_t)sampleRate) << 8;
	int32_t omega = ngsMultiply(TWO_PI, w), sine, cosine, alpha, level, e, amplitude;
	Coefficients k;

	if (!mode || w < 1 || w >= NGS_ONE / 2) {
		filter->b0 = NGS_ONE;
		filter->b1 = 0;
		filter->b2 = 0;
		filter->a1 = 0;
		filter->a2 = 0;
		filter->y2 = 0;
		filter->y1 = 0;
		return 0;
	}
	if (!q)
		q = 1;

	switch (mode) {
	case SCE_NGS_FILTER_LOWPASS_RESONANT:
	case SCE_NGS_FILTER_LOWPASS_RESONANT_NORMALIZED:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		k.b1 = NGS_ONE - cosine;
		/* normalized: the resonant peak held to (0.7 / Q)^(1/4) */
		if (mode == SCE_NGS_FILTER_LOWPASS_RESONANT_NORMALIZED && q >= NORMAL_Q)
			k.b1 = ngsMultiply(NGS_ONE - cosine,
				ngsPower(ngsDivide(NORMAL_Q - 1, q), NGS_ONE / 4));
		k.b0 = k.b2 = half(k.b1);
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_HIGHPASS_RESONANT:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		k.b1 = -(cosine + NGS_ONE);
		k.b0 = k.b2 = half(cosine + NGS_ONE);
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_BANDPASS_PEAK:
		sine = ngsSinCos(omega, &cosine);
		amplitude = ngsPower(TEN, ngsMultiply(half(decibels), TWENTIETH));
		k.b0 = half(ngsMultiply(sine, amplitude));
		k.b1 = 0;
		k.b2 = -k.b0;
		alpha = half(sine);
		if (amplitude)
			alpha = ngsDivide(alpha, amplitude);
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_BANDPASS_ZERO:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		k.b0 = alpha;
		k.b1 = 0;
		k.b2 = -alpha;
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_NOTCH:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		k.b0 = NGS_ONE;
		k.b1 = -cosine << 1;
		k.b2 = NGS_ONE;
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_PEAK:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		amplitude = ngsPower(TEN, ngsMultiply(half(decibels), TWENTIETH));
		level = ngsMultiply(alpha, amplitude);
		k.b0 = level + NGS_ONE;
		k.b1 = -cosine << 1;
		k.b2 = NGS_ONE - level;
		if (amplitude)
			alpha = ngsDivide(alpha, amplitude);
		resonant(&k, alpha, cosine);
		break;
	case SCE_NGS_FILTER_HIGHSHELF:
	case SCE_NGS_FILTER_LOWSHELF:
		shelf(&k, omega, ngsMultiply(decibels, FORTIETH),
			mode == SCE_NGS_FILTER_HIGHSHELF);
		break;
	case SCE_NGS_FILTER_LOWPASS_ONEPOLE:
	case SCE_NGS_FILTER_HIGHPASS_ONEPOLE:
		/* e^(-2 pi f) */
		e = ngsPower(E, ngsMultiply(-TWO_PI, w));
		k.a0 = NGS_ONE;
		k.a1 = -e;
		k.a2 = 0;
		k.b2 = 0;
		if (mode == SCE_NGS_FILTER_LOWPASS_ONEPOLE) {
			k.b0 = NGS_ONE - e;
			k.b1 = 0;
		} else {
			k.b0 = half(e + NGS_ONE);
			k.b1 = -k.b0;
		}
		break;
	case SCE_NGS_FILTER_ALLPASS:
		sine = ngsSinCos(omega, &cosine);
		alpha = ngsDivide(sine, q << 1);
		k.b0 = NGS_ONE - alpha;
		k.b1 = -cosine << 1;
		k.b2 = NGS_ONE + alpha;
		resonant(&k, alpha, cosine);
		break;
	default:
		return -1;
	}
	biquadSet(filter, &k);
	return 0;
}

int32_t ngsFilterInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	uint8_t *user = params->userParams[0];
	NgsBiquad *filters;
	int32_t channel;

	if (user) {
		if (((SceNgsParamsDescriptor *)user)->id == SCE_NGS_FILTER_PARAMS_STRUCT_ID) {
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
				NgsFilterUserParams *design =
					&((NgsFilterUserParams *)params->userParams[0])[channel];

				ngsFilterDesign(design->fFrequency, design->fResonance,
					design->eFilterMode, design->fGain,
					&((NgsBiquad *)params->data[0])[channel], params->sampleRate);
			}
		} else if (((SceNgsParamsDescriptor *)user)->id ==
			SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID) {
			for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
				NgsFilterUserCoefficients *given =
					&((NgsFilterUserCoefficients *)params->userParams[0])[channel];
				NgsBiquad *filter = &((NgsBiquad *)params->data[0])[channel];

				filter->b0 = ngsFixed(given->fB0);
				filter->b1 = ngsFixed(given->fB1);
				filter->b2 = ngsFixed(given->fB2);
				filter->a1 = ngsFixed(-given->fA1);
				filter->a2 = ngsFixed(-given->fA2);
			}
		} else {
			return SCE_NGS_ERROR_INVALID_PARAM;
		}
	}

	if (params->voiceState & SCE_NGS_VOICE_STATE_ACTIVE) {
		filters = (NgsBiquad *)params->data[0];
		for (channel = 0; channel < inputs->buffers[0]->channels; channel++) {
			filters[channel].x1 = 0;
			filters[channel].x2 = 0;
			filters[channel].y1 = 0;
			filters[channel].y2 = 0;
		}
	}
	return 0;
}

/* direct form I; the sum in 64 bits, shifted once (the a terms are stored negated) */
static int32_t biquadStep(NgsBiquad *filter, int32_t x)
{
	int64_t sum = (int64_t)filter->b0 * x + (int64_t)filter->b1 * filter->x1 +
		(int64_t)filter->b2 * filter->x2 + (int64_t)filter->a1 * filter->y1 +
		(int64_t)filter->a2 * filter->y2;
	int32_t y = (int32_t)(sum >> 24);

	filter->x2 = filter->x1;
	filter->x1 = x;
	filter->y2 = filter->y1;
	filter->y1 = y;
	return y;
}

/* 0x87bf80: two channels at once, one per IVC2 lane; mono passes one twice */
void ngsFilterStereo(const int32_t *input, const int32_t *input2, int32_t *output,
	int32_t *output2, uint32_t samples, NgsBiquad *filter, NgsBiquad *filter2)
{
	NgsBiquad left = *filter, right = *filter2;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		int32_t x = input[i], x2 = input2[i];

		output[i] = biquadStep(&left, x);
		output2[i] = biquadStep(&right, x2);
	}
	filter->x1 = left.x1;
	filter->x2 = left.x2;
	filter->y1 = left.y1;
	filter->y2 = left.y2;
	filter2->x1 = right.x1;
	filter2->x2 = right.x2;
	filter2->y1 = right.y1;
	filter2->y2 = right.y2;
}

/* b0 = 1, the rest 0 */
static uint32_t biquadPasses(const NgsBiquad *filter)
{
	return filter->b0 == NGS_ONE && filter->b1 == 0 && filter->b2 == 0 &&
		filter->a1 == 0 && filter->a2 == 0;
}

static void biquadClear(NgsBiquad *filter)
{
	filter->x1 = 0;
	filter->x2 = 0;
	filter->y1 = 0;
	filter->y2 = 0;
}

int32_t ngsFilterProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params)
{
	NgsBiquad *filters = (NgsBiquad *)params->data[0];
	NgsBuffer *input = inputs->buffers[0], *output;
	uint32_t bytes;

	if (!(input->flags & NGS_BUFFER_VALID)) {
		params->flags &= ~NGS_RUN_ENABLED;
		goto channels;
	}

	output = outputs->buffers[0];
	bytes = input->samples * NGS_SAMPLE_SIZE;
	if (!(input->channels & 1)) {
		if (params->flags & NGS_RUN_BYPASSED) {
			biquadClear(&filters[0]);
			biquadClear(&filters[1]);
		} else if (biquadPasses(&filters[0]) && biquadPasses(&filters[1])) {
			ngsCopy(output->channelData[0], input->channelData[0], bytes);
			ngsCopy(output->channelData[1], input->channelData[1], bytes);
		} else {
			ngsFilterStereo((int32_t *)input->channelData[0],
				(int32_t *)input->channelData[1], (int32_t *)output->channelData[0],
				(int32_t *)output->channelData[1], input->samples, &filters[0],
				&filters[1]);
		}
	} else {
		if (params->flags & NGS_RUN_BYPASSED)
			biquadClear(&filters[0]);
		else if (biquadPasses(&filters[0]))
			ngsCopy(output->channelData[0], input->channelData[0], bytes);
		else
			ngsFilterStereo((int32_t *)input->channelData[0],
				(int32_t *)input->channelData[0], (int32_t *)output->channelData[0],
				(int32_t *)output->channelData[0], input->samples, &filters[0],
				&filters[0]);
	}

	outputs->buffers[0]->flags = NGS_BUFFER_VALID;
	if (inputs->buffers[0]->flags & NGS_BUFFER_END) {
		outputs->buffers[0]->flags |= NGS_BUFFER_END;
		params->flags = 0;
	}
channels:
	if (outputs->count)
		outputs->buffers[0]->channelMask = inputs->buffers[0]->channelMask;
	return 0;
}
