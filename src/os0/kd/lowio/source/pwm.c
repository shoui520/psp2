/* ScePwm: two PWM channels and their deferred stop. Private names are inferred. */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/pwm_regs.h>
#include <sce/sysevent.h>

#define PWM_ERROR_CHANNEL ((int)0x803f0200)
#define PWM_ERROR_ARGUMENT ((int)0x803f0201)
#define PWM_ERROR_BUSY ((int)0x803f0280)

typedef enum PwmState {
	PWM_STATE_STOPPED = 0,
	PWM_STATE_RUNNING = 1,
	PWM_STATE_STOPPING = 2,	/* stop requested, update still pending */
} PwmState;

typedef struct PwmContext {
	volatile ScePwmRegisters *registers;
	uint8_t state[2];
	SceUID uid;
} PwmContext;

_Static_assert(offsetof(PwmContext, uid) == 0x8, "PWM context uid");

static PwmContext s_pwm;

static inline __attribute__((always_inline)) void PwmRestoreState(void)
{
	volatile ScePwmRegisters *regs = s_pwm.registers;
	uint32_t control, period;

	control = regs->channel[0].control;
	period = regs->channel[0].period;
	s_pwm.state[0] = (control & SCE_PWM_CONTROL_ENABLE) && period != 0;
	control = regs->channel[1].control;
	period = regs->channel[1].period;
	s_pwm.state[1] = (control & SCE_PWM_CONTROL_ENABLE) && period != 0;
}

static int PwmSysEvent(int resume, int eventId, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 0) {
		if (eventId == 0x4002) {
			s_pwm.state[0] = PWM_STATE_STOPPED;
			s_pwm.state[1] = PWM_STATE_STOPPED;
		}
	} else if (resume == 1) {
		if (eventId == 0x10002)
			PwmRestoreState();
	}
	return 0;
}

int LowioPwmInit(SceSize arglen, const void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	int ret;

	(void)arglen;
	(void)argp;
	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.attr = 2;
	opt.paddr = 0xe20c0000;
	ret = ksceKernelAllocMemBlock("ScePwmReg", 0x20100806, 0x1000, &opt);
	if (ret < 0)
		return 2;
	s_pwm.uid = ret;
	ksceKernelGetMemBlockBase(ret, (void **)&s_pwm.registers);
	PwmRestoreState();
	ksceKernelRegisterSysEventHandler("ScePwm", PwmSysEvent, NULL);
	return 0;
}

int ScePwmForDriver_13161CCC(unsigned int channel, uint32_t flags,
	uint32_t period, uint32_t duty)
{
	volatile ScePwmRegisters *regs = s_pwm.registers;
	volatile ScePwmChannelRegisters *pwm;
	uint32_t control;
	int previous;

	if (channel > 1)
		return PWM_ERROR_CHANNEL;
	if ((flags & ~SCE_PWM_CONTROL_LEVELS) != 0 || duty > period)
		return PWM_ERROR_ARGUMENT;
	previous = s_pwm.state[channel];
	pwm = &regs->channel[channel];
	control = pwm->control;
	if (previous == PWM_STATE_STOPPING) {
		if (control & SCE_PWM_CONTROL_UPDATE)
			return PWM_ERROR_BUSY;
		previous = PWM_STATE_STOPPED;
	}
	s_pwm.state[channel] = PWM_STATE_RUNNING;
	if (control & SCE_PWM_CONTROL_ENABLE)
		control = (flags & SCE_PWM_CONTROL_LEVELS) |
			SCE_PWM_CONTROL_UPDATE | SCE_PWM_CONTROL_ENABLE;
	else
		control = (flags & SCE_PWM_CONTROL_LEVELS) | SCE_PWM_CONTROL_ENABLE;
	pwm->period = period;
	pwm->duty = duty;
	pwm->control = control;
	return previous;
}

