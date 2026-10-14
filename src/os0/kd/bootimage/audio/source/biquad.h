/* Fixed-point second-order filter coefficients and delay history. */

#pragma once

#include <stdint.h>

typedef struct AudioBiquadCoefficients {
	int32_t b0;
	int32_t b1;
	int32_t b2;
	int32_t feedback1;
	int32_t feedback2;
} AudioBiquadCoefficients;

typedef struct AudioBiquadState {
	int32_t previousInput;
	int32_t secondPreviousInput;
	int32_t previousOutput;
	int32_t secondPreviousOutput;
} AudioBiquadState;

_Static_assert(sizeof(AudioBiquadCoefficients) == 20, "biquad coefficients");
_Static_assert(sizeof(AudioBiquadState) == 16, "biquad history");

int32_t *audioBiquadProcess(int32_t *samples, int count,
			   const AudioBiquadCoefficients *coefficients,
			   AudioBiquadState *state);
