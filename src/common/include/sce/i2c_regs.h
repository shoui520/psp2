/* I2C bus registers (SceI2c0Reg at 0xe0500000, SceI2c1Reg at 0xe0510000).
 * Names after the henkaku wiki's I2C Registers page where it has them;
 * the FIFO resets, busy and interrupt enable words are named by their use.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct SceI2cRegisters {
	volatile uint32_t writeFifo;
	volatile uint32_t readFifo;
	volatile uint32_t writeFifoReset;	/* 1 before each transfer */
	volatile uint32_t readFifoReset;
	volatile uint32_t deviceAddress;	/* 7-bit address */
	volatile uint32_t flags;		/* count << 8 | transfer bits */
	volatile uint32_t speed;
	volatile uint32_t busy;			/* polled to 0 after a reset */
	volatile uint32_t reserved[2];
	volatile uint32_t interruptStatus;	/* write back to acknowledge */
	volatile uint32_t interruptEnable;
} SceI2cRegisters;

_Static_assert(offsetof(SceI2cRegisters, flags) == 0x14, "I2C flags");
_Static_assert(offsetof(SceI2cRegisters, busy) == 0x1c, "I2C busy");
_Static_assert(offsetof(SceI2cRegisters, interruptStatus) == 0x28, "I2C interrupts");
_Static_assert(offsetof(SceI2cRegisters, interruptEnable) == 0x2c, "I2C interrupt enable");
