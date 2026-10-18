/*
 * ScePower: the power switch: requests, suspend/resume transitions, switch locks.
 */

#include <sysmem/sysroot.h>
#include <sysmem/cp15.h>
#include "power_private.h"
#include <syscon/syscon_scratchpad.h>

/* the CPU state each core saves before suspend */
static uint8_t s_powerCpuContext[4][0x1000] __attribute__((aligned(8)));
/* optional hooks around the system events of a transition (never set here) */
static void (*s_powerBeforeSysEvent)(int, int, int);
static void (*s_powerAfterSysEvent)(int, int, int, SceUID);

int kscePowerRequestShutdown(uint32_t argument);

int powerAcquireProcessSwitchLock(uint32_t kind);

int powerAcquireSystemSwitchLock(uint32_t kind);

int powerReleaseProcessSwitchLock(uint32_t kind);

int powerReleaseSystemSwitchLock(uint32_t kind);

int scePowerRequestStandby(void);

int scePowerRequestColdReset(void);

static inline __attribute__((always_inline)) int powerDispatchSwitchEvent(int phase, int event, int sleepMode,
				    const void *argument, SceUID *busy);

static void powerReadMonitorRegisters(struct PowerMonitorState *state);

static struct PowerSwitchState s_powerSwitchState;

static int32_t s_powerRequestMask;
static struct PowerResumeInformation s_powerResumeInformation;
/* also read by powerCpuResumeEntry (power_cpu.S) */
void *s_powerScratchPad;

static const SuspendPowerCallbacks s_powerSuspendCallbacks = {
	.size = sizeof(SuspendPowerCallbacks),
	.lockSuspend = powerAcquireSystemSwitchLock,
	.unlockSuspend = powerReleaseSystemSwitchLock,
	.powerLock = powerAcquireProcessSwitchLock,
	.powerUnlock = powerReleaseProcessSwitchLock,
	.requestStandby = scePowerRequestStandby,
	.requestSuspend = scePowerRequestSuspend,
	.requestColdReset = scePowerRequestColdReset,
	.requestShutdown = kscePowerRequestShutdown,
	.powerTick = kscePowerTick,
	.tickProcess = powerTickProcessForPid,
	.setIdleCallback = kscePowerSetIdleCallback,
	.expireIdleTimer = ScePowerForDriver_660D5AB4,
	.addIdleTimerMask = ScePowerForDriver_991BDEAF,
};
void powerSysconWakeupCallback(uint32_t value)
{
	if (value)
		s_powerSwitchState.resumeRequested = 1;
}

void powerHdmiCecCallback(int event)
{
	(void)event;
	ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x800);
}

int powerSwitchInterrupt(void *argument)
{
	uint32_t bit = 1U << (sceKernelCpuId() & 0xff);
	uint32_t intrState;
	unsigned int cpu;

	SceCpuForDriver_BC248C30((SceInt32 *)&s_powerSwitchState.interruptCpuMask, bit);
	dsb();
	while (s_powerSwitchState.interruptState & 1)
		wfe();
	if (s_powerSwitchState.interruptState & 2) {
		intrState = SceCpuForDriver_821FC0EE();
		SceKernelSuspendForDriver_D4958E6F(0);
		cpu = sceKernelCpuId();
		if (powerSaveCpuContext(s_powerCpuContext[cpu])) {
			while (s_powerSwitchState.interruptState & 0x100)
				wfe();
			sceKernelCpuResumeIntr(intrState);
			return -1;
		}
		powerSmcSuspendSecondary();
		SceCpuForDriver_BC248C30((SceInt32 *)&s_powerSwitchState.interruptCpuMask,
					   bit);
		dsb();
		for (;;)
			;
	}
	SceCpuForDriver_DF899E4B((SceInt32 *)&s_powerSwitchState.interruptCpuMask, ~bit);
	return -1;
}

int scePowerRequestSuspend(void)
{
	uint32_t context = sceSyscallEnter();
	int result = 0;

	if (SceSysrootForDriver_834439A7() || SceSysrootForDriver_B0E1FC67()) {
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10);
	} else if (!SceSysrootForDriver_89D19090() &&
		   !SceSysrootForDriver_7918D44E()) {
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x20);
	} else {
		result = SCE_POWER_ERROR_CANT_SUSPEND;
	}
	sceSyscallLeave(context);
	return result;
}

int kscePowerRequestShutdown(uint32_t argument)
{
	uint32_t context = sceSyscallEnter();
	SceUID eventFlag = s_powerSwitchState.eventFlag;

	s_powerSwitchState.shutdownArgument = argument;
	ksceKernelSetEventFlag(eventFlag, 0x200);
	sceSyscallLeave(context);
	return 0;
}

static inline int powerAcquireSwitchProcessLockCount(volatile uint32_t *count)
{
	SceKernelIntrStatus intrState;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, ~0x10000U);
	SceCpuForDriver_5F6A8743((int32_t *)count, 1);
	s_powerSwitchState.processLockCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	if (!s_powerSwitchState.suspendInProgress)
		return 0;
	return ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag, 0x40000,
				       SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
}

/* Appended to process.c; storage key and object layout remain private. */

int powerAcquireProcessSwitchLock(uint32_t kind)
{
	struct PowerProcessState *state;

	if (kind)
		return SCE_POWER_ERROR_INVALID_VALUE;
	state = ksceKernelGetProcessLocalStorageAddr(s_processStorageKey);
	if (!state)
		return SCE_POWER_ERROR_INVALID_VALUE;
	return powerAcquireSwitchProcessLockCount(&state->powerLockCount);
}

