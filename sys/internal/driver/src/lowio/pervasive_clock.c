/* Pervasive clock transitions. Private names and transition nibble meaning unknown. */
#include "lowio_internal.h"
#include "clock_transition_table.h"

#define PERVASIVE_ERROR ((int)0x803f0000)

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
		s_baseClock->armDivisor = 16 - speed;
		LowioDmb();
		return 0;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	raw = s_baseClock->armMultiplier;
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
			s_baseClock->armMultiplier = high | (path & 15);
			LowioDmb();
			path >>= 4;
		}
	}
	LowioDmb();
	s_baseClock->armDivisor = 16 - speed;
	LowioDmb();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 0;
}

int ScePervasiveForDriver_BB67EF58(unsigned head)
{
	int state;

	/* Reset-mask helper accepts the second argument unchanged. */
	ScePervasiveForDriver_4AF7A01E(head, 8);
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
	if (head == 0)
		s_misc->usbPortControl &= ~3U;
	else if (head == 1)
		s_misc->usbPortControl &= ~0xcU;
	else if (head == 2)
		s_misc->usbPortControl &= ~0x30U;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
	return 1;
}
