/* Pervasive configuration not requiring the pixel-clock timing tables. */
#include "lowio_internal.h"
#include "uart_clock_table.h"

#define PERVASIVE_ERROR ((uint32_t)0x803f0000)

/* Hardware-info registers cached by initialization; the final four names unknown. */
uint32_t s_hardwareInfo[5];

uint32_t ScePervasiveForDriver_714EEFB7(void)
{
	return s_hardwareInfo[0];
}

uint32_t ScePervasiveForDriver_41C3C299(void)
{
	return s_hardwareInfo[1];
}

uint32_t ScePervasiveForDriver_FB0EF8C7(void)
{
	return s_hardwareInfo[2];
}

uint32_t ScePervasiveForDriver_443E2A3F(void)
{
	return s_hardwareInfo[3];
}

uint32_t ScePervasiveForDriver_F58EF3A5(void)
{
	return s_hardwareInfo[4];
}

uint32_t ScePervasiveForDriver_270BE98A(unsigned index)
{
	return index < 8 ? (uintptr_t)s_tas[index] : 0;
}

uint32_t ScePervasiveForDriver_453BD2EC(unsigned port, unsigned enable)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, shift;

	if (port >= 4)
		return PERVASIVE_ERROR;
	if (enable >= 2)
		return PERVASIVE_ERROR;
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	shift = port;
	misc->sdifVoltage = (misc->sdifVoltage & ~(1U << shift)) |
		enable << shift;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_043B33F5(unsigned port, unsigned mode)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, shift;

	if (port >= 4)
		return PERVASIVE_ERROR;
	if (mode >= 7)
		return PERVASIVE_ERROR;
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	shift = port * 8;
	misc->sdifMode = (misc->sdifMode & ~(7U << shift)) |
		mode << shift;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
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
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = s_baseClock->auxClockEnable;
	if (previous) {
		s_baseClock->auxClockEnable = 0;
		LowioDmb();
	}
	s_baseClock->auxClockDivider = value;
	s_baseClock->auxClockSource = source;
	if (previous)
		s_baseClock->auxClockEnable = previous;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_8847825F(void)
{
	ScePervasiveBaseClkRegisters *base = s_baseClock;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = base->auxClockEnable;
	base->auxClockEnable = 1;
	LowioDmb();
	(void)base->auxClockEnable;
	LowioDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

uint32_t ScePervasiveForDriver_9E81E057(void)
{
	ScePervasiveBaseClkRegisters *base = s_baseClock;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = base->auxClockEnable;
	base->auxClockEnable = 0;
	LowioDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

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