inline __attribute__((always_inline)) int powerAcquireSystemSwitchLock(uint32_t kind)
{
	SceKernelIntrStatus intrState;

	if (kind)
		return SCE_POWER_ERROR_INVALID_VALUE;
	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, ~0x10000U);
	s_powerSwitchState.systemLockCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

static inline int powerReleaseSwitchProcessLock(volatile uint32_t *count)
{
	SceKernelIntrStatus intrState;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if ((int32_t)*count > 0) {
		(*count)--;
		s_powerSwitchState.processLockCount--;
	}
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10000);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

int powerReleaseProcessSwitchLock(uint32_t kind)
{
	struct PowerProcessState *state;

	if (kind)
		return SCE_POWER_ERROR_INVALID_VALUE;
	state = ksceKernelGetProcessLocalStorageAddr(s_processStorageKey);
	if (!state)
		return SCE_POWER_ERROR_INVALID_VALUE;
	return powerReleaseSwitchProcessLock(&state->powerLockCount);
}

int powerReleaseSystemSwitchLock(uint32_t kind)
{
	SceKernelIntrStatus intrState;

	if (kind)
		return SCE_POWER_ERROR_INVALID_VALUE;
	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if ((int32_t)s_powerSwitchState.systemLockCount > 0)
		s_powerSwitchState.systemLockCount--;
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10000);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

int scePowerRequestStandby(void)
{
	uint32_t context = sceSyscallEnter();

	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID ||
	    SceSysrootForDriver_834439A7() || SceSysrootForDriver_B0E1FC67() ||
	    SceSysrootForDriver_55392965() || sceSblACMgrIsSystemProgram(0))
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10);
	sceSyscallLeave(context);
	return 0;
}

int scePowerRequestColdReset(void)
{
	uint32_t context = sceSyscallEnter();

	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID ||
	    SceSysrootForDriver_834439A7() || SceSysrootForDriver_B0E1FC67() ||
	    SceSysrootForDriver_55392965() ||
	    sceSblACMgrIsSystemProgram(0))
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x80);
	sceSyscallLeave(context);
	return 0;
}

