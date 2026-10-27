/*
 * SceLowio's Pervasive driver, second file (see pervasive.c): the clock
 * transitions, the base clock and Misc register controls, the DSI pixel
 * clocks and the port configuration, in the original's order.
 */
#include "lowio_internal.h"
#include <threadmgr/threadmgr.h>
#include <lowio/pervasive.h>
#include <lowio/pervasive_pixel.h>
#include "uart_clock_table.h"
#include "clock_transition_table.h"

#if !LOWIO_KBL
static int ClockIndex(unsigned mode)
{
	if (mode < 2)
		return (int)mode - 1;
	if (mode < 11)
		return (int)mode - 2;
	return (int)mode - 3;
}

int ScePervasiveForDriver_E9D95643(unsigned mode, unsigned speed)
{
	uint32_t raw, high;
	unsigned current;
	uint16_t path;
	int state;

	if (mode > 15 || mode == 2 ||
#if FW_DEBUG
	    mode == 9 ||
#endif
	    mode == 11 || mode == 15 || speed - 8 > 8)
		return PERVASIVE_ERROR;
	if (mode == 0) {
		BASECLK_REGS->armDivisor = 16 - speed;
		LowioDmb();
		return 0;
	}
	state = PervasiveLock();
	raw = BASECLK_REGS->armMultiplier;
	current = raw & 15;
	if (current != mode) {
		/*
		 * Sony's code indexes the row before the table when the current
		 * mode is 0 (ClockIndex(0) is -1) and steps through whatever
		 * precedes it in rodata.
		 */
		path = s_clockTransition[ClockIndex(current)][ClockIndex(mode)];
		high = raw & ~15U;
		while (path) {
			LowioDmb();
			BASECLK_REGS->armMultiplier = high | (path & 15);
			LowioDmb();
			path >>= 4;
		}
	}
	LowioDmb();
	BASECLK_REGS->armDivisor = 16 - speed;
	LowioDmb();
	PervasiveUnlock(state);
	return 0;
}
#endif

static uint32_t WriteClock(volatile uint32_t *reg, uint32_t value)
{
	*reg = value;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_6AF0196F(unsigned first, unsigned second)
{
	if (first >= 8 || second >= 8)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->gpuClock, second | first << 16);
}

#if !LOWIO_KBL
uint32_t ScePervasiveForDriver_725B4F59(unsigned value)
{
	if (value - 1 >= 7)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->veneziaClock, value);
}

uint32_t ScePervasiveForDriver_12D17D47(unsigned value)
{
	if (value - 1 >= 6)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->vipClock, value);
}

uint32_t ScePervasiveForDriver_C0F7CC39(uint32_t level)
{
	return Smc114(0, level);
}

uint32_t ScePervasiveForDriver_09C35980(void)
{
	return Smc114(1, 0);
}

uint32_t scePervasiveCameraBusClockSelect(unsigned value)
{
	if (value - 1 >= 5)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->cameraBusClock, value);
}

uint32_t ScePervasiveForDriver_925D9D24(unsigned value)
{
	if (value > 1)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->audioClock, value != 0 ? 0x10001 : 0x10000);
}

uint32_t ScePervasiveForDriver_59B7CA34(unsigned source, unsigned divider)
{
	uint32_t state, previous, value;

	if (source == 0) {
		value = divider - 1;
		if (value >= 16)
			return PERVASIVE_ERROR;
	} else if (source == 1) {
		if (divider != 1)
			return PERVASIVE_ERROR;
		value = 0;
	} else {
		return PERVASIVE_ERROR;
	}
	state = PervasiveLock();
	previous = BASECLK_REGS->auxClockEnable;
	if (previous) {
		BASECLK_REGS->auxClockEnable = 0;
		LowioDmb();
	}
	BASECLK_REGS->auxClockDivider = value;
	BASECLK_REGS->auxClockSource = source;
	if (previous)
		BASECLK_REGS->auxClockEnable = previous;
	LowioDmb();
	PervasiveUnlock(state);
	return 0;
}

