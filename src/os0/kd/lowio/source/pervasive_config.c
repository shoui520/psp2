/* Pervasive configuration not requiring the pixel-clock timing tables. */
#include "lowio_internal.h"
#include "uart_clock_table.h"

#define PERVASIVE_ERROR ((uint32_t)0x803f0000)
#define REG(base, offset) (*(volatile uint32_t *)((base) + (offset)))

/* Hardware-info registers cached by initialization; the final four names unknown. */
uint32_t s_hardwareInfo[5];

uint32_t kscePervasiveGetSoCRevision(void)
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
	uint32_t state, shift;

	if (port >= 4 || enable >= 2)
		return PERVASIVE_ERROR;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	shift = port;
	REG(s_misc, 0x124) = (REG(s_misc, 0x124) & ~(1U << shift)) |
		enable << shift;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_043B33F5(unsigned port, unsigned mode)
{
	uint32_t state, shift;

	if (port >= 4 || mode >= 7)
		return PERVASIVE_ERROR;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	shift = port * 8;
	REG(s_misc, 0x310) = (REG(s_misc, 0x310) & ~(7U << shift)) |
		mode << shift;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_59B7CA34(unsigned source, unsigned divider)
{
	uint32_t state, previous, value;

	if (source == 0) {
		value = divider - 1;
		if (value >= 16)
			return PERVASIVE_ERROR;
	} else {
		if (source != 1 || divider != 1)
			return PERVASIVE_ERROR;
		value = 0;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = REG(s_baseClock, 0xa4);
	if (previous) {
		REG(s_baseClock, 0xa4) = 0;
		LowioDmb();
	}
	REG(s_baseClock, 0x60) = value;
	REG(s_baseClock, 0x64) = source;
	if (previous)
		REG(s_baseClock, 0xa4) = previous;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_8847825F(void)
{
	uint32_t state, previous;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = REG(s_baseClock, 0xa4);
	REG(s_baseClock, 0xa4) = 1;
	LowioDmb();
	(void)REG(s_baseClock, 0xa4);
	LowioDsb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

uint32_t ScePervasiveForDriver_9E81E057(void)
{
	uint32_t state, previous;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = REG(s_baseClock, 0xa4);
	REG(s_baseClock, 0xa4) = 0;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

uint32_t kscePervasiveUartSetBaudrate(unsigned port, unsigned baud)
{
	unsigned i;

	if (port >= 7)
		return PERVASIVE_ERROR;
	for (i = 0; i < sizeof(s_uartClockTable) / sizeof(s_uartClockTable[0]); i++) {
		if (s_uartClockTable[i].baud != baud)
			continue;
		REG(s_uartClock, port * 4) = s_uartClockTable[i].value;
		LowioDmb();
		return 0;
	}
	return PERVASIVE_ERROR;
}