int powerSwitchTransition(uint32_t mode)
{
	struct PowerMonitorState monitor;
	struct PowerSuspendEvent suspend = {.size = 0x18};
	struct PowerResumeEvent resume = {.size = 8};
	LedModeParam led = { 0 };
	uint32_t address = 0, zero = 0;
	int displayOff;
	uint32_t intrState, seen, started, now;
	SceUID busy = -1, callback = -1;
	unsigned int request = mode & 0xf, action;
	int powerOff;
	int ret, i, idleLoops = 0, diagnostics = 0;
	uint32_t sysconStatus = 0;

	memset(&monitor, 0, sizeof(monitor));
	monitor.size = sizeof(monitor);
	monitor.powerModeArgument = s_powerSwitchState.powerModeArgument;
	monitor.resumeInformation = &s_powerResumeInformation;
	monitor.resumeEntry = powerCpuResumeEntry;
	suspend.monitorState = &monitor;
	resume.resumeInformation = NULL;
	SceSysmemForDriver_8D160E65(&monitor, &address);
	ksceSysconSendCommand(SYSCON_SCRATCH_PAD(resumeContextPA), &address, 4);
	switch (request) {
	case POWER_REQUEST_STANDBY:
	case POWER_REQUEST_SHUTDOWN:
	case POWER_REQUEST_HIBERNATE:
		suspend.sleepMode = POWER_SLEEP_POWER_OFF;
		break;
	case POWER_REQUEST_COLD_RESET:
		suspend.sleepMode = POWER_SLEEP_COLD_RESET;
		break;
	}
	intrState = SceCpuForDriver_821FC0EE();
	ret = powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_QUERY, suspend.sleepMode, &suspend,
				       &busy);
	if (ret < 0) {
		powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_CANCEL, suspend.sleepMode, &suspend,
					 NULL);
		sceKernelCpuResumeIntr(intrState);
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
					 0xfffff102);
		return ret;
	}
	s_powerSwitchState.resumeRequested = 0;
	powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_START, suspend.sleepMode, &suspend, NULL);
	sceKernelCpuResumeIntr(intrState);
	/* blink the power LED: slowly when powering off, quickly otherwise */
	switch (request) {
	case POWER_REQUEST_STANDBY:
	case POWER_REQUEST_SUSPEND:
	case POWER_REQUEST_SOFT_RESET:
	case POWER_REQUEST_SHUTDOWN:
	case POWER_REQUEST_HIBERNATE:
		powerOff = request == POWER_REQUEST_STANDBY || request == POWER_REQUEST_SHUTDOWN ||
			   request == POWER_REQUEST_HIBERNATE;
		if (s_powerSwitchState.ledPolicy == 1) {
			SceSysconForDriver_9CA6EB70(powerOff ? 0 : 1);
			led.size = sizeof(led);
			led.blink.onTicks = powerOff ? 0x19 : 9;
			led.blink.offTicks = led.blink.onTicks;
			led.blink.repeatCount = -1;
			ksceLedSetMode(2, SCE_LED_MODE_BLINK, &led);
			ksceSysconCtrlLED(0x40, s_powerSwitchState.ledPolicy);
		} else if (s_powerSwitchState.ledPolicy == 2) {
			ksceSysconCtrlDolceLED(powerOff ? 1 : 2);
		}
		break;
	}
	powerNotifyCallback(0, SCE_POWER_CB_SYSTEM_SUSPEND, 0);
	s_powerSwitchState.suspendInProgress = 1;
	for (i = SYSEVENT_SUSPEND_PREPARE(15); i >= SYSEVENT_SUSPEND_PREPARE(0); i--)
		powerDispatchSwitchEvent(0, i, suspend.sleepMode, &suspend,
					 NULL);
	ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, ~0x40000U);
	SceSysconForDriver_D7BEFF8B(&sysconStatus);
	intrState = SceCpuForDriver_821FC0EE();
	powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_IDLE_ENTER, suspend.sleepMode, &suspend, NULL);
	for (;;) {
		busy = -1;
		sceKernelCpuResumeIntr(intrState);
		sceKernelDelayThread(10000);
		if (idleLoops != -1) {
			idleLoops++;
			if ((!s_powerSwitchState.powerOffDelay &&
			     idleLoops > 0x833) ||
			    (s_powerSwitchState.powerOffDelay &&
			     s_powerSwitchState.powerOffDelay <
				     (unsigned int)(idleLoops / 100))) {
				if (s_powerSwitchState.ledPolicy == 1)
					ksceLedSetMode(2, SCE_LED_MODE_ON, NULL);
				else if (s_powerSwitchState.ledPolicy == 2)
					ksceSysconCtrlDolceLED(0);
				if (s_powerSwitchState.powerOffDelay)
					SceSysconForDriver_373ECF8A(5);
				idleLoops = -1;
				SceSysconForDriver_901D6CD4(1);
				SceCtrlForDriver_C8CE3E43(0);
				SceSysconForDriver_0D0B6D25();
			}
		}
		powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_IDLE, suspend.sleepMode, &suspend,
					 NULL);
		s_powerSwitchState.interruptState = 1;
		dsb();
		ksceKernelTriggerSGI(10, 0, 7);
		started = ksceKernelGetSystemTimeLow();
		while ((seen = s_powerSwitchState.interruptCpuMask) != 7) {
			now = ksceKernelGetSystemTimeLow();
			if (now - started > 999999) {
				SceDebugForDriver_391B74B7("[ScePower]: SGI interrupt "
						 "busy: %s%s%s\n",
						 (seen & 1) ? "" : " CPU0",
						 (seen & 2) ? "" : " CPU1",
						 (seen & 4) ? "" : " CPU2");
				started = now;
			}
		}
		intrState = SceCpuForDriver_821FC0EE();
		ret = powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_IDLE_QUERY, suspend.sleepMode,
					       &suspend, &busy);
		if (ret < 0) {
			if (++diagnostics == 500) {
				diagnostics = 0;
				if (busy != -1)
					SceDebugForDriver_391B74B7(
						"[ScePower]: Suspend busy "
						"0x%08X(%s) 0x%08X\n",
						busy, SceSysmemForDriver_E655852F(busy), ret);
			}
		} else if (!s_powerSwitchState.systemLockCount) {
			if (!powerHasBusyCallback(0x10000, &callback))
				break;
			if (++diagnostics == 500) {
				diagnostics = 0;
				SceDebugForDriver_391B74B7("[ScePower]: Power callback "
						 "busy 0x%08X\n",
						 callback);
			}
		} else if (++diagnostics == 500) {
			diagnostics = 0;
			SceDebugForDriver_391B74B7("[ScePower]: Kernel power lock\n");
		}
		s_powerSwitchState.interruptState = 0;
		dsb();
	}
	suspend.started = sceKernelGetSystemTimeWide();
	powerDispatchSwitchEvent(0, SYSEVENT_SUSPEND_COMMIT, suspend.sleepMode, &suspend, NULL);
	if (s_powerSwitchState.ledPolicy == 1)
		ksceLedSetMode(2, SCE_LED_MODE_ON, NULL);
	for (i = SYSEVENT_SUSPEND_DEVICE(15); i >= SYSEVENT_SUSPEND_DEVICE(0); i--)
		powerDispatchSwitchEvent(0, i, suspend.sleepMode, &suspend,
					 NULL);
	s_powerSwitchState.interruptState = 2;
	s_powerSwitchState.interruptCpuMask = 0;
	dsb();
	SceKernelSuspendForDriver_D4958E6F(0);
	while (s_powerSwitchState.interruptCpuMask != 7)
		;
	s_powerSwitchState.interruptState = 0x100;
	for (i = 0; i < 6; i++)
		kscePervasiveUartResetEnable(i);
	ScePervasiveForDriver_81A155F1()->suspendControl = 0x80000000;
	dmb();
	ksceGpioPortClear(0, 0);
	ksceGpioPortClear(0, 15);
	if (suspend.sleepMode == POWER_SLEEP_SUSPEND)
		ksceGpioPortSet(0, 7);
	ksceGpioPortClear(1, 3);
	powerReadMonitorRegisters(&monitor);
	action = mode & 0xf00;
	s_powerSwitchState.suspendInProgress = 0;
	if (action == 0x300 || action == 0x200) {
		if (!powerSaveCpuContext(
			    s_powerCpuContext[sceKernelCpuId()])) {
			ksceSysconSetPowerMode(
				action == 0x300 || s_powerSwitchState
							   .resumeRequested
					? 0x11
					: 1,
				s_powerSwitchState.powerModeArgument);
			for (;;)
				;
		}
	} else if (action == 0x100) {
		ksceSysconSetPowerMode(0, s_powerSwitchState.powerModeArgument);
		for (;;)
			;
	} else if (action == 0x500) {
		i = ksceSysconIsDownLoaderMode();
		ksceSysconSetPowerMode(
			i ? (s_powerSwitchState.shutdownArgument ? 4 : 3)
			  : (s_powerSwitchState.shutdownArgument ? 0x10 : 0),
			0);
		for (;;)
			;
	} else if (action == 0x600) {
		SceSysconForDriver_4278E614(0x5a);
		for (;;)
			;
	} else if (action == 0x400) {
		SceSysconForDriver_14B99945();
		for (;;)
			;
	}
	s_powerSwitchState.requestState = s_powerResumeInformation.requestState;
	s_powerSwitchState.wakeupFactor = s_powerResumeInformation.wakeupFactor;
	if (s_powerSwitchState.primaryMode & 4)
		s_powerSwitchState.primaryMode = 0;
	resume.resumeInformation = &s_powerResumeInformation;
	for (i = SYSEVENT_RESUME_DEVICE(0); i <= SYSEVENT_RESUME_DEVICE(15); i++)
		powerDispatchSwitchEvent(1, i, 0, &resume, NULL);
	ksceLedSetMode(2, SCE_LED_MODE_OFF, NULL);
	ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, 0xfffff102);
	powerNotifyCallback(SCE_POWER_CB_SYSTEM_SUSPEND, SCE_POWER_CB_SYSTEM_RESUMING, 0);
	/* some wakeup factors resume with the display off */
	switch (s_powerSwitchState.wakeupFactor & 0x7f) {
	case 1:
	case 2:
	case 15:
	case 29:
	case 30:
		displayOff = !SceSysrootForDriver_55392965();
		break;
	default:
		displayOff = 0;
		break;
	}
	if (displayOff) {
		powerSwitchDisplay(0, 0);
		kscePowerTick(1);
	} else {
		powerSwitchDisplay(1, 0);
		kscePowerTick(0);
	}
	SceSysconForDriver_901D6CD4(0);
	SceCtrlForDriver_C8CE3E43(1);
	powerDispatchSwitchEvent(1, SYSEVENT_RESUME_INTR, 0, &resume, NULL);
	sceKernelCpuResumeIntr(intrState);
	s_powerSwitchState.interruptState = 0;
	dsb();
	powerDispatchSwitchEvent(1, SYSEVENT_RESUME_START, 0, &resume, NULL);
	ksceSysconSendCommand(SYSCON_SCRATCH_PAD(resumeContextPA), &zero, 4);
	if (s_powerSwitchState.powerOffDelay)
		SceSysconForDriver_373ECF8A(s_powerSwitchState.powerOffDelay);
	diagnostics = 0;
	for (;;) {
		busy = -1;
		powerDispatchSwitchEvent(1, SYSEVENT_RESUME_IDLE, 0, &resume, NULL);
		ret = powerDispatchSwitchEvent(1, SYSEVENT_RESUME_IDLE_QUERY, 0, &resume, &busy);
		if (ret >= 0)
			break;
		if (++diagnostics == 500) {
			diagnostics = 0;
			if (busy != -1)
				SceDebugForDriver_391B74B7("[ScePower]: Resume busy "
						 "0x%08X(%s) 0x%08X\n",
						 busy, SceSysmemForDriver_E655852F(busy), ret);
		}
		sceKernelDelayThread(5000);
	}
	powerDispatchSwitchEvent(1, SYSEVENT_RESUME_COMPLETE, 0, &resume, NULL);
	powerNotifyCallback(SCE_POWER_CB_SYSTEM_RESUMING, 0, SCE_POWER_CB_SYSTEM_RESUME);
	return 0;
}

