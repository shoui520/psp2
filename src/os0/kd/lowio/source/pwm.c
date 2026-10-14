/* ScePwm configuration and deferred stop state. Private names are inferred. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/sysevent.h>

#define PWM_ERROR_CHANNEL ((uint32_t)0x803f0200)
#define PWM_ERROR_ARGUMENT ((uint32_t)0x803f0201)
#define PWM_ERROR_BUSY ((uint32_t)0x803f0280)
#define PWM_REG(channel, offset) \
	(*(volatile uint32_t *)(s_pwmBase + (channel) * 0x20 + (offset)))

uint8_t *s_pwmBase;
uint8_t s_pwmState[2];

uint32_t ScePwmForDriver_13161CCC(unsigned channel, uint32_t flags,
	uint32_t period, uint32_t duty)
{
	uint32_t control, previous;

	if (channel >= 2)
		return PWM_ERROR_CHANNEL;
	if ((flags & ~0x300U) != 0 || duty > period)
		return PWM_ERROR_ARGUMENT;
	previous = s_pwmState[channel];
	control = PWM_REG(channel, 0);
	if (previous == 2) {
		if (control & 2)
			return PWM_ERROR_BUSY;
		previous = 0;
	}
	s_pwmState[channel] = 1;
	control = (flags & 0x300) | (control & 1 ? 3 : 1);
	PWM_REG(channel, 4) = period;
	PWM_REG(channel, 8) = duty;
	PWM_REG(channel, 0) = control;
	return previous;
}

uint32_t ScePwmForDriver_22C17D24(unsigned channel)
{
	uint32_t control, previous;

	if (channel >= 2)
		return PWM_ERROR_CHANNEL;
	previous = s_pwmState[channel];
	control = PWM_REG(channel, 0);
	if (previous == 2) {
		if ((control & 2) == 0) {
			s_pwmState[channel] = 0;
			return 0;
		}
	} else if (previous == 0) {
		return 0;
	}
	PWM_REG(channel, 4) = 0;
	PWM_REG(channel, 0) = control | 2;
	s_pwmState[channel] = 2;
	if ((PWM_REG(channel, 0) & 2) == 0) {
		PWM_REG(channel, 0) &= 1;
		s_pwmState[channel] = 0;
	}
	return previous;
}

uint32_t ScePwmForDriver_514773B1(unsigned channel, uint32_t period,
	uint32_t duty)
{
	uint8_t state;
	uint32_t control;

	if (channel >= 2)
		return PWM_ERROR_CHANNEL;
	state = s_pwmState[channel];
	if (state == 0)
		return 0;
	control = PWM_REG(channel, 0);
	if (state == 2) {
		if ((control & 2) == 0)
			s_pwmState[channel] = 0;
		return 0;
	}
	if (state == 1 && (control & 2))
		return PWM_ERROR_BUSY;
	PWM_REG(channel, 4) = period;
	PWM_REG(channel, 8) = duty;
	PWM_REG(channel, 0) |= 2;
	return 0;
}

uint32_t ScePwmForDriver_0374213C(unsigned channel, uint16_t *periodOut,
	uint16_t *dutyOut, uint32_t *levelOut)
{
	uint32_t state, control, period, duty, level = 2;

	if (channel >= 2)
		return PWM_ERROR_CHANNEL;
	state = s_pwmState[channel];
	control = PWM_REG(channel, 0);
	period = PWM_REG(channel, 4);
	duty = PWM_REG(channel, 8);
	if (state == 2) {
		if ((control & 2) == 0)
			s_pwmState[channel] = 0;
		state = 0;
	}
	if (periodOut)
		*periodOut = period;
	if (dutyOut)
		*dutyOut = duty;
	if (levelOut) {
		if ((control & 3) == 1) {
			if (period == 0)
				level = control >> 9 & 1;
			else if (period == duty)
				level = (control >> 8 & 1) ^ 1;
			else if (duty == 0)
				level = control >> 8 & 1;
		}
		*levelOut = level;
	}
	return state;
}

static int s_pwmUid;

static void PwmRestoreState(void)
{
	unsigned channel;

	for (channel = 0; channel < 2; channel++) {
		uint32_t control = PWM_REG(channel, 0);
		uint32_t period = PWM_REG(channel, 4);

		s_pwmState[channel] = (control & 1) && period != 0;
	}
}

static int PwmSysEvent(int resume, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 0 && event == 0x4002) {
		s_pwmState[0] = 0;
		s_pwmState[1] = 0;
	} else if (resume == 1 && event == 0x10002) {
		PwmRestoreState();
	}
	return 0;
}

int LowioPwmInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.attr = 2;
	opt.paddr = 0xe20c0000;
	ret = ksceKernelAllocMemBlock("ScePwmReg", 0x20100806, 0x1000, &opt);
	if (ret < 0)
		return 2;
	s_pwmUid = ret;
	ksceKernelGetMemBlockBase(ret, (void **)&s_pwmBase);
	PwmRestoreState();
	ksceKernelRegisterSysEventHandler("ScePwm", PwmSysEvent, NULL);
	return 0;
}
