/* SceGpio port and interrupt controls. Private context names are inferred. */
#include "lowio_internal.h"
#include <sce/gpio_regs.h>
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/sysevent.h>

#define GPIO_ERROR_BANK ((int)0x803f0100)
#define GPIO_ERROR_PORT ((int)0x803f0101)
#define GPIO_ERROR_MODE ((int)0x803f0102)

typedef struct GpioContext {
	volatile SceGpioRegisters *base;
	uint32_t portMode;
	uint32_t intrMode[2];
	uint32_t intrEnable[5];
	uint32_t excludedPorts;
	uint32_t edgeMask;
	uint32_t nextPort;
	SceKernelSpinlock lock;
	int uid;
	uint32_t intrCodes[5];
} GpioContext;

_Static_assert(sizeof(GpioContext) == 0x4c, "GPIO context layout");
static GpioContext s_gpio[2];

int ksceGpioPortRead(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t value;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	ctx = &s_gpio[bank];
	if (ctx->portMode & (1U << port))
		value = ctx->base->output;
	else
		value = ctx->base->input;
	return value >> port & 1;
}

int ksceGpioPortSet(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	s_gpio[bank].base->outputSet = 1U << port;
	(void)s_gpio[bank].base->output;
	LowioDsb();
	return 0;
}

int ksceGpioPortClear(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	s_gpio[bank].base->outputClear = 1U << port;
	(void)s_gpio[bank].base->output;
	LowioDsb();
	return 0;
}

int ksceGpioPortReset(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	volatile SceGpioRegisters *base;
	uint32_t state, previous, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	base = ctx->base;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	previous = base->input;
	bit = 1U << port;
	base->outputClear = previous & bit;
	base->outputSet = ~previous & bit;
	(void)base->output;
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return previous >> port & 1;
}

int ksceGpioSetPortMode(unsigned bank, unsigned port, unsigned mode)
{
	GpioContext *ctx;
	volatile SceGpioRegisters *base;
	uint32_t state, previous, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	if (mode >= 2)
		return GPIO_ERROR_MODE;
	ctx = &s_gpio[bank];
	base = ctx->base;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	previous = ctx->portMode;
	bit = 1U << port;
	ctx->portMode = (previous & ~bit) | (mode ^ 1U) << port;
	base->portMode = ctx->portMode;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return (previous & bit) == 0;
}

int ksceGpioGetPortMode(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	return (s_gpio[bank].portMode & (1U << port)) == 0;
}

