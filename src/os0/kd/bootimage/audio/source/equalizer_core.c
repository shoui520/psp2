/* Fixed-point equalizer processing. */

#include <arm_neon.h>
#include "equalizer.h"

void audioProcessEqualizerSamples(AudioEqualizer *equalizer,
				 const int16_t *input, int16_t *output,
				 int sampleCount)
{
	int32_t *left = equalizer->samples[0];
	int32_t *right = equalizer->samples[1];
	int index = 0;

	if (sampleCount > 8) {
		int chunkEnd = ((sampleCount - 9) & ~7) + 8;

		do {
			int left0 = input[0];
			int right0 = input[1];
			int left1 = input[2];
			int right1 = input[3];
			int left2 = input[4];
			int right2 = input[5];
			int left3 = input[6];
			int right3 = input[7];
			int left4 = input[8];
			int right4 = input[9];
			int left5 = input[10];
			int right5 = input[11];
			int left6 = input[12];
			int right6 = input[13];
			int left7 = input[14];
			int right7 = input[15];

			__builtin_prefetch(left + index + 16);
			__builtin_prefetch(input + 33);
			left[index] = (uint32_t)left0 << 15;
			right[index] = (uint32_t)right0 << 15;
			left[index + 1] = (uint32_t)left1 << 15;
			right[index + 1] = (uint32_t)right1 << 15;
			left[index + 2] = (uint32_t)left2 << 15;
			right[index + 2] = (uint32_t)right2 << 15;
			left[index + 3] = (uint32_t)left3 << 15;
			right[index + 3] = (uint32_t)right3 << 15;
			left[index + 4] = (uint32_t)left4 << 15;
			right[index + 4] = (uint32_t)right4 << 15;
			left[index + 5] = (uint32_t)left5 << 15;
			right[index + 5] = (uint32_t)right5 << 15;
			left[index + 6] = (uint32_t)left6 << 15;
			right[index + 6] = (uint32_t)right6 << 15;
			left[index + 7] = (uint32_t)left7 << 15;
			right[index + 7] = (uint32_t)right7 << 15;
			input += 16;
			index += 8;
		} while (index != chunkEnd);
	}
	for (; index < sampleCount; ++index) {
		int leftSample = input[0];
		int rightSample = input[1];

		left[index] = (uint32_t)leftSample << 15;
		right[index] = (uint32_t)rightSample << 15;
		input += 2;
	}
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[0],
			   &equalizer->history[0][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[0],
			   &equalizer->history[0][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[1],
			   &equalizer->history[1][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[1],
			   &equalizer->history[1][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[2],
			   &equalizer->history[2][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[2],
			   &equalizer->history[2][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[3],
			   &equalizer->history[3][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[3],
			   &equalizer->history[3][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[4],
			   &equalizer->history[4][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[4],
			   &equalizer->history[4][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[5],
			   &equalizer->history[5][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[5],
			   &equalizer->history[5][1]);
	audioBiquadProcess(left, sampleCount, &equalizer->coefficients[6],
			   &equalizer->history[6][0]);
	audioBiquadProcess(right, sampleCount, &equalizer->coefficients[6],
			   &equalizer->history[6][1]);
	for (index = 0; index < sampleCount; ++index) {
		int sample = (int32_t)((uint32_t)left[index] + 0x4000) >> 15;
		int rightSample;

		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		rightSample = right[index];
		output[0] = sample;
		sample = (int32_t)((uint32_t)rightSample + 0x4000) >> 15;
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		output[1] = sample;
		output += 2;
	}
}

int32_t *audioResetEqualizerHistory(AudioEqualizer *equalizer)
{
	int32x2_t zero = vdup_n_s32(0);

	vst1_s32(&equalizer->history[0][0].previousInput, zero);
	vst1_s32(&equalizer->history[0][0].previousOutput, zero);
	vst1_s32(&equalizer->history[0][1].previousInput, zero);
	vst1_s32(&equalizer->history[0][1].previousOutput, zero);
	vst1_s32(&equalizer->history[1][0].previousInput, zero);
	vst1_s32(&equalizer->history[1][0].previousOutput, zero);
	vst1_s32(&equalizer->history[1][1].previousInput, zero);
	vst1_s32(&equalizer->history[1][1].previousOutput, zero);
	vst1_s32(&equalizer->history[2][0].previousInput, zero);
	vst1_s32(&equalizer->history[2][0].previousOutput, zero);
	vst1_s32(&equalizer->history[2][1].previousInput, zero);
	vst1_s32(&equalizer->history[2][1].previousOutput, zero);
	vst1_s32(&equalizer->history[3][0].previousInput, zero);
	vst1_s32(&equalizer->history[3][0].previousOutput, zero);
	vst1_s32(&equalizer->history[3][1].previousInput, zero);
	vst1_s32(&equalizer->history[3][1].previousOutput, zero);
	vst1_s32(&equalizer->history[4][0].previousInput, zero);
	vst1_s32(&equalizer->history[4][0].previousOutput, zero);
	vst1_s32(&equalizer->history[4][1].previousInput, zero);
	vst1_s32(&equalizer->history[4][1].previousOutput, zero);
	vst1_s32(&equalizer->history[5][0].previousInput, zero);
	vst1_s32(&equalizer->history[5][0].previousOutput, zero);
	vst1_s32(&equalizer->history[5][1].previousInput, zero);
	vst1_s32(&equalizer->history[5][1].previousOutput, zero);
	vst1_s32(&equalizer->history[6][0].previousInput, zero);
	vst1_s32(&equalizer->history[6][0].previousOutput, zero);
	vst1_s32(&equalizer->history[6][1].previousInput, zero);
	vst1_s32(&equalizer->history[6][1].previousOutput, zero);
	return &equalizer->history[6][1].previousOutput;
}