/*
 * The power switch thread: the power and PS buttons (through the controller
 * callback), HDMI-CEC, and the requests the API leaves in the event flag.
 * held is set after a press has been acted on, so a button still down from it
 * is ignored until released.
 */
int powerSwitchWorker(SceSize args, void *argp)
{
	SceCtrlData ctrl;
	SceUInt32 pattern, timeout = 0;
	uint32_t buttons, pressed, modes;
	int held, woke, ret;

	ksceSysconSendCommand(SYSCON_SCRATCH_PAD(resumeContextPA), &timeout, 4);
	if (ksceCtrlPeekBufferPositive(0, &ctrl, 1) >= 0)
		s_powerSwitchState.controllerButtons = ctrl.buttons;
	held = (s_powerSwitchState.controllerButtons & s_powerSwitchState.controllerMask) != 0;

	for (;;) {
		ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
					POWER_SWITCH_EVENT_EXIT | POWER_SWITCH_EVENT_HDMI_CEC |
					POWER_SWITCH_EVENT_REQUESTS | POWER_SWITCH_EVENT_BUTTONS,
					SCE_KERNEL_EVF_WAITMODE_OR, &pattern, NULL);
		if (pattern & POWER_SWITCH_EVENT_EXIT)
			break;
		if (pattern & POWER_SWITCH_EVENT_REQUESTS)
			goto request;

		buttons = s_powerSwitchState.controllerButtons;
		if (held && (buttons & s_powerSwitchState.controllerMask)) {
			if (!ScePowerForDriver_394DE492())
				continue;
			woke = 0;
			buttons = 0;
		} else {
			pressed = buttons & (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON);
			held = 0;
			if (ScePowerForDriver_394DE492()) {
				woke = 0;
			} else if (pressed) {
				/* the first press only wakes the display */
				powerSwitchDisplay(1, 1);
				woke = 1;
			} else {
				continue;
			}
			if (pressed == (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON))
				goto powerAndPs;
		}

buttonsDown:
		if ((buttons & (SCE_CTRL_PSBUTTON | SCE_CTRL_START)) ==
		    (SCE_CTRL_PSBUTTON | SCE_CTRL_START)) {
			held = 1;
			powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_PS_START_PRESS);
			continue;
		}
		if (buttons & SCE_CTRL_POWER) {
			timeout = s_powerSwitchState.standbyButtonPushTime;
			ret = ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
						      POWER_SWITCH_EVENT_EXIT |
						      POWER_SWITCH_EVENT_REQUESTS |
						      POWER_SWITCH_EVENT_BUTTONS,
						      SCE_KERNEL_EVF_WAITMODE_OR, &pattern,
						      &timeout);
			if (pattern & POWER_SWITCH_EVENT_EXIT)
				break;
			buttons = s_powerSwitchState.controllerButtons;
			modes = s_powerSwitchState.primaryMode | s_powerSwitchState.secondaryMode;
			if ((buttons & (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON)) ==
			    (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON))
				goto powerAndPs;
			if (!woke && !(buttons & SCE_CTRL_POWER)) {
				/* a short press */
				if (modes & POWER_SW_MODE_PRESS_DISPLAY_OFF) {
					powerSwitchDisplay(0, 2);
					powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_POWER_PRESS);
					continue;
				}
				if (modes & POWER_SW_MODE_PRESS_SUSPEND) {
					s_powerSwitchState.buttonRequest = POWER_REQUEST_SUSPEND;
				} else if (modes & POWER_SW_MODE_PRESS_STANDBY) {
					s_powerSwitchState.buttonRequest = POWER_REQUEST_STANDBY;
				} else {
					powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_POWER_PRESS);
					continue;
				}
			} else if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				/* held for the standby push time */
				if (modes & POWER_SW_MODE_HOLD_STANDBY) {
					s_powerSwitchState.buttonRequest = POWER_REQUEST_STANDBY;
					goto request;
				}
				powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_POWER_HOLD);
				/* still held four seconds after the press: standby */
				if (s_powerSwitchState.standbyButtonPushTime < 4000000)
					timeout = 4000000 - s_powerSwitchState.standbyButtonPushTime;
				else
					timeout = 4000000;
				for (;;) {
					ret = ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
								      POWER_SWITCH_EVENT_EXIT |
								      POWER_SWITCH_EVENT_REQUESTS |
								      POWER_SWITCH_EVENT_BUTTONS,
								      SCE_KERNEL_EVF_WAITMODE_OR,
								      &pattern, &timeout);
					if (pattern & POWER_SWITCH_EVENT_EXIT)
						break;
					if (pattern & POWER_SWITCH_EVENT_REQUESTS)
						goto request;
					if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
						break;
					buttons = s_powerSwitchState.controllerButtons;
					if (!(buttons & SCE_CTRL_POWER) || timeout == 0)
						break;
				}
				if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
					s_powerSwitchState.buttonRequest = POWER_REQUEST_STANDBY;
					goto request;
				}
				if (buttons & s_powerSwitchState.controllerMask)
					held = 1;
				continue;
			}
		} else if (buttons & SCE_CTRL_PSBUTTON) {
			timeout = s_powerSwitchState.psButtonPushTime;
			ret = ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
						      POWER_SWITCH_EVENT_EXIT |
						      POWER_SWITCH_EVENT_REQUESTS |
						      POWER_SWITCH_EVENT_BUTTONS,
						      SCE_KERNEL_EVF_WAITMODE_OR, &pattern,
						      &timeout);
			if (pattern & POWER_SWITCH_EVENT_EXIT)
				break;
			buttons = s_powerSwitchState.controllerButtons;
			if ((buttons & (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON)) ==
			    (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON))
				goto powerAndPs;
			if ((buttons & (SCE_CTRL_PSBUTTON | SCE_CTRL_START)) ==
			    (SCE_CTRL_PSBUTTON | SCE_CTRL_START)) {
				held = 1;
				powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_PS_START_PRESS);
				continue;
			}
			if (!woke && !(buttons & SCE_CTRL_PSBUTTON)) {
				/* a short press */
				if (SceSysrootForDriver_89532E2F() != 1)
					powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_PS_PRESS);
				continue;
			}
			if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				held = 1;
				powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_PS_HOLD);
				continue;
			}
		} else if (pattern & POWER_SWITCH_EVENT_HDMI_CEC) {
			/* the TV's power key: a short press of the power button */
			ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
						 ~POWER_SWITCH_EVENT_HDMI_CEC);
			modes = s_powerSwitchState.primaryMode | s_powerSwitchState.secondaryMode;
			if (modes & POWER_SW_MODE_PRESS_DISPLAY_OFF) {
				powerSwitchDisplay(0, 2);
				powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_POWER_PRESS);
				continue;
			}
			if (modes & POWER_SW_MODE_PRESS_SUSPEND)
				s_powerSwitchState.buttonRequest = POWER_REQUEST_SUSPEND;
			else if (modes & POWER_SW_MODE_PRESS_STANDBY)
				s_powerSwitchState.buttonRequest = POWER_REQUEST_STANDBY;
			else
				powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_POWER_PRESS);
		}

