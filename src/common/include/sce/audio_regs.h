/*
 * The audio block's registers: the I2S ports, the sample-rate
 * converter/mixers, the S/PDIF transmitter and the audio clock generator.
 * SceAudio maps them (the block names are the memory blocks it allocates:
 * SceI2s<n>Reg, SceSrcMix<n>Reg, SceSpdifReg, SceAclkgenReg); no source
 * names the registers inside, so those names are descriptive.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* the I2S ports' 4 KiB windows; there is no port 6 */
#define I2S0_REG_BASE		0xe0420000
#define I2S1_REG_BASE		0xe0430000
#define I2S2_REG_BASE		0xe0440000
#define I2S3_REG_BASE		0xe0450000
#define I2S4_REG_BASE		0xe0470000
#define I2S5_REG_BASE		0xe0460000
#define I2S7_REG_BASE		0xe0490000
#define I2S_REG_SIZE		0x1000

typedef struct SceI2sRegisters {
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
	volatile uint32_t data;			/* 0x100: the DMA data port */
} SceI2sRegisters;

_Static_assert(offsetof(SceI2sRegisters, data) == 0x100, "I2S data port");

/* the three sample-rate converter/mixers' 4 KiB windows */
#define SRCMIX_REG_STRIDE	0x10000
#define SRCMIX_REG_BASE(n)	(0xe04a0000 + (n) * SRCMIX_REG_STRIDE)
#define SRCMIX_REG_SIZE		0x1000
/* SrcMix0's mixed output, which the capture DMA reads */
#define SRCMIX0_OUTPUT_DATA	(SRCMIX_REG_BASE(0) + 0x240)

typedef struct SceSrcMixRegisters {
	volatile uint32_t command;
	uint32_t reserved[31];
	volatile uint32_t control;		/* 0x80 */
	volatile uint32_t status;
	volatile uint32_t volumeLeft;
	volatile uint32_t volumeRight;
	volatile uint32_t transmitControl;
	volatile uint32_t secondTransmitControl;
	volatile uint32_t captureControl;
	volatile uint32_t auxiliaryTransmitControl;
} SceSrcMixRegisters;

_Static_assert(offsetof(SceSrcMixRegisters, control) == 0x80, "SrcMix control");

/* the S/PDIF transmitter's 16 KiB window, and the data port past it that
 * the output DMA writes */
#define SPDIF_REG_BASE		0xe04d0000
#define SPDIF_REG_SIZE		0x4000
#define SPDIF_DATA		(SPDIF_REG_BASE + 0x8000)

typedef struct SceSpdifRegisters {
	volatile uint32_t control;
	uint32_t reservedControl[6];
	volatile uint32_t outputEnable;
	uint32_t reservedOutput[2040];
	volatile uint32_t transmitControl;	/* 0x2000 */
	uint32_t reservedTransmit[1023];
	volatile uint32_t clockControl;		/* 0x3000 */
	uint32_t reservedClock[127];
	volatile uint32_t configurationControl;	/* 0x3200 */
	volatile uint32_t configuration[18];
} SceSpdifRegisters;

_Static_assert(offsetof(SceSpdifRegisters, transmitControl) == 0x2000, "S/PDIF transmit");
_Static_assert(offsetof(SceSpdifRegisters, configurationControl) == 0x3200, "S/PDIF configuration");

/* the audio clock generator's 4 KiB window */
#define ACLKGEN_REG_BASE	0xe04dc000
#define ACLKGEN_REG_SIZE	0x1000

typedef struct SceAclkgenChannel {
	volatile uint32_t inputDivider[2];
	volatile uint32_t outputDivider[2];
	volatile uint32_t clockDivider[2];
} SceAclkgenChannel;

typedef struct SceAclkgenRegisters {
	volatile uint32_t control;
	uint32_t reserved;
	SceAclkgenChannel channels[3];
	uint32_t reservedChannels[44];
	volatile uint32_t restartControl;	/* 0x100 */
} SceAclkgenRegisters;

_Static_assert(offsetof(SceAclkgenRegisters, restartControl) == 0x100, "Aclkgen restart");