int ScePwmForDriver_22C17D24(unsigned int channel)
{
	volatile ScePwmRegisters *regs = s_pwm.registers;
	volatile ScePwmChannelRegisters *pwm;
	uint32_t control;
	int previous;

	if (channel > 1)
		return PWM_ERROR_CHANNEL;
	previous = s_pwm.state[channel];
	pwm = &regs->channel[channel];
	control = pwm->control;
	if (previous == PWM_STATE_STOPPING) {
		if ((control & SCE_PWM_CONTROL_UPDATE) == 0) {
			s_pwm.state[channel] = PWM_STATE_STOPPED;
			return 0;
		}
	} else if (previous == PWM_STATE_STOPPED) {
		return previous;
	}
	pwm->period = 0;
	pwm->control = control | SCE_PWM_CONTROL_UPDATE;
	s_pwm.state[channel] = PWM_STATE_STOPPING;
	if ((pwm->control & SCE_PWM_CONTROL_UPDATE) == 0) {
		pwm->control &= SCE_PWM_CONTROL_ENABLE;
		s_pwm.state[channel] = PWM_STATE_STOPPED;
	}
	return previous;
}

int ScePwmForDriver_514773B1(unsigned int channel, uint32_t period,
	uint32_t duty)
{
	volatile ScePwmRegisters *regs = s_pwm.registers;
	volatile ScePwmChannelRegisters *pwm;
	uint32_t control;
	int state;

	if (channel > 1)
		return PWM_ERROR_CHANNEL;
	state = s_pwm.state[channel];
	if (state == PWM_STATE_STOPPED)
		return state;
	pwm = &regs->channel[channel];
	control = pwm->control;
	if (state == PWM_STATE_STOPPING) {
		if ((control & SCE_PWM_CONTROL_UPDATE) == 0)
			s_pwm.state[channel] = PWM_STATE_STOPPED;
		return 0;
	}
	if (state == PWM_STATE_RUNNING && (control & SCE_PWM_CONTROL_UPDATE) != 0)
		return PWM_ERROR_BUSY;
	pwm->period = period;
	pwm->duty = duty;
	pwm->control |= SCE_PWM_CONTROL_UPDATE;
	return 0;
}

int ScePwmForDriver_0374213C(unsigned int channel, uint16_t *periodOut,
	uint16_t *dutyOut, uint32_t *levelOut)
{
	volatile ScePwmChannelRegisters *pwm;
	uint32_t control, period, duty, level;
	int state;

	if (channel > 1)
		return PWM_ERROR_CHANNEL;
	state = s_pwm.state[channel];
	pwm = &s_pwm.registers->channel[channel];
	control = pwm->control;
	period = pwm->period;
	duty = pwm->duty;
	if (state == PWM_STATE_STOPPING) {
		if ((control & SCE_PWM_CONTROL_UPDATE) == 0)
			s_pwm.state[channel] = PWM_STATE_STOPPED;
		state = PWM_STATE_STOPPED;
	}
	if (periodOut != NULL)
		*periodOut = period;
	if (dutyOut != NULL)
		*dutyOut = duty;
	if (levelOut != NULL) {
		/* A steady output has a level; a running wave has none (2). */
		level = 2;
		if ((control & (SCE_PWM_CONTROL_UPDATE | SCE_PWM_CONTROL_ENABLE)) ==
		    SCE_PWM_CONTROL_ENABLE) {
			if (period == 0)
				level = (control & SCE_PWM_CONTROL_IDLE_LEVEL) != 0;
			else if (period == duty)
				level = (control & SCE_PWM_CONTROL_INVERT) == 0;
			else if (duty == 0)
				level = (control & SCE_PWM_CONTROL_INVERT) != 0;
		}
		*levelOut = level;
	}
	return state;
}

int ScePwmForDriver_99911920(unsigned int channel)
{
	volatile ScePwmRegisters *regs = s_pwm.registers;

	if (channel > 1)
		return PWM_ERROR_CHANNEL;
	return (regs->channel[channel].control & SCE_PWM_CONTROL_UPDATE) != 0;
}