request:
		if (pattern & POWER_SWITCH_EVENT_COLD_RESET) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_COLD_RESET;
		} else if (pattern & POWER_SWITCH_EVENT_STANDBY) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_STANDBY;
		} else if (pattern & POWER_SWITCH_EVENT_SUSPEND) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_SUSPEND;
		} else if (pattern & POWER_SWITCH_EVENT_SOFT_RESET) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_SOFT_RESET;
		} else if (pattern & POWER_SWITCH_EVENT_SHUTDOWN) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_SHUTDOWN;
		} else if (pattern & POWER_SWITCH_EVENT_HIBERNATE) {
			s_powerSwitchState.pendingRequest = POWER_REQUEST_HIBERNATE;
		} else if (pattern & POWER_SWITCH_EVENT_DISPLAY_ON) {
			ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
						 ~POWER_SWITCH_EVENT_DISPLAY_ON);
			powerSwitchDisplay(1, 1);
			continue;
		} else if (pattern & POWER_SWITCH_EVENT_DISPLAY_OFF) {
			ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
						 ~POWER_SWITCH_EVENT_DISPLAY_OFF);
			powerSwitchDisplay(0, 2);
			continue;
		}
		if (s_powerSwitchState.pendingRequest == POWER_REQUEST_NONE &&
		    s_powerSwitchState.buttonRequest == POWER_REQUEST_NONE)
			continue;

		/* wait until no process holds the switch lock */
		ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
					POWER_SWITCH_EVENT_EXIT | POWER_SWITCH_EVENT_UNLOCKED,
					SCE_KERNEL_EVF_WAITMODE_OR, &pattern, NULL);
		if (pattern & POWER_SWITCH_EVENT_EXIT)
			break;
		if (pattern & POWER_SWITCH_EVENT_COLD_RESET)
			s_powerSwitchState.pendingRequest = POWER_REQUEST_COLD_RESET;
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, POWER_SWITCH_EVENT_BUSY);
		switch (s_powerSwitchState.pendingRequest) {
		case POWER_REQUEST_COLD_RESET:
			powerSwitchTransition(0x404);
			break;
		case POWER_REQUEST_STANDBY:
			powerSwitchTransition(0x101);
			break;
		case POWER_REQUEST_SUSPEND:
			powerSwitchTransition(0x202);
			break;
		case POWER_REQUEST_SOFT_RESET:
			powerSwitchTransition(0x303);
			break;
		case POWER_REQUEST_SHUTDOWN:
			powerSwitchTransition(0x505);
			break;
		case POWER_REQUEST_HIBERNATE:
			powerSwitchTransition(0x606);
			break;
		default:
			if (s_powerSwitchState.buttonRequest == POWER_REQUEST_STANDBY)
				powerSwitchTransition(0x101);
			else if (s_powerSwitchState.buttonRequest == POWER_REQUEST_SUSPEND)
				powerSwitchTransition(0x202);
			break;
		}
		s_powerSwitchState.buttonRequest = POWER_REQUEST_NONE;
		s_powerSwitchState.pendingRequest = POWER_REQUEST_NONE;
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, ~POWER_SWITCH_EVENT_BUSY);
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, POWER_SWITCH_EVENT_IDLE);

		/* woken by the power button: it may still be down */
		if (ksceCtrlPeekBufferPositive(0, &ctrl, 1) < 0)
			continue;
		buttons = ctrl.buttons;
		s_powerSwitchState.controllerButtons = buttons;
		if (!(buttons & SCE_CTRL_POWER))
			continue;
		woke = 1;
		if ((buttons & (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON)) !=
		    (SCE_CTRL_POWER | SCE_CTRL_PSBUTTON))
			goto buttonsDown;
