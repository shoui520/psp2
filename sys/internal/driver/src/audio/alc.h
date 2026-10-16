/* Automatic level control and its planar sample workspace. */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct AudioAlcParameters {
	int32_t attackCoefficient;
	int32_t releaseCoefficient;
	int32_t upperThreshold;
	int32_t lowerThreshold;
	int16_t upperSlope;
	int16_t lowerSlope;
} AudioAlcParameters;

typedef struct AudioAlc {
	uint32_t state;
	AudioAlcParameters parameters;
	uint32_t initializationValue;
	int32_t inputBits;
	int32_t outputBits;
	int16_t outputGain;
	int16_t outputShift;
	int32_t peak;
	int32_t samples[2][1024];
} AudioAlc;

_Static_assert(sizeof(AudioAlcParameters) == 20, "ALC parameters");
_Static_assert(offsetof(AudioAlc, peak) == 40, "ALC peak");
_Static_assert(offsetof(AudioAlc, samples) == 44, "ALC planar samples");
_Static_assert(sizeof(AudioAlc) == 8236, "ALC workspace");


int audioInitializeAlc(AudioAlc *alc, int inputBits, int outputBits);
int audioProcessAlc(AudioAlc *alc, const void *input, void *output,
		    unsigned int sampleCount);
int32_t *audioResetAlcPeak(int32_t *peak);
void audioProcessAlcSamples(AudioAlc *alc, const void *input, void *output,
			    int sampleCount);
void audioApplyAlcGain(int32_t *left, int32_t *right,
		       const AudioAlcParameters *parameters, int32_t *peakState,
		       int sampleCount);

