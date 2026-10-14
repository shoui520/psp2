/*
 * ScePower: display brightness, dimming and the default idle timer.
 */

#include "power_private.h"

void powerDisplayIdleExpired(uint32_t timerId, uint32_t elapsed,
			     void *argument, uint32_t elapsedHigh);

void powerDisplayIdleDimOrReset(uint32_t timerId, uint32_t elapsed,
				void *argument, uint32_t elapsedHigh);

/* Display words are shared by brightness and idle gating; names are unresolved. */
struct PowerDisplayState s_powerDisplayState;
int powerDisplayLedWorker(void)
{
	uint32_t reason = s_powerDisplayState.ledReason;

	if (reason == 1) {
		ksceSysconCtrlDolceLED(0);
	} else if (reason == 2) {
		ksceSysconCtrlDolceLED(2);
		sceKernelDelayThread(900000);
		if (s_powerDisplayState.ledReason == 2) {
			s_powerDisplayState.ledReason = 1;
			ksceSysconCtrlDolceLED(0);
		}
	}
	return 0;
}

int powerInitializeDisplayControl(void)
{
	s_powerDisplayState.controllerIdleGate = 0;
	s_powerDisplayState.ledWorkEnabled = 0;
	s_powerDisplayState.ledReason = 0;
	s_powerDisplayState.requestedBrightness = 0x10000;
	s_powerDisplayState.maxBrightness[0] = 0x10000;
	s_powerDisplayState.maxBrightness[1] = 0x10000;
	s_powerDisplayState.brightness[0] = 0x10000;
	s_powerDisplayState.brightness[1] = 0x10000;
	s_powerDisplayState.brightness[2] = 0x10000;
	s_powerDisplayState.displayEnabled = 1;
	if (ksceSblAimgrIsGenuineDolce()) {
		s_powerDisplayState.controllerIdleGate = 1;
		s_powerDisplayState.ledWorkEnabled = 1;
	}
	kscePowerSetIdleCallback(1, 0x400, 45000000,
				powerDisplayIdleDimOrReset, NULL);
	kscePowerSetIdleCallback(2, 0x400, 60000000,
				powerDisplayIdleExpired, NULL);
	return 0;
}

/* The internal head uses the requested level without the external-head cap. */
static uint32_t powerGetDisplayHeadBrightness(unsigned int head)
{
	uint32_t requested = s_powerDisplayState.requestedBrightness;
	uint32_t maximum;

	if (head == 1)
		return requested;
	maximum = s_powerDisplayState.maxBrightness[!SceSysconForDriver_9DA2A5AB()];
	return requested < maximum ? requested : maximum;
}

void powerRefreshDisplayBrightness(void)
{
	SceKernelIntrStatus intrState;
	uint32_t brightness;
	unsigned int head;

	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	if (s_powerDisplayState.displayEnabled) {
		for (head = 0; head < 3; head += 2) {
			if (!s_powerDisplayState.brightness[head])
				continue;
			brightness = powerGetDisplayHeadBrightness(head);
			s_powerDisplayState.brightness[head] = brightness;
			ksceDisplaySetBrightness(head, brightness);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

uint32_t ScePowerForDriver_394DE492(void)
{
	return s_powerDisplayState.displayEnabled;
}

uint32_t powerSwitchDisplay(uint32_t enabled, uint32_t reason)
{
	SceKernelIntrStatus intrState;
	uint32_t previous;
	uint32_t brightness;
	unsigned int head;

	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	previous = s_powerDisplayState.displayEnabled;
	if ((int32_t)enabled < 0)
		enabled = previous ^ 1;
	if (previous == enabled) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		return previous;
	}
	s_powerDisplayState.displayEnabled = enabled;
	for (head = 0; head < 3; head++) {
		if (head == 1 && ksceDisplayGetPrimaryHead() != 1)
			continue;
		brightness = enabled ? powerGetDisplayHeadBrightness(head) : 0;
		s_powerDisplayState.brightness[head] = brightness;
		ksceDisplaySetBrightness(head, brightness);
	}
	if (enabled) {
		ksceCtrlUpdateMaskForAll(0, 0x0400ffff);
		ksceTouchSetEnableFlag(0, 1);
		ksceTouchSetEnableFlag(1, 1);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_OFF, 0, POWER_CALLBACKARG_DISPLAY_ON);
		kscePowerTick(0);
	} else {
		ksceCtrlUpdateMaskForAll(0x0400ffff, 0);
		ksceTouchSetEnableFlag(0, 0);
		ksceTouchSetEnableFlag(1, 0);
		ScePowerForDriver_660D5AB4(1);
		ScePowerForDriver_660D5AB4(2);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_ON, POWER_CALLBACKARG_DISPLAY_OFF, 0);
		SceCtrlForDriver_69E45836();
	}
	if (s_powerDisplayState.ledWorkEnabled && reason &&
	    s_powerDisplayState.ledReason != reason) {
		s_powerDisplayState.ledReason = reason;
		return ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "ScePowerDisplayLed",
						powerDisplayLedWorker, NULL);
	}
	return previous;
}