powerAndPs:
		held = 1;
		powerNotifyCallback(0, 0, SCE_POWER_CB_BUTTON_PS_POWER_PRESS);
	}
	return 0;
}

static inline __attribute__((always_inline)) void powerReleaseSwitchInitHold(void)
{
	SceKernelIntrStatus intrState;

	s_powerSwitchState.initHold = 0;
	intrState = SceCpuForDriver_821FC0EE();
	/* Native volatile read precedes the reset. */
	(void)s_powerSwitchState.buttonRequest;
	s_powerSwitchState.buttonRequest = 0;
	sceKernelCpuResumeIntr(intrState);
	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if ((int32_t)s_powerSwitchState.systemLockCount > 0)
		s_powerSwitchState.systemLockCount--;
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10000);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

int powerSwitchInitPhase7(uint32_t kind, void *argument)
{
	if (kind == 1 && s_powerSwitchState.initHold)
		powerReleaseSwitchInitHold();
	return 0;
}

void powerControllerCallback(uint32_t buttons, uint32_t previous)
{
	s_powerSwitchState.controllerButtons = buttons;
	if ((buttons ^ previous) & 0x10000) {
		if (!(buttons & 0x10000))
			ksceCtrlUpdateMaskForNonShell(0, 8);
		else if (!(buttons & 8))
			ksceCtrlUpdateMaskForNonShell(8, 0);
	}
	ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 1);
	ksceKernelClearEventFlag(s_powerSwitchState.eventFlag, ~1U);
}

int powerSwitchInitPhase8(uint32_t kind, void *argument)
{
	if (s_powerSwitchState.initHold)
		powerReleaseSwitchInitHold();
	return 0;
}

