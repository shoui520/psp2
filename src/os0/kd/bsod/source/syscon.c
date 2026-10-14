/* BSOD polls GPIO/SPI directly after disabling normal interrupt delivery. */
#include "bsod_internal.h"
#include <sce/gpio_regs.h>
#include <sce/spi_regs.h>
#include <sce/barrier.h>
#include <sce/moncall.h>

static inline void bsodDsb(void)
{
	dsb();
}

uint32_t bsodReadButtons(void)
{
	volatile SceGpioRegisters *gpio = (void *)s_bsodSyscon[0];
	volatile SceSpiRegisters *spi = (void *)s_bsodSyscon[1];
	uint16_t low, high;

	gpio->outputClear = 8;
	gpio->interruptStatus[0] = 0x10;
	(void)gpio->interruptStatus[0];
	bsodDsb();
	while (spi->receiveCount)
		(void)spi->receive;
	spi->transmit = 0x100;
	spi->transmit = 0xfd01;
	spi->control = 0;
	spi->enable = 1;
	(void)spi->enable;
	bsodDsb();
	gpio->outputSet = 8;
	bsodDsb();
	while (!(gpio->interruptStatus[0] & 0x10))
		;
	gpio->outputClear = 8;
	gpio->interruptStatus[0] = 0x10;
	(void)gpio->interruptStatus[0];
	bsodDsb();
	(void)spi->receive;
	(void)spi->receive;
	low = spi->receive;
	high = spi->receive;
	while (spi->receiveCount)
		(void)spi->receive;
	spi->enable = 0;
	(void)spi->enable;
	bsodDsb();
	return ~((uint32_t)low | (uint32_t)high << 16) ^ 0x8b000000;
}

void bsodPowerOff(void)
{
	moncall2(MONCALL_SYSCON_SET_POWER_MODE, 0, 0);
}
