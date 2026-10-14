#include "audio_work.h"
/* Seven-band stereo equalizer. */

#include "equalizer.h"


int audioInitializeEqualizer(AudioEqualizer *equalizer)
{
	if (equalizer == NULL)
		return -2;
	if (((uintptr_t)equalizer & 3) != 0)
		return -2;
	equalizer->coefficients[0] = audioEqualizerCoefficients[0][3];
	equalizer->coefficients[1] = audioEqualizerCoefficients[1][3];
	equalizer->coefficients[2] = audioEqualizerCoefficients[2][3];
	equalizer->coefficients[3] = audioEqualizerCoefficients[3][3];
	equalizer->coefficients[4] = audioEqualizerCoefficients[4][3];
	equalizer->coefficients[5] = audioEqualizerCoefficients[5][3];
	equalizer->coefficients[6] = audioEqualizerCoefficients[6][3];
	audioResetEqualizerHistory(equalizer);
	equalizer->state = 1;
	return 0;
}

int audioSetEqualizer(AudioEqualizer *equalizer, unsigned int setting)
{
	int bandLevels[7];

	if (equalizer == NULL || ((uintptr_t)equalizer & 3) != 0)
		return -2;
	bandLevels[0] = setting & 15;
	if (bandLevels[0] > 6)
		return -4;
	bandLevels[1] = (setting >> 4) & 15;
	if (bandLevels[1] > 6)
		return -4;
	bandLevels[2] = (setting >> 8) & 15;
	if (bandLevels[2] > 6)
		return -4;
	bandLevels[3] = (setting >> 12) & 15;
	if (bandLevels[3] > 6)
		return -4;
	bandLevels[4] = (setting >> 16) & 15;
	if (bandLevels[4] > 6)
		return -4;
	bandLevels[5] = (setting >> 20) & 15;
	if (bandLevels[5] > 6)
		return -4;
	bandLevels[6] = (setting >> 24) & 15;
	if (bandLevels[6] > 6)
		return -4;
	equalizer->coefficients[0] = audioEqualizerCoefficients[0][bandLevels[0]];
	equalizer->coefficients[1] = audioEqualizerCoefficients[1][bandLevels[1]];
	equalizer->coefficients[2] = audioEqualizerCoefficients[2][bandLevels[2]];
	equalizer->coefficients[3] = audioEqualizerCoefficients[3][bandLevels[3]];
	equalizer->coefficients[4] = audioEqualizerCoefficients[4][bandLevels[4]];
	equalizer->coefficients[5] = audioEqualizerCoefficients[5][bandLevels[5]];
	equalizer->coefficients[6] = audioEqualizerCoefficients[6][bandLevels[6]];
	return 0;
}

int audioProcessEqualizer(AudioEqualizer *equalizer, const int16_t *input,
			  int16_t *output, unsigned int sampleCount)
{
	if (equalizer == NULL || ((uintptr_t)equalizer & 3) != 0 ||
	    input == NULL || output == NULL)
		return -2;
	if (sampleCount > 1024)
		return -3;
	if (equalizer->state - 1 > 1)
		return -1;
	if (sampleCount != 0)
		audioProcessEqualizerSamples(equalizer, input, output, sampleCount);
	equalizer->state = 2;
	return 0;
}