uint32_t ScePervasiveForDriver_8847825F(void)
{
	ScePervasiveBaseClkRegisters *base = BASECLK_REGS;
	uint32_t state, previous;

	state = PervasiveLock();
	previous = base->auxClockEnable;
	base->auxClockEnable = 1;
	LowioDmb();
	(void)base->auxClockEnable;
	LowioDsb();
	PervasiveUnlock(state);
	return previous != 0;
}

uint32_t ScePervasiveForDriver_9E81E057(void)
{
	ScePervasiveBaseClkRegisters *base = BASECLK_REGS;
	uint32_t state, previous;

	state = PervasiveLock();
	previous = base->auxClockEnable;
	base->auxClockEnable = 0;
	LowioDmb();
	PervasiveUnlock(state);
	return previous != 0;
}

uint32_t scePervasiveCecClockOutEnable(void)
{
	return WriteClock(&BASECLK_REGS->cecClockOut, 1);
}

uint32_t scePervasiveCecClockOutDisable(void)
{
	return WriteClock(&BASECLK_REGS->cecClockOut, 0);
}

uint32_t ScePervasiveForDriver_64ABE589(unsigned value)
{
	if ((value & ~4U) >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->msifClock, (value & 3) | (value & 4 ? 0x10000 : 0));
}

uint32_t ScePervasiveForDriver_7D02215D(uint32_t level)
{
	return Smc114(2, level);
}

uint32_t ScePervasiveForDriver_ECA675E0(void)
{
	return Smc114(3, 0);
}

uint32_t ScePervasiveForDriver_A6832B33(unsigned value)
{
	if (value - 1 >= 4)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->dmac5Clock, value);
}

uint32_t ScePervasiveForDriver_2FB5F88F(unsigned value)
{
	if (value >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->srcClock, value);
}

uint32_t ScePervasiveForDriver_98D8914A(unsigned value)
{
	if (value >= 2)
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->sysClock, value);
}
#endif

/* DSI pixel clocks (ScePervasiveBaseClk dsi0PixelClock, dsi1PixelClock). */
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
	s_pixelClock[0].registers = &BASECLK_REGS->dsi0PixelClock;
	s_pixelClock[0].pllConfig = s_pixelClock[0].registers->pllConfig;
	s_pixelClock[1].registers = &BASECLK_REGS->dsi1PixelClock;
	s_pixelClock[1].pllConfig = s_pixelClock[1].registers->pllConfig;
	return 0;
}

#if !LOWIO_KBL
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
		start = SceThreadmgrForDriver_47F6DE49();
		while ((uint32_t)(SceThreadmgrForDriver_47F6DE49() - start) < 1000)
			;
	} else {
		sceKernelDelayThread(1000);
	}
}

int ScePervasiveForDriver_91C80C41(unsigned head, uint32_t clock)
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

uint32_t ScePervasiveForDriver_A96642E3(unsigned value)
{
	if (value <= 1)
		value |= 2;
	else if (value == 0xff)
		value = 0;
	else
		return PERVASIVE_ERROR;
	return WriteClock(&BASECLK_REGS->gatedClockSelect, value);
}
#endif

#if !LOWIO_SKBL
uint32_t ScePervasiveForDriver_453BD2EC(unsigned port, unsigned enable)
{
	ScePervasiveMiscRegisters *misc = MISC_REGS;
	uint32_t state, shift;

	if (port >= 4)
		return PERVASIVE_ERROR;
	if (enable >= 2)
		return PERVASIVE_ERROR;
	state = PervasiveLock();
	shift = port;
	misc->sdifVoltage = (misc->sdifVoltage & ~(1U << shift)) |
		enable << shift;
	LowioDmb();
	PervasiveUnlock(state);
	return 0;
}

