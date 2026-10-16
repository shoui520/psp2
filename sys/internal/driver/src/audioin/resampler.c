/* Two-stage 48 kHz to 16 kHz capture filter. */
#include "audioin_internal.h"

void initializeAudioInResampler(void)
{
	g_audioInResamplerState[0] = (AudioInResamplerState){ { 0, 0 }, { 0, 0 } };
	g_audioInResamplerState[1] = (AudioInResamplerState){ { 0, 0 }, { 0, 0 } };
}

void resampleAudioInCapture(unsigned int stream, const int16_t *input,
		int16_t *output, unsigned int count, int channel)
{
	const float firstInputGain = 0.13640479743480682f;
	const float firstFeedbackNear = -0.938020646572113f;
	const float firstFeedbackOlder = 0.24445614218711853f;
	const float outputNear = 0.5616288185119629f;
	const float outputMiddle = 1.1232576370239258f;
	const float secondInputGain = 0.1751684695482254f;
	const float secondFeedbackNear = -1.204588532447815f;
	const float secondFeedbackOlder = 0.5981071591377258f;
	AudioInResamplerState *state;
	float firstNear, firstOlder, secondNear, secondOlder;
	float first, second, third, firstOutput, secondOutput;
	float nextSecond, finalSecond;
	int firstSample, secondSample, thirdSample, result;
	int groups = (int)count / 3;

	if (channel > 0)
		input += channel;
	if (!groups)
		return;
	state = &g_audioInResamplerState[stream];
	firstNear = state->firstStageHistory[0];
	firstOlder = state->firstStageHistory[1];
	secondNear = state->secondStageHistory[0];
	secondOlder = state->secondStageHistory[1];
	do {
		firstSample = input[0];
		if (channel < 0)
			firstSample = (firstSample + input[1]) / 2;
		input += 6;
		__builtin_prefetch(input + 12);
		secondSample = input[-4];
		if (channel < 0)
			secondSample = (secondSample + input[-3]) / 2;
		first = ((float)firstSample * firstInputGain -
			firstNear * firstFeedbackNear) - firstOlder * firstFeedbackOlder;
		firstOutput = (first * outputNear + firstNear * outputMiddle) +
			firstOlder * outputNear;
		thirdSample = input[-2];
		if (channel < 0)
			thirdSample = (thirdSample + input[-1]) / 2;
		nextSecond = (firstOutput * secondInputGain -
			secondNear * secondFeedbackNear) - secondOlder * secondFeedbackOlder;
		second = ((float)secondSample * firstInputGain -
			first * firstFeedbackNear) - firstNear * firstFeedbackOlder;
		result = (int)((secondNear * outputMiddle + nextSecond * outputNear) +
			secondOlder * outputNear);
		if (result >= 32768)
			result = 32767;
		else if (result < -32767)
			result = -32767;
		secondOutput = (((first * outputMiddle + second * outputNear) +
			firstNear * outputNear) * secondInputGain -
			nextSecond * secondFeedbackNear) - secondNear * secondFeedbackOlder;
		third = ((float)thirdSample * firstInputGain -
			second * firstFeedbackNear) - first * firstFeedbackOlder;
		finalSecond = ((first * outputNear +
			(second * outputMiddle + third * outputNear)) * secondInputGain -
			secondOutput * secondFeedbackNear) - nextSecond * secondFeedbackOlder;
		firstNear = third;
		firstOlder = second;
		secondNear = finalSecond;
		secondOlder = secondOutput;
		*output++ = result;
	} while (--groups);
	state->firstStageHistory[0] = firstNear;
	state->firstStageHistory[1] = firstOlder;
	state->secondStageHistory[0] = secondNear;
	state->secondStageHistory[1] = secondOlder;
}
