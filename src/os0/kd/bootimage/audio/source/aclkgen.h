#ifndef SCE_AUDIO_ACLKGEN_H
#define SCE_AUDIO_ACLKGEN_H

#include <stdint.h>

/* The audio clock generator's 4 KiB window (SceAclkgenReg). */
#define ACLKGEN_REG_BASE	0xe04dc000
#define ACLKGEN_REG_SIZE	0x1000

struct AudioClockDivider {
	uint16_t sampleRate;
	uint16_t divider;
	uint16_t primaryControl;
	uint16_t alternateControl;
};

struct SceAclkgenChannel {
	volatile uint32_t inputDivider[2];
	volatile uint32_t outputDivider[2];
	volatile uint32_t clockDivider[2];
};

/* Observed register bank in the mapped 4 KiB audio clock window. */
struct SceAclkgenRegisters {
	volatile uint32_t control;
	uint32_t reserved;
	struct SceAclkgenChannel channels[3];
	uint32_t reservedChannels[44];
	volatile uint32_t restartControl;
};

extern const struct AudioClockDivider audioClockDividers[10];

int audioSetClockRates(unsigned int channel, int inputRate, int outputRate,
	int clockRate);

#endif
