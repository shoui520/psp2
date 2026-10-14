/* Pervasive clock transitions. Private names and transition nibble meaning unknown. */
#include "lowio_internal.h"
#include "clock_transition_table.h"
#include CLOCK_PREFIX_HEADER

#define PERVASIVE_ERROR ((uint32_t)0x803f0000)
#define REG(offset) (*(volatile uint32_t *)(s_baseClock + (offset)))

static int ClockIndex(unsigned mode)
{
	if (mode < 2)
		return (int)mode - 1;
	if (mode < 11)
		return (int)mode - 2;
	return (int)mode - 3;
}

uint32_t ScePervasiveForDriver_E9D95643(unsigned mode, unsigned speed)
{
	uint32_t state, current, raw, high, path;
	unsigned shift;

	if (mode >= 16 || mode == 2 || mode == 11 || mode == 15 ||
	    speed - 8 >= 9)
		return PERVASIVE_ERROR;
#if FW_DEBUG
	if (mode == 9)
		return PERVASIVE_ERROR;
#endif
	if (mode == 0) {
		REG(4) = 16 - speed;
		LowioDmb();
		return 0;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	raw = REG(0);
	current = raw & 15;
	if (current != mode) {
		if (current == 0)
			path = s_clockTransitionPrefix[ClockIndex(mode)];
		else
			path = s_clockTransition[ClockIndex(current)][ClockIndex(mode)];
		if (path) {
			LowioDmb();
			high = raw & ~15U;
			for (shift = 0; shift < 16 && (path >> shift); shift += 4) {
				if (shift)
					LowioDmb();
				REG(0) = high | ((path >> shift) & 15);
				LowioDmb();
			}
		}
	}
	LowioDmb();
	REG(4) = 16 - speed;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

uint32_t ScePervasiveForDriver_BB67EF58(unsigned head)
{
	uint32_t state;

	/* Reset-mask helper accepts the second argument unchanged. */
	ScePervasiveForDriver_4AF7A01E(head, 8);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head < 3) {
		volatile uint32_t *reg = (volatile uint32_t *)(s_misc + 0x17c);

		*reg &= ~(3U << (head * 2));
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}
