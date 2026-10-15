/*
 * SPI controller registers (SceSpi0Reg: SceSyscon, SceSpi1Reg: SceMotionDev,
 * SceSpi2Reg: SceOled/SceLcd); names after the Vita wiki's SPI_* registers.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define SPI_INT_RX_NOT_EMPTY	0x200

/* SceSpi<n>Reg physical addresses */
#define SPI_REG_BASE(n)		(0xe0a00000 + (n) * 0x10000)

typedef struct SceSpiRegisters {
	uint32_t receive;			/* SPI_RXFIFO */
	uint32_t transmit;			/* SPI_TXFIFO */
	uint32_t control;			/* SPI_CTL */
	uint32_t intControl;			/* SPI_INTCTL */
	uint32_t enable;			/* SPI_STATUS: 1 starts a transfer */
	uint32_t dmaControl;			/* SPI_DMACTL */
	uint32_t transferControl;
	uint32_t reserved;
	uint32_t mode;				/* cleared at init */
	uint32_t intStatus;			/* SPI_INT_STATUS, write 1 to clear */
	uint32_t receiveCount;			/* SPI_RXFIFO_STATUS */
	uint32_t transmitCount;			/* SPI_TXFIFO_STATUS */
} SceSpiRegisters;

_Static_assert(offsetof(SceSpiRegisters, enable) == 0x10, "SPI enable");
_Static_assert(offsetof(SceSpiRegisters, receiveCount) == 0x28, "SPI RX count");
