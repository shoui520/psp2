/*
 * ScePower: LED control (SceLedForDriver).
 *
 * Each LED is a GPIO line driven from the display vblank: it is on for onTicks
 * and off for offTicks, repeatCount times, or follows a byte script. LED 2 is
 * the power LED, which Syscon also drives.
 */

#include <sysmem/sysroot.h>
#include <sysmem/uid_guid.h>
#include "power_private.h"

/* LED 2 across a suspend */
static uint32_t s_powerLedSavedLevel;
static uint32_t s_powerLedSavedMode;

static SceKernelSpinlock s_powerLedLock;
static struct PowerLedState s_powerLeds[8];
static uint32_t s_powerLedHardwareEnabled;	/* Syscon drives the power LED */
static uint32_t s_powerLedBootEnabled;		/* lit at boot */
static uint32_t s_powerLedSavedIndex;

/* the Syscon pattern of SCE_LED_MODE_LEVEL (24 bytes for command 0x32, 10 for 0x4a) */
static const uint8_t s_powerLedSysconPattern[34] = {
	0, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 1,
	1, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100, 0, 12, 18
};

static int powerLedInitCallback(uint32_t kind, void *argument)
{
	return 0;
}

static void powerLedSetOutput(unsigned int led, int on)
{
	uint8_t gpio = s_powerLeds[led].gpio;

	if (gpio == 0)
		return;
	if (s_powerLeds[led].polarity == on)
		sceGpioPortClear((gpio & 0x20) != 0, gpio & 0x1f);
	else
		sceGpioPortSet((gpio & 0x20) != 0, gpio & 0x1f);
}

static inline __attribute__((always_inline)) void powerLedWriteSysconPattern(const uint8_t *pattern, uint32_t level)
{
	if (SceSysconForDriver_4E0EB57B(0x32, pattern, 24) < 0)
		return;
	if (SceSysconForDriver_4E0EB57B(0x4a, pattern + 24, 10) < 0)
		return;
	SceSysconForDriver_6F586D1A(0x40, 0);
	SceSysconForDriver_6F586D1A(0x40, level);
}

inline int sceLedSetMode(uint32_t led, uint32_t mode, const void *option)
{
	const LedModeParam *param = option;
	struct PowerLedState *state;
	SceKernelIntrStatus intrState;
	uint32_t previous, level;

	state = &s_powerLeds[led];
	if (led > 7 || state->gpio == 0)
		return SCE_LED_ERROR_INVALID_LED;
	if (mode > SCE_LED_MODE_LEVEL || (mode == SCE_LED_MODE_LEVEL && led != 2))
		return SCE_LED_ERROR_INVALID_MODE;

	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerLedLock);
	state->repeatCount = -1;
	state->scriptCursor = NULL;
	previous = state->mode;
	state->sysconLevel = 0;
	state->mode = mode;
	switch (mode) {
	case SCE_LED_MODE_ON:
		state->sysconIndex = param != NULL ? param->sysconIndex : 15;
		state->phase = 0;
		state->onTicks = 1;
		state->offTicks = 0;
		break;
	case SCE_LED_MODE_BLINK:
		if (previous != SCE_LED_MODE_BLINK)
			state->phase = 0;
		if (param != NULL) {
			state->onTicks = param->blink.onTicks;
			state->offTicks = param->blink.offTicks;
			state->repeatCount = param->blink.repeatCount;
			state->endState = param->blink.endState;
		} else {
			state->endState = 0;
			state->onTicks = 4;
			state->offTicks = 4;
		}
		break;
	case SCE_LED_MODE_SCRIPT:
		if (previous != SCE_LED_MODE_SCRIPT)
			state->scriptMode = previous;
		state->scriptCursor = option;
		state->phase = 0;
		state->scriptLoop = NULL;
		state->scriptDepth = 0;
		state->scriptDelayTicks = 1;
		break;
	case SCE_LED_MODE_LEVEL:
		state->sysconLevel = param != NULL ? param->sysconLevel : 1;
		state->phase = 0;
		state->onTicks = 1;
		state->offTicks = 0;
		break;
	default:
		state->phase = 0;
		state->onTicks = 0;
		state->offTicks = 1;
		break;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerLedLock, intrState);

	/* the power LED also goes to Syscon, unless called with interrupts off */
	if (led != 2 || (intrState & 0xc0) != 0)
		return 0;
	if (mode == SCE_LED_MODE_ON && state->sysconIndex <= 14) {
		uint8_t pattern[34] = { 0 };

		pattern[state->sysconIndex] = 100;
		powerLedWriteSysconPattern(pattern, 1);
	} else if (mode == SCE_LED_MODE_LEVEL) {
		level = state->sysconLevel;
		powerLedWriteSysconPattern(s_powerLedSysconPattern, level);
	} else if (mode == SCE_LED_MODE_OFF) {
		sceSysconCtrlLED(0x40, 0);
		return ScePowerForDriver_38415146(1);
	} else {
		sceSysconCtrlLED(0x40, 1);
	}
	return ScePowerForDriver_38415146(0);
}

