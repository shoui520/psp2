/* SceGpio port and interrupt controls. Private context names are inferred. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/sysevent.h>

#define GPIO_ERROR_BANK ((uint32_t)0x803f0100)
#define GPIO_ERROR_PORT ((uint32_t)0x803f0101)
#define GPIO_ERROR_MODE ((uint32_t)0x803f0102)
#define GPIO_REG(ctx, offset) (*(volatile uint32_t *)((ctx)->base + (offset)))

typedef struct GpioContext {
	uint8_t *base;
	uint32_t portMode;
	uint32_t intrMode[2];
	uint32_t intrEnable[5];
	uint32_t unk24;
	uint32_t edgeMask;
	uint32_t nextPort;
	SceKernelSpinlock lock;
	int uid;
	uint32_t intrCodes[5];
} GpioContext;

_Static_assert(sizeof(GpioContext) == 0x4c, "GPIO context layout");
GpioContext s_gpio[2];

/* ARM register shifts use the low byte; counts >= 32 produce zero. */
static uint32_t PortBit(unsigned port)
{
	port &= 0xff;
	return port < 32 ? 1U << port : 0;
}

uint32_t ksceGpioPortRead(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t value;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	ctx = &s_gpio[bank];
	value = GPIO_REG(ctx, ctx->portMode & PortBit(port) ? 0x34 : 4);
	port &= 0xff;
	return port < 32 ? value >> port & 1 : 0;
}

uint32_t ksceGpioPortSet(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	GPIO_REG(&s_gpio[bank], 8) = 1U << port;
	(void)GPIO_REG(&s_gpio[bank], 0x34);
	LowioDsb();
	return 0;
}

uint32_t ksceGpioPortClear(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	GPIO_REG(&s_gpio[bank], 0xc) = 1U << port;
	(void)GPIO_REG(&s_gpio[bank], 0x34);
	LowioDsb();
	return 0;
}

uint32_t ksceGpioPortReset(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t state, previous, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	previous = GPIO_REG(ctx, 4);
	bit = 1U << port;
	GPIO_REG(ctx, 0xc) = previous & bit;
	GPIO_REG(ctx, 8) = ~previous & bit;
	(void)GPIO_REG(ctx, 0x34);
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return previous >> port & 1;
}

uint32_t ksceGpioSetPortMode(unsigned bank, unsigned port, unsigned mode)
{
	GpioContext *ctx;
	uint32_t state, previous, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	if (mode >= 2)
		return GPIO_ERROR_MODE;
	ctx = &s_gpio[bank];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	previous = ctx->portMode;
	bit = 1U << port;
	ctx->portMode = (previous & ~bit) | (mode ^ 1U) << port;
	GPIO_REG(ctx, 0) = ctx->portMode;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return (previous & bit) == 0;
}

uint32_t ksceGpioGetPortMode(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	return (s_gpio[bank].portMode & (1U << port)) == 0;
}

