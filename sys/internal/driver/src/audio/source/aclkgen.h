#ifndef SCE_AUDIO_ACLKGEN_H
#define SCE_AUDIO_ACLKGEN_H

#include <stdint.h>
#include <sce/audio_regs.h>

struct AudioClockDivider {
	uint16_t sampleRate;
	uint16_t divider;
	uint16_t primaryControl;
	uint16_t alternateControl;
};

extern const struct AudioClockDivider audioClockDividers[10];

int audioSetClockRates(unsigned int channel, int inputRate, int outputRate,
	int clockRate);

#endif
