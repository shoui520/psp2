#include "audio_work.h"
/* Fixed-point automatic level control processing. */

#include "alc.h"
#include "fixed_point.h"

const int32_t audioAlcLogarithms[12] = {
	0, -186064426, -372130999, -558195425,
	-744261998, -930326424, -1116390849, -1302457422,
	-1488521848, -1674588421, -1860652847, -2046717273
};

const int32_t audioAlcExponentials[8] = {
	2147483647, 790015084, 290630307, 106916914,
	39332535, 14469631, 5323079, 1958252
};

void audioApplyAlcGain(int32_t *left, int32_t *right,
		       const AudioAlcParameters *parameters, int32_t *peakState,
		       int sampleCount)
{
	int32_t peak = *peakState;
	int blockCount = sampleCount >> 3;
	int block;

	for (block = 0; block < blockCount; ++block) {
		int32_t maximum;
		int32_t delta;
		int32_t mantissa;
		int32_t fraction;
		int32_t square;
		int32_t logarithm;
		int32_t gainArgument;
		int32_t polynomial;
		uint32_t gain;
		int logarithmIndex;
		int exponentIndex;
		int index;

		__builtin_prefetch(left + 15);
		__builtin_prefetch(right + 15);
		maximum = right[0] < 0 ? (int32_t)(0U - (uint32_t)right[0]) : right[0];
		for (index = 0; index < 8; ++index) {
			int32_t value = left[index];
			int32_t magnitude = value < 0 ?
				(int32_t)(0U - (uint32_t)value) : value;

			if (maximum < magnitude)
				maximum = magnitude;
			value = right[index];
			magnitude = value < 0 ?
				(int32_t)(0U - (uint32_t)value) : value;
			if (maximum < magnitude)
				maximum = magnitude;
		}
		delta = (uint32_t)maximum - (uint32_t)peak;
		if (delta > 0)
			peak = (uint32_t)peak +
				(MUL_HIGH(delta, parameters->attackCoefficient) << 1);
		else
			peak = MUL_HIGH(peak, parameters->releaseCoefficient) << 1;
		mantissa = peak;
		logarithmIndex = 0;
		if (peak <= 0x3fffffff) {
			for (logarithmIndex = 1; logarithmIndex <= 10; ++logarithmIndex) {
				mantissa = (uint32_t)peak << logarithmIndex;
				if (mantissa > 0x3fffffff)
					break;
			}
		}
		fraction = (uint32_t)mantissa + 0x80000000U;
		if (logarithmIndex == 11)
			fraction = -1;
		square = MUL_HIGH(fraction, fraction) << 1;
		polynomial = MUL_HIGH(square, fraction) << 1;
		polynomial = MUL_HIGH(polynomial, 715827882) << 1;
		polynomial = (uint32_t)polynomial + (uint32_t)fraction -
			(MUL_HIGH(square, 0x40000000) << 1);
		logarithm = (uint32_t)audioAlcLogarithms[logarithmIndex] +
			(uint32_t)(polynomial >> 3);
		if (parameters->upperThreshold < logarithm) {
			delta = (uint32_t)logarithm -
				(uint32_t)parameters->upperThreshold;
			gainArgument = (uint32_t)((int64_t)delta *
				parameters->upperSlope >> 16) << 1;
		} else if (parameters->lowerThreshold <= logarithm) {
			gain = 0x7fff;
			goto applyGain;
		} else {
			delta = (uint32_t)parameters->lowerThreshold -
				(uint32_t)logarithm;
			gainArgument = (uint32_t)((int64_t)delta *
				parameters->lowerSlope >> 16) << 1;
		}
		for (exponentIndex = 0; exponentIndex <= 6; ++exponentIndex) {
			fraction = (uint32_t)gainArgument +
				((uint32_t)exponentIndex << 28);
			if (fraction >= -134217728)
				break;
		}
		if (exponentIndex == 7)
			fraction = -1073741824;
		else
			fraction = (uint32_t)fraction << 3;
		polynomial = (MUL_HIGH(fraction, 178956970) << 1) + 0x20000000U;
		polynomial = (MUL_HIGH(polynomial, fraction) << 1) + 0x40000000U;
		polynomial = MUL_HIGH(polynomial, fraction) << 2;
		gain = ((MUL_HIGH(polynomial, audioAlcExponentials[exponentIndex]) << 1) +
			(uint32_t)audioAlcExponentials[exponentIndex]) >> 16;
	applyGain:
		for (index = 0; index < 8; ++index) {
			left[index] = (uint32_t)((int64_t)left[index] *
				(int16_t)gain >> 16) << 1;
			right[index] = (uint32_t)((int64_t)right[index] *
				(int16_t)gain >> 16) << 1;
		}
		left += 8;
		right += 8;
	}
	*peakState = peak;
}

