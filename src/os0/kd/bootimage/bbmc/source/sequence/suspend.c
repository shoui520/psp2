#include <sce/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../usb_extif.h"

void bbmcEnterCS(struct ThreadmgrFastMutex *mutex);
void bbmcLeaveCS(struct ThreadmgrFastMutex *mutex);
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int message);
int bbmcTimerCancelHandler(int timerId);

typedef int (*BbmcSuspendHandler)(BbmcControl *, BbmcMessage *);

int suspendInitialToComBoot(BbmcControl *control, BbmcMessage *message);
int suspendSteadyComBoot(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromComBootToStandby(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromComBootToInitial(BbmcControl *control, BbmcMessage *message);
int suspendSteadyStandby(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromStandbyToAuthorized(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromStandbyToComBoot(BbmcControl *control, BbmcMessage *message);
int suspendSteadyAuthorized(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromAuthorizedToActivated(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromAuthorizedToStandby(BbmcControl *control, BbmcMessage *message);
int suspendTestModeToAuthorized(BbmcControl *control, BbmcMessage *message);
int suspendSteadyTestMode(BbmcControl *control, BbmcMessage *message);
int suspendTestModeToStandby(BbmcControl *control, BbmcMessage *message);
int suspendSteadyActivated(BbmcControl *control, BbmcMessage *message);
int suspendActivatedToStandbyOrAuthorized(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromActivatedToConnected(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromActivatedToLowPowerMode(BbmcControl *control, BbmcMessage *message);
int suspendSteadyLowPowerMode(BbmcControl *control, BbmcMessage *message);
int suspendDuringFromLpmToConnected(BbmcControl *control, BbmcMessage *message);
int suspendLowPowerModeToStandby(BbmcControl *control, BbmcMessage *message);
int suspendConnectedToLowPowerModeOrStandby(BbmcControl *control, BbmcMessage *message);
int suspendSteadyConnected(BbmcControl *control, BbmcMessage *message);
int suspendGotoLPM(BbmcControl *control, BbmcMessage *message);
int suspendGotoConnected(BbmcControl *control, BbmcMessage *message);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_seqSuspend_L375
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/suspend.c",
	.line = 375,
	.func = "seqSuspend",
};

static const SceKernelDebugInfoSource dbg_seqSuspend_L375Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqSuspend_L375, "sequence/suspend.c", 375, "seqSuspend",
};

static const SceKernelDebugInfo dbg_seqSuspend_L338
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/suspend.c",
	.line = 338,
	.func = "seqSuspend",
};

static const SceKernelDebugInfoSource dbg_seqSuspend_L338Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqSuspend_L338, "sequence/suspend.c", 338, "seqSuspend",
};
#endif

/* The selector supplies a live message; this initial-state handler ignores it. */
int seqSuspendInitial(BbmcControl *control, BbmcMessage *message)
{
	int mode;

	(void)message;
	control->suspendError = (int)0x80010100;
	if (bbmcUsbExtIfGetMode(&mode) == 0 && mode >= 0) {
		control->backendShutdownRequested = 1;
		bbmcUsbExtIfReleaseGpio();
	}
	return 0;
}

BbmcSuspendHandler seqSuspendSelectHandler(int currentState, int targetState)
{
	switch (currentState) {
	case 0:
		if (targetState == 0)
			return seqSuspendInitial;
		return targetState == 1 ? suspendInitialToComBoot : NULL;
	case 1:
		if (targetState == 1)
			return suspendSteadyComBoot;
		if (targetState == 2)
			return suspendDuringFromComBootToStandby;
		return targetState == 0 ? suspendDuringFromComBootToInitial : NULL;
	case 2:
		if (targetState == 2)
			return suspendSteadyStandby;
		if (targetState == 3)
			return suspendDuringFromStandbyToAuthorized;
		return targetState == 1 ? suspendDuringFromStandbyToComBoot : NULL;
	case 3:
		if (targetState == 3)
			return suspendSteadyAuthorized;
		if (targetState == 5)
			return suspendDuringFromAuthorizedToActivated;
		return targetState == 2 ? suspendDuringFromAuthorizedToStandby : NULL;
	case 4:
		if (targetState == 3)
			return suspendTestModeToAuthorized;
		if (targetState == 4)
			return suspendSteadyTestMode;
		return targetState == 2 ? suspendTestModeToStandby : NULL;
	case 5:
		switch (targetState) {
		case 2:
		case 3:
			return suspendActivatedToStandbyOrAuthorized;
		case 5:
			return suspendSteadyActivated;
		case 6:
			return suspendDuringFromActivatedToLowPowerMode;
		case 7:
			return suspendDuringFromActivatedToConnected;
		default:
			return NULL;
		}
	case 6:
		if (targetState == 6)
			return suspendSteadyLowPowerMode;
		if (targetState == 7)
			return suspendDuringFromLpmToConnected;
		return targetState == 2 ? suspendLowPowerModeToStandby : NULL;
	case 7:
		if (targetState == 6 || targetState == 2)
			return suspendConnectedToLowPowerModeOrStandby;
		return targetState == 7 ? suspendSteadyConnected : NULL;
	default:
		return NULL;
	}
}

int seqSuspend(BbmcControl *control, BbmcMessage *message)
{
	BbmcSuspendHandler handler;
	SceUInt32 active, recent;
	int currentState, targetState, newTarget, notification;
	int result;

	if (message->command == 42) {
		control->suspendError = (int)0x80020101;
		bbmcEnterCS(&control->mutex);
		targetState = control->targetState;
		currentState = control->currentState;
		bbmcLeaveCS(&control->mutex);
		handler = seqSuspendSelectHandler(currentState, targetState);
		control->suspendMessageHandler = handler;
#if FW_DEBUG
		ksceKernelAssert(handler != NULL, &dbg_seqSuspend_L375,
				 __builtin_return_address(0));
#endif
		return bbmcSendSimpleMessage(control, 43);
	}
	if (message->command != 40)
		return 0;

	control->suspendError = (int)0x80020100;
	bbmcNotifyFeatureEvent(control, 0, 0x80000);
	bbmcEnterCS(&control->mutex);
	if (control->powerEventTimer > 0) {
		bbmcTimerCancelHandler(control->powerEventTimer);
		control->powerEventTimer = 0;
	}
	active = control->powerEventActive;
	recent = control->recentPowerEvent;
	control->recentPowerEvent = 0;
	if (active != 0) {
		if (control->targetState != 7)
			goto normalSuspend;
		handler = suspendGotoLPM;
		newTarget = 6;
		notification = 29;
	} else {
		if (recent != 0)
			bbmcNotifyFeatureEvent(control, 0, 0x8000000);
		if (control->powerPolicy == 0 || control->targetState != 6)
			goto normalSuspend;
		handler = suspendGotoConnected;
		newTarget = 7;
		notification = 30;
	}
	if (control->powerTransitionBusy == 0) {
		control->powerTransitionBusy = 1;
		if (bbmcSendSimpleMessage(control, notification) >= 0) {
			control->targetState = newTarget;
			control->savedTargetState = newTarget;
			control->suspendMessageHandler = handler;
			control->suspendState = 2;
			bbmcLeaveCS(&control->mutex);
			return 0;
		}
		control->powerTransitionBusy = 0;
	}

normalSuspend:
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	currentState = control->currentState;
	targetState = control->targetState;
	control->savedTargetState = targetState;
	if (currentState == targetState) {
		control->suspendState = 2;
		handler = seqSuspendSelectHandler(targetState, targetState);
		control->suspendMessageHandler = handler;
#if FW_DEBUG
		ksceKernelAssert(handler != NULL, &dbg_seqSuspend_L338,
				 __builtin_return_address(0));
#endif
		result = bbmcSendSimpleMessage(control, 43);
	} else {
		result = bbmcSendSimpleMessage(control, 41);
	}
	bbmcLeaveCS(&control->mutex);
	return result;
}
