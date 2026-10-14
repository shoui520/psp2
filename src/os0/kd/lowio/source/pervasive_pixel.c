/* Pervasive DSI pixel clocks. Numeric timing field meanings remain unknown. */
#include "lowio_internal.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>


typedef struct PixelClockSetting {
	uint32_t clock;
	uint32_t unk00;
	uint32_t unk04;
	const uint32_t *aux;
	uint32_t unk0c[3];
	uint32_t timing[4];
} PixelClockSetting;

#include "pixel_clock_table.h"

typedef struct PixelClockContext {
	uint8_t *base;
	const PixelClockSetting *setting;
	uint32_t unk08;
	uint32_t clock;
	uint32_t timing[4];
} PixelClockContext;

_Static_assert(sizeof(PixelClockContext) == 0x20, "pixel clock context");
PixelClockContext s_pixelClock[2];

#define PIXEL_REG(ctx, offset) \
	(*(volatile uint32_t *)((ctx)->base + (offset)))

int LowioPixelClockInit(void)
{
	s_pixelClock[0].setting = NULL;
	s_pixelClock[1].setting = NULL;
	s_pixelClock[0].base = s_baseClock + 0x180;
	s_pixelClock[0].unk08 = PIXEL_REG(&s_pixelClock[0], 0x24);
	s_pixelClock[1].base = s_baseClock + 0x100;
	s_pixelClock[1].unk08 = PIXEL_REG(&s_pixelClock[1], 0x24);
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

uint32_t ScePervasiveForDriver_243D0E78(uint32_t clock,
	uint32_t *first, uint32_t *second)
{
	const PixelClockSetting *setting = PixelClockFind(clock);

	if (!setting)
		return 0x803f0000;
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

uint32_t kscePervasiveDsiSetPixelClock(unsigned head, uint32_t clock)
{
	const PixelClockSetting *setting;
	PixelClockContext *ctx;
	unsigned i;

	if (head >= 2)
		return 0x803f0000;
	ctx = &s_pixelClock[head];
	if (clock == 0) {
		if (ctx->setting) {
			PIXEL_REG(ctx, 8) = 1;
			PIXEL_REG(ctx, 4) = 1;
			PIXEL_REG(ctx, 0) = 1;
			(void)PIXEL_REG(ctx, 0);
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
		return 0x803f0000;
	if (setting == ctx->setting)
		return 0;
	PIXEL_REG(ctx, 8) = 1;
	PIXEL_REG(ctx, 4) = 1;
	(void)PIXEL_REG(ctx, 4);
	LowioDsb();
	if (ctx->unk08 != setting->unk00 || !ctx->setting ||
		ctx->setting->aux != setting->aux) {
		PIXEL_REG(ctx, 0x24) = setting->unk00;
		PIXEL_REG(ctx, 0) = 1;
		(void)PIXEL_REG(ctx, 0);
		LowioDsb();
		if (setting->unk00 & 0x10) {
			for (i = 0; i < 3; i++)
				PIXEL_REG(ctx, 0x30 + i * 4) = setting->aux[i];
			PIXEL_REG(ctx, 0) = 0;
			(void)PIXEL_REG(ctx, 0);
			LowioDsb();
			PixelClockDelay();
		}
	}
	for (i = 0; i < 3; i++)
		PIXEL_REG(ctx, 0x40 + i * 4) = setting->unk0c[i];
	PIXEL_REG(ctx, 0x20) = setting->unk04;
	PIXEL_REG(ctx, 4) = 0;
	(void)PIXEL_REG(ctx, 4);
	LowioDsb();
	PixelClockDelay();
	PIXEL_REG(ctx, 8) = 0;
	(void)PIXEL_REG(ctx, 8);
	ctx->setting = setting;
	ctx->clock = clock;
	for (i = 0; i < 4; i++)
		ctx->timing[i] = setting->timing[i];
	LowioDsb();
	return 0;
}

uint32_t ScePervasiveForDriver_7E813FC5(unsigned head, uint32_t *clock)
{
	if (head >= 2)
		return 0x803f0000;
	*clock = s_pixelClock[head].clock;
	return 0;
}

uint32_t ScePervasiveForDriver_AD1E81EB(unsigned head, unsigned mask);
uint32_t ScePervasiveForDriver_13CC07C9(unsigned head, unsigned mask);

uint32_t ScePervasiveForDriver_4F64DEF6(unsigned head)
{
	uint32_t state;

	ScePervasiveForDriver_4AF7A01E(head, 0xb);
	ScePervasiveForDriver_AD1E81EB(head, 0xb);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head < 3) {
		(void)*(volatile uint32_t *)(s_misc + 0x84 + head * 4);
		*(volatile uint32_t *)(s_misc + 0x84 + head * 4) = 1;
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	ScePervasiveForDriver_13CC07C9(head, 8);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head < 3) {
		volatile uint32_t *reg = (volatile uint32_t *)(s_misc + 0x17c);
		unsigned shift = head * 2;

		*reg = (*reg & ~(3U << shift)) | (2U << shift);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}
