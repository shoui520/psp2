/* SceI2c transfers. Private helper and context field names are inferred. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/sysevent.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sce/oled.h>

#define I2C_ERROR_BUS ((int)0x803f0300)
#define I2C_ERROR_SIZE ((int)0x803f0302)
#define I2C_ERROR_ADDRESS ((int)0x803f0303)
#define I2C_STATUS_MASK 0xfb0fU
#define I2C_REG(ctx, offset) \
	(*(volatile uint32_t *)((ctx)->base + (offset)))

/* The end-hook third word preserves Sony's observable r2 scratch state. */
typedef struct I2cDebugHandlers {
	uint32_t size;
	void (*writeStart)(unsigned, unsigned, const uint8_t *, unsigned);
	void (*writeEnd)(unsigned, int, uint32_t);
	void (*readStart)(unsigned, unsigned, uint8_t *, unsigned);
	void (*readEnd)(unsigned, int, uint32_t);
	void (*writeReadStart)(unsigned, unsigned, const uint8_t *, unsigned,
		unsigned, uint8_t *, unsigned);
	void (*writeReadEnd)(unsigned, int, uint32_t);
} I2cDebugHandlers;

typedef struct I2cContext {
	uint8_t *base;
	const uint8_t *txBuffer;
	uint32_t txCount;
	uint8_t *rxBuffer;
	uint32_t rxCount;
	uint32_t rxRemaining;
	SceUID event;
	SceUID mutex;
	SceUID uid;
	uint32_t intrCode;
	uint32_t noLock;
	uint32_t busy;
	uint32_t intrPending;
	uint32_t timing[2];
	I2cDebugHandlers *debugHandlers;
} I2cContext;

_Static_assert(sizeof(I2cContext) == 0x40, "I2cContext layout");
I2cContext s_i2c[2];

int ksceI2cSetDebugHandlers(unsigned bus, I2cDebugHandlers *handlers)
{
	if (bus >= 2)
		return I2C_ERROR_BUS;
	s_i2c[bus].debugHandlers = handlers;
	return 0;
}

int SceI2cForDriver_E449AC6E(unsigned bus, uint32_t noLock)
{
	if (bus >= 2)
		return I2C_ERROR_BUS;
	s_i2c[bus].noLock = noLock;
	return 0;
}

int ksceI2cInit(unsigned bus)
{
	I2cContext *ctx;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	ctx = &s_i2c[bus];
	I2C_REG(ctx, 0x2c) = 0x0100f70f;
	I2C_REG(ctx, 8) = 1;
	I2C_REG(ctx, 0xc) = 1;
	I2C_REG(ctx, 0x14) = 7;
	LowioDsb();
	while (I2C_REG(ctx, 0x1c) != 0)
		;
	I2C_REG(ctx, 0x28) = I2C_REG(ctx, 0x28);
	I2C_REG(ctx, 0x2c) = 0x01000000;
	ksceKernelClearEventFlag(ctx->event, 0xffff04f0);
	return 0;
}

int SceI2cForDriver_30CF9469(unsigned bus, uint32_t first, uint32_t second)
{
	I2cContext *ctx;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	ctx = &s_i2c[bus];
	ctx->timing[0] = first;
	ctx->timing[1] = second;
	I2C_REG(ctx, 0x18) =
		(ksceSysrootGetHardwareInfo() & 0xff0000) < 0x800000 ? 5 : 4;
	return 0;
}

int ksceI2cReset(int enable)
{
	if (enable == 0)
		return ksceKernelClearEventFlag(s_i2c[1].event, 0x7fffffff);
	return ksceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
}

