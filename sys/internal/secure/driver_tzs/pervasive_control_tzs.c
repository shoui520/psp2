/*
 * pervasive_control_tzs.c: the secure Pervasive words, one helper per device
 * and operation, and the monitor calls that reach them by offset (0x111,
 * 0x113) or by clock (0x114). Each reset and gate change runs under
 * s_pervasiveLock and returns whether the bits were set before.
 *
 * Internal names are unknown; they follow SceLowio's (pervasive_reset.c,
 * pervasive_gate.c) where the code is the same.
 */
#include "pervasive_tzs.h"

/* the CMeP clock's way from step [from - 2] to step [to - 2] (steps 2-6):
 * the steps to write, one per nibble, the first in the lowest */
static const uint16_t s_cmepClockSteps[5][5] = {
	{ 0x0000, 0x0003, 0x0004, 0x0054, 0x0006 },
	{ 0x0002, 0x0000, 0x0042, 0x0005, 0x0645 },
	{ 0x0002, 0x0032, 0x0000, 0x0005, 0x0006 },
	{ 0x0023, 0x0003, 0x0004, 0x0000, 0x0064 },
	{ 0x0024, 0x0354, 0x0004, 0x0054, 0x0000 },
};

/* ===== reset ===== */

/* Hold a device in reset, or release it. The flag word, when given, is set
 * before the device goes into reset and cleared after it comes out. */
__attribute__((noinline)) int ResetSetWithFlag(volatile uint32_t *reg, uint32_t mask,
	volatile uint32_t *flag, int enable)
{
	uint32_t state, previous;
	int ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	ret = (previous & mask) != 0;
	if (enable) {
		if (flag != NULL) {
			*flag = 1;
			(void)*flag;
			PervasiveDsb();
		}
		*reg = previous | mask;
		PervasiveDmb();
	} else {
		*reg = previous & ~mask;
		PervasiveDmb();
		(void)*reg;
		PervasiveDsb();
		if (flag != NULL && *flag != 0) {
			*flag = 0;
			(void)*flag;
			PervasiveDsb();
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return ret;
}

__attribute__((noinline)) int ResetDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag, int enable)
{
	return ResetSetWithFlag(reg, 1, flag, enable);
}

__attribute__((noinline)) int ResetDisableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetSetWithFlag(reg, 1, flag, 0);
}

__attribute__((noinline)) int ResetSet(volatile uint32_t *reg, uint32_t mask, int enable)
{
	uint32_t state, previous;
	int ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	ret = (previous & mask) != 0;
	if (enable) {
		*reg = previous | mask;
		PervasiveDmb();
	} else {
		*reg = previous & ~mask;
		PervasiveDmb();
		(void)*reg;
		PervasiveDsb();
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return ret;
}

__attribute__((noinline)) int ResetDevice(volatile uint32_t *reg, int enable)
{
	return ResetSet(reg, 1, enable);
}

__attribute__((noinline)) int ResetDisableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 0);
}

__attribute__((noinline)) int ResetEnableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 1);
}

__attribute__((noinline)) int ResetEnableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	uint32_t state, previous;
	int ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	ret = previous & 1;
	if (flag != NULL) {
		*flag = 1;
		(void)*flag;
		PervasiveDsb();
	}
	*reg = previous | 1;
	PervasiveDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return ret;
}

__attribute__((noinline)) int ResetEnableDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetEnableWithFlag(reg, flag);
}

/* monitor call 0x111: a secure device's reset word, by its offset */
int PervasiveResetMonitorCall(int offset, int enable, int arg2, int arg3)
{
	volatile uint32_t *reg = (volatile uint32_t *)((char *)s_reset + (offset & 0xfff));

	(void)arg2;
	(void)arg3;
	enable = enable != 0;
	if (reg == &s_reset->sonyRegbus || reg == &s_reset->sdio[1] ||
	    reg == &s_reset->dmac[0] || reg == &s_reset->dmac[1] ||
	    reg == &s_reset->dmac[2] || reg == &s_reset->dmac[3] ||
	    reg == &s_reset->dmac[4] || reg == &s_reset->veneziaRegisterAccess ||
	    reg == &s_reset->vipRegisterAccess)
		return ResetDevice(reg, enable);
	if (reg == &s_reset->allegrex)
		return ResetDeviceWithFlag(reg, &s_misc->allegrexResetHold, enable);
	if (reg == &s_reset->compatRam)
		return ResetDeviceWithFlag(reg, &s_misc->compatRamResetHold, enable);
	return PERVASIVE_ERROR;
}

__attribute__((noinline)) int GpuResetEnable(void)
{
	return ResetEnableDevice(&s_reset->gpu);
}

__attribute__((noinline)) int GpuResetDisable(void)
{
	return ResetDisableDevice(&s_reset->gpu);
}

