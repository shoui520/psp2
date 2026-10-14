/* SBC frame codec; original source filename unavailable. */
#include <string.h>
#include "bt_sbc_codec.h"

void SbcAllocateBits(SbcContext *context, int encoding)
{
	int firstChannel = 0;
	int endChannel = context->mode == 1 ? 1 : context->channels;

	for (;;) {
		int maximumNeed = 0;
		int bitSlice, bitCount = 0, sliceCount = 0;
		int channel, subband;

		for (channel = firstChannel; channel < endChannel; channel++) {
			for (subband = 0; subband < context->subbands; subband++) {
				int scale = context->scaleFactors[channel][subband];
				int need;

				if (context->allocation == 1) {
					need = scale;
				} else if (!scale) {
					need = -5;
				} else {
					int offset;

					if (context->subbands == 4)
						offset = gSbcLoudnessOffsetsFour[context->frequency][subband];
					else
						offset = gSbcLoudnessOffsetsEight[context->frequency][subband];
					need = scale - offset;
					if (need > 0)
						need >>= 1;
				}
				context->bitNeed[channel][subband] = need;
				if (need > maximumNeed)
					maximumNeed = need;
			}
		}
		if (encoding)
			context->bitpool = context->maximumBitpool;
		bitSlice = maximumNeed + 1;
		do {
			bitSlice--;
			bitCount += sliceCount;
			sliceCount = 0;
			for (channel = firstChannel; channel < endChannel; channel++) {
				for (subband = 0; subband < context->subbands; subband++) {
					int need = context->bitNeed[channel][subband];

					if (need > bitSlice + 1 && need < bitSlice + 16)
						sliceCount++;
					else if (need == bitSlice + 1)
						sliceCount += 2;
				}
			}
		} while (bitCount + sliceCount < context->bitpool && bitSlice >= -29);
		if (bitCount + sliceCount == context->bitpool) {
			bitCount += sliceCount;
			bitSlice--;
		}
		for (channel = firstChannel; channel < endChannel; channel++) {
			for (subband = 0; subband < context->subbands; subband++) {
				int need = context->bitNeed[channel][subband];
				int bits = need <= bitSlice + 1 ? 0 : need - bitSlice;

				context->bits[channel][subband] = bits > 16 ? 16 : bits;
			}
		}
		for (subband = 0; subband < context->subbands && bitCount < context->bitpool;
		     subband++) {
			for (channel = firstChannel;
			     channel < endChannel && bitCount < context->bitpool; channel++) {
				int bits = context->bits[channel][subband];

				if (bits >= 2 && bits < 16) {
					context->bits[channel][subband] = bits + 1;
					bitCount++;
				} else if (context->bitNeed[channel][subband] == bitSlice + 1 &&
					   bitCount + 1 < context->bitpool) {
					context->bits[channel][subband] = 2;
					bitCount += 2;
				}
			}
		}
		for (subband = 0; subband < context->subbands && bitCount < context->bitpool;
		     subband++) {
			for (channel = firstChannel;
			     channel < endChannel && bitCount < context->bitpool; channel++) {
				if (context->bits[channel][subband] < 16) {
					context->bits[channel][subband]++;
					bitCount++;
				}
			}
		}
		if (context->mode != 1 || firstChannel)
			return;
		firstChannel = 1;
		endChannel = 2;
	}
}

void *SbcResetDecoder(SbcContext *context)
{
	return memset(context->history, 0, sizeof(context->history));
}

static inline unsigned int SbcReadBits(SbcContext *context, int count)
{
	while (context->bitCount < count) {
		context->bitBuffer = (context->bitBuffer << 8) | *context->input++;
		context->bitCount += 8;
	}
	context->bitCount -= count;
	return (context->bitBuffer >> context->bitCount) & ((1u << count) - 1);
}