uint32_t ScePervasiveForDriver_043B33F5(unsigned port, unsigned mode)
{
	ScePervasiveMiscRegisters *misc = MISC_REGS;
	uint32_t state, shift;

	if (port >= 4)
		return PERVASIVE_ERROR;
	if (mode >= 7)
		return PERVASIVE_ERROR;
	state = PervasiveLock();
	shift = port * 8;
	misc->sdifMode = (misc->sdifMode & ~(7U << shift)) |
		mode << shift;
	LowioDmb();
	PervasiveUnlock(state);
	return 0;
}
#endif

#if !LOWIO_KBL
static uint32_t SetMiscBits(volatile uint32_t *reg, uint32_t mask,
	unsigned enable)
{
	uint32_t previous = *reg;

	*reg = enable ? previous | mask : previous & ~mask;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_5E20D7D5(void)
{
	return SetMiscBits(&MISC_REGS->gameCard, 1, 1);
}

uint32_t ScePervasiveForDriver_E6728E30(void)
{
	return SetMiscBits(&MISC_REGS->gameCard, 1, 0);
}
#endif

#if !LOWIO_SKBL
uint32_t ScePervasiveForDriver_9F8E589C(unsigned port, uint32_t value)
{
	if (port >= 4 || (value & ~0x700U) != 0)
		return PERVASIVE_ERROR;
	MISC_REGS->sdifConfig[port] = value;
	LowioDmb();
	return 0;
}
#endif

#if !LOWIO_KBL
uint32_t ScePervasiveForDriver_8BAB45F8(unsigned port, unsigned baud)
{
	unsigned i;

	if (port >= 7)
		return PERVASIVE_ERROR;
	for (i = 0; i < sizeof(s_uartClockTable) / sizeof(s_uartClockTable[0]); i++) {
		if (s_uartClockTable[i].baud != baud)
			continue;
		s_uartClock->port[port] = s_uartClockTable[i].value;
		LowioDmb();
		return 0;
	}
	return PERVASIVE_ERROR;
}

uint32_t ScePervasiveForDriver_34537B12(void)
{
	MISC_REGS->uart5PullDownRx &= ~SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	MISC_REGS->uart5PullDownTx &= ~SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_4B99558F(void)
{
	MISC_REGS->uart5PullDownRx |= SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	MISC_REGS->uart5PullDownTx |= SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	LowioDmb();
	return 0;
}
#endif

#if LOWIO_SKBL
/* the secure devices' power-on */
__attribute__((noinline)) int CompatRamPowerOn(void)
{
	BASECLK_REGS->compatClock = 0;
	LowioDmb();
	CompatRamResetEnable();
	CompatRamGateEnable();
	CompatRamIsolationDisable();
	CompatRamBusIdle();
	CompatRamGateDisable();
	CompatRamResetDisable();
	CompatRamGateEnable();
	return 0;
}

__attribute__((noinline)) int SonyRegbusPowerOn(void)
{
	SonyRegbusIsolationDisable();
	SonyRegbusBusIdle();
	BASECLK_REGS->allegrexBusClockEnable = 0;
	LowioDmb();
	SonyRegbusGateEnable();
	SonyRegbusResetDisable();
	return 0;
}
#endif

/* name unknown: part of the clock setup at start and on resume; does nothing */
int PervasiveMiscInit(void)
{
	return 0;
}

#if !LOWIO_KBL
uint32_t ScePervasiveForDriver_6B2BE37E(unsigned port, uint32_t value)
{
	uint32_t intr, ret;

	intr = PervasiveLock();
	switch (port) {
	case 0:
		ret = MiscReplace(&MISC_REGS->usbMode[0], value & 1);
		break;
	case 1:
		ret = MiscReplace(&MISC_REGS->usbMode[1], value & 1);
		break;
	case 2:
		ret = MiscReplace(&MISC_REGS->usbMode[2], value & 1);
		break;
	default:
		ret = PERVASIVE_ERROR;
		break;
	}
	PervasiveUnlock(intr);
	return ret;
}

int ScePervasiveForDriver_4F64DEF6(unsigned head)
{
	uint32_t state;

	ScePervasiveForDriver_4AF7A01E(head, 0xb);
	ScePervasiveForDriver_AD1E81EB(head, 0xb);
	state = PervasiveLock();
	if (head == 0) {
		(void)MISC_REGS->usbMode[0];
		MISC_REGS->usbMode[0] = 1;
		LowioDmb();
	} else if (head == 1) {
		(void)MISC_REGS->usbMode[1];
		MISC_REGS->usbMode[1] = 1;
		LowioDmb();
	} else if (head == 2) {
		(void)MISC_REGS->usbMode[2];
		MISC_REGS->usbMode[2] = 1;
		LowioDmb();
	}
	PervasiveUnlock(state);
	ScePervasiveForDriver_13CC07C9(head, 8);
	state = PervasiveLock();
	if (head == 0)
		MISC_REGS->usbPortControl = (MISC_REGS->usbPortControl & ~3U) | 2;
	else if (head == 1)
		MISC_REGS->usbPortControl = (MISC_REGS->usbPortControl & ~0xcU) | 8;
	else if (head == 2)
		MISC_REGS->usbPortControl = (MISC_REGS->usbPortControl & ~0x30U) | 0x20;
	PervasiveUnlock(state);
	return 1;
}

int ScePervasiveForDriver_BB67EF58(unsigned head)
{
	int state;

	/* Reset-mask helper accepts the second argument unchanged. */
	ScePervasiveForDriver_4AF7A01E(head, 8);
	state = PervasiveLock();
	if (head == 0)
		MISC_REGS->usbPortControl &= ~3U;
	else if (head == 1)
		MISC_REGS->usbPortControl &= ~0xcU;
	else if (head == 2)
		MISC_REGS->usbPortControl &= ~0x30U;
	PervasiveUnlock(state);
	return 1;
}

uint32_t ScePervasiveForDriver_03EA9418(void)
{
	return SetMiscBits(&MISC_REGS->spareEnable, 1, 1);
}

uint32_t ScePervasiveForDriver_BD868E6E(void)
{
	return SetMiscBits(&MISC_REGS->spareEnable, 1, 0);
}

uint32_t ScePervasiveForDriver_FDD445C3(void)
{
	return SetMiscBits(&MISC_REGS->motionSpiDisable, 1, 1);
}

uint32_t ScePervasiveForDriver_132E514A(void)
{
	return SetMiscBits(&MISC_REGS->motionSpiDisable, 1, 0);
}

uint32_t ScePervasiveForDriver_29E65AE1(unsigned port, uint32_t address,
	uint32_t size)
{
	uint32_t intr;

	intr = PervasiveLock();
	if (port == 0) {
		MISC_REGS->usbBufferAddress[0] = address;
		MISC_REGS->usbBufferSize[0] = size;
	} else if (port == 1) {
		MISC_REGS->usbBufferAddress[1] = address;
		MISC_REGS->usbBufferSize[1] = size;
	} else if (port == 2) {
		MISC_REGS->usbBufferAddress[2] = address;
		MISC_REGS->usbBufferSize[2] = size;
	}
	PervasiveUnlock(intr);
	return 1;
}

uint32_t ScePervasiveForDriver_E3A735B9(unsigned port)
{
	switch (port) {
	case 0:
		return MISC_REGS->usbBufferAddress[0];
	case 1:
		return MISC_REGS->usbBufferAddress[1];
	case 2:
		return MISC_REGS->usbBufferAddress[2];
	}
	return 0;
}

uint32_t ScePervasiveForDriver_551EEE82(void)
{
	return s_pervasive2->memoryCardInsert & 1;
}

uint32_t ScePervasiveForDriver_F9E7B023(void)
{
	uint32_t previous = s_pervasive2->memoryCardInterrupt;

	s_pervasive2->memoryCardInterrupt = previous;
	(void)s_pervasive2->memoryCardInterrupt;
	LowioDsb();
	return previous;
}
#endif
