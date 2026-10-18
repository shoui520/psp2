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
		SceSysconForDriver_727F985A(0);
	} else if (reason == 2) {
		SceSysconForDriver_727F985A(2);
		sceKernelDelayThread(900000);
		if (s_powerDisplayState.ledReason == 2) {
			s_powerDisplayState.ledReason = 1;
			SceSysconForDriver_727F985A(0);
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
	if (SceSblAIMgrForDriver_C6E83F34()) {
		s_powerDisplayState.controllerIdleGate = 1;
		s_powerDisplayState.ledWorkEnabled = 1;
	}
	ScePowerForDriver_1BA2FCAE(1, 0x400, 45000000,
				powerDisplayIdleDimOrReset, NULL);
	ScePowerForDriver_1BA2FCAE(2, 0x400, 60000000,
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

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if (s_powerDisplayState.displayEnabled) {
		for (head = 0; head < 3; head += 2) {
			if (!s_powerDisplayState.brightness[head])
				continue;
			brightness = powerGetDisplayHeadBrightness(head);
			s_powerDisplayState.brightness[head] = brightness;
			SceDisplayForDriver_9E3C6DC6(head, brightness);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

SceUInt32 ScePowerForDriver_394DE492(void)
{
	return s_powerDisplayState.displayEnabled;
}

uint32_t powerSwitchDisplay(uint32_t enabled, uint32_t reason)
{
	SceKernelIntrStatus intrState;
	uint32_t previous;
	uint32_t brightness;
	unsigned int head;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	previous = s_powerDisplayState.displayEnabled;
	if ((int32_t)enabled < 0)
		enabled = previous ^ 1;
	if (previous == enabled) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		return previous;
	}
	s_powerDisplayState.displayEnabled = enabled;
	for (head = 0; head < 3; head++) {
		if (head == 1 && SceDisplayForDriver_C8E554C5() != 1)
			continue;
		brightness = enabled ? powerGetDisplayHeadBrightness(head) : 0;
		s_powerDisplayState.brightness[head] = brightness;
		SceDisplayForDriver_9E3C6DC6(head, brightness);
	}
	if (enabled) {
		SceCtrlForDriver_12C13A72(0, 0x0400ffff);
		ksceTouchSetEnableFlag(0, 1);
		ksceTouchSetEnableFlag(1, 1);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_OFF, 0, POWER_CALLBACKARG_DISPLAY_ON);
		ScePowerForDriver_EFD3C963(0);
	} else {
		SceCtrlForDriver_12C13A72(0x0400ffff, 0);
		ksceTouchSetEnableFlag(0, 0);
		ksceTouchSetEnableFlag(1, 0);
		ScePowerForDriver_660D5AB4(1);
		ScePowerForDriver_660D5AB4(2);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_ON, POWER_CALLBACKARG_DISPLAY_OFF, 0);
		SceCtrlForDriver_69E45836();
	}
	if (s_powerDisplayState.ledWorkEnabled && reason &&
	    s_powerDisplayState.ledReason != reason) {
		s_powerDisplayState.ledReason = reason;
		return sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "ScePowerDisplayLed",
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
		if (head == 1 && SceDisplayForDriver_C8E554C5() != 1)
			continue;
		if (!s_powerDisplayState.brightness[head])
			continue;
		s_powerDisplayState.brightness[head] = 0;
		SceDisplayForDriver_9E3C6DC6(head, 0);
		if (SceDisplayForDriver_C8E554C5() == head)
			primaryAffected = 1;
	}
	if (!primaryAffected)
		return;
	if ((s_powerDisplayState.controllerIdleGate &&
	     !SceCtrlForDriver_FA88D837()) || (ScePowerForDriver_165CE085() & 4))
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
			if (head == 1 && SceDisplayForDriver_C8E554C5() != 1)
				continue;
			if (s_powerDisplayState.brightness[head]) {
				s_powerDisplayState.brightness[head] = 1;
				SceDisplayForDriver_9E3C6DC6(head, 1);
			}
		}
		return;
	}
	if (!s_powerDisplayState.displayEnabled) {
		if ((s_powerDisplayState.controllerIdleGate &&
		     !SceCtrlForDriver_FA88D837()) ||
		    (ScePowerForDriver_165CE085() & 4))
			return;
		powerSwitchDisplay(1, 1);
	}
	for (head = 0; head < 3; head++) {
		if (head == 1 && SceDisplayForDriver_C8E554C5() != 1)
			continue;
		brightness = powerGetDisplayHeadBrightness(head);
		if (!s_powerDisplayState.brightness[head] &&
		    SceDisplayForDriver_C8E554C5() == head)
			primaryRestored = 1;
		s_powerDisplayState.brightness[head] = brightness;
		SceDisplayForDriver_9E3C6DC6(head, brightness);
	}
	if (primaryRestored)
		powerNotifyCallback(POWER_CALLBACKARG_DISPLAY_OFF, 0, POWER_CALLBACKARG_DISPLAY_ON);
}

/* Sony's internal name is unknown. */
int powerTickInternalPredicate(void)
{
	if (s_powerDisplayState.controllerIdleGate && !SceCtrlForDriver_FA88D837())
		return 0;
	return !(ScePowerForDriver_165CE085() & 4);
}

int ScePowerForDriver_43D5CE1D(uint32_t brightness)
{
	uint32_t limit;
	unsigned int head;

	if (brightness <= 20)
		brightness = (brightness << 16) / 20;
	s_powerDisplayState.requestedBrightness = brightness;
	for (head = 0; head < 3; head++) {
		if (head == 1) {
			if (SceDisplayForDriver_C8E554C5() != 1)
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
			SceDisplayForDriver_9E3C6DC6(head, limit);
	}
	return 0;
}

uint32_t ScePowerForDriver_D8759B55(void)
{
	uint32_t first, second;
	int status;

	if (SceSblAIMgrForDriver_71608CA3())
		return 0x10000;
	status = SceSysconForDriver_9DA2A5AB();
	first = s_powerDisplayState.maxBrightness[0];
	second = s_powerDisplayState.maxBrightness[1];
	return status ? first : second;
}

int ScePowerForDriver_77027B6B(uint32_t limit)
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
			SceDisplayForDriver_9E3C6DC6(head, maximum);
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
	sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x200);
	return 0;
}