uint32_t ksceGpioSetIntrMode(unsigned bank, unsigned port, unsigned mode)
{
	GpioContext *ctx;
	uint32_t state, shift, value, word, bit;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	if (mode >= 4)
		return GPIO_ERROR_MODE;
	ctx = &s_gpio[bank];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	word = port >> 4;
	shift = (port & 15) * 2;
	value = GPIO_REG(ctx, 0x14 + word * 4);
	value = (value & ~(3U << shift)) | mode << shift;
	GPIO_REG(ctx, 0x14 + word * 4) = value;
	ctx->intrMode[word] = value;
	LowioDmb();
	bit = 1U << port;
	ctx->edgeMask = mode >= 2 ? ctx->edgeMask & ~bit : ctx->edgeMask | bit;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

uint32_t ksceGpioGetIntrMode(unsigned bank, unsigned port)
{
	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	return s_gpio[bank].intrMode[port >> 4] >> ((port & 15) * 2) & 3;
}

uint32_t ksceGpioQueryIntr(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t pending;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	pending = GPIO_REG(ctx, 0x38) & ctx->intrEnable[0];
	pending |= GPIO_REG(ctx, 0x3c) & ctx->intrEnable[1];
	pending |= GPIO_REG(ctx, 0x40) & ctx->intrEnable[2];
	pending |= GPIO_REG(ctx, 0x44) & ctx->intrEnable[3];
	pending |= GPIO_REG(ctx, 0x48) & ctx->intrEnable[4];
	return (pending & (1U << port)) != 0;
}

uint32_t ksceGpioAcquireIntr(unsigned bank, unsigned port)
{
	GpioContext *ctx;
	uint32_t state, pending = 0, bit, status[5];
	unsigned gate;

	if (bank >= 2)
		return GPIO_ERROR_BANK;
	if (port >= 32)
		return GPIO_ERROR_PORT;
	ctx = &s_gpio[bank];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	for (gate = 0; gate < 5; gate++)
		status[gate] = GPIO_REG(ctx, 0x38 + gate * 4);
	bit = 1U << port;
	for (gate = 0; gate < 5; gate++) {
		pending |= status[gate] & ctx->intrEnable[gate];
		GPIO_REG(ctx, 0x38 + gate * 4) = bit;
	}
	(void)GPIO_REG(ctx, 0x48);
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return (pending & bit) != 0;
}

uint32_t kscePervasiveGpioClockEnable(void);

static GpioContext *GpioIntrContext(int intrCode, unsigned *gate)
{
	if (intrCode >= 0xf8 && intrCode < 0xfd) {
		*gate = intrCode - 0xf8;
		return &s_gpio[0];
	}
	if (intrCode >= 0xfd && intrCode <= 0xfe) {
		*gate = intrCode - 0xfd;
		return &s_gpio[1];
	}
	if ((unsigned)(intrCode - 0x31) <= 2) {
		*gate = intrCode - 0x2f;
		return &s_gpio[1];
	}
	return NULL;
}

static int GpioIntrPending(int intrCode)
{
	unsigned gate = (unsigned)intrCode - 0xf8;
	GpioContext *ctx;

	/* Sony's pending query is unchecked, unlike the option callbacks below. */
	if (gate < 5)
		ctx = &s_gpio[0];
	else {
		ctx = &s_gpio[1];
		gate = intrCode >= 0xfd ? intrCode - 0xfd : intrCode - 0x2f;
	}
	return GPIO_REG(ctx, 0x38 + gate * 4) != 0;
}

static int GpioSetSubIntr(int intrCode, unsigned port, int enable)
{
	unsigned gate;
	GpioContext *ctx = GpioIntrContext(intrCode, &gate);
	uint32_t state, mask, previous;

	if (!ctx)
		return 0;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	mask = ctx->intrEnable[gate] & ~PortBit(port);
	if (enable)
		mask |= PortBit(port);
	ctx->intrEnable[gate] = mask;
	previous = GPIO_REG(ctx, 0x1c + gate * 4);
	GPIO_REG(ctx, 0x1c + gate * 4) =
		(ctx->unk24 & previous) | ~(mask | ctx->unk24);
	(void)GPIO_REG(ctx, 0x1c + gate * 4);
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static int GpioEnableSubIntr(int intrCode, unsigned port)
{
	return GpioSetSubIntr(intrCode, port, 1);
}

static int GpioDisableSubIntr(int intrCode, unsigned port)
{
	return GpioSetSubIntr(intrCode, port, 0);
}

static int GpioSuspendSubIntr(int intrCode, unsigned port, uint32_t *previous)
{
	unsigned gate;
	GpioContext *ctx = GpioIntrContext(intrCode, &gate);
	uint32_t state;

	if (!ctx)
		return 0;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	if (previous) {
		unsigned shift = port & 0xff;

		*previous = shift < 32 ? ctx->intrEnable[gate] >> shift & 1 : 0;
	}
	ctx->intrEnable[gate] &= ~PortBit(port);
	GPIO_REG(ctx, 0x1c + gate * 4) =
		(ctx->unk24 & GPIO_REG(ctx, 0x1c + gate * 4)) |
		~(ctx->intrEnable[gate] | ctx->unk24);
	(void)GPIO_REG(ctx, 0x1c + gate * 4);
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, state);
	return 0;
}

static int GpioIntrHandler(int intrCode, void *arg)
{
	GpioContext *ctx = arg;
	unsigned gate, port, start;
	uint32_t state, pending, rotated;

	state = ksceKernelCpuSuspendIntr();
	if (!GpioIntrContext(intrCode, &gate))
		goto empty;
	pending = GPIO_REG(ctx, 0x38 + gate * 4) &
		ctx->intrEnable[gate] & ~ctx->unk24;
	if (!pending)
		goto empty;
	start = ctx->nextPort;
	ctx->nextPort = (start + 1) & 31;
	start &= 31;
	rotated = start ? (pending >> start) | (pending << (32 - start)) :
		pending;
	port = (start + 31 - __builtin_clz(rotated)) & 31;
	GPIO_REG(ctx, 0x38 + gate * 4) = 1U << port;
	(void)GPIO_REG(ctx, 0x38 + gate * 4);
	LowioDsb();
	ksceKernelCpuResumeIntr(state);
	if (ksceKernelTriggerSubIntr(intrCode, port, (void *)(uintptr_t)port) == -2)
		GpioDisableSubIntr(intrCode, port);
	return -1;
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
	if (resume == 1 && event == 0x10002) {
		kscePervasiveGpioClockEnable();
		for (bank = 0; bank < 2; bank++) {
			ctx = &s_gpio[bank];
			ctx->portMode = GPIO_REG(ctx, 0);
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
} s_gpioIntrs[2][5] = {
	{
		{ 0xf8, "SceGpio0Gate0", 0x50 },
		{ 0xf9, "SceGpio0Gate1", 0x70 },
		{ 0xfa, "SceGpio0Gate2", 0xa0 },
		{ 0xfb, "SceGpio0Gate3", 0xc0 },
		{ 0xfc, "SceGpio0Gate4", 0xd0 },
	}, {
		{ 0xfd, "SceGpio1Gate0", 0x50 },
		{ 0xfe, "SceGpio1Gate1", 0x70 },
		{ 0x31, "SceGpio1Gate2", 0xa0 },
		{ 0x32, "SceGpio1Gate3", 0xc0 },
		{ 0x33, "SceGpio1Gate4", 0xd0 },
	},
};

int LowioGpioInit(void)
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
		opt.paddr = bank ? 0xe0100000 : 0xe20a0000;
		ret = ksceKernelAllocMemBlock(bank ? "SceGpio1Reg" : "SceGpio0Reg",
			0x20100806, 0x1000, &opt);
		if (ret < 0)
			return ret;
		ctx->uid = ret;
		ksceKernelGetMemBlockBase(ret, &base);
		ctx->base = base;
		for (gate = 0; gate < 5; gate++) {
			ctx->intrCodes[gate] = s_gpioIntrs[bank][gate].code;
			ksceKernelRegisterIntrHandler(ctx->intrCodes[gate],
				s_gpioIntrs[bank][gate].name, 0, GpioIntrHandler,
				ctx, s_gpioIntrs[bank][gate].priority, 0xf,
				&s_gpioIntrOpt);
		}
	}
	kscePervasiveGpioClockEnable();
	for (bank = 0; bank < 2; bank++) {
		ctx = &s_gpio[bank];
		ctx->unk24 = 0;
		ctx->nextPort = 0;
		ctx->portMode = GPIO_REG(ctx, 0);
		for (gate = 0; gate < 5; gate++)
			GPIO_REG(ctx, 0x38 + gate * 4) =
				GPIO_REG(ctx, 0x38 + gate * 4);
		ctx->intrMode[0] = GPIO_REG(ctx, 0x14);
		ctx->intrMode[1] = GPIO_REG(ctx, 0x18);
		for (gate = 0; gate < 5; gate++)
			ctx->intrEnable[gate] = ~GPIO_REG(ctx, 0x1c + gate * 4);
		for (gate = 0; gate < 5; gate++)
			ksceKernelEnableIntr(ctx->intrCodes[gate]);
	}
	ksceKernelRegisterSysEventHandler("SceGpio", GpioSysEvent, NULL);
	return 0;
}
