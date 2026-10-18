/* SceI2c transfers. Private helper and context field names are inferred. */
#include "lowio_internal.h"
#include "i2c_regs.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysevent.h>
#include <psp2kern/kernel/threadmgr.h>
#include <oled/oled.h>
#include <lowio/pervasive.h>

#define I2C_ERROR_BUS ((int)0x803f0300)
#define I2C_ERROR_SIZE ((int)0x803f0302)
#define I2C_ERROR_ADDRESS ((int)0x803f0303)
#define I2C_STATUS_MASK 0xfb0fU

typedef struct I2cDebugHandlers {
	uint32_t size;
	void (*writeStart)(unsigned, unsigned, const uint8_t *, unsigned);
	void (*writeEnd)(unsigned, int);
	void (*readStart)(unsigned, unsigned, uint8_t *, unsigned);
	void (*readEnd)(unsigned, int);
	void (*writeReadStart)(unsigned, unsigned, const uint8_t *, unsigned,
		unsigned, uint8_t *, unsigned);
	void (*writeReadEnd)(unsigned, int);
} I2cDebugHandlers;

typedef struct I2cContext {
	SceI2cRegisters *base;
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
static I2cContext s_i2c[2];

int SceI2cForDriver_A2C7CE62(unsigned bus, I2cDebugHandlers *handlers)
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

int SceI2cForDriver_9CF8F3D6(unsigned bus)
{
	SceI2cRegisters *base;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	base = s_i2c[bus].base;
	base->interruptEnable = 0x0100f70f;
	base->writeFifoReset = 1;
	base->readFifoReset = 1;
	base->flags = 7;
	LowioDsb();
	while (base->busy != 0)
		;
	base->interruptStatus = base->interruptStatus;
	base->interruptEnable = 0x01000000;
	sceKernelClearEventFlag(s_i2c[bus].event, 0xffff04f0);
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
	ctx->base->speed =
		(SceSysrootForDriver_930B1342() & 0xff0000) < 0x800000 ? 5 : 4;
	return 0;
}

int SceI2cForDriver_76D277AB(int enable)
{
	if (enable == 0)
		return sceKernelClearEventFlag(s_i2c[1].event, 0x7fffffff);
	return sceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
}

static int I2cWaitReady(I2cContext *ctx)
{
	int ret = sceKernelWaitEventFlag(ctx->event, 0x80000000, 0, NULL, NULL);

	return ret < 0 ? ret : 0;
}

static int I2cBegin(unsigned bus, unsigned address)
{
	I2cContext *ctx = &s_i2c[bus];
	int ret;

	if (bus == 1 && address == 1) {
		ret = I2cWaitReady(ctx);
		if (ret != 0)
			return ret;
	}
	if (ctx->noLock == 0) {
		ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	ctx->busy = 1;
	return 0;
}

static void I2cEnd(I2cContext *ctx)
{
	if (ctx->noLock != 0 || sceKernelUnlockMutex(ctx->mutex, 1) >= 0)
		ctx->busy = 0;
}

static int I2cWait(I2cContext *ctx, uint32_t *status)
{
	return sceKernelWaitEventFlag(ctx->event, I2C_STATUS_MASK,
		SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, status, NULL);
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

int SceI2cForDriver_CA94A759(unsigned bus, unsigned address,
	const uint8_t *buffer, unsigned size)
{
	I2cContext *ctx;
	SceI2cRegisters *base;
	uint32_t status[2];
	unsigned i;
	int ret;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	if (address > 0xff)
		return I2C_ERROR_ADDRESS;
	if (size > 64)
		return I2C_ERROR_SIZE;
	ctx = &s_i2c[bus];
	base = ctx->base;
	ret = I2cBegin(bus, address);
	if (ret < 0)
		return ret;
	status[0] = 0;
	status[1] = 0;
	ctx->txBuffer = buffer;
	ctx->txCount = size;
	ctx->rxBuffer = NULL;
	ctx->rxCount = 0;
	ctx->rxRemaining = 0;
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeStart(bus, address, buffer, size);
	base->writeFifoReset = 1;
	base->readFifoReset = 1;
	base->deviceAddress = address >> 1;
	for (i = 0; i < size; i++)
		base->writeFifo = buffer[i];
	base->flags = size << 8 | 2;
	ret = I2cWait(ctx, &status[0]);
	if (ret >= 0) {
		base->flags = 4;
		ret = I2cWait(ctx, &status[1]);
		if (ret >= 0) {
			ret = (status[0] & I2C_STATUS_MASK) == 1 ?
				I2cStopError(status[1]) : I2cError(status[0], 0x803f0341);
		}
	}
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeEnd(bus, ret);
	I2cEnd(ctx);
	return ret;
}

int SceI2cForDriver_D1D0A9A4(unsigned bus, unsigned address,
	uint8_t *buffer, unsigned size)
{
	I2cContext *ctx;
	SceI2cRegisters *base;
	uint32_t status[2];
	unsigned i;
	int ret;

	if (bus >= 2)
		return I2C_ERROR_BUS;
	if (address > 0xff)
		return I2C_ERROR_ADDRESS;
	if (size - 1 >= 64)
		return I2C_ERROR_SIZE;
	ctx = &s_i2c[bus];
	base = ctx->base;
	ret = I2cBegin(bus, address);
	if (ret < 0)
		return ret;
	status[0] = 0;
	status[1] = 0;
	ctx->txBuffer = NULL;
	ctx->txCount = 0;
	ctx->rxBuffer = buffer;
	ctx->rxCount = size;
	ctx->rxRemaining = size;
	if (ctx->debugHandlers)
		ctx->debugHandlers->readStart(bus, address, buffer, size);
	base->writeFifoReset = 1;
	base->readFifoReset = 1;
	base->deviceAddress = address >> 1;
	base->flags = size << 8 | 0x13;
	ret = I2cWait(ctx, &status[0]);
	if (ret >= 0) {
		for (i = 0; i < size; i++)
			buffer[i] = base->readFifo;
		base->flags = 4;
		ret = I2cWait(ctx, &status[1]);
		if (ret >= 0) {
			ret = (status[0] & I2C_STATUS_MASK) == 1 ?
				I2cStopError(status[1]) : I2cError(status[0], 0x803f0342);
		}
	}
	if (ctx->debugHandlers)
		ctx->debugHandlers->readEnd(bus, ret);
	I2cEnd(ctx);
	return ret;
}

int SceI2cForDriver_0A40B7BF(unsigned bus, unsigned writeAddress,
	const uint8_t *writeBuffer, unsigned writeSize, unsigned readAddress,
	uint8_t *readBuffer, unsigned readSize)
{
	I2cContext *ctx;
	SceI2cRegisters *base;
	uint32_t status[4];
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
	base = ctx->base;
	ret = I2cBegin(bus, writeAddress);
	if (ret < 0)
		return ret;
	status[0] = 0;
	status[1] = 0;
	status[2] = 0;
	status[3] = 0;
	ctx->txBuffer = writeBuffer;
	ctx->txCount = writeSize;
	ctx->rxBuffer = readBuffer;
	ctx->rxCount = readSize;
	ctx->rxRemaining = readSize;
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeReadStart(bus, writeAddress, writeBuffer,
			writeSize, readAddress, readBuffer, readSize);
	base->writeFifoReset = 1;
	base->readFifoReset = 1;
	base->deviceAddress = writeAddress >> 1;
	for (i = 0; i < writeSize; i++)
		base->writeFifo = writeBuffer[i];
	base->flags = writeSize << 8 | 2;
	ret = I2cWait(ctx, &status[0]);
	if (ret < 0)
		goto out;
	if ((status[0] & I2C_STATUS_MASK) == 1) {
		base->flags = 5;
		ret = I2cWait(ctx, &status[1]);
		if (ret < 0)
			goto out;
		if ((status[1] & I2C_STATUS_MASK) == 8) {
			base->flags = readSize << 8 | 0x13;
			ret = I2cWait(ctx, &status[2]);
			if (ret < 0)
				goto out;
			if ((status[2] & I2C_STATUS_MASK) == 1) {
				for (i = 0; i < readSize; i++)
					readBuffer[i] = base->readFifo;
			}
		}
	}
	base->flags = 4;
	ret = I2cWait(ctx, &status[3]);
	if (ret >= 0) {
		if ((status[0] & I2C_STATUS_MASK) != 1)
			ret = I2cError(status[0], 0x803f0341);
		else if ((status[1] & I2C_STATUS_MASK) != 8)
			ret = I2cError(status[1], 0x803f0343);
		else if ((status[2] & I2C_STATUS_MASK) != 1)
			ret = I2cError(status[2], 0x803f0342);
		else
			ret = I2cStopError(status[3]);
	}
out:
	if (ctx->debugHandlers)
		ctx->debugHandlers->writeReadEnd(bus, ret);
	I2cEnd(ctx);
	return ret;
}

static int I2cIntrHandler(int intrCode, void *arg)
{
	I2cContext *ctx = arg;
	uint32_t status = ctx->base->interruptStatus;

	(void)intrCode;
	ctx->base->interruptStatus = status;
	LowioDmb();
	ctx->intrPending |= 1;
	sceKernelSetEventFlag(ctx->event, status);
	return -1;
}

static void I2cHardwareTiming(I2cContext *ctx)
{
	unsigned speed;

	if ((SceSysrootForDriver_930B1342() & 0xff0000) > 0x7fffff)
		speed = 4;
	else
		speed = 5;
	ctx->base->speed = speed;
}

static int I2cSysEvent(int resume, int event, void *args, void *opt)
{
	unsigned bus;

	(void)args;
	(void)opt;
	if (resume) {
		if (resume == 1 && event == 0x10002) {
			for (bus = 0; bus < 2; bus++) {
				ScePervasiveForDriver_86BEA9A4(bus);
				ScePervasiveForDriver_C37CFDFC(bus);
				SceI2cForDriver_9CF8F3D6(bus);
				I2cHardwareTiming(&s_i2c[bus]);
				s_i2c[bus].base->interruptEnable = 0x01000000;
				sceKernelEnableIntr(s_i2c[bus].intrCode);
			}
			sceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
		}
	} else if (event == 0x402) {
		if (s_i2c[0].busy || s_i2c[1].busy)
			return (int)0x803f0340;
	} else if (event == 0x4002) {
		ScePervasiveForDriver_3F3E1AEA(0);
		ScePervasiveForDriver_3F3E1AEA(1);
	}
	return 0;
}

/* name unknown */
static __attribute__((noinline)) int I2cInit(void)
{
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
		if (bus) {
			opt.paddr = I2C_REG_BASE(1);
			ret = SceSysmemForDriver_C94850C9("SceI2c1Reg", 0x20100806,
				0x1000, &opt);
		} else {
			opt.paddr = I2C_REG_BASE(0);
			ret = SceSysmemForDriver_C94850C9("SceI2c0Reg", 0x20100806,
				0x1000, &opt);
		}
		if (ret < 0)
			return ret;
		s_i2c[bus].uid = ret;
		SceSysmemForDriver_A841EDDA(ret, &base);
		s_i2c[bus].base = base;
	}
	s_i2c[0].mutex = sceKernelCreateMutex("SceI2c0", 0, 0, NULL);
	s_i2c[1].mutex = sceKernelCreateMutex("SceI2c1", 0, 0, NULL);
	s_i2c[0].event = sceKernelCreateEventFlag("SceI2c0", 0, 0, NULL);
	s_i2c[1].event = sceKernelCreateEventFlag("SceI2c1", 0, 0, NULL);
	s_i2c[0].intrCode = 0x8e;
	s_i2c[1].intrCode = 0x8f;
	sceKernelRegisterIntrHandler(0x8e, "SceI2c0", 0, I2cIntrHandler,
		&s_i2c[0], 0xa0, 0xf, NULL);
	sceKernelRegisterIntrHandler(0x8f, "SceI2c1", 0, I2cIntrHandler,
		&s_i2c[1], 0xa0, 0xf, NULL);
	sceKernelEnableIntr(0x8e);
	sceKernelEnableIntr(0x8f);
	for (bus = 0; bus < 2; bus++) {
		ctx = &s_i2c[bus];
		ScePervasiveForDriver_86BEA9A4(bus);
		ScePervasiveForDriver_C37CFDFC(bus);
		SceI2cForDriver_9CF8F3D6(bus);
		ctx->timing[0] = 0x8c;
		ctx->timing[1] = 0x46;
		I2cHardwareTiming(ctx);
		ctx->timing[0] = 0xe8;
		ctx->timing[1] = 0x77;
		I2cHardwareTiming(ctx);
		ctx->base->interruptEnable = 0x01000000;
	}
	sceKernelSetEventFlag(s_i2c[1].event, 0x80000000);
	sceKernelRegisterSysEventHandler("SceI2c", I2cSysEvent, NULL);
	return 0;
}

int LowioI2cInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (I2cInit() < 0)
		return 2;
	return 0;
}
