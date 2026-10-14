#ifndef SCE_AUDIO_I2S_H
#define SCE_AUDIO_I2S_H

#include <stdint.h>

/* Observed registers and DMA data port in each mapped 4 KiB I2S window. */
struct AudioI2sRegisters {
	uint32_t reserved;
	volatile uint32_t control;
	uint32_t reservedControl[2];
	volatile uint32_t formatControl;
	volatile uint32_t interruptStatus;
	volatile uint32_t clockControl;
	uint32_t reservedClock;
	volatile uint32_t transferMode;
	volatile uint32_t transferEnable;
	uint32_t reservedTransfer[52];
	volatile uint32_t interruptControl;
	uint32_t reservedInterrupt;
	volatile uint32_t data;
};


#endif