static int powerLedInitWorker(void *arg)
{
	LedModeParam param = { .size = sizeof(param), .sysconIndex = 9 };
	uint32_t capable;

	SceSysconForDriver_55DF1C9B();
	capable = (SceSysconForDriver_CBD6D8BC() & 0xff0000) < 0x800000;
	if (capable)
		sceSysconCtrlLED(0x40, 1);
	s_powerLedBootEnabled = capable;
	s_powerLedHardwareEnabled = capable;
	if (SceSysrootForDriver_55392965() || SceSysrootForDriver_89D19090() ||
	    SceSysrootForDriver_7918D44E() || SceSysrootForDriver_79C9AE10())
		s_powerLedBootEnabled = 0;
	if (capable) {
		SceSysconForDriver_9CA6EB70(0);
		if (s_powerLedBootEnabled)
			sceLedSetMode(2, SCE_LED_MODE_ON, &param);
	}
	SceSysrootForDriver_246AAAB3(sceLedSetMode);
	return 1;
}

static int powerLedSysEventHandler(int resume, int eventid, void *args, void *opt);
static int powerLedVblankHandler(void *subintrArg, void *registerArg, unsigned char intrPriority);

int powerInitializeLedControl(void)
{
	unsigned int led;

	memset(s_powerLeds, 0, sizeof(s_powerLeds));
	s_powerLeds[0].gpio = 2;
	s_powerLeds[1].gpio = 6;
	s_powerLeds[2].gpio = 7;
	s_powerLeds[3].gpio = 0x20;
	for (led = 0; led < 8; led++) {
		uint8_t gpio = s_powerLeds[led].gpio;

		if (gpio == 0)
			continue;
		SceGpioForDriver_372022A4((gpio & 0x20) != 0, gpio & 0x1f, 0);
		powerLedSetOutput(led, 0);
	}
	sceKernelRegisterSubIntrHandler(8, 0x19, "SceLedVblank", powerLedVblankHandler, NULL);
	sceKernelEnableSubIntr(8, 0x19);
	sceKernelRegisterSysEventHandler("SceLed", powerLedSysEventHandler, NULL);
	sceKernelRegisterInitCallback(powerLedInitCallback, 8, 0, NULL);
	return 0;
}

/*
 * Run an LED's script until it waits. A byte with bit 7 set waits that many
 * ticks (low seven bits); the others are commands.
 */
static inline void powerLedRunScript(struct PowerLedState *state)
{
	const uint8_t *script = state->scriptCursor;
	struct PowerLedScriptLoop *loop;
	int depth;
	uint8_t op;

	while (script != NULL) {
		op = *script++;
		if (op & 0x80) {
			state->scriptDelayTicks = op & 0x7f;
			break;
		}
		switch (op) {
		case 1:		/* mark the restart point */
			state->scriptLoop = script;
			break;
		case 2:		/* restart */
			script = state->scriptLoop;
			break;
		case 3:		/* open a loop: count byte */
			depth = state->scriptDepth;
			if (depth > 3) {
				script = NULL;
				break;
			}
			loop = &state->scriptStack[depth];
			loop->cursor = script + 1;
			loop->count = *script++;
			state->scriptDepth = depth + 1;
			break;
		case 4:		/* close the loop */
			depth = state->scriptDepth;
			if (depth <= 0) {
				script = NULL;
				break;
			}
			loop = &state->scriptStack[depth - 1];
			if (--loop->count <= 0)
				state->scriptDepth--;
			else
				/*
				 * Sony's bug, kept: the jump reads slot depth,
				 * not depth - 1 (at depth four, the first word of
				 * the next LED).
				 */
				script = state->scriptStack[depth].cursor;
			break;
		case 0x10:	/* on */
			state->scriptMode = 1;
			state->onTicks = 1;
			state->offTicks = 0;
			break;
		case 0x11:	/* off */
			state->scriptMode = 0;
			state->onTicks = 0;
			state->offTicks = 1;
			break;
		case 0x12:	/* toggle */
			if (state->scriptMode == 1) {
				state->scriptMode = 0;
				state->onTicks = 1;
				state->offTicks = 0;
			} else {
				state->scriptMode = 1;
				state->onTicks = 0;
				state->offTicks = 1;
			}
			break;
		case 0x13:	/* blink: on and off ticks */
			state->scriptMode = 2;
			state->onTicks = script[0];
			state->offTicks = script[1];
			script += 2;
			break;
		default:
			script = NULL;
			break;
		}
	}
	state->scriptCursor = script;
}

