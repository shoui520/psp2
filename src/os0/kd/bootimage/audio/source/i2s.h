#ifndef SCE_AUDIO_I2S_H
#define SCE_AUDIO_I2S_H

#include <stddef.h>
#include <stdint.h>

/* The I2S ports' 4 KiB register windows (SceI2s<n>Reg); there is no port 6. */
#define I2S0_REG_BASE		0xe0420000
#define I2S1_REG_BASE		0xe0430000
#define I2S2_REG_BASE		0xe0440000
#define I2S3_REG_BASE		0xe0450000
#define I2S4_REG_BASE		0xe0470000
#define I2S5_REG_BASE		0xe0460000
#define I2S7_REG_BASE		0xe0490000
#define I2S_REG_SIZE		0x1000

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

_Static_assert(offsetof(struct AudioI2sRegisters, data) == 0x100, "I2S data port");


#endif
