/* GPIO registers (SceGpio0Reg/SceGpio1Reg); names after the Vita wiki. */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* SceGpio0Reg (Sysmem maps it as SceLedReg) and SceGpio1Reg */
#define GPIO0_REG_BASE			0xe20a0000
#define GPIO1_REG_BASE			0xe0100000

typedef struct SceGpioRegisters {
	uint32_t portMode;
	uint32_t input;
	uint32_t outputSet;
	uint32_t outputClear;
	uint32_t enable;			/* 0x10 */
	uint32_t interruptMode[2];		/* 2 bits per port */
	uint32_t interruptMask[5];		/* per gate, 1 disables */
	uint32_t reserved;
	uint32_t output;
	uint32_t interruptStatus[5];
} SceGpioRegisters;

_Static_assert(offsetof(SceGpioRegisters, outputClear) == 0xc, "GPIO clear");
_Static_assert(offsetof(SceGpioRegisters, output) == 0x34, "GPIO output");
_Static_assert(offsetof(SceGpioRegisters, interruptStatus) == 0x38, "GPIO status");
