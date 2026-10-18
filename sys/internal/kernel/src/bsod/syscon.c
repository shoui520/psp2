/* BSOD maps GPIO0 and SPI0 and polls them directly after disabling normal interrupt delivery. */
#include "bsod_internal.h"
#include <lowio/gpio_regs.h>
#include <lowio/spi_regs.h>
#include <sysmem/barrier.h>
#include <excpmgr/moncall.h>

uint8_t *s_bsodSyscon[2];

#if FW_DEBUG
static const SceKernelDebugInfo s_sysconAssert = {
	0x0f48c989, 0x80b93d37, 0x91de3a7a,
	"syscon.c", 92, "_sceKernelBsodSysconInit",
};
#endif

int _sceKernelBsodSysconInit(void)
{
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	int uid;

	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 2;
	options.paddr = GPIO0_REG_BASE;
	uid = sceKernelAllocMemBlockForKernel("SceBsodGpio0", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				    0x1000, &options);
	if (uid < 0)
		goto error;
	sceKernelGetMemBlockBase(uid, &base);
	s_bsodSyscon[0] = base;

	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 2;
	options.paddr = SPI_REG_BASE(0);
	uid = sceKernelAllocMemBlockForKernel("SceBsodSpi0", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				    0x1000, &options);
	if (uid < 0)
		goto error;
	sceKernelGetMemBlockBase(uid, &base);
	s_bsodSyscon[1] = base;
	return 0;

error:
#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0)
		SceDebugForDriver_FD753E7A(1, 0, &s_sysconAssert, __builtin_return_address(0));
#endif
	return uid;
}

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