void powerDisplayIdleExpired(uint32_t timerId, uint32_t elapsed,
			     void *argument, uint32_t elapsedHigh)
{
	unsigned int head;
	int primaryAffected = 0;

	(void)timerId;
	(void)argument;
	(void)elapsedHigh;
	if (!elapsed)
		return;
	for (head = 0; head < 3; head++) {
		if (head == 1 && ksceDisplayGetPrimaryHead() != 1)
			continue;
		if (!s_powerDisplayState.brightness[head])
			continue;
		s_powerDisplayState.brightness[head] = 0;
		ksceDisplaySetBrightness(head, 0);
		if (ksceDisplayGetPrimaryHead() == head)
			primaryAffected = 1;
	}
	if (!primaryAffected)
		return;
	if ((s_powerDisplayState.controllerIdleGate &&
	     !ksceCtrlGetActiveControllerPort()) || (kscePowerGetPowerSwMode() & 4))
		powerSwitchDisplay(0, 2);
	powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_ON, POWER_CALLBACKARG_DISPLAY_OFF, 0);
}

void powerDisplayIdleDimOrReset(uint32_t timerId, uint32_t elapsed,
				void *argument, uint32_t elapsedHigh)
{
	uint32_t brightness;
	unsigned int head;
	int primaryRestored = 0;

	(void)timerId;
	(void)argument;
	(void)elapsedHigh;
	if (elapsed) {
		for (head = 0; head < 3; head++) {
			if (head == 1 && ksceDisplayGetPrimaryHead() != 1)
				continue;
			if (s_powerDisplayState.brightness[head]) {
				s_powerDisplayState.brightness[head] = 1;
				ksceDisplaySetBrightness(head, 1);
			}
		}
		return;
	}
	if (!s_powerDisplayState.displayEnabled) {
		if ((s_powerDisplayState.controllerIdleGate &&
		     !ksceCtrlGetActiveControllerPort()) ||
		    (kscePowerGetPowerSwMode() & 4))
			return;
		powerSwitchDisplay(1, 1);
	}
	for (head = 0; head < 3; head++) {
		if (head == 1 && ksceDisplayGetPrimaryHead() != 1)
			continue;
		brightness = powerGetDisplayHeadBrightness(head);
		if (!s_powerDisplayState.brightness[head] &&
		    ksceDisplayGetPrimaryHead() == head)
			primaryRestored = 1;
		s_powerDisplayState.brightness[head] = brightness;
		ksceDisplaySetBrightness(head, brightness);
	}
	if (primaryRestored)
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_OFF, 0, POWER_CALLBACKARG_DISPLAY_ON);
}

/* Sony's internal name is unknown. */
int powerTickInternalPredicate(void)
{
	if (s_powerDisplayState.controllerIdleGate && !ksceCtrlGetActiveControllerPort())
		return 0;
	return !(kscePowerGetPowerSwMode() & 4);
}

int kscePowerSetDisplayBrightness(uint32_t brightness)
{
	uint32_t limit;
	unsigned int head;

	if (brightness <= 20)
		brightness = (brightness << 16) / 20;
	s_powerDisplayState.requestedBrightness = brightness;
	for (head = 0; head < 3; head++) {
		if (head == 1) {
			if (ksceDisplayGetPrimaryHead() != 1)
				continue;
			limit = brightness;
		} else {
			limit = s_powerDisplayState.maxBrightness[
					!SceSysconForDriver_9DA2A5AB()];
			if (limit > brightness)
				limit = brightness;
		}
		s_powerDisplayState.brightness[head] = limit;
		if (s_powerDisplayState.displayEnabled)
			ksceDisplaySetBrightness(head, limit);
	}
	return 0;
}

uint32_t kscePowerGetDisplayMaxBrightness(void)
{
	uint32_t first, second;
	int status;

	if (ksceSblAimgrIsDolce())
		return 0x10000;
	status = SceSysconForDriver_9DA2A5AB();
	first = s_powerDisplayState.maxBrightness[0];
	second = s_powerDisplayState.maxBrightness[1];
	return status ? first : second;
}

int kscePowerSetDisplayMaxBrightness(uint32_t limit)
{
	uint32_t requested, maximum, previous;
	unsigned int head;

	if (limit == 1)
		limit = 2;
	if (s_powerDisplayState.maxBrightness[0] == limit)
		return 0;
	s_powerDisplayState.maxBrightness[0] = limit;
	s_powerDisplayState.maxBrightness[1] = limit;
	for (head = 0; head < 3; head += 2) {
		if (!s_powerDisplayState.brightness[head])
			continue;
		requested = s_powerDisplayState.requestedBrightness;
		maximum = s_powerDisplayState.maxBrightness[
					!SceSysconForDriver_9DA2A5AB()];
		previous = s_powerDisplayState.brightness[head];
		if (maximum > requested)
			maximum = requested;
		if (maximum == previous)
			continue;
		s_powerDisplayState.brightness[head] = maximum;
		if (s_powerDisplayState.displayEnabled)
			ksceDisplaySetBrightness(head, maximum);
	}
	return 0;
}

void powerDefaultIdleCallback(uint32_t timerId, uint32_t elapsed,
			      void *argument, uint32_t elapsedHigh)
{
	(void)timerId;
	(void)argument;
	(void)elapsedHigh;
	if (elapsed)
		scePowerRequestSuspend();
}

int powerChargingCommandComplete(SceSysconPacket *packet, void *argp)
{
	uint32_t *pending = argp;

	(void)packet;
	*pending = 0;
	return 0;
}

int powerPermitChargeTimer(void *argp)
{
	(void)argp;
	s_powerBatteryBase.chargingAlarm = -1;
	ksceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x200);
	return 0;
}