int32_t *audioResetAlcPeak(int32_t *peak)
{
	*peak = 0;
	return peak;
}

void audioProcessAlcSamples(AudioAlc *alc, const void *input, void *output,
			    int sampleCount)
{
	int32_t *left = alc->samples[0];
	int32_t *right = alc->samples[1];
	int index = 0;
	int outputBits;

	if (alc->inputBits == 16) {
		const int16_t *source = input;

		if (sampleCount > 8) {
			int chunkEnd = ((sampleCount - 9) & ~7) + 8;

			do {
				int left0 = source[0];
				int right0 = source[1];
				int left1 = source[2];
				int right1 = source[3];
				int left2 = source[4];
				int right2 = source[5];
				int left3 = source[6];
				int right3 = source[7];
				int left4 = source[8];
				int right4 = source[9];
				int left5 = source[10];
				int right5 = source[11];
				int left6 = source[12];
				int right6 = source[13];
				int left7 = source[14];
				int right7 = source[15];

				__builtin_prefetch(left + index + 16);
				__builtin_prefetch(source + 33);
				left[index] = (uint32_t)left0 << 16;
				right[index] = (uint32_t)right0 << 16;
				left[index + 1] = (uint32_t)left1 << 16;
				right[index + 1] = (uint32_t)right1 << 16;
				left[index + 2] = (uint32_t)left2 << 16;
				right[index + 2] = (uint32_t)right2 << 16;
				left[index + 3] = (uint32_t)left3 << 16;
				right[index + 3] = (uint32_t)right3 << 16;
				left[index + 4] = (uint32_t)left4 << 16;
				right[index + 4] = (uint32_t)right4 << 16;
				left[index + 5] = (uint32_t)left5 << 16;
				right[index + 5] = (uint32_t)right5 << 16;
				left[index + 6] = (uint32_t)left6 << 16;
				right[index + 6] = (uint32_t)right6 << 16;
				left[index + 7] = (uint32_t)left7 << 16;
				right[index + 7] = (uint32_t)right7 << 16;
				source += 16;
				index += 8;
			} while (index != chunkEnd);
		}
		for (; index < sampleCount; ++index) {
			int leftSample = source[0];
			int rightSample = source[1];

			left[index] = (uint32_t)leftSample << 16;
			right[index] = (uint32_t)rightSample << 16;
			source += 2;
		}
	} else if (alc->inputBits == 32) {
		const int32_t *source = input;

		for (index = 0; index < sampleCount; ++index) {
			left[index] = source[0];
			right[index] = source[1];
			source += 2;
		}
	}
	audioApplyAlcGain(left, right, &alc->parameters, &alc->peak, sampleCount);
	outputBits = alc->outputBits;
	if (outputBits == 16 || outputBits == 32) {
		int gain = alc->outputGain;
		unsigned int shift = (alc->outputShift + 1) & 255;
		int channel;

		for (index = 0; index < sampleCount; ++index) {
			for (channel = 0; channel < 2; ++channel) {
				int32_t product = (int64_t)alc->samples[channel][index] *
					gain >> 16;
				int32_t scaled = shift < 32 ? (uint32_t)product << shift : 0;
				int32_t recovered = shift < 32 ? scaled >> shift : scaled >> 31;

				if (product != recovered)
					scaled = (uint32_t)(product >> 31) ^ 0x7fffffffU;
				if (outputBits == 16) {
					int16_t *destination = output;

					if (scaled > 0x7fff7fff)
						scaled = 0x7fffffff;
					else
						scaled += 0x8000;
					destination[2 * index + channel] = scaled >> 16;
				} else {
					int32_t *destination = output;

					destination[2 * index + channel] = scaled;
				}
			}
		}
	}
}