__attribute__((noinline)) int AllegrexResetEnable(void)
{
	return ResetEnableDeviceWithFlag(&s_reset->allegrex, &s_misc->allegrexResetHold);
}

__attribute__((noinline)) int AllegrexResetDisable(void)
{
	return ResetDisableWithFlag(&s_reset->allegrex, &s_misc->allegrexResetHold);
}

__attribute__((noinline)) int SonyRegbusResetEnable(void)
{
	return ResetEnableDevice(&s_reset->sonyRegbus);
}

__attribute__((noinline)) int SonyRegbusResetDisable(void)
{
	return ResetDisableDevice(&s_reset->sonyRegbus);
}

__attribute__((noinline)) int VeneziaResetEnable(void)
{
	return ResetEnableDevice(&s_reset->venezia);
}

__attribute__((noinline)) int VipResetEnable(void)
{
	return ResetEnableDevice(&s_reset->vip);
}

__attribute__((noinline)) int VeneziaRegisterAccessResetEnable(void)
{
	return ResetEnableDevice(&s_reset->veneziaRegisterAccess);
}

__attribute__((noinline)) int VeneziaRegisterAccessResetDisable(void)
{
	return ResetDisableDevice(&s_reset->veneziaRegisterAccess);
}

__attribute__((noinline)) int VipRegisterAccessResetEnable(void)
{
	return ResetEnableDevice(&s_reset->vipRegisterAccess);
}

__attribute__((noinline)) int VipRegisterAccessResetDisable(void)
{
	return ResetDisableDevice(&s_reset->vipRegisterAccess);
}

/* ===== isolation and bus idle (ScePervasiveMisc) ===== */

__attribute__((noinline)) int GpuIsolationEnable(void)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = misc->gpuIsolation;
	misc->gpuIsolation = 1;
	PervasiveDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

__attribute__((noinline)) int GpuIsolationDisable(void)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = misc->gpuIsolation;
	misc->gpuIsolation = 0;
	PervasiveDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

__attribute__((noinline)) int VeneziaIsolationEnable(void)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = misc->veneziaIsolation;
	misc->veneziaIsolation = 1;
	PervasiveDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

__attribute__((noinline)) int VeneziaIsolationDisable(void)
{
	ScePervasiveMiscRegisters *misc = s_misc;
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = misc->veneziaIsolation;
	misc->veneziaIsolation = 0;
	PervasiveDmb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return previous != 0;
}

