/* Pervasive DSI pixel clocks (ScePervasiveBaseClk dsi0PixelClock, dsi1PixelClock). */
#include "lowio_internal.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>
#include <sce/pervasive_pixel.h>


typedef struct PixelClockSetting {
	uint32_t pllConfig;
	uint32_t source;
	const uint32_t *pllCoefficient;
	uint32_t divider[3];
	double frequency;
	double pixelFrequency;
} PixelClockSetting;

typedef struct PixelClockEntry {
	uint32_t clock;
	const PixelClockSetting *setting;
} PixelClockEntry;

#include "pixel_clock_table.h"

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
		if (s_pixelClocks[i].clock == clock)
			return s_pixelClocks[i].setting;
	}
	return NULL;
}

int ScePervasiveForDriver_243D0E78(uint32_t clock,
	double *frequency, double *pixelFrequency)
{
	const PixelClockSetting *setting = PixelClockFind(clock);

	if (!setting)
		return (int)0x803f0000;
	if (frequency)
		*frequency = setting->frequency;
	if (pixelFrequency)
		*pixelFrequency = setting->pixelFrequency;
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
	volatile ScePervasivePixelClockRegisters *regs;
	const PixelClockSetting *setting, *current;
	PixelClockContext *ctx;
	uint32_t pllConfig;
	unsigned i;

	if (head >= 2)
		return (int)0x803f0000;
	if (clock == 0) {
		ctx = &s_pixelClock[head];
		regs = ctx->registers;
		if (ctx->setting) {
			regs->outputReset = 1;
			regs->dividerReset = 1;
			regs->pllReset = 1;
			(void)regs->pllReset;
			ctx->setting = NULL;
			ctx->clock = 0;
			ctx->frequency = 0;
			ctx->pixelFrequency = 0;
			LowioDsb();
		}
		return 0;
	}
	setting = PixelClockFind(clock);
	if (!setting)
		return (int)0x803f0000;
	ctx = &s_pixelClock[head];
	regs = ctx->registers;
	current = ctx->setting;
	if (setting == current)
		return 0;
	regs->outputReset = 1;
	regs->dividerReset = 1;
	(void)regs->dividerReset;
	LowioDsb();
	pllConfig = setting->pllConfig;
	if (ctx->pllConfig != pllConfig || !current ||
		current->pllCoefficient != setting->pllCoefficient) {
		regs->pllConfig = pllConfig;
		regs->pllReset = 1;
		(void)regs->pllReset;
		LowioDsb();
		if (pllConfig & 0x10) {
			const uint32_t *coefficient = setting->pllCoefficient;
			uint32_t c0 = coefficient[0], c1 = coefficient[1], c2 = coefficient[2];

			regs->pllCoefficient[0] = c0;
			regs->pllCoefficient[1] = c1;
			regs->pllCoefficient[2] = c2;
			regs->pllReset = 0;
			(void)regs->pllReset;
			LowioDsb();
			PixelClockDelay();
		}
	}
	for (i = 0; i < 3; i++)
		regs->divider[i] = setting->divider[i];
	regs->source = setting->source;
	regs->dividerReset = 0;
	(void)regs->dividerReset;
	LowioDsb();
	PixelClockDelay();
	regs->outputReset = 0;
	(void)regs->outputReset;
	ctx->setting = setting;
	ctx->clock = clock;
	ctx->frequency = setting->frequency;
	ctx->pixelFrequency = setting->pixelFrequency;
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
	if (head == 0) {
		(void)s_misc->usbMode[0];
		s_misc->usbMode[0] = 1;
		LowioDmb();
	} else if (head == 1) {
		(void)s_misc->usbMode[1];
		s_misc->usbMode[1] = 1;
		LowioDmb();
	} else if (head == 2) {
		(void)s_misc->usbMode[2];
		s_misc->usbMode[2] = 1;
		LowioDmb();
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	ScePervasiveForDriver_13CC07C9(head, 8);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head == 0)
		s_misc->usbPortControl = (s_misc->usbPortControl & ~3U) | 2;
	else if (head == 1)
		s_misc->usbPortControl = (s_misc->usbPortControl & ~0xcU) | 8;
	else if (head == 2)
		s_misc->usbPortControl = (s_misc->usbPortControl & ~0x30U) | 0x20;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}