static int I2cBegin(unsigned bus, unsigned address)
{
	I2cContext *ctx = &s_i2c[bus];
	int ret;

	if (bus == 1 && address == 1) {
		ret = ksceKernelWaitEventFlag(ctx->event, 0x80000000, 0,
			NULL, NULL);
		if (ret < 0)
			return ret;
	}
	if (ctx->noLock == 0) {
		ret = ksceKernelLockMutex(ctx->mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	ctx->busy = 1;
	return 0;
}

static void I2cEnd(I2cContext *ctx)
{
	if (ctx->noLock != 0 || ksceKernelUnlockMutex(ctx->mutex, 1) >= 0)
		ctx->busy = 0;
}

uint64_t I2cWaitWithScratch(SceUID event, uint32_t *status);

static int I2cWait(I2cContext *ctx, uint32_t *status, uint32_t *scratch)
{
	uint64_t result = I2cWaitWithScratch(ctx->event, status);

	*scratch = result >> 32;
	return (int)result;
}

/* Reproduce the discarded register operations exposed by the end hooks.
 * Write/read share a classifier; write-read has distinct intermediate shifts.
 */
static uint32_t I2cErrorScratch(uint32_t status, unsigned shift)
{
	unsigned bit;

	for (bit = 0; bit < 8; bit++) {
		if (status & (0x100U << bit))
			return bit;
	}
	return status << shift;
}

static uint32_t I2cTransferScratch(uint32_t first, uint32_t stop)
{
	if ((first & I2C_STATUS_MASK) != 1) {
		if (first & 2)
			return 2;
		return I2cErrorScratch(first, 17);
	}
	if ((stop & I2C_STATUS_MASK) == 4 || (stop & 2))
		return 1;
	return I2cErrorScratch(stop, 17);
}

static uint32_t I2cWriteReadScratch(uint32_t first, uint32_t restart,
	uint32_t read, uint32_t stop)
{
	unsigned bit;

	if ((first & I2C_STATUS_MASK) != 1) {
		if (first & 2)
			return 2;
		return I2cErrorScratch(first, 18);
	}
	if ((restart & I2C_STATUS_MASK) != 8) {
		if (restart & 2)
			return 1;
		return I2cErrorScratch(restart, 18);
	}
	if ((read & I2C_STATUS_MASK) != 1) {
		if (read & 2)
			return 1;
		return I2cErrorScratch(read, 17);
	}
	if ((stop & I2C_STATUS_MASK) == 4)
		return 4;
	if (stop & 2)
		return 2;
	for (bit = 0; bit < 8; bit++) {
		if (!(stop & (0x100U << bit)))
			continue;
		if (bit < 2)
			return 0;
		return stop << (bit < 6 ? 21 : 17);
	}
	return stop << 17;
}

static int I2cError(uint32_t status, int nack)
{
	unsigned bit;

	if (status & 2)
		return nack;
	for (bit = 0; bit < 8; bit++) {
		if (status & (0x100U << bit))
			return (int)(0x803f0380U + bit);
	}
	return 0;
}

static int I2cStopError(uint32_t status)
{
	if ((status & I2C_STATUS_MASK) == 4 || (status & 2))
		return 0;
	return I2cError(status, 0);
}

int ksceI2cTransferWrite(unsigned bus, unsigned address,
	const uint8_t *buffer, unsigned size)
{
	I2cContext *ctx;
	uint32_t first = 0, stop = 0, scratch = 0;
	unsigned i;
	int ret;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	if (address > 0xff)
		return I2C_ERROR_ADDRESS;
	if (size > 64)
		return I2C_ERROR_SIZE;
	ctx = &s_i2c[bus];
	ret = I2cBegin(bus, address);
	if (ret < 0)
		return ret;
	ctx->txBuffer = buffer;
	ctx->txCount = size;
	ctx->rxBuffer = NULL;
	ctx->rxCount = 0;
	ctx->rxRemaining = 0;
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeStart(bus, address, buffer, size);
	I2C_REG(ctx, 8) = 1;
	I2C_REG(ctx, 0xc) = 1;
	I2C_REG(ctx, 0x10) = address >> 1;
	for (i = 0; i < size; i++)
		I2C_REG(ctx, 0) = buffer[i];
	I2C_REG(ctx, 0x14) = size << 8 | 2;
	ret = I2cWait(ctx, &first, &scratch);
	if (ret >= 0) {
		I2C_REG(ctx, 0x14) = 4;
		ret = I2cWait(ctx, &stop, &scratch);
		if (ret >= 0) {
			ret = (first & I2C_STATUS_MASK) == 1 ?
				I2cStopError(stop) : I2cError(first, 0x803f0341);
			scratch = I2cTransferScratch(first, stop);
		}
	}
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeEnd(bus, ret, scratch);
	I2cEnd(ctx);
	return ret;
}

int ksceI2cTransferRead(unsigned bus, unsigned address,
	uint8_t *buffer, unsigned size)
{
	I2cContext *ctx;
	uint32_t first = 0, stop = 0, scratch = 0;
	unsigned i;
	int ret;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	if (address > 0xff)
		return I2C_ERROR_ADDRESS;
	if (size - 1 >= 64)
		return I2C_ERROR_SIZE;
	ctx = &s_i2c[bus];
	ret = I2cBegin(bus, address);
	if (ret < 0)
		return ret;
	ctx->txBuffer = NULL;
	ctx->txCount = 0;
	ctx->rxBuffer = buffer;
	ctx->rxCount = size;
	ctx->rxRemaining = size;
	if (ctx->debugHandlers)
		ctx->debugHandlers->readStart(bus, address, buffer, size);
	I2C_REG(ctx, 8) = 1;
	I2C_REG(ctx, 0xc) = 1;
	I2C_REG(ctx, 0x10) = address >> 1;
	I2C_REG(ctx, 0x14) = size << 8 | 0x13;
	ret = I2cWait(ctx, &first, &scratch);
	if (ret >= 0) {
		for (i = 0; i < size; i++)
			buffer[i] = I2C_REG(ctx, 4);
		I2C_REG(ctx, 0x14) = 4;
		ret = I2cWait(ctx, &stop, &scratch);
		if (ret >= 0) {
			ret = (first & I2C_STATUS_MASK) == 1 ?
				I2cStopError(stop) : I2cError(first, 0x803f0342);
			scratch = I2cTransferScratch(first, stop);
		}
	}
	if (ctx->debugHandlers)
		ctx->debugHandlers->readEnd(bus, ret, scratch);
	I2cEnd(ctx);
	return ret;
}

int ksceI2cTransferWriteRead(unsigned bus, unsigned writeAddress,
	const uint8_t *writeBuffer, unsigned writeSize, unsigned readAddress,
	uint8_t *readBuffer, unsigned readSize)
{
	I2cContext *ctx;
	uint32_t first = 0, restart = 0, read = 0, stop = 0, scratch = 0;
	unsigned i;
	int ret;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	if (writeAddress > 0xff)
		return I2C_ERROR_ADDRESS;
	if (writeSize - 1 >= 64)
		return I2C_ERROR_SIZE;
	if (readAddress > 0xff)
		return I2C_ERROR_ADDRESS;
	if (readSize - 1 >= 64)
		return I2C_ERROR_SIZE;
	ctx = &s_i2c[bus];
	ret = I2cBegin(bus, writeAddress);
	if (ret < 0)
		return ret;
	ctx->txBuffer = writeBuffer;
	ctx->txCount = writeSize;
	ctx->rxBuffer = readBuffer;
	ctx->rxCount = readSize;
	ctx->rxRemaining = readSize;
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeReadStart(bus, writeAddress, writeBuffer,
			writeSize, readAddress, readBuffer, readSize);
	I2C_REG(ctx, 8) = 1;
	I2C_REG(ctx, 0xc) = 1;
	I2C_REG(ctx, 0x10) = writeAddress >> 1;
	for (i = 0; i < writeSize; i++)
		I2C_REG(ctx, 0) = writeBuffer[i];
	I2C_REG(ctx, 0x14) = writeSize << 8 | 2;
	ret = I2cWait(ctx, &first, &scratch);
	if (ret < 0)
		goto out;
	if ((first & I2C_STATUS_MASK) == 1) {
		I2C_REG(ctx, 0x14) = 5;
		ret = I2cWait(ctx, &restart, &scratch);
		if (ret < 0)
			goto out;
		if ((restart & I2C_STATUS_MASK) == 8) {
			I2C_REG(ctx, 0x14) = readSize << 8 | 0x13;
			ret = I2cWait(ctx, &read, &scratch);
			if (ret < 0)
				goto out;
			if ((read & I2C_STATUS_MASK) == 1) {
				for (i = 0; i < readSize; i++)
					readBuffer[i] = I2C_REG(ctx, 4);
			}
		}
	}
	I2C_REG(ctx, 0x14) = 4;
	ret = I2cWait(ctx, &stop, &scratch);
	if (ret >= 0) {
		if ((first & I2C_STATUS_MASK) != 1)
			ret = I2cError(first, 0x803f0341);
		else if ((restart & I2C_STATUS_MASK) != 8)
			ret = I2cError(restart, 0x803f0343);
		else if ((read & I2C_STATUS_MASK) != 1)
			ret = I2cError(read, 0x803f0342);
		else
			ret = I2cStopError(stop);
		scratch = I2cWriteReadScratch(first, restart, read, stop);
	}
out:
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeReadEnd(bus, ret, scratch);
	I2cEnd(ctx);
	return ret;
}

uint32_t ScePervasiveForDriver_86BEA9A4(unsigned bus);
uint32_t ScePervasiveForDriver_C37CFDFC(unsigned bus);
uint32_t ScePervasiveForDriver_3F3E1AEA(unsigned bus);

static int I2cIntrHandler(int intrCode, void *arg)
{
	I2cContext *ctx = arg;
	uint32_t status = I2C_REG(ctx, 0x28);

	(void)intrCode;
	I2C_REG(ctx, 0x28) = status;
	LowioDmb();
	ctx->intrPending |= 1;
	ksceKernelSetEventFlag(ctx->event, status);
	return -1;
}

static void I2cHardwareTiming(I2cContext *ctx)
{
	I2C_REG(ctx, 0x18) =
		(ksceSysrootGetHardwareInfo() & 0xff0000) < 0x800000 ? 5 : 4;
}

static int I2cSysEvent(int resume, int event, void *args, void *opt)
{
	unsigned bus;

	(void)args;
	(void)opt;
	if (resume == 1 && event == 0x10002) {
		for (bus = 0; bus < 2; bus++) {
			ScePervasiveForDriver_86BEA9A4(bus);
			ScePervasiveForDriver_C37CFDFC(bus);
			ksceI2cInit(bus);
			I2cHardwareTiming(&s_i2c[bus]);
			I2C_REG(&s_i2c[bus], 0x2c) = 0x01000000;
			ksceKernelEnableIntr(s_i2c[bus].intrCode);
		}
		ksceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
	} else if (resume == 0 && event == 0x402) {
		if (s_i2c[0].busy || s_i2c[1].busy)
			return (int)0x803f0340;
	} else if (resume == 0 && event == 0x4002) {
		ScePervasiveForDriver_3F3E1AEA(0);
		ScePervasiveForDriver_3F3E1AEA(1);
	}
	return 0;
}

/* Initialization wrapper in module_start converts negative results to 2. */
int LowioI2cInit(void)
{
	static const char *const names[2] = { "SceI2c0", "SceI2c1" };
	static const char *const regNames[2] = { "SceI2c0Reg", "SceI2c1Reg" };
	SceKernelAllocMemBlockKernelOpt opt;
	I2cContext *ctx;
	void *base;
	unsigned bus;
	int ret;

	memset(s_i2c, 0, sizeof(s_i2c));
	for (bus = 0; bus < 2; bus++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = 0xe0500000 + bus * 0x10000;
		ret = ksceKernelAllocMemBlock(regNames[bus], 0x20100806,
			0x1000, &opt);
		if (ret < 0)
			return ret;
		s_i2c[bus].uid = ret;
		ksceKernelGetMemBlockBase(ret, &base);
		s_i2c[bus].base = base;
	}
	for (bus = 0; bus < 2; bus++)
		s_i2c[bus].mutex = ksceKernelCreateMutex(names[bus], 0, 0, NULL);
	for (bus = 0; bus < 2; bus++)
		s_i2c[bus].event = ksceKernelCreateEventFlag(names[bus], 0, 0, NULL);
	for (bus = 0; bus < 2; bus++) {
		s_i2c[bus].intrCode = 0x8e + bus;
		ksceKernelRegisterIntrHandler(s_i2c[bus].intrCode, names[bus],
			0, I2cIntrHandler, &s_i2c[bus], 0xa0, 0xf, NULL);
	}
	ksceKernelEnableIntr(0x8e);
	ksceKernelEnableIntr(0x8f);
	for (bus = 0; bus < 2; bus++) {
		ctx = &s_i2c[bus];
		ScePervasiveForDriver_86BEA9A4(bus);
		ScePervasiveForDriver_C37CFDFC(bus);
		ksceI2cInit(bus);
		ctx->timing[0] = 0x8c;
		ctx->timing[1] = 0x46;
		I2cHardwareTiming(ctx);
		ctx->timing[0] = 0xe8;
		ctx->timing[1] = 0x77;
		I2cHardwareTiming(ctx);
		I2C_REG(ctx, 0x2c) = 0x01000000;
	}
	ksceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
	ksceKernelRegisterSysEventHandler("SceI2c", I2cSysEvent, NULL);
	return 0;
}
