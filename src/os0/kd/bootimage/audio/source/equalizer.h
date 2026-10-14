/* Seven-band stereo equalizer and its planar sample workspace. */

#pragma once

#include <stddef.h>
#include "biquad.h"

typedef struct AudioEqualizer {
	uint32_t state;
	AudioBiquadCoefficients coefficients[7];
	AudioBiquadState history[7][2];
	int32_t samples[2][1024];
} AudioEqualizer;

_Static_assert(offsetof(AudioEqualizer, history) == 144, "equalizer history");
_Static_assert(offsetof(AudioEqualizer, samples) == 368, "equalizer samples");
_Static_assert(sizeof(AudioEqualizer) == 8560, "equalizer workspace");

extern const AudioBiquadCoefficients audioEqualizerCoefficients[7][7];

int audioInitializeEqualizer(AudioEqualizer *equalizer);
int audioSetEqualizer(AudioEqualizer *equalizer, unsigned int setting);
int audioProcessEqualizer(AudioEqualizer *equalizer, const int16_t *input,
			  int16_t *output, unsigned int sampleCount);
int32_t *audioResetEqualizerHistory(AudioEqualizer *equalizer);
void audioProcessEqualizerSamples(AudioEqualizer *equalizer,
				 const int16_t *input, int16_t *output,
				 int sampleCount);