static inline int SbcRoundSample(float sample)
{
	int value;

	if (sample >= 0.0f) {
		value = (int)(sample + 0.5f);
		if (value >= 32767)
			value = 32767;
	} else {
		value = (int)(sample - 0.5f);
		if (value < -32768)
			value = -32768;
	}
	return value;
}

int SbcDecodeFrame(SbcContext *context, const uint8_t *encoded,
		   int16_t *pcm, int interleaved)
{
	int block, channel, band, bands, blocks;
	float *position[2];
	unsigned int header, syncword, bitpool, crc;

	context->input = encoded + 1;
	context->bitCount = 0;
	syncword = encoded[0];
	context->bitBuffer = (context->bitBuffer << 8) | syncword;
	if (syncword != 0x9c)
		return 0;
	context->input = encoded + 2;
	header = encoded[1];
	context->input = encoded + 3;
	context->bitBuffer = (context->bitBuffer << 8) | header;
	context->subbands = bands = 4 * ((header & 1) + 1);
	context->blocks = blocks = 4 * (((header >> 4) & 3) + 1);
	context->frequency = header >> 6;
	context->mode = (header >> 2) & 3;
	context->allocation = (header & 2) != 0;
	bitpool = encoded[2];
	context->input = encoded + 4;
	context->bitBuffer = (context->bitBuffer << 8) | bitpool;
	context->bitpool = bitpool;
	crc = encoded[3];
	context->bitBuffer = (context->bitBuffer << 8) | crc;
	context->crc = crc;
	context->channels = context->mode ? 2 : 1;
	if (context->mode == 3)
		context->jointSubbands = SbcReadBits(context, bands);
	for (channel = 0; channel < context->channels; channel++)
		for (band = 0; band < bands; band++)
			context->scaleFactors[channel][band] = SbcReadBits(context, 4);
	SbcAllocateBits(context, 0);
	for (block = 0; block < blocks; block++) {
		for (channel = 0; channel < context->channels; channel++) {
			for (band = 0; band < bands; band++) {
				int bits = context->bits[channel][band];
				float sample = 0.0f;

				if (bits > 0) {
					unsigned int value = SbcReadBits(context, bits);
					int scale = context->scaleFactors[channel][band];

					sample = (float)(1 << (scale + 1)) *
						((float)(2 * value + 1) /
						 (float)((1 << bits) - 1) - 1.0f);
				}
				context->samples[channel][block][band] = sample;
			}
		}
	}
	if (context->mode == 3) {
		for (block = 0; block < blocks; block++) {
			for (band = 0; band < bands; band++) {
				if (context->jointSubbands & (1 << (bands - band - 1))) {
					float left = context->samples[0][block][band];
					float right = context->samples[1][block][band];

					context->samples[0][block][band] = left + right;
					context->samples[1][block][band] = left - right;
				}
			}
		}
	}
	for (channel = 0; channel < context->channels; channel++) {
		memmove(context->history[channel] + 2 * bands * blocks,
			context->history[channel], 144 * sizeof(float));
		position[channel] = context->history[channel] + 2 * bands * blocks;
	}
	if (context->channels == 2 && blocks == 16 && bands == 8) {
		SbcSynthesizeEight(position[0], &context->samples[0][0][0],
				   &context->workingSamples[0][0][0]);
		SbcSynthesizeEight(position[1], &context->samples[1][0][0],
				   &context->workingSamples[1][0][0]);
	} else {
		for (channel = 0; channel < context->channels; channel++) {
			float *history = position[channel];

			for (block = 0; block < blocks; block++) {
				const float *s = context->samples[channel][block];
				const float *window = bands == 4 ?
					gSbcPrototypeWindowFour : gSbcPrototypeWindow;
				float a, b, c, d, e, f, g, h;

				history -= 2 * bands;
				if (bands == 4) {
					a = ((s[0] * 0.707106769f - s[1] * 0.707106769f) -
					     s[2] * 0.707106769f) + s[3] * 0.707106769f;
					b = ((s[0] * 0.382683426f - s[1] * 0.923879504f) +
					     s[2] * 0.923879504f) - s[3] * 0.382683426f;
					c = ((-s[0] * 0.923879504f - s[1] * 0.382683426f) +
					     s[2] * 0.382683426f) + s[3] * 0.923879504f;
					d = -(((s[0] + s[1]) + s[2]) + s[3]);
					history[0] = a;
					history[1] = b;
					history[2] = 0.0f;
					history[3] = -b;
					history[4] = -a;
					history[5] = c;
					history[6] = d;
					history[7] = c;
				} else {
					a = ((((((s[0] * 0.707106769f - s[1] * 0.707106769f) -
					      s[2] * 0.707106769f) + s[3] * 0.707106769f) +
					      s[4] * 0.707106769f) - s[5] * 0.707106769f) -
					      s[6] * 0.707106769f) + s[7] * 0.707106769f;
					b = ((((((s[0] * 0.555570245f - s[1] * 0.980785251f) +
					      s[2] * 0.195090324f) + s[3] * 0.831469595f) -
					      s[4] * 0.831469595f) - s[5] * 0.195090324f) +
					      s[6] * 0.980785251f) - s[7] * 0.555570245f;
					c = ((((((s[0] * 0.382683426f - s[1] * 0.923879504f) +
					      s[2] * 0.923879504f) - s[3] * 0.382683426f) -
					      s[4] * 0.382683426f) + s[5] * 0.923879504f) -
					      s[6] * 0.923879504f) + s[7] * 0.382683426f;
					d = ((((((s[0] * 0.195090324f - s[1] * 0.555570245f) +
					      s[2] * 0.831469595f) - s[3] * 0.980785251f) +
					      s[4] * 0.980785251f) - s[5] * 0.831469595f) +
					      s[6] * 0.555570245f) - s[7] * 0.195090324f;
					e = ((((((s[1] * 0.195090324f + -s[0] * 0.831469595f) +
					      s[2] * 0.980785251f) + s[3] * 0.555570245f) -
					      s[4] * 0.555570245f) - s[5] * 0.980785251f) -
					      s[6] * 0.195090324f) + s[7] * 0.831469595f;
					f = ((((((-s[0] * 0.923879504f - s[1] * 0.382683426f) +
					      s[2] * 0.382683426f) + s[3] * 0.923879504f) +
					      s[4] * 0.923879504f) + s[5] * 0.382683426f) -
					      s[6] * 0.382683426f) - s[7] * 0.923879504f;
					g = ((((((-s[0] * 0.980785251f - s[1] * 0.831469595f) -
					      s[2] * 0.555570245f) - s[3] * 0.195090324f) +
					      s[4] * 0.195090324f) + s[5] * 0.555570245f) +
					      s[6] * 0.831469595f) + s[7] * 0.980785251f;
					h = ((((((s[0] + s[1]) + s[2]) + s[3]) + s[4]) +
					      s[5]) + s[6]) + s[7];
					history[0] = a;
					history[1] = b;
					history[2] = c;
					history[3] = d;
					history[4] = 0.0f;
					history[5] = -d;
					history[6] = -c;
					history[7] = -b;
					history[8] = -a;
					history[9] = e;
					history[10] = f;
					history[11] = g;
					history[12] = -h;
					history[13] = g;
					history[14] = f;
					history[15] = e;
				}
				for (band = 0; band < bands; band++) {
					float sum;
					int phase;

					sum = history[3 * bands + band] * window[bands + band];
					sum += history[band] * window[band];
					for (phase = 1; phase < 5; phase++) {
						sum += history[4 * bands * phase + band] *
							window[2 * bands * phase + band];
						sum += history[(4 * phase + 3) * bands + band] *
							window[(2 * phase + 1) * bands + band];
					}
					context->workingSamples[channel][block][band] =
						sum * -(float)bands;
				}
			}
		}
	}
	for (block = 0; block < blocks; block++) {
		for (band = 0; band < bands; band++) {
			int left = SbcRoundSample(context->workingSamples[0][block][band]);

			if (context->channels == 1 && !interleaved) {
				*pcm++ = left;
			} else {
				int right = context->channels == 1 ? left :
					SbcRoundSample(context->workingSamples[1][block][band]);

				*pcm++ = left;
				*pcm++ = right;
			}
		}
	}
	return context->input - encoded;
}

