/*
 * The frame's PCM (0x8b62e0): the channels, each in its own stride of the
 * output, are downmixed to stereo when asked for, then rounded to 16 or 24
 * bits and packed into the same buffer, interleaved or one channel after
 * another. 24-bit samples are three bytes, low first. Interleaving
 * overtakes the first channels' samples, so they are moved to the decoder's
 * spectra first (from 3 channels on; at 24 bits the first one always, and
 * four from 3 channels on).
 */
#include "aac.h"

#define BLOCK		16	/* words the IVC2 moves at once */
#define MIX_RIGHT	8	/* the right channel's gains, after the left's */

typedef struct Conversion {
	int32_t up;		/* the core's Q31, or SBR's output (4 bits lower) */
	int32_t round;
	int32_t gain;
	int32_t down;
} Conversion;

static int32_t convert(const Conversion *conversion, int32_t sample)
{
	int32_t value = aacShiftSaturate(sample, conversion->up);

	value = aacAddSaturate(value, conversion->round);
	value = aacShiftSaturate(value, conversion->gain);
	return value >> conversion->down;
}

static void put24(uint8_t *out, int32_t value)
{
	out[0] = value;
	out[1] = value >> 8;
	out[2] = value >> 16;
}

/* whole blocks only */
static int32_t *moveChannel(int32_t *to, const int32_t *from, int32_t samples)
{
	int32_t i;

	for (i = 0; i < (samples >> 4) * BLOCK; i++)
		to[i] = from[i];
	return to;
}

/* L = sum x mix[0][ch], R = sum x mix[1][ch], into channels 0 and 1 */
static void downmix(int32_t *output, int32_t samples, int32_t channels, int32_t stride,
	const int32_t *mix)
{
	int32_t i, ch;

	for (i = 0; i < samples; i++) {
		int32_t left = 0, right = 0;

		for (ch = 0; ch < channels; ch++) {
			int32_t x = output[ch * stride + i];

			left += (int32_t)(((int64_t)x * mix[ch]) >> 34);
			right += (int32_t)(((int64_t)x * mix[MIX_RIGHT + ch]) >> 34);
		}
		output[i] = aacShiftSaturate(left, 3);
		output[stride + i] = aacShiftSaturate(right, 3);
	}
}

void aacOutput(int32_t *output, int32_t samples, int32_t channels, uint32_t downmixed,
	uint32_t bits, int32_t interleaved, AacDecoder *decoder, int32_t sbrPresent,
	const int32_t *mix)
{
	int32_t stride = decoder->layout == AAC_LAYOUT_PLAIN ? 1024 : 2048;
	int32_t *spectra = decoder->spectra;
	const int32_t *source[AAC_MAX_CHANNELS];
	Conversion conversion = { 0, 0, 0, 0 };
	int32_t i, ch;

	if (bits == 16) {
		conversion.down = 16;
		conversion.round = sbrPresent ? 2048 : 0x8000;
	} else if (bits == 24) {
		conversion.down = 8;
		conversion.round = sbrPresent ? 8 : 128;
	}
	if (sbrPresent)
		conversion.gain = 4;
	else
		conversion.up = 1;

	if (downmixed && mix) {
		downmix(output, samples, channels, stride, mix);
		channels = 2;
	}

	if (!interleaved) {
		for (ch = 0; ch < channels; ch++) {
			const int32_t *in = output + ch * stride;

			if (bits == 16) {
				int16_t *out = (int16_t *)(output + ch * stride);

				for (i = 0; i < samples; i++)
					out[i] = convert(&conversion, in[i]);
			} else if (bits == 24) {
				uint8_t *out = (uint8_t *)(output + ch * stride);

				for (i = 0; i < samples; i++)
					put24(out + 3 * i, convert(&conversion, in[i]));
			}
		}
		return;
	}

	for (ch = 0; ch < AAC_MAX_CHANNELS; ch++)
		source[ch] = output + ch * stride;
	if (bits == 16) {
		int16_t *out = (int16_t *)output;

		if (channels >= 3) {
			source[0] = moveChannel(spectra, output, samples);
			source[1] = moveChannel(spectra + stride, output + stride, samples);
		}
		for (i = 0; i < samples; i++)
			for (ch = 0; ch < channels; ch++)
				*out++ = convert(&conversion, source[ch][i]);
	} else if (bits == 24) {
		uint8_t *out = (uint8_t *)output;

		source[0] = moveChannel(spectra, output, samples);
		if (channels >= 3)
			for (ch = 1; ch < 4; ch++)
				source[ch] = moveChannel(spectra + ch * stride,
					output + ch * stride, samples);
		for (i = 0; i < samples; i++)
			for (ch = 0; ch < channels; ch++, out += 3)
				put24(out, convert(&conversion, source[ch][i]));
	}
}