int ksceGpioSetIntrMode(unsigned bank, unsigned port, unsigned mode)
{
	GpioContext *ctx;
	volatile SceGpioRegisters *base;
	uint32_t state, shift, value, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	if (mode >= 4)
		return GPIO_ERROR_MODE;
	ctx = &s_gpio[bank];
	base = ctx->base;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	shift = port * 2;
	if (port > 15) {
		shift -= 32;
		value = (base->interruptMode[1] & ~(3U << shift)) | mode << shift;
		base->interruptMode[1] = value;
		ctx->intrMode[1] = value;
	} else {
		value = (base->interruptMode[0] & ~(3U << shift)) | mode << shift;
		base->interruptMode[0] = value;
		ctx->intrMode[0] = value;
	}
	LowioDmb();
	bit = 1U << port;
	if (mode == 2 || mode == 3)
		ctx->edgeMask &= ~bit;
	else
		ctx->edgeMask |= bit;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

int ksceGpioGetIntrMode(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t mode;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	if (port < 16)
		mode = ctx->intrMode[0] >> (port * 2);
	else
		mode = ctx->intrMode[1] >> (port * 2 - 32);
	return mode & 3;
}

int ksceGpioQueryIntr(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t pending;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	pending = ctx->base->interruptStatus[0] & ctx->intrEnable[0];
	pending |= ctx->base->interruptStatus[1] & ctx->intrEnable[1];
	pending |= ctx->base->interruptStatus[2] & ctx->intrEnable[2];
	pending |= ctx->base->interruptStatus[3] & ctx->intrEnable[3];
	pending |= ctx->base->interruptStatus[4] & ctx->intrEnable[4];
	return (pending & (1U << port)) != 0;
}

int ksceGpioAcquireIntr(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	volatile SceGpioRegisters *base;
	uint32_t state, pending, bit;
	int ret;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	base = ctx->base;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	bit = 1U << port;
	pending = (base->interruptStatus[0] & ctx->intrEnable[0]) |
		(base->interruptStatus[1] & ctx->intrEnable[1]) |
		(base->interruptStatus[2] & ctx->intrEnable[2]) |
		(base->interruptStatus[3] & ctx->intrEnable[3]) |
		(base->interruptStatus[4] & ctx->intrEnable[4]);
	base->interruptStatus[0] = bit;
	base->interruptStatus[1] = bit;
	base->interruptStatus[2] = bit;
	base->interruptStatus[3] = bit;
	base->interruptStatus[4] = bit;
	ret = (pending & bit) != 0;
	(void)base->interruptStatus[4];
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return ret;
}

int kscePervasiveGpioClockEnable(void);

/* The GPIO block and gate an interrupt code belongs to; -1 for other codes. */
static inline int GpioIntrContext(int intrCode, GpioContext **ctx, unsigned *gate)
{
	if (intrCode > 0xf7) {
		if (intrCode > 0xfc) {
			if (intrCode > 0xfe)
				return -1;
			*ctx = &s_gpio[1];
			*gate = intrCode - 0xfd;
		} else {
			*ctx = &s_gpio[0];
			*gate = intrCode - 0xf8;
		}
	} else {
		if ((unsigned)(intrCode - 0x31) > 2)
			return -1;
		*ctx = &s_gpio[1];
		*gate = intrCode - 0x2f;
	}
	return 0;
}

static int GpioIntrPending(int intrCode)
{
	GpioContext *ctx;
	volatile uint32_t *status;
	int gate = intrCode - 0xf8;

	/* Sony's pending query is unchecked, unlike the option callbacks below. */
	if ((unsigned)gate > 4) {
		ctx = &s_gpio[1];
		gate = intrCode - 0xfd;
		if (gate < 0)
			gate = intrCode - 0x2f;
	} else {
		ctx = &s_gpio[0];
	}
	status = ctx->base->interruptStatus;
	return status[gate] != 0;
}

static int GpioSetSubIntr(int intrCode, unsigned port, int enable)
{
	GpioContext *ctx;
	volatile uint32_t *intrMask;
	unsigned gate;
	uint32_t state, mask;

	if (GpioIntrContext(intrCode, &ctx, &gate) < 0)
		return 0;
	intrMask = ctx->base->interruptMask;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	mask = (enable != 0) << port | (ctx->intrEnable[gate] & ~(1U << port));
	ctx->intrEnable[gate] = mask;
	intrMask[gate] = (ctx->excludedPorts & intrMask[gate]) |
		~(mask | ctx->excludedPorts);
	(void)intrMask[gate];
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static int GpioEnableSubIntr(int intrCode, unsigned port)
{
	GpioContext *ctx;
	volatile uint32_t *intrMask;
	unsigned gate;
	uint32_t state, mask;

	if (GpioIntrContext(intrCode, &ctx, &gate) < 0)
		return 0;
	intrMask = ctx->base->interruptMask;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	mask = ctx->intrEnable[gate] | 1U << port;
	ctx->intrEnable[gate] = mask;
	intrMask[gate] = (ctx->excludedPorts & intrMask[gate]) |
		~(mask | ctx->excludedPorts);
	(void)intrMask[gate];
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static inline __attribute__((always_inline)) int GpioDisableSubIntr(
	int intrCode, unsigned port)
{
	GpioContext *ctx;
	volatile uint32_t *intrMask;
	unsigned gate;
	uint32_t state, mask;

	if (GpioIntrContext(intrCode, &ctx, &gate) < 0)
		return 0;
	intrMask = ctx->base->interruptMask;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	mask = ctx->intrEnable[gate] & ~(1U << port);
	ctx->intrEnable[gate] = mask;
	intrMask[gate] = (ctx->excludedPorts & intrMask[gate]) |
		~(mask | ctx->excludedPorts);
	(void)intrMask[gate];
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static int GpioSuspendSubIntr(int intrCode, unsigned port, uint32_t *previous)
{
	GpioContext *ctx;
	volatile uint32_t *intrMask;
	unsigned gate;
	uint32_t state, mask;

	if (GpioIntrContext(intrCode, &ctx, &gate) < 0)
		return 0;
	intrMask = ctx->base->interruptMask;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	mask = ctx->intrEnable[gate];
	if (previous)
		*previous = mask >> port & 1;
	mask &= ~(1U << port);
	ctx->intrEnable[gate] = mask;
	intrMask[gate] = (ctx->excludedPorts & intrMask[gate]) |
		~(mask | ctx->excludedPorts);
	(void)intrMask[gate];
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static int GpioIntrHandler(int intrCode, void *arg)
{
	GpioContext *ctx = arg;
	volatile uint32_t *status = ctx->base->interruptStatus;
	unsigned start, port;
	uint32_t pending;
	int state, gate;

	state = ksceKernelCpuSuspendIntr();
	if (intrCode > 0xf7) {
		if (intrCode <= 0xfc) {
			gate = intrCode - 0xf8;
		} else {
			if (intrCode > 0xfe)
				goto empty;
			gate = intrCode - 0xfd;
		}
	} else {
		if ((unsigned)(intrCode - 0x31) > 2)
			goto empty;
		gate = intrCode - 0x2f;
	}
	pending = status[gate] & ctx->intrEnable[gate] & ~ctx->excludedPorts;
	if (pending) {
		start = ctx->nextPort;
		ctx->nextPort = (start + 1) & 31;
		port = (start + 31 - __builtin_clz((pending >> start) |
			(pending << (32 - start)))) & 31;
		status[gate] = 1U << port;
		(void)status[gate];
		LowioDsb();
		ksceKernelCpuResumeIntr(state);
		if (ksceKernelTriggerSubIntr(intrCode, port,
			(void *)(uintptr_t)port) == -2)
			GpioDisableSubIntr(intrCode, port);
		return -1;
	}
empty:
	ksceKernelCpuResumeIntr(state);
	return -1;
}

static int GpioSysEvent(int resume, int event, void *args, void *opt)
{
	unsigned bank, gate;
	GpioContext *ctx;

	(void)args;
	(void)opt;
	if (resume != 1)
		return 0;
	if (event == 0x10002) {
		kscePervasiveGpioClockEnable();
		for (bank = 0; bank < 2; bank++) {
			ctx = &s_gpio[bank];
			ctx->portMode = ctx->base->portMode;
			for (gate = 0; gate < 5; gate++)
				ctx->intrEnable[gate] = 0;
			for (gate = 0; gate < 5; gate++)
				ksceKernelEnableIntr(ctx->intrCodes[gate]);
		}
	}
	return 0;
}

/* Raw option layout avoids the SDK's extra callback pointer indirection. */
static const struct {
	uint32_t size;
	void *callbacks[9];
} s_gpioIntrHandlers = {
	0x28, {
		NULL, NULL, NULL, GpioDisableSubIntr, GpioEnableSubIntr,
		GpioDisableSubIntr, GpioSuspendSubIntr, GpioSetSubIntr,
		GpioIntrPending,
	},
};

static SceKernelIntrOptParam s_gpioIntrOpt = {
	0x14, 0x20, (SceKernelIntrOptHandlers *)&s_gpioIntrHandlers, 0, 0,
};

static const struct {
	int code;
	const char *name;
	unsigned priority;
	unsigned targetCpu;
} s_gpioIntrs[2][5] = {
	{
		{ 0xf8, "SceGpio0Gate0", 0x50, 0xf },
		{ 0xf9, "SceGpio0Gate1", 0x70, 0xf },
		{ 0xfa, "SceGpio0Gate2", 0xa0, 0xf },
		{ 0xfb, "SceGpio0Gate3", 0xc0, 0xf },
		{ 0xfc, "SceGpio0Gate4", 0xd0, 0xf },
	}, {
		{ 0xfd, "SceGpio1Gate0", 0x50, 0xf },
		{ 0xfe, "SceGpio1Gate1", 0x70, 0xf },
		{ 0x31, "SceGpio1Gate2", 0xa0, 0xf },
		{ 0x32, "SceGpio1Gate3", 0xc0, 0xf },
		{ 0x33, "SceGpio1Gate4", 0xd0, 0xf },
	},
};

static __attribute__((noinline)) int GpioInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	GpioContext *ctx;
	void *base;
	unsigned bank, gate;
	int ret;

	memset(s_gpio, 0, sizeof(s_gpio));
	for (bank = 0; bank < 2; bank++) {
		ctx = &s_gpio[bank];
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		if (bank) {
			opt.paddr = GPIO1_REG_BASE;
			ret = ksceKernelAllocMemBlock("SceGpio1Reg", 0x20100806,
				0x1000, &opt);
		} else {
			opt.paddr = GPIO0_REG_BASE;
			ret = ksceKernelAllocMemBlock("SceGpio0Reg", 0x20100806,
				0x1000, &opt);
		}
		if (ret < 0)
			return ret;
		ctx->uid = ret;
		ksceKernelGetMemBlockBase(ret, &base);
		ctx->base = base;
		for (gate = 0; gate < 5; gate++) {
			ctx->intrCodes[gate] = s_gpioIntrs[bank][gate].code;
			ksceKernelRegisterIntrHandler(ctx->intrCodes[gate],
				s_gpioIntrs[bank][gate].name, 0, GpioIntrHandler,
				ctx, s_gpioIntrs[bank][gate].priority,
				s_gpioIntrs[bank][gate].targetCpu,
				&s_gpioIntrOpt);
		}
	}
	kscePervasiveGpioClockEnable();
	for (bank = 0; bank < 2; bank++) {
		ctx = &s_gpio[bank];
		ctx->excludedPorts = 0;
		ctx->nextPort = 0;
		ctx->portMode = ctx->base->portMode;
		for (gate = 0; gate < 5; gate++)
			ctx->base->interruptStatus[gate] =
				ctx->base->interruptStatus[gate];
		ctx->intrMode[0] = ctx->base->interruptMode[0];
		ctx->intrMode[1] = ctx->base->interruptMode[1];
		for (gate = 0; gate < 5; gate++)
			ctx->intrEnable[gate] = ~ctx->base->interruptMask[gate];
		for (gate = 0; gate < 5; gate++)
			ksceKernelEnableIntr(ctx->intrCodes[gate]);
	}
	ksceKernelRegisterSysEventHandler("SceGpio", GpioSysEvent, NULL);
	return 0;
}

int LowioGpioInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (GpioInit() < 0)
		return 2;
	return 0;
}
