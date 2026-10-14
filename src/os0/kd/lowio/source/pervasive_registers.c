/* Pervasive register configuration. Private register names remain unresolved. */
#include "lowio_internal.h"

#define PERVASIVE_ERROR ((uint32_t)0x803f0000)
#define REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))

static uint32_t WriteClock(unsigned offset, uint32_t value)
{
	REG(s_baseClock, offset) = value;
	LowioDmb();
	return 0;
}

static uint32_t ReplaceRegister(unsigned offset, uint32_t value,
	unsigned synchronize)
{
	uint32_t state, previous;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = REG(s_misc, offset);
	REG(s_misc, offset) = value;
	LowioDmb();
	if (synchronize)
		LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

uint32_t ScePervasiveForDriver_A1D2E440(uint32_t value)
{
	REG(s_misc, 0x190) = value;
	return 0;
}

uint32_t ScePervasiveForDriver_6AF0196F(unsigned first, unsigned second)
{
	if (first >= 8 || second >= 8)
		return PERVASIVE_ERROR;
	return WriteClock(0x10, second | first << 16);
}

uint32_t ScePervasiveForDriver_725B4F59(unsigned value)
{
	if (value - 1 >= 7)
		return PERVASIVE_ERROR;
	return WriteClock(0x20, value);
}

uint32_t ScePervasiveForDriver_12D17D47(unsigned value)
{
	if (value - 1 >= 6)
		return PERVASIVE_ERROR;
	return WriteClock(0x30, value);
}

uint32_t ScePervasiveForDriver_589D0291(unsigned value)
{
	if (value - 1 >= 5)
		return PERVASIVE_ERROR;
	return WriteClock(0x44, value);
}

uint32_t ScePervasiveForDriver_925D9D24(unsigned value)
{
	if (value >= 2)
		return PERVASIVE_ERROR;
	return WriteClock(0x70, 0x10000 | value);
}

uint32_t ScePervasiveForDriver_26B51075(void)
{
	return WriteClock(0x1d0, 1);
}

uint32_t ScePervasiveForDriver_D185D235(void)
{
	return WriteClock(0x1d0, 0);
}

uint32_t kscePervasiveMsifSetClock(unsigned value)
{
	if ((value & ~4U) >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(0xb0, (value & 3) | (value & 4 ? 0x10000 : 0));
}

uint32_t ScePervasiveForDriver_A6832B33(unsigned value)
{
	if (value - 1 >= 4)
		return PERVASIVE_ERROR;
	return WriteClock(0x1f0, value);
}

uint32_t ScePervasiveForDriver_2FB5F88F(unsigned value)
{
	if (value >= 3)
		return PERVASIVE_ERROR;
	return WriteClock(0x210, value);
}

uint32_t ScePervasiveForDriver_98D8914A(unsigned value)
{
	if (value >= 2)
		return PERVASIVE_ERROR;
	return WriteClock(0x214, value);
}

uint32_t ScePervasiveForDriver_A96642E3(unsigned value)
{
	if (value < 2)
		return WriteClock(0x68, value | 2);
	if (value == 0xff)
		return WriteClock(0x68, 0);
	return PERVASIVE_ERROR;
}

uint32_t ScePervasiveForDriver_F1908D3C(unsigned head)
{
	return ReplaceRegister(head == 0 ? 0x140 : 0x148, 1, 0);
}

uint32_t ScePervasiveForDriver_350BF570(unsigned head)
{
	return ReplaceRegister(head == 0 ? 0x140 : 0x148, 0, 0);
}

uint32_t ScePervasiveForDriver_0C1A7FFA(void)
{
	return ReplaceRegister(0x144, 1, 0);
}

uint32_t ScePervasiveForDriver_5B4EF352(void)
{
	return ReplaceRegister(0x144, 0, 0);
}

static uint32_t SetMiscBits(unsigned offset, uint32_t mask, unsigned enable)
{
	uint32_t previous = REG(s_misc, offset);

	REG(s_misc, offset) = enable ? previous | mask : previous & ~mask;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_5E20D7D5(void)
{
	return SetMiscBits(0x194, 1, 1);
}

uint32_t ScePervasiveForDriver_E6728E30(void)
{
	return SetMiscBits(0x194, 1, 0);
}

uint32_t ScePervasiveForDriver_03EA9418(void)
{
	return SetMiscBits(0x198, 1, 1);
}

uint32_t ScePervasiveForDriver_BD868E6E(void)
{
	return SetMiscBits(0x198, 1, 0);
}

uint32_t ScePervasiveForDriver_FDD445C3(void)
{
	return SetMiscBits(0x188, 1, 1);
}

uint32_t ScePervasiveForDriver_132E514A(void)
{
	return SetMiscBits(0x188, 1, 0);
}

uint32_t ScePervasiveForDriver_34537B12(void)
{
	REG(s_misc, 0x1ac) &= ~3U;
	REG(s_misc, 0x18c) &= ~3U;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_4B99558F(void)
{
	REG(s_misc, 0x1ac) |= 3;
	REG(s_misc, 0x18c) |= 3;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_9F8E589C(unsigned port, uint32_t value)
{
	if (port >= 4 || (value & ~0x700U) != 0)
		return PERVASIVE_ERROR;
	REG(s_misc, 0x110 + port * 4) = value;
	LowioDmb();
	return 0;
}

uint32_t ScePervasiveForDriver_6B2BE37E(unsigned port, uint32_t value)
{
	uint32_t state, previous;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (port >= 3) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
		return PERVASIVE_ERROR;
	}
	previous = REG(s_misc, 0x84 + port * 4);
	REG(s_misc, 0x84 + port * 4) = value & 1;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

uint32_t ScePervasiveForDriver_29E65AE1(unsigned port, uint32_t first,
	uint32_t second)
{
	uint32_t state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (port < 3) {
		REG(s_misc, 0x150 + port * 4) = first;
		REG(s_misc, 0x15c + port * 4) = second;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}

uint32_t ScePervasiveForDriver_E3A735B9(unsigned port)
{
	return port < 3 ? REG(s_misc, 0x150 + port * 4) : 0;
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