__attribute__((noinline)) int GpuBusIdle(void)
{
	volatile uint32_t *request = &s_misc->gpuBusIdleRequest;
	uint32_t state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	*request = 1;
	while (*request & 1)
		;
	while ((s_misc->busIdleStatus & 2) == 0)
		;
	s_misc->busIdleStatus = 2;
	while (s_misc->busIdleStatus & 2)
		;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

__attribute__((noinline)) int VeneziaBusIdle(void)
{
	volatile uint32_t *request = &s_misc->veneziaBusIdleRequest;
	uint32_t state;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	*request = 1;
	while (*request & 1)
		;
	while ((s_misc->busIdleStatus & 4) == 0)
		;
	s_misc->busIdleStatus = 4;
	while (s_misc->busIdleStatus & 4)
		;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

/* ===== gate ===== */

__attribute__((noinline)) int GateSet(volatile uint32_t *reg, uint32_t mask, int enable)
{
	uint32_t state, previous;
	int ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	ret = (previous & mask) != 0;
	if (enable)
		*reg = previous | mask;
	else
		*reg = previous & ~mask;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return ret;
}

__attribute__((noinline)) int GateDevice(volatile uint32_t *reg, int enable)
{
	uint32_t state, previous;
	int ret;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	ret = previous & 1;
	if (enable)
		*reg = previous | 1;
	else
		*reg = previous & ~1;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return ret;
}

__attribute__((noinline)) int GateSetDevice(volatile uint32_t *reg, int enable)
{
	return GateDevice(reg, enable);
}

__attribute__((noinline)) int GateDisableDevice(volatile uint32_t *reg)
{
	return GateDevice(reg, 0);
}

__attribute__((noinline)) int GateEnableDevice(volatile uint32_t *reg)
{
	return GateDevice(reg, 1);
}

__attribute__((noinline)) int GateEnable(volatile uint32_t *reg, uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous | mask;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

__attribute__((noinline)) int GateDisable(volatile uint32_t *reg, uint32_t mask)
{
	uint32_t state, previous;

	state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	previous = *reg;
	*reg = previous & ~mask;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return (previous & mask) != 0;
}

/* monitor call 0x113: a secure device's gate word, by its offset */
int PervasiveGateMonitorCall(int offset, int enable, int arg2, int arg3)
{
	volatile uint32_t *reg = (volatile uint32_t *)((char *)s_gate + (offset & 0xfff));

	(void)arg2;
	(void)arg3;
	enable = enable != 0;
	if (reg == &s_gate->sonyRegbus || reg == &s_gate->compatRam || reg == &s_gate->sdio[1] ||
	    reg == &s_gate->dmac[0] || reg == &s_gate->dmac[1] || reg == &s_gate->dmac[2] ||
	    reg == &s_gate->dmac[3] || reg == &s_gate->dmac[4] || reg == &s_gate->dmac[5])
		return GateSetDevice(reg, enable);
	if (reg == &s_gate->allegrex)
		return GateSet(reg, 3, enable);
	return PERVASIVE_ERROR;
}

__attribute__((noinline)) int GpuGateEnable(unsigned int mask)
{
	return GateEnable(&s_gate->gpu, mask & 0xf000f);
}

__attribute__((noinline)) int GpuGateDisable(unsigned int mask)
{
	return GateDisable(&s_gate->gpu, mask & 0xf000f);
}

__attribute__((noinline)) int AllegrexGateEnable(void)
{
	return GateEnable(&s_gate->allegrex, 3);
}

__attribute__((noinline)) int AllegrexGateDisable(void)
{
	return GateDisable(&s_gate->allegrex, 3);
}

__attribute__((noinline)) int SonyRegbusGateEnable(void)
{
	return GateEnableDevice(&s_gate->sonyRegbus);
}

__attribute__((noinline)) int SonyRegbusGateDisable(void)
{
	return GateDisableDevice(&s_gate->sonyRegbus);
}

__attribute__((noinline)) int CompatRamGateEnable(void)
{
	return GateEnableDevice(&s_gate->compatRam);
}

__attribute__((noinline)) int CompatRamGateDisable(void)
{
	return GateDisableDevice(&s_gate->compatRam);
}

__attribute__((noinline)) int VeneziaGateEnable(void)
{
	return GateDevice(&s_gate->venezia, 1);
}

__attribute__((noinline)) int VeneziaGateDisable(void)
{
	return GateDevice(&s_gate->venezia, 0);
}

__attribute__((noinline)) int VeneziaSecureGateEnable(void)
{
	return GateEnable(&s_gate->venezia, 2);
}

__attribute__((noinline)) int VeneziaSecureGateDisable(void)
{
	return GateDisable(&s_gate->venezia, 2);
}

__attribute__((noinline)) int VipGateEnable(void)
{
	return GateEnableDevice(&s_gate->vip);
}

__attribute__((noinline)) int VipGateDisable(void)
{
	return GateDisableDevice(&s_gate->vip);
}

__attribute__((noinline)) int DmacGateEnable(int n)
{
	volatile uint32_t *dmac = s_gate->dmac;

	return GateEnableDevice(&dmac[n]);
}

__attribute__((noinline)) int GpuPowerClockEnable(void)
{
	return GateEnable(&s_baseClock->allegrexClock, 0x4000000);
}

__attribute__((noinline)) int GpuPowerClockDisable(void)
{
	return GateDisable(&s_baseClock->allegrexClock, 0x4000000);
}

/* ===== clocks ===== */

/* Walk the CMeP clock through up to four steps, one per nibble. */
static inline __attribute__((always_inline)) void CmepClockStep(unsigned int steps)
{
	ScePervasiveBaseClkRegisters *baseClock;

	if (steps == 0)
		return;
	s_baseClock->cmepClock = (steps & 0xf) | 0x10000;
	PervasiveDmb();
	steps >>= 4;
	if (steps == 0)
		return;
	baseClock = s_baseClock;
	baseClock->cmepClock = (steps & 0xf) | 0x10000;
	PervasiveDmb();
	steps >>= 4;
	if (steps == 0)
		return;
	baseClock->cmepClock = (steps & 0xf) | 0x10000;
	PervasiveDmb();
	steps >>= 4;
	if (steps == 0)
		return;
	baseClock->cmepClock = steps | 0x10000;
	PervasiveDmb();
}

/* monitor call 0x114: the CMeP clock step (0, 1) and the compat clock (2, 3) */
int PervasiveClockMonitorCall(int op, unsigned int value, int arg2, int arg3)
{
	unsigned int state, current, steps;

	(void)arg2;
	(void)arg3;
	switch (op) {
	case 0:
		if (value - 2 > 4)
			break;
		state = sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
		current = s_baseClock->cmepClock & ~0x10000;
		if (value != current) {
			steps = s_cmepClockSteps[current - 2][value - 2];
			CmepClockStep(steps);

		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
		return 0;
	case 1:
		return s_baseClock->cmepClock & ~0x10000;
	case 2:
		if (value > 2)
			break;
		s_baseClock->compatClock = value;
		PervasiveDmb();
		return 0;
	case 3:
		return s_baseClock->compatClock;
	}
	return PERVASIVE_ERROR;
}