int powerInitializeSwitchControl(void)
{
	SceKernelAllocMemBlockKernelOpt option;
	SceUID block;
	int addedLocks;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.attr = 2;
	option.paddr = 0x1f000000;
	block = SceSysmemForDriver_C94850C9("ScePowerScratchPad32KiB", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
					0x8000, &option);
	if (block < 0)
		return block;
	SceSysmemForDriver_A841EDDA(block, &s_powerScratchPad);
	memset((void *)&s_powerSwitchState, 0, sizeof(s_powerSwitchState));
	s_powerSwitchState.primaryMode = 2;
	s_powerSwitchState.standbyButtonPushTime = 2000000;
	s_powerSwitchState.psButtonPushTime = 1000000;
	if (SceSysrootForDriver_834439A7()) {
		s_powerSwitchState.primaryMode = 1;
		s_powerSwitchState.standbyButtonPushTime = 4000000;
	} else if (SceSysrootForDriver_B0E1FC67()) {
		s_powerSwitchState.primaryMode = 0;
	} else if (SceSysrootForDriver_89D19090() ||
		   SceSysrootForDriver_7918D44E()) {
		s_powerSwitchState.primaryMode = 9;
	}
	s_powerSwitchState.powerModeArgument = 2;
	ksceKernelRegisterIntrHandler(
		10, "ScePowerMain", 0,
		(SceKernelIntrHandler)powerSwitchInterrupt, NULL, 0x50, 0xf,
		NULL);
	s_powerSwitchState.eventFlag = ksceKernelCreateEventFlag(
		"ScePowerMain", SCE_KERNEL_EVF_ATTR_MULTI, 0x50000, NULL);
	s_powerSwitchState.mainThread =
		ksceKernelCreateThread("ScePowerMain", powerSwitchWorker, 0x10,
				       0x2000, 0, 0x80000, NULL);
	ksceKernelStartThread(s_powerSwitchState.mainThread, 0, NULL);
	s_powerSwitchState.controllerMask = 0x40010008;
	s_powerSwitchState.controllerCallback = SceCtrlForDriver_FF9D4DD4(
		0x40010008, powerControllerCallback, NULL, 0);
	sceHdmiCecRegisterEventCallback(1, powerHdmiCecCallback);
	SceSysconForDriver_9F8340FF(powerSysconWakeupCallback, NULL);
	powerAcquireSystemSwitchLock(0);
	sceKernelRegisterInitCallback(powerSwitchInitPhase7, 7, 0, NULL);
	sceKernelRegisterInitCallback(powerSwitchInitPhase8, 8, 0, NULL);
	addedLocks =
		SceKernelSuspendForDriver_250ACD90(&s_powerSuspendCallbacks);
	s_powerSwitchState.initHold = 1;
	s_powerSwitchState.systemLockCount += addedLocks;
	return 0;
}

int powerInitializeSwitchHardware(void)
{
	uint32_t hardware;
	uint8_t value = 0x1c;

	SceSysconForDriver_CF5B2F2F(&s_powerSwitchState.wakeupFactor);
	s_powerSwitchState.powerOffDelay = 0x14;
	hardware = ksceSysconGetHardwareInfo() & 0xff0000;
	if (hardware == 0x310000 || hardware == 0x410000 ||
	    hardware == 0x100000)
		s_powerSwitchState.powerOffDelay = 0;
	s_powerSwitchState.ledPolicy =
		!SceSblAIMgrForDriver_C6E83F34() && hardware < 0x800000 ? 1 : 2;
	if (SceSysrootForDriver_55392965()) {
		s_powerSwitchState.powerOffDelay = 0;
		s_powerSwitchState.ledPolicy = 0;
	} else if (s_powerSwitchState.powerOffDelay) {
		SceSysconForDriver_373ECF8A(s_powerSwitchState.powerOffDelay);
	}
	SceSysconForDriver_4E0EB57B(0x2f, &value, 1);
	return 0;
}

/* Native display handler specialized for off with no LED work request. */
int kscePowerSetPowerSwMode2(uint32_t mode)
{
	if (mode & ~7U)
		return SCE_POWER_ERROR_INVALID_VALUE;
	s_powerSwitchState.secondaryMode = mode;
	if (((mode | s_powerSwitchState.primaryMode) & 4) && powerIsIdleTimer(2))
		powerSwitchDisplay(0, 0);
	return 0;
}

int kscePowerSetPowerSwMode(uint32_t mode)
{
	uint32_t context;

	if (mode & ~7U)
		return SCE_POWER_ERROR_INVALID_VALUE;
	context = sceSyscallEnter();
	s_powerSwitchState.primaryMode = mode;
	if (((mode | s_powerSwitchState.secondaryMode) & 4) && powerIsIdleTimer(2))
		powerSwitchDisplay(0, 0);
	sceSyscallLeave(context);
	return 0;
}

uint32_t kscePowerGetPowerSwMode(void)
{
	uint32_t mode = s_powerSwitchState.secondaryMode;
	uint32_t otherMode = s_powerSwitchState.primaryMode;

	return mode | otherMode;
}

int kscePowerSetStandbyButtonPushTime(uint32_t time)
{
	s_powerSwitchState.standbyButtonPushTime = time;
	return 0;
}

int kscePowerSetPsButtonPushTime(uint32_t time)
{
	s_powerSwitchState.psButtonPushTime = time;
	return 0;
}

static inline int powerReleaseSwitchProcessLockCount(volatile uint32_t *count)
{
	SceKernelIntrStatus intrState;
	uint32_t locks;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	locks = *count;
	if ((int32_t)locks > 0) {
		*count = 0;
		s_powerSwitchState.processLockCount -= locks;
	}
	if (!s_powerSwitchState.processLockCount && !s_powerSwitchState.systemLockCount)
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x10000);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