int SbcInitializeEncoder(SbcContext *context, int frequency, int blocks,
			 int mode, int allocation, int subbands,
			 int minimumBitpool, int maximumBitpool)
{
	context->mode = mode;
	context->allocation = allocation;
	context->frequency = frequency;
	context->blocks = blocks;
	context->channels = mode ? 2 : 1;
	context->subbands = subbands;
	context->minimumBitpool = minimumBitpool;
	context->maximumBitpool = maximumBitpool;
	memset(context->history, 0, sizeof(context->history));
	return 0;
}

static inline void SbcUpdateCrc(SbcContext *context, unsigned int value, int bits)
{
	for (int bit = 0; bit < bits; bit++) {
		context->crc <<= 1;
		value <<= 1;
		if ((context->crc ^ value) & 0x100)
			context->crc ^= 0x1d;
	}
}

static inline void SbcWriteBits(SbcContext *context, unsigned int value,
				int bits, int checksum)
{
	context->bitCount += bits;
	context->bitBuffer = (context->bitBuffer << bits) |
		(value & ((1u << bits) - 1));
	while (context->bitCount > 7) {
		unsigned int byte = context->bitBuffer >> (context->bitCount - 8);

		*context->stream++ = byte;
		if (checksum)
			SbcUpdateCrc(context, byte, 8);
		context->bitCount -= 8;
	}
}

