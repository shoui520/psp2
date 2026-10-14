/* Pervasive register configuration. Private register names remain unresolved. */
#include "lowio_internal.h"

#define PERVASIVE_ERROR ((uint32_t)0x803f0000)
#define REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))

static uint32_t WriteClock(volatile uint32_t *reg, uint32_t value)
{
	*reg = value;
	LowioDmb();
	return 0;
}

/* Store a Misc word; returns whether it was set before. */
static inline __attribute__((always_inline)) uint32_t MiscReplace(
	volatile uint32_t *reg, uint32_t value)
{
	uint32_t previous = *reg;

	*reg = value;
	previous = previous != 0;
	LowioDmb();
	return previous;
}

static inline __attribute__((always_inline)) uint32_t MiscReplaceLocked(
	volatile uint32_t *reg, uint32_t value)
{
	uint32_t intr, previous;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = MiscReplace(reg, value);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, intr);
	return previous;
}

uint32_t ScePervasiveForDriver_A1D2E440(uint32_t value)
{
	s_misc->audioMode = value;
	return 0;
}

uint32_t ScePervasiveForDriver_6AF0196F(unsigned first, unsigned second)
{
	if (first >= 8 || second >= 8)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->gpuClock, second | first << 16);
}

uint32_t ScePervasiveForDriver_725B4F59(unsigned value)
{
	if (value - 1 >= 7)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->veneziaClock, value);
}

uint32_t ScePervasiveForDriver_12D17D47(unsigned value)
{
	if (value - 1 >= 6)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->vipClock, value);
}

uint32_t ScePervasiveForDriver_589D0291(unsigned value)
{
	if (value - 1 >= 5)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->cameraBusClock, value);
}

uint32_t ScePervasiveForDriver_925D9D24(unsigned value)
{
	if (value > 1)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->audioClock, value != 0 ? 0x10001 : 0x10000);
}

uint32_t ScePervasiveForDriver_26B51075(void)
{
	return WriteClock(&s_baseClock->cecClockOut, 1);
}

uint32_t ScePervasiveForDriver_D185D235(void)
{
	return WriteClock(&s_baseClock->cecClockOut, 0);
}

uint32_t kscePervasiveMsifSetClock(unsigned value)
{
	if ((value & ~4U) >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->msifClock, (value & 3) | (value & 4 ? 0x10000 : 0));
}

uint32_t ScePervasiveForDriver_A6832B33(unsigned value)
{
	if (value - 1 >= 4)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->dmac5Clock, value);
}

uint32_t ScePervasiveForDriver_2FB5F88F(unsigned value)
{
	if (value >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->srcClock, value);
}

uint32_t ScePervasiveForDriver_98D8914A(unsigned value)
{
	if (value >= 2)
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->sysClock, value);
}

uint32_t ScePervasiveForDriver_A96642E3(unsigned value)
{
	if (value <= 1)
		value |= 2;
	else if (value == 0xff)
		value = 0;
	else
		return PERVASIVE_ERROR;
	return WriteClock(&s_baseClock->gatedClockSelect, value);
}

uint32_t ScePervasiveForDriver_F1908D3C(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 1);
	return MiscReplaceLocked(&misc->dsi0Control, 1);
}

uint32_t ScePervasiveForDriver_350BF570(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 0);
	return MiscReplaceLocked(&misc->dsi0Control, 0);
}

uint32_t ScePervasiveForDriver_0C1A7FFA(void)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	return MiscReplaceLocked(&misc->csiControl, 1);
}

uint32_t ScePervasiveForDriver_5B4EF352(void)
{
	volatile ScePervasiveMiscRegisters *misc = s_misc;

	return MiscReplaceLocked(&misc->csiControl, 0);
}

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
	return SetMiscBits(&s_misc->gameCard, 1, 1);
}

uint32_t ScePervasiveForDriver_E6728E30(void)
{
	return SetMiscBits(&s_misc->gameCard, 1, 0);
}

uint32_t ScePervasiveForDriver_03EA9418(void)
{
	return SetMiscBits(&s_misc->spareEnable, 1, 1);
}

uint32_t ScePervasiveForDriver_BD868E6E(void)
{
	return SetMiscBits(&s_misc->spareEnable, 1, 0);
}

uint32_t ScePervasiveForDriver_FDD445C3(void)
{
	return SetMiscBits(&s_misc->motionSpiDisable, 1, 1);
}

uint32_t ScePervasiveForDriver_132E514A(void)
{
	return SetMiscBits(&s_misc->motionSpiDisable, 1, 0);
}

uint32_t ScePervasiveForDriver_34537B12(void)
{
	s_misc->uart5PullDownRx &= ~SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	s_misc->uart5PullDownTx &= ~SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_4B99558F(void)
{
	s_misc->uart5PullDownRx |= SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	s_misc->uart5PullDownTx |= SCE_PERVASIVE_UART5_PULL_DOWN_PINS;
	LowioDmb();
	return 0;
}

/* name unknown: part of the clock setup at start and on resume; does nothing */
int PervasiveMiscInit(void)
{
	return 0;
}

uint32_t ScePervasiveForDriver_9F8E589C(unsigned port, uint32_t value)
{
	if (port >= 4 || (value & ~0x700U) != 0)
		return PERVASIVE_ERROR;
	s_misc->sdifConfig[port] = value;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_6B2BE37E(unsigned port, uint32_t value)
{
	uint32_t intr, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	switch (port) {
	case 0:
		ret = MiscReplace(&s_misc->usbMode[0], value & 1);
		break;
	case 1:
		ret = MiscReplace(&s_misc->usbMode[1], value & 1);
		break;
	case 2:
		ret = MiscReplace(&s_misc->usbMode[2], value & 1);
		break;
	default:
		ret = PERVASIVE_ERROR;
		break;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, intr);
	return ret;
}

uint32_t ScePervasiveForDriver_29E65AE1(unsigned port, uint32_t address,
	uint32_t size)
{
	uint32_t intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (port == 0) {
		s_misc->usbBufferAddress[0] = address;
		s_misc->usbBufferSize[0] = size;
	} else if (port == 1) {
		s_misc->usbBufferAddress[1] = address;
		s_misc->usbBufferSize[1] = size;
	} else if (port == 2) {
		s_misc->usbBufferAddress[2] = address;
		s_misc->usbBufferSize[2] = size;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, intr);
	return 1;
}

uint32_t ScePervasiveForDriver_E3A735B9(unsigned port)
{
	switch (port) {
	case 0:
		return s_misc->usbBufferAddress[0];
	case 1:
		return s_misc->usbBufferAddress[1];
	case 2:
		return s_misc->usbBufferAddress[2];
	}
	return 0;
}

uint32_t kscePervasiveRemovableMemoryGetCardInsertState(void)
{
	return REG(s_pervasive2, 0xf40) & 1;
}

uint32_t ScePervasiveForDriver_F9E7B023(void)
{
	uint32_t previous = REG(s_pervasive2, 0xf44);

	REG(s_pervasive2, 0xf44) = previous;
	(void)REG(s_pervasive2, 0xf44);
	LowioDsb();
	return previous;
}
