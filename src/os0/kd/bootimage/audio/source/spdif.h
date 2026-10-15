#ifndef SCE_AUDIO_SPDIF_H
#define SCE_AUDIO_SPDIF_H

#include <stdint.h>

/* The S/PDIF transmitter's 16 KiB window (SceSpdifReg), and the data port
 * past it that the output DMA writes. */
#define SPDIF_REG_BASE		0xe04d0000
#define SPDIF_REG_SIZE		0x4000
#define SPDIF_DATA		(SPDIF_REG_BASE + 0x8000)

/* Observed register banks in the mapped 16 KiB S/PDIF window. */
struct AudioSpdifRegisters {
	volatile uint32_t control;
	uint32_t reservedControl[6];
	volatile uint32_t outputEnable;
	uint32_t reservedOutput[2040];
	volatile uint32_t transmitControl;
	uint32_t reservedTransmit[1023];
	volatile uint32_t clockControl;
	uint32_t reservedClock[127];
	volatile uint32_t configurationControl;
	volatile uint32_t configuration[18];
};


#endif