static inline int SbcScaleFactor(const float samples[16][8],
				int blocks, int band)
{
	float maximum;
	unsigned int magnitude;
	int count, block;

	if (blocks <= 0)
		return 0;
	maximum = __builtin_fabsf(samples[0][band]);
	if (maximum <= 0.0f)
		maximum = 0.0f;
	count = blocks > 12 ? 16 : blocks > 8 ? 12 : blocks > 4 ? 8 : 4;
	for (block = 1; block < count; block++) {
		float value = __builtin_fabsf(samples[block][band]);

		if (maximum < value)
			maximum = value;
	}
	magnitude = (unsigned int)maximum;
	return magnitude <= 1 ? 0 : 31 - __builtin_clz(magnitude);
}

int SbcEncodeFrames(SbcContext *context, const uint8_t *pcm,
		    uint8_t *encoded, int frameCount)
{
	context->stream = encoded;
	do {
		int bands = context->subbands;
		int blocks = context->blocks;
		int channels;
		int count = bands * blocks;
		int block, band, channel, sample;
		float *position[2];
		uint8_t *checksum;

		memmove(context->history[0] + count, context->history[0],
			(80 - bands) * sizeof(float));
		blocks = context->blocks;
		bands = context->subbands;
		channels = context->channels;
		count = bands * blocks;
		position[0] = context->history[0] + count;
		if (channels == 2) {
			memmove(context->history[1] + count, context->history[1],
				(80 - bands) * sizeof(float));
			position[1] = context->history[1] + count;
		}
		if (channels != 1 && blocks == 16 && bands == 8) {
			SbcConvertStereo(position[0], position[1],
					 (const int16_t *)pcm, 0.8f);
			pcm += 512;
		} else {
			for (block = 0; block < blocks; block++) {
				for (band = 0; band < bands; band++) {
					int left = (int16_t)(pcm[0] | (pcm[1] << 8));
					float value = (float)left * 0.8f;

					pcm += 2;
					if (channels != 1 || context->stereoInput) {
						int right = (int16_t)(pcm[0] | (pcm[1] << 8));
						float other = (float)right * 0.8f;

						pcm += 2;
						if (channels == 1)
							value = (other + value) * 0.5f;
						else
							*--position[1] = other;
					}
					*--position[0] = value;
				}
			}
		}
		position[0] = context->history[0] + count;
		position[1] = context->history[1] + count;
		if (context->fadeState == 1 || context->fadeState == 3) {
			int progress = context->fadeProgress;

			for (sample = 0; sample < count && progress < 768; sample++) {
				float factor;

				if (context->fadeState == 1)
					factor = (float)(767 - progress) / 768.0f;
				else
					factor = (float)(progress + 1) / 768.0f;
				progress++;
				position[0][-sample - 1] *= factor;
				if (channels == 2)
					position[1][-sample - 1] *= factor;
			}
			context->fadeProgress = progress;
			if (progress >= 768)
				context->fadeState = context->fadeState == 1 ? 2 : 0;
		}
		if (blocks == 16 && channels == 2 && bands == 8) {
			SbcAnalyzeEight(position[0], &context->samples[0][0][0]);
			SbcAnalyzeEight(position[1], &context->samples[1][0][0]);
		} else {
			for (block = 0; block < blocks; block++) {
				position[0] -= bands;
				position[1] -= bands;
				for (channel = 0; channel < channels; channel++) {
					const float *history = position[channel];
					const float *window = bands == 4 ?
						gSbcPrototypeWindowFour : gSbcPrototypeWindow;
					float windowed[16];
					int point, phase;

					windowed[0] = history[4 * bands] * window[4 * bands];
					windowed[0] += history[2 * bands] * window[2 * bands];
					windowed[0] += history[6 * bands] * window[6 * bands];
					windowed[0] += history[8 * bands] * window[8 * bands];
					windowed[0] *= 0.707106769f;
					for (point = 1; point < 2 * bands; point++) {
						if (point == 3 * bands / 2)
							continue;
						windowed[point] =
							history[2 * bands + point] * window[2 * bands + point];
						windowed[point] += history[point] * window[point];
						for (phase = 2; phase < 5; phase++)
							windowed[point] += history[2 * bands * phase + point] *
								window[2 * bands * phase + point];
					}
					windowed[bands] *= 0.707106769f;
					if (bands == 4) {
					context->samples[channel][block][0] =
						((((((windowed[0] + 0.0f) + windowed[1] * 0.923879504f) + windowed[2]) + windowed[3] * 0.923879504f) +
							windowed[4]) + windowed[5] * 0.382683426f) - windowed[7] * 0.382683426f;
					context->samples[channel][block][1] =
						((((((0.0f - windowed[0]) + windowed[1] * 0.382683426f) + windowed[2]) + windowed[3] * 0.382683426f) -
							windowed[4]) - windowed[5] * 0.923879504f) + windowed[7] * 0.923879504f;
					context->samples[channel][block][2] =
						((((((0.0f - windowed[0]) - windowed[1] * 0.382683426f) + windowed[2]) - windowed[3] * 0.382683426f) -
							windowed[4]) + windowed[5] * 0.923879504f) - windowed[7] * 0.923879504f;
					context->samples[channel][block][3] =
						((((((windowed[0] + 0.0f) - windowed[1] * 0.923879504f) + windowed[2]) - windowed[3] * 0.923879504f) +
							windowed[4]) - windowed[5] * 0.382683426f) + windowed[7] * 0.382683426f;

					} else {
					context->samples[channel][block][0] =
						((((((((((((((windowed[0] + 0.0f) + windowed[1] * 0.831469595f) + windowed[2] * 0.923879504f) +
							windowed[3] * 0.980785251f) + windowed[4]) + windowed[5] * 0.980785251f) + windowed[6] * 0.923879504f)
							+ windowed[7] * 0.831469595f) + windowed[8]) + windowed[9] * 0.555570245f) + windowed[10] *
							0.382683426f) + windowed[11] * 0.195090324f) - windowed[13] * 0.195090324f) - windowed[14] *
							0.382683426f) - windowed[15] * 0.555570245f;
					context->samples[channel][block][1] =
						((((((((((((((0.0f - windowed[0]) - windowed[1] * 0.195090324f) + windowed[2] * 0.382683426f) +
							windowed[3] * 0.831469595f) + windowed[4]) + windowed[5] * 0.831469595f) + windowed[6] * 0.382683426f)
							- windowed[7] * 0.195090324f) - windowed[8]) - windowed[9] * 0.980785251f) - windowed[10] *
							0.923879504f) - windowed[11] * 0.555570245f) + windowed[13] * 0.555570245f) + windowed[14] *
							0.923879504f) + windowed[15] * 0.980785251f;
					context->samples[channel][block][2] =
						((((((((((((((0.0f - windowed[0]) - windowed[1] * 0.980785251f) - windowed[2] * 0.382683426f) +
							windowed[3] * 0.555570245f) + windowed[4]) + windowed[5] * 0.555570245f) - windowed[6] * 0.382683426f)
							- windowed[7] * 0.980785251f) - windowed[8]) + windowed[9] * 0.195090324f) + windowed[10] *
							0.923879504f) + windowed[11] * 0.831469595f) - windowed[13] * 0.831469595f) - windowed[14] *
							0.923879504f) - windowed[15] * 0.195090324f;
					context->samples[channel][block][3] =
						((((((((((((((windowed[0] + 0.0f) - windowed[1] * 0.555570245f) - windowed[2] * 0.923879504f) +
							windowed[3] * 0.195090324f) + windowed[4]) + windowed[5] * 0.195090324f) - windowed[6] * 0.923879504f)
							- windowed[7] * 0.555570245f) + windowed[8]) + windowed[9] * 0.831469595f) - windowed[10] *
							0.382683426f) - windowed[11] * 0.980785251f) + windowed[13] * 0.980785251f) + windowed[14] *
							0.382683426f) - windowed[15] * 0.831469595f;
					context->samples[channel][block][4] =
						((((((((((((((windowed[0] + 0.0f) + windowed[1] * 0.555570245f) - windowed[2] * 0.923879504f) -
							windowed[3] * 0.195090324f) + windowed[4]) - windowed[5] * 0.195090324f) - windowed[6] * 0.923879504f)
							+ windowed[7] * 0.555570245f) + windowed[8]) - windowed[9] * 0.831469595f) - windowed[10] *
							0.382683426f) + windowed[11] * 0.980785251f) - windowed[13] * 0.980785251f) + windowed[14] *
							0.382683426f) + windowed[15] * 0.831469595f;
					context->samples[channel][block][5] =
						((((((((((((((0.0f - windowed[0]) + windowed[1] * 0.980785251f) - windowed[2] * 0.382683426f) -
							windowed[3] * 0.555570245f) + windowed[4]) - windowed[5] * 0.555570245f) - windowed[6] * 0.382683426f)
							+ windowed[7] * 0.980785251f) - windowed[8]) - windowed[9] * 0.195090324f) + windowed[10] *
							0.923879504f) - windowed[11] * 0.831469595f) + windowed[13] * 0.831469595f) - windowed[14] *
							0.923879504f) + windowed[15] * 0.195090324f;
					context->samples[channel][block][6] =
						((((((((((((((0.0f - windowed[0]) + windowed[1] * 0.195090324f) + windowed[2] * 0.382683426f) -
							windowed[3] * 0.831469595f) + windowed[4]) - windowed[5] * 0.831469595f) + windowed[6] * 0.382683426f)
							+ windowed[7] * 0.195090324f) - windowed[8]) + windowed[9] * 0.980785251f) - windowed[10] *
							0.923879504f) + windowed[11] * 0.555570245f) - windowed[13] * 0.555570245f) + windowed[14] *
							0.923879504f) - windowed[15] * 0.980785251f;
					context->samples[channel][block][7] =
						((((((((((((((windowed[0] + 0.0f) - windowed[1] * 0.831469595f) + windowed[2] * 0.923879504f) -
							windowed[3] * 0.980785251f) + windowed[4]) - windowed[5] * 0.980785251f) + windowed[6] * 0.923879504f)
							- windowed[7] * 0.831469595f) + windowed[8]) - windowed[9] * 0.555570245f) + windowed[10] *
							0.382683426f) - windowed[11] * 0.195090324f) + windowed[13] * 0.195090324f) - windowed[14] *
							0.382683426f) + windowed[15] * 0.555570245f;

					}
				}
			}
		}
		for (channel = 0; channel < channels; channel++)
			for (band = 0; band < bands; band++)
				context->scaleFactors[channel][band] =
					SbcScaleFactor(context->samples[channel], blocks, band);
		if (context->mode == 3) {
			for (block = 0; block < blocks; block++) {
				for (band = 0; band < bands - 1; band++) {
					float right = context->samples[1][block][band];
					float middle = (right + context->samples[0][block][band]) * 0.5f;

					context->workingSamples[0][block][band] = middle;
					context->workingSamples[1][block][band] = middle - right;
				}
			}
			for (channel = 0; channel < 2; channel++)
				for (band = 0; band < bands - 1; band++)
					context->jointScaleFactors[channel][band] =
						SbcScaleFactor(context->workingSamples[channel], blocks, band);
			context->jointSubbands = 0;
			for (band = 0; band < bands - 1; band++) {
				if (context->jointScaleFactors[0][band] +
				    context->jointScaleFactors[1][band] <
				    context->scaleFactors[0][band] +
				    context->scaleFactors[1][band]) {
					context->jointSubbands |= 1 << (bands - 1 - band);
					for (block = 0; block < blocks; block++) {
						context->samples[0][block][band] =
							context->workingSamples[0][block][band];
						context->samples[1][block][band] =
							context->workingSamples[1][block][band];
					}
					context->scaleFactors[0][band] = context->jointScaleFactors[0][band];
					context->scaleFactors[1][band] = context->jointScaleFactors[1][band];
				}
			}
		}
		SbcAllocateBits(context, 1);
		context->bitCount = 0;
		context->crc = 15;
		SbcWriteBits(context, 0x9c, 8, 0);
		SbcWriteBits(context, (uint8_t)((context->frequency << 6) |
			((context->blocks / 4 - 1) << 4) | (context->mode << 2) |
			(context->allocation << 1) | (context->subbands / 4 - 1)), 8, 1);
		SbcWriteBits(context, context->bitpool, 8, 1);
		context->bitCount = 0;
		checksum = context->stream++;
		if (context->mode == 3)
			SbcWriteBits(context, context->jointSubbands, context->subbands, 1);
		for (channel = 0; channel < context->channels; channel++)
			for (band = 0; band < context->subbands; band++)
				SbcWriteBits(context, context->scaleFactors[channel][band], 4, 1);
		if (context->bitCount)
			SbcUpdateCrc(context, context->bitBuffer << context->bitCount,
				     context->bitCount);
		for (block = 0; block < context->blocks; block++) {
			for (channel = 0; channel < context->channels; channel++) {
				for (band = 0; band < context->subbands; band++) {
					int bits = context->bits[channel][band];

					if (bits) {
						int scale = context->scaleFactors[channel][band];
						unsigned int maximum = (1u << bits) - 1;
						float value = context->samples[channel][block][band];
						unsigned int quantized = (unsigned int)
							(((float)(1 << (scale + 1)) + value) *
							 (float)maximum / (float)(1 << (scale + 2)));

						SbcWriteBits(context, quantized, bits, 0);
					}
				}
			}
		}
		if (context->bitCount)
			SbcWriteBits(context, 0, 8 - context->bitCount, 0);
		*checksum = context->crc;
	} while (frameCount-- > 1);
	return context->stream - encoded;
}