static int powerLedVblankHandler(void *subintrArg, void *registerArg, unsigned char intrPriority)
{
	struct PowerLedState *state;
	SceKernelIntrStatus intrState;
	unsigned int led;
	int phase;

	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerLedLock);
	for (led = 0; led < 8; led++) {
		state = &s_powerLeds[led];
		if (state->gpio == 0)
			continue;
		if (state->scriptDelayTicks > 0 && --state->scriptDelayTicks != 0)
			continue;
		powerLedRunScript(state);
	}
	for (led = 0; led < 8; led++) {
		state = &s_powerLeds[led];
		if (state->gpio == 0)
			continue;
		if (state->repeatCount == 0) {
			/* the repeats are over: settle in the end state */
			state->phase = 0;
			state->onTicks = 0;
			state->offTicks = 0;
			if (state->endState)
				state->onTicks = 1;
			else
				state->offTicks = 1;
			phase = 0;
		} else {
			if (state->repeatCount > 0)
				state->repeatCount--;
			phase = state->phase + 1;
			if (phase >= state->offTicks + state->onTicks)
				phase = 0;
		}
		state->phase = phase;
		powerLedSetOutput(led, phase < state->onTicks);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerLedLock, intrState);
	return -1;
}

static int powerLedSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	const struct PowerSuspendEvent *suspend = args;
	struct PowerLedState *power = &s_powerLeds[2];
	unsigned int led;
	int sleepMode;

	if (resume == 0) {
		if (eventid == SYSEVENT_SUSPEND_START) {
			s_powerLedSavedMode = power->mode;
			s_powerLedSavedIndex = power->sysconIndex;
			s_powerLedSavedLevel = power->mode == SCE_LED_MODE_LEVEL ? power->sysconLevel : 0;
		} else if (eventid == SYSEVENT_SUSPEND_DEVICE(10)) {
			sleepMode = suspend->sleepMode;
			sceKernelDisableSubIntr(8, 0x19);
			for (led = 0; led < 8; led++) {
				if (s_powerLeds[led].gpio == 0)
					continue;
				if (led != 2) {
					powerLedSetOutput(led, 0);
				} else if (sleepMode == POWER_SLEEP_SUSPEND && (s_powerLedSavedLevel & 0x20)) {
					/* keep the Syscon level through the sleep */
					if ((SceSysconForDriver_CBD6D8BC() & 0xff0000) < 0x800000) {
						powerLedSetOutput(2, 0);
						powerLedWriteSysconPattern(s_powerLedSysconPattern,
									   s_powerLedSavedLevel);
						powerLedSetOutput(2, 1);
					}
				} else {
					powerLedSetOutput(2, 0);
					if (sleepMode == POWER_SLEEP_SUSPEND || sleepMode == POWER_SLEEP_POWER_OFF) {
						if (s_powerLedBootEnabled)
							sceSysconCtrlLED(0x40, 0);
						if (s_powerLedHardwareEnabled)
							SceSysconForDriver_9CA6EB70(1);
					}
				}
			}
		}
	} else if (resume == 1 && eventid == SYSEVENT_RESUME_START) {
		for (led = 0; led < 8; led++) {
			uint8_t gpio = s_powerLeds[led].gpio;

			if (gpio == 0)
				continue;
			SceGpioForDriver_372022A4((gpio & 0x20) != 0, gpio & 0x1f, 0);
			powerLedSetOutput(led, 0);
		}
		if (s_powerLedSavedMode == SCE_LED_MODE_ON) {
			LedModeParam param = { .size = sizeof(param),
					       .sysconIndex = s_powerLedSavedIndex };

			if (s_powerLedHardwareEnabled)
				SceSysconForDriver_9CA6EB70(0);
			sceLedSetMode(2, SCE_LED_MODE_ON, &param);
		} else if (s_powerLedSavedLevel != 0) {
			power->sysconLevel = s_powerLedSavedLevel;
			power->mode = SCE_LED_MODE_LEVEL;
			power->phase = 0;
			power->onTicks = 1;
			power->offTicks = 0;
			if (s_powerLedHardwareEnabled)
				SceSysconForDriver_9CA6EB70(0);
			if (s_powerLedBootEnabled)
				powerLedWriteSysconPattern(s_powerLedSysconPattern,
							   s_powerLedSavedLevel);
			powerLedSetOutput(2, 1);
		} else {
			sceLedSetMode(2, SCE_LED_MODE_OFF, NULL);
			if (s_powerLedHardwareEnabled)
				SceSysconForDriver_9CA6EB70(1);
		}
		sceKernelEnableSubIntr(8, 0x19);
	}
	return 0;
}

int powerStartLedControl(void)
{
	powerInitializeLedControl();
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceLedInit", powerLedInitWorker, NULL);
	return 0;
}
