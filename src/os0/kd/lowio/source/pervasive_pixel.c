/* Pervasive DSI pixel clocks (ScePervasiveBaseClk dsi0PixelClock, dsi1PixelClock). */
#include "lowio_internal.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>


typedef struct PixelClockSetting {
	uint32_t clock;
	uint32_t pllConfig;
	uint32_t source;
	const uint32_t *pllCoefficient;
	uint32_t divider[3];
	uint32_t timing[4];
} PixelClockSetting;

#include "pixel_clock_table.h"

typedef struct PixelClockContext {
	volatile ScePervasivePixelClockRegisters *registers;
	const PixelClockSetting *setting;
	uint32_t pllConfig;
	uint32_t clock;
	uint32_t timing[4];
} PixelClockContext;

_Static_assert(sizeof(PixelClockContext) == 0x20, "pixel clock context");
static PixelClockContext s_pixelClock[2];

int LowioPixelClockInit(void)
{
	s_pixelClock[0].setting = NULL;
	s_pixelClock[1].setting = NULL;
	s_pixelClock[0].registers = &s_baseClock->dsi0PixelClock;
	s_pixelClock[0].pllConfig = s_pixelClock[0].registers->pllConfig;
	s_pixelClock[1].registers = &s_baseClock->dsi1PixelClock;
	s_pixelClock[1].pllConfig = s_pixelClock[1].registers->pllConfig;
	return 0;
}

static const PixelClockSetting *PixelClockFind(uint32_t clock)
{
	unsigned i;

	for (i = 0; i < 20; i++) {
		if (s_pixelSettings[i].clock == clock)
			return &s_pixelSettings[i];
	}
	return NULL;
}

int ScePervasiveForDriver_243D0E78(uint32_t clock,
	uint32_t *first, uint32_t *second)
{
	const PixelClockSetting *setting = PixelClockFind(clock);

	if (!setting)
		return (int)0x803f0000;
	if (first) {
		first[0] = setting->timing[0];
		first[1] = setting->timing[1];
	}
	if (second) {
		second[0] = setting->timing[2];
		second[1] = setting->timing[3];
	}
	return 0;
}

static void PixelClockDelay(void)
{
	uint32_t start;

	if (sceKernelCheckWaitableStatus() < 0) {
		start = ksceKernelGetSystemTimeLow();
		while ((uint32_t)(ksceKernelGetSystemTimeLow() - start) < 1000)
			;
	} else {
		sceKernelDelayThread(1000);
	}
}

int kscePervasiveDsiSetPixelClock(unsigned head, uint32_t clock)
{
	const PixelClockSetting *setting;
	PixelClockContext *ctx;
	unsigned i;

	if (head >= 2)
		return (int)0x803f0000;
	ctx = &s_pixelClock[head];
	if (clock == 0) {
		if (ctx->setting) {
			ctx->registers->outputReset = 1;
			ctx->registers->dividerReset = 1;
			ctx->registers->pllReset = 1;
			(void)ctx->registers->pllReset;
			ctx->setting = NULL;
			ctx->clock = 0;
			for (i = 0; i < 4; i++)
				ctx->timing[i] = 0;
			LowioDsb();
		}
		return 0;
	}
	setting = PixelClockFind(clock);
	if (!setting)
		return (int)0x803f0000;
	if (setting == ctx->setting)
		return 0;
	ctx->registers->outputReset = 1;
	ctx->registers->dividerReset = 1;
	(void)ctx->registers->dividerReset;
	LowioDsb();
	if (ctx->pllConfig != setting->pllConfig || !ctx->setting ||
		ctx->setting->pllCoefficient != setting->pllCoefficient) {
		ctx->registers->pllConfig = setting->pllConfig;
		ctx->registers->pllReset = 1;
		(void)ctx->registers->pllReset;
		LowioDsb();
		if (setting->pllConfig & 0x10) {
			for (i = 0; i < 3; i++)
				ctx->registers->pllCoefficient[i] = setting->pllCoefficient[i];
			ctx->registers->pllReset = 0;
			(void)ctx->registers->pllReset;
			LowioDsb();
			PixelClockDelay();
		}
	}
	for (i = 0; i < 3; i++)
		ctx->registers->divider[i] = setting->divider[i];
	ctx->registers->source = setting->source;
	ctx->registers->dividerReset = 0;
	(void)ctx->registers->dividerReset;
	LowioDsb();
	PixelClockDelay();
	ctx->registers->outputReset = 0;
	(void)ctx->registers->outputReset;
	ctx->setting = setting;
	ctx->clock = clock;
	for (i = 0; i < 4; i++)
		ctx->timing[i] = setting->timing[i];
	LowioDsb();
	return 0;
}

int ScePervasiveForDriver_7E813FC5(unsigned head, uint32_t *clock)
{
	if (head >= 2)
		return (int)0x803f0000;
	*clock = s_pixelClock[head].clock;
	return 0;
}

uint32_t ScePervasiveForDriver_AD1E81EB(unsigned head, unsigned mask);
uint32_t ScePervasiveForDriver_13CC07C9(unsigned head, unsigned mask);

int ScePervasiveForDriver_4F64DEF6(unsigned head)
{
	uint32_t state;

	ScePervasiveForDriver_4AF7A01E(head, 0xb);
	ScePervasiveForDriver_AD1E81EB(head, 0xb);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head < 3) {
		(void)s_misc->usbMode[head];
		s_misc->usbMode[head] = 1;
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	ScePervasiveForDriver_13CC07C9(head, 8);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head < 3) {
		unsigned shift = head * 2;

		s_misc->usbPortControl = (s_misc->usbPortControl & ~(3U << shift)) |
			(2U << shift);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}
