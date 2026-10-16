/* Automatic level control. */

#include "alc.h"


int audioInitializeAlc(AudioAlc *alc, int inputBits, int outputBits)
{
	if (alc == NULL || ((uintptr_t)alc & 3) != 0)
		return -2;
	if ((inputBits != 16) & (inputBits != 32))
		return -4;
	if ((outputBits != 16) & (outputBits != 32))
		return -4;
	alc->initializationValue = 0;
	alc->inputBits = inputBits;
	alc->outputBits = outputBits;
	alc->outputGain = 8152;
	alc->outputShift = 4;
	alc->parameters.upperThreshold = -679905027;
	alc->parameters.lowerThreshold = -710809801;
	alc->parameters.attackCoefficient = 35494782;
	alc->parameters.releaseCoefficient = 2147438909;
	alc->parameters.upperSlope = -19661;
	alc->parameters.lowerSlope = -13926;
	audioResetAlcPeak(&alc->peak);
	alc->state = 1;
	return 0;
}

int audioProcessAlc(AudioAlc *alc, const void *input, void *output,
		    unsigned int sampleCount)
{
	if (alc == NULL || ((uintptr_t)alc & 3) != 0 ||
	    input == NULL || ((uintptr_t)input & 3) != 0 ||
	    output == NULL || ((uintptr_t)output & 3) != 0)
		return -2;
	if (sampleCount > 1024 || (sampleCount & 7) != 0)
		return -3;
	if (alc->state - 1 > 1)
		return -1;
	audioProcessAlcSamples(alc, input, output, sampleCount);
	alc->state = 2;
	return 0;
}