int powerReleaseProcessSwitchLocks(SceUID pid)
{
	struct PowerProcessState *state;
	int result;

	result = ksceKernelGetProcessLocalStorageAddrForPid(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		return result;
	if (!state)
		return 0;
	return powerReleaseSwitchProcessLockCount(&state->powerLockCount);
}

/* The two request-state words have unresolved individual meanings. */
int scePowerIsRequest(void)
{
	uint32_t first = s_powerSwitchState.buttonRequest;
	uint32_t second = s_powerSwitchState.pendingRequest;

	return (first | second) != 0;
}

uint32_t scePowerCancelRequest(void)
{
	SceKernelIntrStatus intrState;
	uint32_t request;

	intrState = SceCpuForDriver_821FC0EE();
	request = s_powerSwitchState.buttonRequest;
	s_powerSwitchState.buttonRequest = 0;
	sceKernelCpuResumeIntr(intrState);
	return request;
}

int scePowerRequestDisplayOff(void)
{
	uint32_t context = sceSyscallEnter();

	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID ||
	    sceSblACMgrIsSystemProgram(0)) {
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
				       ~UINT32_C(4));
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 8);
	}
	sceSyscallLeave(context);
	return 0;
}

int scePowerRequestDisplayOn(void)
{
	uint32_t context = sceSyscallEnter();

	if (sceKernelGetProcessId() == SCE_KERNEL_PROCESS_ID ||
	    sceSblACMgrIsSystemProgram(0)) {
		ksceKernelClearEventFlag(s_powerSwitchState.eventFlag,
				       ~UINT32_C(8));
		ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 4);
	}
	sceSyscallLeave(context);
	return 0;
}

int kscePowerRequestSoftReset(void)
{
	uint32_t context = sceSyscallEnter();

	ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x40);
	sceSyscallLeave(context);
	return 0;
}

int kscePowerRequestHibernate(void)
{
	uint32_t context = sceSyscallEnter();

	ksceKernelSetEventFlag(s_powerSwitchState.eventFlag, 0x400);
	sceSyscallLeave(context);
	return 0;
}

int kscePowerRequestResume(void)
{
	s_powerSwitchState.resumeRequested = 1;
	return 0;
}

int kscePowerEncodeUBattery(void)
{
	uint32_t pattern;
	int result;

	result = ksceKernelPollEventFlag(s_powerSwitchState.eventFlag,
				       UINT32_MAX, SCE_KERNEL_EVF_WAITMODE_OR, &pattern);
	/* Native Poll writes the current pattern before its no-match error. */
	if (result < 0 && result != SCE_KERNEL_ERROR_EVF_COND)
		return result;
	if (!(pattern & 0x6f0))
		return 0;
	if (!(pattern & 0x20000)) {
		result = ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
					       0x20000, SCE_KERNEL_EVF_WAITMODE_OR, &pattern, NULL);
		if (result < 0)
			return result;
	}
	result = ksceKernelWaitEventFlag(s_powerSwitchState.eventFlag,
				       0x40000, SCE_KERNEL_EVF_WAITMODE_OR, &pattern, NULL);
	return result < 0 ? result : 0;
}

uint32_t kscePowerGetResumeCount(void)
{
	return s_powerSwitchState.resumeCount;
}

int ScePowerForDriver_F535D928(uint32_t mask, int enabled)
{
	if (enabled)
		SceCpuForDriver_2A40BB93((int32_t *)&s_powerRequestMask, mask);
	else
		SceCpuForDriver_10EB35EB((int32_t *)&s_powerRequestMask, ~mask);
	return 0;
}

int ScePowerForDriver_D516D446(uint32_t *state)
{
	if (state)
		*state = s_powerSwitchState.requestState;
	return 0;
}

uint32_t kscePowerGetWakeupFactor(void)
{
	return s_powerSwitchState.wakeupFactor;
}

void powerResumeCpuContinuation(void)
{
	unsigned int cpu = sceKernelCpuId();

	if (cpu)
		goto resumeSecondary;
	for (;;) {
		powerSmcResumePrimary();
		powerSmcResumeSecondary(1);
		s_powerSwitchState.resumeCount++;
	resumeSecondary:
		SceKernelSuspendForDriver_FE2118BD(0);
		cpu = sceKernelCpuId();
		powerRestoreCpuContext(s_powerCpuContext[cpu], 1);
	}
}

static __attribute__((noinline)) void powerNotifyDisplayCallbacks(void)
{
	powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_ON, POWER_CALLBACKARG_DISPLAY_OFF, 0);
}

static inline __attribute__((always_inline)) int powerDispatchSwitchEvent(int phase, int event, int sleepMode,
				    const void *argument, SceUID *busy)
{
	int ret;

	if (s_powerBeforeSysEvent)
		s_powerBeforeSysEvent(phase, event, sleepMode);
	ret = SceKernelSuspendForDriver_D4622EA8(phase, event, argument, busy);
	if (s_powerAfterSysEvent)
		s_powerAfterSysEvent(phase, event, busy ? ret : 0,
				     busy ? *busy : 0);
	return ret;
}

static void powerReadMonitorRegisters(struct PowerMonitorState *state)
{
state->cp15[0] = readSctlr();
	state->cp15[1] = readActlr();
	state->cp15[2] = readCpacr();
	state->cp15[3] = readTtbr0();
	state->cp15[4] = readTtbr1();
	state->cp15[5] = readTtbcr();
	state->cp15[6] = readDacr();
	state->cp15[7] = readPrrr();
	state->cp15[8] = readNmrr();
	state->cp15[9] = readVbar();
	state->cp15[10] = readContextId();
	state->cp15[11] = readTpidrurw();
	state->cp15[12] = readTpidruro();
	state->cp15[13] = readTpidrprw();
}
