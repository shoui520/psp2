/*
 * SceBbmc manager helpers and exported GPIO reference operations.
 * The remaining exports are explicit skeleton placeholders.
 */

#include <skeleton.h>
#include <bbmc/bbmc.h>
#include <sysmem/debug.h>
#include <threadmgr/fast_mutex_object.h>
#include <power/power.h>
#include <syscon/syscon.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <modulemgr/modulemgr.h>
#include <psp2kern/idstorage.h>
#include <sysmem/procevent.h>
#include <arm_neon.h>
#include <stdarg.h>
#include <string.h>
#include "bbmc_message.h"
#include "bbmc_gps_cell_info.h"
#include "bbmc_pin.h"
#include "bbmc_proactive.h"
#include "bbmc_setup.h"
#include "bbmc_terminal_response.h"
#include "bbmc_update_work.h"
#include "bbmc_control.h"
#include "bbmc_modem_configuration.h"
#include <sysmem/sysmem.h>
#include "bbmc_memory.h"

static const BbmcSetupDescriptor bbmcSetupDescriptors[8];

extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
	const char *name, int attr, void *opt)
	__asm__("sceKernelInitializeFastMutex");
int bbmcAcquireGpio(int *previous);
int bbmcReleaseGpio(int *previous);
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcTimerSetHandler(SceUInt32 delay, void (*callback)(void *arg), void *arg);
int bbmcTimerCancelHandler(int timerId);
void bbmcPowerEventTimeout(void *argp);
int bbmcPowerOffStateHandler(BbmcControl *control, BbmcMessage *message);
int stmInitial_ComBoot(BbmcControl *control, BbmcMessage *message);
int bbmcLookupModeValue(int mode, SceUInt32 *output);
int bbmcInitializeGpio(void);
int bbmcFinalizeGpio(void);
int bbmcInitializeUsbSerialHost(void);
int bbmcFinalizeUsbSerialHost(void);
int bbmcInitializeUsbExtIf(void);
int bbmcFinalizeUsbExtIf(void);
int bbmcInitializeTimer(void);
int bbmcFinalizeTimer(void);
int bbmcInitializeMailbox(void);
int bbmcFinalizeMailbox(void);
int bbmcCreateMailbox(struct BbmcMailbox **mailbox);
unsigned int bbmcDeleteMailbox(struct BbmcMailbox *mailbox);
int managerThread(SceSize argumentSize, void *arguments);
BbmcControl *g_bbmcControl;
static BbmcControl bbmcMainControl;
int sceBbmcUnsubscribeFeatureForDriver(int subscriberId);
int sceBbmcTurnOffForDriver(int subscriberId);
int bbmcProcessKillHandler(SceUID processId,
	ProcEventParam *parameters, int argument);

const SceKernelProcEventHandler bbmcProcessEventHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.kill = bbmcProcessKillHandler,
};

int bbmcProcessKillHandler(SceUID processId,
	ProcEventParam *parameters, int argument)
{
	BbmcControl *control = g_bbmcControl;
	int keepPower = 0, result = 0;

	if (control == NULL)
		return 0;
	if (control->subscribers[0].active == 1 &&
	    control->subscribers[0].parameters.featureId == 0 &&
	    control->subscribers[0].processId != processId)
		keepPower = 1;
	if (control->subscribers[1].active == 1 &&
	    control->subscribers[1].parameters.featureId == 0 &&
	    control->subscribers[1].processId != processId)
		keepPower = 1;
	if (control->subscribers[0].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[0].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[0].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[0].parameters.subscriberId);
	}
	if (control->subscribers[1].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[1].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[1].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[1].parameters.subscriberId);
	}
	if (control->subscribers[2].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[2].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[2].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[2].parameters.subscriberId);
	}
	if (control->subscribers[3].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[3].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[3].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[3].parameters.subscriberId);
	}
	if (control->subscribers[4].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[4].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[4].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[4].parameters.subscriberId);
	}
	if (control->subscribers[5].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[5].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[5].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[5].parameters.subscriberId);
	}
	if (control->subscribers[6].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[6].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[6].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[6].parameters.subscriberId);
	}
	if (control->subscribers[7].processId == processId) {
		if (keepPower)
			keepPower = 1;
		else if (control->subscribers[7].parameters.featureId == 0) {
			keepPower = 1;
			sceBbmcTurnOffForDriver(control->subscribers[7].parameters.subscriberId);
		}
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[7].parameters.subscriberId);
	}
	if (control->subscribers[8].processId == processId) {
		if (!keepPower && control->subscribers[8].parameters.featureId == 0)
			sceBbmcTurnOffForDriver(control->subscribers[8].parameters.subscriberId);
		result = sceBbmcUnsubscribeFeatureForDriver(control->subscribers[8].parameters.subscriberId);
	}
	return result;
}

void bbmcSysconNotification(int enable, void *argp)
{
	BbmcControl *control = argp;

	if (control != NULL && enable != 0) {
		if (control->suspendState == 3)
			control->deferredSysconEvent = 1;
		else
			bbmcSendSimpleMessage(control, 1);
	}
}

int bbmcSystemEventHandler(int resume, int eventId, void *args, void *opt)
{
	BbmcControl *control = opt;
	struct PowerSuspendEvent *event = args;
	int active;

	if (control == NULL)
		return 0;
	if (resume != 0) {
		if (resume == 1 && eventId == SYSEVENT_RESUME_START)
			return bbmcSendSimpleMessage(control, 45);
		return 0;
	}
	if (eventId == SYSEVENT_SUSPEND_START) {
		control->suspendArgument = event->sleepMode != 0;
		if (control->suspendArgument)
			control->suspendEligible = 1;
		else {
			active = SceSysconForDriver_50CAE242();
			active |= SceSysconForDriver_03C50DC3();
			active |= SceSysconForDriver_D7F5A797();
			control->suspendEligible = active != 0;
		}
		if (control->suspendState == 0)
			control->suspendState = 1;
		control->flags |= 4;
		return bbmcSendSimpleMessage(control, 40);
	}
	if (eventId == SYSEVENT_SUSPEND_IDLE_QUERY && control->suspendState != 3)
		return control->suspendError;
	return 0;
}

void bbmcSysconPowerStateChanged(int enable, void *argp)
{
	BbmcControl *control = argp;
	unsigned int inverted = enable == 0;
	unsigned int events;

	if (control->sysconPowerInverted != inverted) {
		events = inverted ? 0x400000 : 0x200000;
		control->sysconPowerInverted = inverted;
		vst1_u32(control->pendingEvents,
			vorr_u32(vld1_u32(control->pendingEvents),
				vdup_n_u32(events)));
		bbmcSendSimpleMessage(control, 14);
	}
}

void bbmcNetworkRetryTimeout(void *argp)
{
	BbmcControl *control = argp;

	if (bbmcLockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1375, "bbmcEnterCS");
	control->networkRetryTimer = 0;
	control->recentPowerEvent = 0;
	if (control->networkStopBusy == 0 && control->deferredNetworkStart != 0 &&
	    control->targetState == 7 && control->currentState == 7) {
		bbmcSendSimpleMessage(control, 109);
		control->deferredNetworkStart = 0;
	}
	if (bbmcUnlockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1398, "bbmcLeaveCS");
}

void bbmcPowerProcessFlagsChanged(SceUID processId, SceUInt32 flags, void *argp)
{
	BbmcControl *control = argp;
	unsigned int active, events = 0, previousEvent;
	int timerId;

	if (processId != 65569)
		return;
	if (bbmcLockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1375, "bbmcEnterCS");
	active = (flags & 0x20) != 0;
	if (active == control->powerEventActive)
		goto out;
	control->powerEventActive = active;
	if ((unsigned int)(control->targetState - 6) > 1)
		goto out;
	if (control->suspendState != 0) {
		if (active != 0 && control->targetState == 7) {
			control->pendingPowerUp = 1;
			control->pendingPowerDown = 0;
		} else if (active == 0 && control->targetState == 6 &&
			   control->powerPolicy != 0) {
			control->pendingPowerUp = 0;
			control->pendingPowerDown = 1;
		} else {
			control->pendingPowerUp = 0;
			control->pendingPowerDown = 0;
		}
		goto out;
	}
	if (active != 0) {
		if (control->powerEventTimer > 0) {
			bbmcTimerCancelHandler(control->powerEventTimer);
			control->powerEventTimer = 0;
			if (control->networkStartBusy != 0 &&
			    control->deferredNetworkStart != 0) {
				control->deferredNetworkStart = 0;
				bbmcSendSimpleMessage(control, 110);
			}
		}
		previousEvent = control->recentPowerEvent;
		control->recentPowerEvent = 1;
		timerId = bbmcTimerSetHandler(30000000, bbmcPowerEventTimeout, control);
		if (previousEvent == 0)
			events = 0x4000000;
		control->powerEventTimer = timerId & ~(timerId >> 31);
		if (control->targetState != 6 && control->powerTransitionBusy == 0) {
			control->powerTransitionBusy = 1;
			if (bbmcSendSimpleMessage(control, 29) < 0)
				control->powerTransitionBusy = 0;
		}
	} else if (control->powerEventTimer == 0) {
		if (control->powerTransitionBusy == 0 && control->powerPolicy != 0) {
			control->powerTransitionBusy = 1;
			if (bbmcSendSimpleMessage(control, 30) < 0)
				control->powerTransitionBusy = 0;
		}
		events = 0x8000000;
	}
out:
	if (bbmcUnlockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1398, "bbmcLeaveCS");
	if (events != 0) {
		vst1_u32(control->pendingEvents,
			vorr_u32(vld1_u32(control->pendingEvents), vdup_n_u32(events)));
		bbmcSendSimpleMessage(control, 14);
	}
}

void bbmcPowerEventTimeout(void *argp)
{
	BbmcControl *control = argp;
	int timerId;

	if (bbmcLockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1375, "bbmcEnterCS");
	control->powerEventTimer = 0;
	if (control->powerEventActive == 0) {
		vst1_u32(control->pendingEvents,
			vorr_u32(vld1_u32(control->pendingEvents),
				vdup_n_u32(0x8000000)));
		bbmcSendSimpleMessage(control, 14);
		if ((control->powerPolicy != 0 || control->targetState != 6) &&
		    control->targetState != 7 && control->powerTransitionBusy == 0) {
			control->powerTransitionBusy = 1;
			if (bbmcSendSimpleMessage(control, 30) < 0)
				control->powerTransitionBusy = 0;
			if (control->deferredNetworkStart != 0) {
				if (control->networkRetryTimer > 0) {
					bbmcTimerCancelHandler(control->networkRetryTimer);
					control->networkRetryTimer = 0;
				}
				timerId = bbmcTimerSetHandler(15000000,
					bbmcNetworkRetryTimeout, control);
				if (timerId > 0)
					control->networkRetryTimer = timerId;
				else
					control->networkRetryTimer = 0;
			}
		}
	}
	if (bbmcUnlockFastMutex(&control->mutex) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1398, "bbmcLeaveCS");
}

void bbmcEnterCS(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcLockFastMutex(mutex);

	if (ret < 0)
		SCE_DBG_ASSERT(0, 1375);
}

void bbmcLeaveCS(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcUnlockFastMutex(mutex);

	if (ret < 0)
		SCE_DBG_ASSERT(0, 1398);
}

/* name unknown: translate each selected notification bit into a message. */
int msgForwardNotifications(SceUInt32 mask, const BbmcNotificationArgs *args)
{
	BbmcControl *control = args->control;
	SceUInt32 first = args->forwardedValue1;
	SceUInt32 second = args->forwardedValue2;
	int i = 0;
	unsigned char bit = 0;

	do {
		BbmcMessage *message;
		SceUInt32 command;

		switch ((1U << bit) & mask) {
		case 1:
			command = 127;
			break;
		case 2:
			command = 128;
			break;
		case 4:
			command = 129;
			break;
		case 8:
			command = 130;
			break;
		case 16:
			command = 131;
			break;
		case 32:
			command = 132;
			break;
		case 64:
			command = 133;
			break;
		case 128:
			command = 134;
			break;
		case 256:
			command = 135;
			break;
		case 512:
			command = 136;
			break;
		case 1024:
			command = 137;
			break;
		case 2048:
			command = 138;
			break;
		case 4096:
			command = 139;
			break;
		case 8192:
			command = 140;
			break;
		case 16384:
			command = 141;
			break;
		default:
			goto next_bit;
		}
		message = msgAllocMsgPacket(control, command);
		if (message == NULL)
			goto next_bit;
		message->notificationValue2 = first;
		message->notificationValue1 = second;
		if (msgSendMsgPacket(control, message) < 0)
			return 0;
next_bit:
		bit = ++i;
	} while (i != 15);
	return 0;
}

int SceBbmcForDriver_D98DB926(void)
{
	return bbmcAcquireGpio(NULL);
}

int SceBbmcForDriver_D16D0845(void)
{
	return bbmcReleaseGpio(NULL);
}

/* name unknown: returns the active manager control object. */
__attribute__((noinline)) BbmcControl *bbmcGetControl(void)
{
	return g_bbmcControl;
}

int bbmcNotifyFeatureEvent(BbmcControl *control, SceUInt32 featureId,
	SceUInt32 eventMask)
{
	unsigned int index, firstEvents, secondEvents;

	switch (featureId) {
	case 0:
		firstEvents = control->pendingEvents[0] | eventMask;
		secondEvents = control->pendingEvents[1] | eventMask;
		control->pendingEvents[0] = firstEvents;
		control->pendingEvents[1] = secondEvents;
		return bbmcSendSimpleMessage(control, 14);
	case 1:
		index = 2;
		break;
	case 2:
		index = 3;
		break;
	case 3:
		index = 4;
		break;
	case 4:
		index = 5;
		break;
	case 5:
		index = 6;
		break;
	case 6:
		index = 7;
		break;
	case 7:
		index = 8;
		break;
	default:
		return (int)(uintptr_t)control;
	}
	control->pendingEvents[index] |= eventMask;
	return bbmcSendSimpleMessage(control, 14);
}

int module_start(SceSize argumentSize, const void *arguments)
{
	BbmcModemConfigurationLeaf modemConfiguration;
	BbmcControl *control = &bbmcMainControl;
	int result;

	(void)argumentSize;
	(void)arguments;
	if (SceModulemgrForDriver_BBE1771C("SceDbgUsb") >= 0 ||
	    (SceSysconForDriver_CBD6D8BC() & 2) == 0 ||
	    g_bbmcControl != NULL || bbmcInitializeGpio() < 0)
		return 1;
	if (bbmcInitializeUsbSerialHost() < 0)
		goto finalizeGpio;
	if (bbmcInitializeUsbExtIf() < 0)
		goto finalizeUsbSerialHost;
	if (bbmcInitializeTimer() < 0)
		goto finalizeUsbExtIf;
	if (bbmcInitializeMailbox() < 0)
		goto finalizeTimer;
	memset(control, 0, sizeof(*control));
	control->networkInfo.signalLevel = -1;
	control->baudRate = 115200;
	control->defaultProfile = 1;
	control->countedBytes.length = 3;
	control->countedBytes.values[0] = 94;
	control->countedBytes.values[1] = 83;
	control->countedBytes.values[2] = 72;
	control->sysconPowerInverted = 1;
	control->powerAvailable = 1;
	if (ksceIdStorageReadLeaf(0x113, &modemConfiguration) < 0 ||
	    modemConfiguration.configurationMode - 1 > 11)
		control->configurationMode = 1;
	else
		control->configurationMode = modemConfiguration.configurationMode;
	if (bbmcCreateHeap(84992) < 0)
		goto finalizeMailbox;
	if (bbmcInitializeFastMutex(&control->mutex, "SceBbmcCS", 2,
		NULL) < 0)
		SCE_DBG_ASSERT_FUNC(0, 1322, "bbmcCreateCS");
	if (bbmcCreateMailbox(&control->mailbox) < 0)
		goto deleteHeap;
	result = sceKernelCreateThread("SceBbmcMain", managerThread, 96,
		0x2000, 0x10000000, 0x80000, NULL);
	if (result < 0)
		goto deleteMailbox;
	control->threadId = result;
	result = sceKernelRegisterSysEventHandler("SceBbmcMain",
		bbmcSystemEventHandler, control);
	if (result < 0)
		goto deleteThread;
	control->eventFlag = result;
	result = sceKernelRegisterProcEventHandler("SceBbmcMain",
		&bbmcProcessEventHandlers, 0);
	if (result < 0)
		goto unregisterSystemEvent;
	control->processEventHandler = result;
	if (ScePowerForDriver_F535D928(256, 1) < 0)
		goto unregisterProcessEvent;
	if (SceSysconForDriver_4E88B4D9(bbmcSysconNotification, control) < 0)
		goto disablePower;
	g_bbmcControl = control;
	if (sceKernelStartThread(control->threadId, sizeof(g_bbmcControl),
		&g_bbmcControl) >= 0)
		return 0;
	SceSysconForDriver_4E88B4D9(NULL, NULL);
disablePower:
	ScePowerForDriver_F535D928(256, 0);
unregisterProcessEvent:
	SceProcEventForDriver_3DED57CC(control->processEventHandler);
unregisterSystemEvent:
	sceKernelUnregisterSysEventHandler(control->eventFlag);
deleteThread:
	sceKernelDeleteThread(control->threadId);
deleteMailbox:
	bbmcDeleteMailbox(control->mailbox);
deleteHeap:
	bbmcDeleteHeap();
finalizeMailbox:
	bbmcFinalizeMailbox();
finalizeTimer:
	bbmcFinalizeTimer();
finalizeUsbExtIf:
	bbmcFinalizeUsbExtIf();
finalizeUsbSerialHost:
	bbmcFinalizeUsbSerialHost();
finalizeGpio:
	bbmcFinalizeGpio();
	g_bbmcControl = NULL;
	return 1;
}

int bbmcAcquirePowerReference(BbmcControl *control)
{
	int result;

	bbmcEnterCS(&control->mutex);
	if (control->stateHandler == bbmcPowerOffStateHandler) {
		result = 0x80330003;
		goto out;
	}
	result = 0;
	if (control->references == 0) {
		result = bbmcSendSimpleMessage(control, 18);
		if (result < 0)
			goto out;
	}
	control->references++;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int bbmcReleasePowerReference(BbmcControl *control)
{
	int result;

	bbmcEnterCS(&control->mutex);
	if (control->stateHandler == stmInitial_ComBoot) {
		result = 0x80330003;
		goto out;
	}
	result = 0;
	if (control->references == 1) {
		result = bbmcSendSimpleMessage(control, 19);
		if (result < 0)
			goto out;
	}
	control->references--;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int bbmcRequestOperationMode(BbmcControl *control, SceUInt32 operationMode)
{
	BbmcMessage *message;
	int result;

	message = msgAllocMsgPacket(control, 116);
	if (message == NULL)
		return (int)0x80330005;
	message->body.operationMode = operationMode;
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
}

int sceBbmcGetNetworkInfoForDriver(BbmcNetworkInfo *info)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return (int)0x80330003;
	if (info == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	memcpy(info, &control->networkInfo, sizeof(*info));
	bbmcLeaveCS(&control->mutex);
	return 0;
}

int sceBbmcSubscribeFeatureIdForDriver(BbmcSubscribeFeatureIdParam *parameters)
{
	BbmcControl *control = bbmcGetControl();
	BbmcSubscriber *subscriber;
	int first, count, index, identifier, result;

	if (control == NULL)
		return 0x80330003;
	if (parameters == NULL)
		return 0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	switch (parameters->featureId) {
	case 0:
		first = 0;
		count = 2;
		result = 0;
		break;
	case 1:
		count = 1;
		first = 2;
		result = 0;
		break;
	case 2:
		count = 1;
		first = 3;
		result = 0;
		break;
	case 3:
		count = 1;
		first = 4;
		result = 0;
		break;
	case 4:
		count = 1;
		first = 5;
		result = 0;
		break;
	case 5:
		count = 1;
		first = 6;
		result = 0;
		break;
	case 6:
		count = 1;
		first = 7;
		result = 0;
		break;
	case 7:
		count = 1;
		first = 8;
		result = 0;
		break;
	default:
		count = 0;
		result = 0x80330002;
		first = 0;
		break;
	}
	do {
		identifier = SceThreadmgrForDriver_47F6DE49();
	} while (identifier == 0);
	bbmcEnterCS(&control->mutex);
	if (count == 0)
		goto full;
	index = control->subscribers[first].active;
	if (index != 0) {
		if (count <= 1)
			index = 1;
		else if (control->subscribers[first + 1].active != 0)
			index = 2;
		else
			index = 1;
		if (index == count)
			goto full;
	}
	index += first;
	if (identifier < 0)
		identifier = -identifier;
	parameters->subscriberId = identifier;
	subscriber = &control->subscribers[index];
	subscriber->active = 1;
	subscriber->processId = sceKernelGetProcessId();
	subscriber->parameters = *parameters;
	bbmcLeaveCS(&control->mutex);
	return result;
full:
	bbmcLeaveCS(&control->mutex);
	return 0x80330007;
}

int sceBbmcUnsubscribeFeatureForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcSubscriber *subscriber;
	int index, result;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000a;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000b;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = 0x80330002;
		goto out;
	}
	subscriber = &control->subscribers[index];
	subscriber->processId = 0;
	memset(subscriber, 0, sizeof(*subscriber));
	control->deliveredEvents[index] = 0;
	control->acknowledgedEvents[index] = 0;
	control->pendingEvents[index] = 0;
	control->callbackPending[index] = 0;
	result = 0;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcClearEventForDriver(int subscriberId, SceUInt32 eventMask)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		bbmcLeaveCS(&control->mutex);
		return 0x80330002;
	}
	control->acknowledgedEvents[index] |= eventMask;
	bbmcLeaveCS(&control->mutex);
	return 0;
}

int sceBbmcResumeSubscriberCallbackForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		bbmcLeaveCS(&control->mutex);
		return 0x80330002;
	}
	control->callbackPending[index] = 0;
	result = bbmcSendSimpleMessage(control, 14);
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcTurnOnForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000a;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000b;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 7 &&
	    control->subscribers[index].parameters.featureId != 0)
		goto invalid;
	result = bbmcAcquirePowerReference(control);
	bbmcLeaveCS(&control->mutex);
	return result;
invalid:
	bbmcLeaveCS(&control->mutex);
	return 0x80330002;
}

int sceBbmcTurnOffForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000a;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return 0x8033000b;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 7 &&
	    control->subscribers[index].parameters.featureId != 0)
		goto invalid;
	result = bbmcReleasePowerReference(control);
	bbmcLeaveCS(&control->mutex);
	return result;
invalid:
	bbmcLeaveCS(&control->mutex);
	return 0x80330002;
}

int sceBbmcGetPinStatusForDriver(int subscriberId, SceUInt32 *status,
			       SceUInt16 *retries)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt32 pinStatus, statusMask;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (status == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	pinStatus = control->pinStatus.pin1Status;
	*status = pinStatus;
	result = 0;
	if (retries != NULL) {
		if (pinStatus > 4)
			goto noRetries;
		statusMask = 1U << pinStatus;
		if (statusMask & 0x15)
			*retries = control->pinStatus.pin1VerifyRetries;
		else if (statusMask & 0xA)
			*retries = control->pinStatus.pin1UnblockRetries;
		else {
noRetries:
			*retries = 0;
			bbmcLeaveCS(&control->mutex);
			return 0;
		}
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcUnblockPinForDriver(int subscriberId, BbmcUnblockPinParam *parameters)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt8 pinLength, pukLength;
	int index, result;

	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (parameters == NULL || parameters->pinLength > 9 ||
	    parameters->pukLength > 33)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState <= 1 || control->pinStatus.pin1Status != 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	pinLength = parameters->pinLength;
	pukLength = parameters->pukLength;
	control->unblockPin.pinLength = pinLength;
	control->unblockPin.pukLength = pukLength;
	__builtin_memcpy(control->unblockPin.puk, parameters->puk,
			 sizeof(parameters->puk));
	__builtin_memcpy(control->unblockPin.pin, parameters->pin,
			 sizeof(parameters->pin));
	result = bbmcSendSimpleMessage(control, 32);
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcVerifyPinForDriver(int subscriberId, const void *pin, SceSize length)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if ((length > 9) | (pin == NULL))
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 4)
		goto invalidSubscriber;
	if (control->currentState <= 1 || control->pinStatus.pin1Status != 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memcpy(control->unblockPin.pin, pin, length);
	control->unblockPin.pinLength = length;
	bbmcLeaveCS(&control->mutex);
	return bbmcSendSimpleMessage(control, 33);
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcSMSGetListStartForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState == 8 || control->currentState <= 3 ||
	    control->smsListBusy != 0 || control->smsReadState != 0 ||
	    control->smsSendBusy == 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memset(control->smsIndices, 0, sizeof(control->smsIndices));
	control->smsCount = 0;
	message = msgAllocMsgPacket(control, 84);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.smsListOutput.count = &control->smsCount;
	message->body.smsListOutput.indices = control->smsIndices;
	control->smsListBusy = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->smsListBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSMSGetListDoneForDriver(int subscriberId, SceUInt32 *count,
				 SceUInt32 *indices, SceUInt32 capacity)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt32 copiedCount, listState;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (indices == NULL || count == NULL || capacity > 64)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState == 8 || control->currentState <= 3) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	listState = control->smsListBusy;
	if (listState == 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	if (listState == 1) {
		result = (int)0x80330008;
		goto out;
	}
	copiedCount = control->smsCount;
	result = 0;
	if (capacity < copiedCount)
		copiedCount = capacity;
	*count = copiedCount;
	memcpy(indices, control->smsIndices, copiedCount * sizeof(*indices));
	control->smsListBusy = 0;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSMSReadStartForDriver(int subscriberId, SceUInt32 memoryIndex)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState == 8 || control->currentState <= 2 ||
	    control->smsListBusy != 0 || control->smsReadState != 0 ||
	    control->smsSendBusy == 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memset(control->smsReadData, 0, sizeof(control->smsReadData));
	message = msgAllocMsgPacket(control, 86);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.smsReadRequest.length = &control->smsReadLength;
	message->body.smsReadRequest.data = control->smsReadData;
	message->body.smsReadRequest.memoryIndex = memoryIndex;
	message->body.smsReadRequest.format = &control->smsFormat;
	control->smsReadState = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->smsReadState = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSMSReadDoneForDriver(int subscriberId, void *data, SceUInt32 *format)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt32 readState;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (format == NULL || data == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState == 8 || control->currentState <= 2 ||
	    control->smsListBusy != 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	readState = control->smsReadState;
	if (readState == 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	if (readState == 1) {
		result = (int)0x80330008;
		goto out;
	}
	if (readState == 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	result = control->smsReadLength;
	if ((SceUInt32)result > sizeof(control->smsReadData)) {
		result = (int)0x80330002;
		goto out;
	}
	if (result != 0)
		memcpy(data, control->smsReadData, result);
	else
		memset(data, 0, sizeof(control->smsReadData));
	*format = control->smsFormat;
	control->smsReadState = 2;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSMSReadAckForDriver(int subscriberId, SceUInt32 memoryIndex,
			      SceUInt32 deleteMessage)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState == 8 || control->currentState <= 2 ||
	    control->smsListBusy != 0 || control->smsReadState != 2 ||
	    control->smsSendBusy == 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	message = msgAllocMsgPacket(control, 88);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->smsReadState = 3;
	message->body.smsReadAcknowledgement.memoryIndex = memoryIndex;
	message->body.smsReadAcknowledgement.deleteMessage = deleteMessage;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->smsReadState = 2;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSMSSendForDriver(int subscriberId, const void *data, SceSize length,
			 SceUInt32 format)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if ((length > 256) | (data == NULL))
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->currentState == 8 || control->currentState <= 2 ||
	    control->smsListBusy != 0 || control->smsReadState != 0 ||
	    control->smsSendBusy == 1) {
		result = (int)0x80330006;
		goto out;
	}
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 2) {
		result = (int)0x80330002;
		goto out;
	}
	memset(control->smsSendData, 0, sizeof(control->smsSendData));
	memcpy(control->smsSendData, data, length);
	message = msgAllocMsgPacket(control, 90);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.smsSendRequest.data = control->smsSendData;
	message->body.smsSendRequest.length = length;
	message->body.smsSendRequest.format = format;
	control->smsSendBusy = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->smsSendBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcGetDriverInfoForDriver(int subscriberId, BbmcDriverInfo *info)
{
	BbmcControl *control = bbmcGetControl();
	unsigned int featureId, firstBootReply, secondBootReply;
	int32x2_t states;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (info == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	featureId = control->subscribers[index].parameters.featureId;
	if (featureId != 7 && featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	secondBootReply = control->secondBootReply;
	firstBootReply = control->firstBootReply;
	states = vld1_s32(&control->currentState);
	info->secondBootReply = secondBootReply;
	vst1_s32(&info->currentState, states);
	info->firstBootReply = firstBootReply;
	result = 0;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcReserveOperationModeForDriver(int subscriberId,
	SceUInt32 operationMode)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt32 featureId;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (operationMode > 7)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	featureId = control->subscribers[index].parameters.featureId;
	if (featureId != 7 && featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	result = bbmcRequestOperationMode(control, operationMode);
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSetupGetValueStartForDriver(int subscriberId, SceUInt32 type)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	SceSize valueSize;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState <= 3 ||
	    (control->currentState == 4 && type != 0) ||
	    control->setupReadState != 0 || control->setupWriteState == 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	message = msgAllocMsgPacket(control, 145);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.setupMessage.type = type;
	switch (type) {
	case 0:
		valueSize = sizeof(control->deviceInformation);
		message->body.setupMessage.outputBuffer = &control->deviceInformation;
		break;
	case 1:
		valueSize = sizeof(control->smscAddress);
		message->body.setupMessage.outputBuffer = &control->smscAddress;
		break;
	case 4:
		valueSize = sizeof(control->systemSelection);
		message->body.setupMessage.outputBuffer = &control->systemSelection;
		break;
	case 5:
		valueSize = sizeof(control->profile);
		message->body.setupMessage.outputBuffer = &control->profile;
		break;
	case 7:
		valueSize = sizeof(control->countedBytes);
		message->body.setupMessage.outputBuffer = &control->countedBytes;
		break;
	default:
		message->body.setupMessage.outputBuffer = NULL;
		result = (int)0x80330002;
		msgFreeMsgPacket(control, message);
		goto out;
	}
	message->body.setupMessage.valueSize = valueSize;
	control->setupReadState = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->setupReadState = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSetupGetValueDoneForDriver(int subscriberId, SceUInt32 type,
	void *output, SceSize size)
{
	BbmcControl *control = bbmcGetControl();
	const BbmcSetupDescriptor *descriptor;
	const void *value;
	SceSize valueSize;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (output == NULL)
		return (int)0x80330002;
	if (type != 0 && type != 1 && type != 2 && type != 3 &&
	    type != 4 && type != 5 && type != 6 && type != 7)
		return (int)0x80330002;
	descriptor = &bbmcSetupDescriptors[type];
	if (size != descriptor->size || (descriptor->access & ~2U) != 0)
		return (int)0x80330002;
	switch (type) {
	case 0:
		valueSize = sizeof(control->deviceInformation);
		value = &control->deviceInformation;
		break;
	case 1:
		valueSize = sizeof(control->smscAddress);
		value = &control->smscAddress;
		break;
	case 4:
		valueSize = sizeof(control->systemSelection);
		value = &control->systemSelection;
		break;
	case 5:
		valueSize = sizeof(control->profile);
		value = &control->profile;
		break;
	case 7:
		valueSize = sizeof(control->countedBytes);
		value = &control->countedBytes;
		break;
	default:
		return (int)0x80330002;
	}
	if (valueSize > size)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState <= 3 ||
	    (control->currentState == 4 && type != 0) ||
	    control->setupReadState == 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	if (control->setupReadState == 1) {
		result = (int)0x80330008;
		goto out;
	}
	result = 0;
	memcpy(output, value, valueSize);
	control->setupReadState = 0;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcSetupSetValueForDriver(int subscriberId, SceUInt32 type,
	const void *input, SceSize size)
{
	BbmcControl *control = bbmcGetControl();
	const BbmcSetupDescriptor *descriptor;
	BbmcSetupValue *value;
	BbmcMessage *message;
	SceUInt32 featureId;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (input == NULL)
		return (int)0x80330002;
	if (type != 0 && type != 1 && type != 2 && type != 3 &&
	    type != 4 && type != 5 && type != 6 && type != 7)
		return (int)0x80330002;
	descriptor = &bbmcSetupDescriptors[type];
	if (size != descriptor->size || descriptor->access - 1U > 1)
		return (int)0x80330002;
	switch (type) {
	case 1:
	case 4:
	case 5:
	case 6:
	case 7:
		value = &control->setupValue;
		bbmcEnterCS(&control->mutex);
		memset(value, 0, size);
		featureId = 0;
		break;
	case 2:
	case 3:
		value = &control->setupValue;
		bbmcEnterCS(&control->mutex);
		memset(value, 0, size);
		featureId = 4;
		break;
	default:
		return (int)0x80330002;
	}
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != featureId)
		goto invalidSubscriber;
	if (subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	switch (type) {
	case 0:
	case 1:
	case 4:
	case 5:
	case 6:
	case 7:
		featureId = 0;
		break;
	case 2:
	case 3:
		featureId = 4;
		break;
	default:
		goto invalidSubscriber;
	}
	if (control->subscribers[index].parameters.featureId != featureId)
		goto invalidSubscriber;
	if (control->currentState <= 4 ||
	    (control->setupReadState != 0 && control->setupWriteState == 1)) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memcpy(value, input, size);
	message = msgAllocMsgPacket(control, 147);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.setupMessage.type = type;
	message->body.setupMessage.inputBuffer = value;
	message->body.setupMessage.valueSize = size;
	control->setupWriteState = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->setupWriteState = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
invalidSubscriber:
	result = (int)0x80330002;
	goto out;
}

int sceBbmcSetLPMModeForDriver(int subscriberId, int mode)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (mode < 0)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else {
		result = (int)0x80330002;
		goto out;
	}
	if (control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->powerTransitionBusy != 0)
		goto invalidState;
	if (control->recentPowerEvent != 0)
		goto setPolicy;
	if (control->currentState == 7) {
		if (mode != 1)
			goto invalidState;
		message = msgAllocMsgPacket(control, 29);
	} else if (control->currentState == 6 && mode == 0) {
		message = msgAllocMsgPacket(control, 30);
	} else {
		goto invalidState;
	}
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->powerTransitionBusy = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->powerTransitionBusy = 0;
		goto out;
	}
	if (result != 0)
		goto out;
setPolicy:
	control->powerPolicy = mode == 0;
	bbmcLeaveCS(&control->mutex);
	return 0;
invalidState:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330006;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcDLQBackupEFSNVForDriver(int subscriberId,
	const BbmcStringPair *parameters)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	const char *source;
	char *copy;
	SceSize length, copySize, allocationSize, remainder;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (parameters == NULL ||
	    (parameters->firstLength == 0 && parameters->secondLength == 0))
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 119);
	if (message == NULL)
		return (int)0x80330005;
	memset(&message->body, 0,
		sizeof(*message) - offsetof(BbmcMessage, body));
	length = parameters->firstLength;
	if (length != 0) {
		source = parameters->first;
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto firstAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto firstAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->firstLength;
		message->body.stringPair.first = copy;
		message->body.stringPair.firstLength = length;
	}
	length = parameters->secondLength;
	if (length != 0) {
		source = parameters->second;
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto secondAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto secondAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->secondLength;
		message->body.stringPair.second = copy;
		message->body.stringPair.secondLength = length;
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
secondAllocationFailed:
	message->body.stringPair.second = NULL;
	if (message->body.stringPair.first != NULL)
		bbmcFree(message->body.stringPair.first);
	goto freeSecond;
firstAllocationFailed:
	message->body.stringPair.first = NULL;
freeSecond:
	if (message->body.stringPair.second != NULL)
		bbmcFree(message->body.stringPair.second);
	msgFreeMsgPacket(control, message);
	return (int)0x80330005;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcDLQRestoreEFSNVForDriver(int subscriberId,
	const BbmcStringPair *parameters)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	const char *source;
	char *copy;
	SceSize length, copySize, allocationSize, remainder;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (parameters == NULL ||
	    (parameters->firstLength == 0 && parameters->secondLength == 0))
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 122);
	if (message == NULL)
		return (int)0x80330005;
	memset(&message->body, 0,
		sizeof(*message) - offsetof(BbmcMessage, body));
	length = parameters->firstLength;
	if (length != 0) {
		source = parameters->first;
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto firstAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto firstAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->firstLength;
		message->body.stringPair.first = copy;
		message->body.stringPair.firstLength = length;
	}
	length = parameters->secondLength;
	if (length != 0) {
		source = parameters->second;
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto secondAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto secondAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->secondLength;
		message->body.stringPair.second = copy;
		message->body.stringPair.secondLength = length;
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
secondAllocationFailed:
	message->body.stringPair.second = NULL;
	if (message->body.stringPair.first != NULL)
		bbmcFree(message->body.stringPair.first);
	goto freeSecond;
firstAllocationFailed:
	message->body.stringPair.first = NULL;
freeSecond:
	if (message->body.stringPair.second != NULL)
		bbmcFree(message->body.stringPair.second);
	msgFreeMsgPacket(control, message);
	return (int)0x80330005;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcDLQDownloadSafeImageForDriver(int subscriberId,
	const BbmcStringSet *parameters)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	const char *source;
	char *copy;
	SceSize length, copySize, allocationSize, remainder;
	SceUInt32 firstValue, secondValue, thirdValue;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (parameters == NULL ||
	    parameters->lengths[1] == 0 || parameters->lengths[0] == 0 ||
	    (parameters->lengths[2] == 0 && parameters->lengths[3] == 0 &&
	     parameters->lengths[4] == 0))
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 120);
	if (message == NULL)
		return (int)0x80330005;
	memset(&message->body, 0,
		sizeof(*message) - offsetof(BbmcMessage, body));
	firstValue = parameters->forwardedValues[0];
	secondValue = parameters->forwardedValues[1];
	thirdValue = parameters->forwardedValues[2];
	length = parameters->lengths[0];
	message->body.stringSet.forwardedValues[0] = firstValue;
	message->body.stringSet.forwardedValues[1] = secondValue;
	message->body.stringSet.forwardedValues[2] = thirdValue;
	if (length != 0) {
		source = parameters->strings[0];
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto stringOneAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto stringOneAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->lengths[0];
		message->body.stringSet.strings[0] = copy;
		message->body.stringSet.lengths[0] = length;
	}
	length = parameters->lengths[1];
	if (length != 0) {
		source = parameters->strings[1];
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto stringTwoAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto stringTwoAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->lengths[1];
		message->body.stringSet.strings[1] = copy;
		message->body.stringSet.lengths[1] = length;
	}
	length = parameters->lengths[2];
	if (length != 0) {
		source = parameters->strings[2];
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto stringThreeAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto stringThreeAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->lengths[2];
		message->body.stringSet.strings[2] = copy;
		message->body.stringSet.lengths[2] = length;
	}
	length = parameters->lengths[3];
	if (length != 0) {
		source = parameters->strings[3];
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto stringFourAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto stringFourAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->lengths[3];
		message->body.stringSet.strings[3] = copy;
		message->body.stringSet.lengths[3] = length;
	}
	length = parameters->lengths[4];
	if (length != 0) {
		source = parameters->strings[4];
		copySize = length + 1;
		if (source == NULL || copySize == 0)
			goto stringFiveAllocationFailed;
		remainder = copySize % 124;
		allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
		copy = bbmcAlloc(allocationSize, 0);
		if (copy == NULL)
			goto stringFiveAllocationFailed;
		memcpy(copy, source, copySize);
		length = parameters->lengths[4];
		message->body.stringSet.strings[4] = copy;
		message->body.stringSet.lengths[4] = length;
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
stringFiveAllocationFailed:
	message->body.stringSet.strings[4] = NULL;
	goto allocationFailed;
stringFourAllocationFailed:
	message->body.stringSet.strings[3] = NULL;
	goto allocationFailed;
stringThreeAllocationFailed:
	message->body.stringSet.strings[2] = NULL;
	goto allocationFailed;
stringTwoAllocationFailed:
	message->body.stringSet.strings[1] = NULL;
	goto allocationFailed;
stringOneAllocationFailed:
	message->body.stringSet.strings[0] = NULL;
	goto allocationFailed;
allocationFailed:
	msgFreeMsgPacket(control, message);
	return (int)0x80330005;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcDLQDownloadSlotImageForDriver(int subscriberId,
	const BbmcStringPairList *parameters)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	const char *source;
	char *copy;
	SceSize length, copySize, allocationSize, remainder;
	SceUInt32 count, selectedIndex, validCount;
	int index, entry, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (parameters == NULL || parameters->count == 0)
		return (int)0x80330002;
	validCount = parameters->entries[0].strings.firstLength != 0 &&
		parameters->entries[0].strings.first != NULL &&
		parameters->entries[0].strings.secondLength != 0 &&
		parameters->entries[0].strings.second != NULL;
	if (parameters->entries[1].strings.firstLength != 0 &&
	    parameters->entries[1].strings.first != NULL &&
	    parameters->entries[1].strings.secondLength != 0 &&
	    parameters->entries[1].strings.second != NULL)
		++validCount;
	if (parameters->entries[2].strings.firstLength != 0 &&
	    parameters->entries[2].strings.first != NULL &&
	    parameters->entries[2].strings.secondLength != 0 &&
	    parameters->entries[2].strings.second != NULL)
		++validCount;
	if (parameters->entries[3].strings.firstLength != 0 &&
	    parameters->entries[3].strings.first != NULL &&
	    parameters->entries[3].strings.secondLength != 0 &&
	    parameters->entries[3].strings.second != NULL)
		++validCount;
	if (parameters->entries[4].strings.firstLength != 0 &&
	    parameters->entries[4].strings.first != NULL &&
	    parameters->entries[4].strings.secondLength != 0 &&
	    parameters->entries[4].strings.second != NULL)
		++validCount;
	if (validCount != parameters->count ||
	    parameters->entries[parameters->selectedIndex].strings.first == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 121);
	if (message == NULL)
		return (int)0x80330005;
	memset(&message->body, 0,
		sizeof(*message) - offsetof(BbmcMessage, body));
	count = parameters->count;
	selectedIndex = parameters->selectedIndex;
	message->body.stringPairs.count = count;
	message->body.stringPairs.selectedIndex = selectedIndex;
	for (entry = 0; entry != 5; ++entry) {
		length = parameters->entries[entry].strings.firstLength;
		if (length != 0) {
			source = parameters->entries[entry].strings.first;
			copySize = length + 1;
			if (source == NULL || copySize == 0)
				goto firstAllocationFailed;
			remainder = copySize % 124;
			allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
			copy = bbmcAlloc(allocationSize, 0);
			if (copy == NULL)
				goto firstAllocationFailed;
			memcpy(copy, source, copySize);
			message->body.stringPairs.entries[entry].strings.first = copy;
			message->body.stringPairs.entries[entry].strings.firstLength =
				parameters->entries[entry].strings.firstLength;
			message->body.stringPairs.entries[entry].forwardedValue =
				parameters->entries[entry].forwardedValue;
		}
		length = parameters->entries[entry].strings.secondLength;
		if (length != 0) {
			source = parameters->entries[entry].strings.second;
			copySize = length + 1;
			if (source == NULL || copySize == 0)
				goto secondAllocationFailed;
			remainder = copySize % 124;
			allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
			copy = bbmcAlloc(allocationSize, 0);
			if (copy == NULL)
				goto secondAllocationFailed;
			memcpy(copy, source, copySize);
			message->body.stringPairs.entries[entry].strings.second = copy;
			message->body.stringPairs.entries[entry].strings.secondLength =
				parameters->entries[entry].strings.secondLength;
		}
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
firstAllocationFailed:
	message->body.stringPairs.entries[entry].strings.first = NULL;
	goto allocationFailed;
secondAllocationFailed:
	message->body.stringPairs.entries[entry].strings.second = NULL;
allocationFailed:
	msgFreeMsgPacket(control, message);
	return (int)0x80330005;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcDLGetSwdlErrorForDriver(int subscriberId, SceInt32 *error)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (error == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalid;
	*error = control->lastError;
	bbmcLeaveCS(&control->mutex);
	return 0;
invalid:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int sceBbmcDLQConfirmChangePartitionForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 124);
	if (message == NULL)
		return (int)0x80330005;
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

int bbmcStartCellLocationQuery(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 3)
		goto invalidSubscriber;
	if (control->currentState != 7 || control->cellQueryState != 0) {
		result = (int)0x80330006;
		goto out;
	}
	message = msgAllocMsgPacket(control, 163);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->cellQueryState = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->cellQueryState = 0;
	}
	goto out;
invalidSubscriber:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcDLUpdaterStartForDriver(int subscriberId, BbmcUpdatePackage *package)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	if (package == NULL || package->buffer == NULL || package->size == 0)
		return (int)0x80330002;
	if (package->size > ~(SceUInt32)(uintptr_t)package->buffer)
		return (int)0x8033070E;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 175);
	if (message == NULL)
		return (int)0x80330005;
	if (package->chunkCount == 0 || package->chunkIndex != 0) {
		result = (int)0x80330002;
		goto freeMessage;
	}
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	bbmcLeaveCS(&control->mutex);
	message->body.updatePackage.chunkCount = package->chunkCount;
	message->body.updatePackage.chunkIndex = package->chunkIndex;
	message->body.updatePackage.buffer = package->buffer;
	message->body.updatePackage.size = package->size;
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto freeMessage;
invalidSubscriber:
	result = (int)0x80330002;
	bbmcLeaveCS(&control->mutex);
freeMessage:
	msgFreeMsgPacket(control, message);
	return result;
}

int sceBbmcDLUpdaterContinueForDriver(int subscriberId, BbmcUpdatePackage *package)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	if (package == NULL || package->buffer == NULL || package->size == 0)
		return (int)0x80330002;
	if (package->size > ~(SceUInt32)(uintptr_t)package->buffer)
		return (int)0x8033070E;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 176);
	if (message == NULL)
		return (int)0x80330005;
	if (package->chunkCount == 0 || package->chunkIndex == 0) {
		result = (int)0x80330002;
		goto freeMessage;
	}
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalidSubscriber;
	message->body.updatePackage.chunkCount = package->chunkCount;
	message->body.updatePackage.chunkIndex = package->chunkIndex;
	message->body.updatePackage.buffer = package->buffer;
	message->body.updatePackage.size = package->size;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto freeMessage;
invalidSubscriber:
	result = (int)0x80330002;
	bbmcLeaveCS(&control->mutex);
freeMessage:
	msgFreeMsgPacket(control, message);
	return result;
}

int sceBbmcDLUpdaterGetArchiveVersionForDriver(int subscriberId,
	SceUInt32 *version)
{
	BbmcControl *control = bbmcGetControl();
	int index, result, firstActive;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	if (version == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	firstActive = control->subscribers[0].active;
	if (firstActive == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto checkSystemFeature;
	if (control->subscribers[index].parameters.featureId == 7)
		goto getVersion;
checkSystemFeature:
	if (firstActive == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 0)
		goto invalid;
getVersion:
	if (control->configurationReplyValid != 1) {
		result = (int)0x8033070F;
		goto out;
	}
	result = control->decodedReply;
	if (result != 0) {
		*version = result;
		result = 0;
	} else {
		*version = 1;
	}
	goto out;
invalid:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcDLUpdaterGetSequenceForDriver(int subscriberId,
	SceUInt32 *firstStatus, SceUInt32 *secondStatus, BbmcDriverInfo *info)
{
	BbmcControl *control = bbmcGetControl();
	unsigned int firstBootReply, secondBootReply;
	int32x2_t states;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 7)
		goto invalid;
	if (firstStatus != NULL)
		*firstStatus = control->firstStatusValue;
	if (secondStatus != NULL)
		*secondStatus = control->secondStatusValue;
	if (info != NULL) {
		secondBootReply = control->secondBootReply;
		firstBootReply = control->firstBootReply;
		states = vld1_s32(&control->currentState);
		info->secondBootReply = secondBootReply;
		vst1_s32(&info->currentState, states);
		info->firstBootReply = firstBootReply;
		bbmcLeaveCS(&control->mutex);
		return 0;
	}
	result = 0;
	goto out;
invalid:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int sceBbmcDLUpdaterGetPkgIdForDriver(int subscriberId,
	SceUInt32 *pkgId)
{
	BbmcControl *control = bbmcGetControl();
	int index, result, firstActive;

	if (pkgId == NULL)
		return (int)0x80330002;
	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->suspendState - 1U <= 2) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000A;
	}
	if (control->suspendState == 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x8033000B;
	}
	bbmcLeaveCS(&control->mutex);
	bbmcEnterCS(&control->mutex);
	firstActive = control->subscribers[0].active;
	if (firstActive == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto checkSystemFeature;
	if (control->subscribers[index].parameters.featureId == 7)
		goto getPkgId;
checkSystemFeature:
	if (firstActive == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		index = 0;
	else if (control->subscribers[1].active == 1 &&
		 subscriberId == control->subscribers[1].parameters.subscriberId)
		index = 1;
	else if (control->subscribers[2].active == 1 &&
		 subscriberId == control->subscribers[2].parameters.subscriberId)
		index = 2;
	else if (control->subscribers[3].active == 1 &&
		 subscriberId == control->subscribers[3].parameters.subscriberId)
		index = 3;
	else if (control->subscribers[4].active == 1 &&
		 subscriberId == control->subscribers[4].parameters.subscriberId)
		index = 4;
	else if (control->subscribers[5].active == 1 &&
		 subscriberId == control->subscribers[5].parameters.subscriberId)
		index = 5;
	else if (control->subscribers[6].active == 1 &&
		 subscriberId == control->subscribers[6].parameters.subscriberId)
		index = 6;
	else if (control->subscribers[7].active == 1 &&
		 subscriberId == control->subscribers[7].parameters.subscriberId)
		index = 7;
	else if (control->subscribers[8].active == 1 &&
		 subscriberId == control->subscribers[8].parameters.subscriberId)
		index = 8;
	else
		goto invalid;
	if (control->subscribers[index].parameters.featureId != 0)
		goto invalid;
getPkgId:
	result = bbmcLookupModeValue(control->configurationMode, pkgId);
	goto out;
invalid:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

int bbmcSendSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 operation, unsigned int byteCount, ...)
{
	va_list args;
	unsigned int i;

	if (byteCount > 27)
		return (int)0x80330002;
	memset(packet, 0, sizeof(*packet));
	packet->request.command[0] = 0x80;
	packet->request.data[0] = selector;
	packet->request.data[1] = operation;
	packet->request.command[1] = 0x0a;
	packet->request.length = 3;
	va_start(args, byteCount);
	for (i = 0; i < byteCount; i++)
		packet->request.data[i + 2] = va_arg(args, int);
	va_end(args);
	return SceSysconForDriver_C2224E82(packet, 0, callback, control);
}

int bbmcSendSizedSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 payloadLength, unsigned int byteCount, ...)
{
	va_list args;
	unsigned int i;

	if (byteCount > 27 || payloadLength > 252)
		return (int)0x80330002;
	memset(packet, 0, sizeof(*packet));
	packet->request.data[0] = selector;
	packet->request.data[1] = payloadLength;
	packet->request.length = payloadLength + 3;
	packet->request.command[0] = 0x81;
	packet->request.command[1] = 0x0a;
	va_start(args, byteCount);
	for (i = 0; i < byteCount; i++)
		packet->request.data[i + 2] = va_arg(args, int);
	va_end(args);
	return SceSysconForDriver_C2224E82(packet, 0, callback, control);
}

int bbmcSendSysconByteCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *packet, void *argp),
	SceSysconPacket *packet, SceUInt8 value)
{
	memset(packet, 0, sizeof(*packet));
	packet->request.command[0] = 0x82;
	packet->request.command[1] = 0x0a;
	packet->request.data[0] = value;
	packet->request.length = 2;
	return SceSysconForDriver_C2224E82(packet, 0, callback, control);
}

int bbmcGetBaryonVersion(void)
{
	return SceSysconForDriver_FF86F4C5();
}

int bbmcCancelSysconCommand(SceSysconPacket *packet, int *pending)
{
	int ret;

	if (pending == NULL || *pending != 1)
		return (int)0x80330002;
	ret = SceSysconForDriver_C9DB5BFF(packet);
	if (ret != 0)
		return sceSysconCmdSync(packet, 0);
	*pending = 0;
	return ret;
}

static const BbmcSetupDescriptor bbmcSetupDescriptors[8] = {
	{ 0, sizeof(BbmcDeviceInformation), 0 },
	{ 1, sizeof(BbmcSmscAddress), 2 },
	{ 2, sizeof(BbmcPinProtection), 1 },
	{ 3, sizeof(BbmcPinChange), 1 },
	{ 4, sizeof(BbmcSystemSelectionPreference), 2 },
	{ 5, sizeof(BbmcProfileSettings), 2 },
	{ 6, sizeof(BbmcNetworkRegistration), 1 },
	{ 7, sizeof(QmiCountedBytes), 2 },
};

/* The selector and returned value occupy one complete eight-byte entry. */
typedef struct BbmcModeValue {
	SceUInt32 mode;
	SceUInt32 value;
} BbmcModeValue;

_Static_assert(sizeof(BbmcModeValue) == 8, "BBMC mode value size");
_Static_assert(offsetof(BbmcModeValue, value) == 4, "BBMC mode value layout");

static const BbmcModeValue bbmcModeValues[12] = {
	{ 1, 0 },
	{ 2, 0 },
	{ 3, 0 },
	{ 4, 0 },
	{ 12, 0 },
	{ 5, 0 },
	{ 6, 5 },
	{ 7, 5 },
	{ 8, 5 },
	{ 9, 5 },
	{ 10, 5 },
	{ 11, 5 },
};

/* Descriptive names: the records supply a value and optional equality checks. */
typedef struct BbmcModeConfiguration {
	SceUInt32 mode;
	SceUInt32 checkEnabled;
	SceUInt32 outputValue;
	SceUInt32 expectedWords[2];
	SceUInt16 expectedHalfwords[2];
	SceUInt8 expectedByte;
} BbmcModeConfiguration;

_Static_assert(sizeof(BbmcModeConfiguration) == 28,
	"BBMC mode configuration size");
_Static_assert(offsetof(BbmcModeConfiguration, checkEnabled) == 4 &&
	offsetof(BbmcModeConfiguration, outputValue) == 8 &&
	offsetof(BbmcModeConfiguration, expectedWords) == 12 &&
	offsetof(BbmcModeConfiguration, expectedHalfwords) == 20 &&
	offsetof(BbmcModeConfiguration, expectedByte) == 24,
	"BBMC mode configuration layout");

static const BbmcModeConfiguration bbmcModeConfigurations[12] = {
	{ 1, 0, 29, { 13, 13 }, { 0x641, 0x400 }, 6 },
	{ 2, 0, 29, { 13, 13 }, { 0x641, 0x400 }, 6 },
	{ 3, 0, 32, { 13, 13 }, { 0x641, 0x400 }, 7 },
	{ 4, 0, 29, { 13, 13 }, { 0x641, 0x400 }, 6 },
	{ 12, 0, 29, { 13, 13 }, { 0x641, 0x400 }, 6 },
	{ 5, 0, 29, { 13, 13 }, { 0x641, 0x400 }, 6 },
	{ 6, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
	{ 7, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
	{ 8, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
	{ 9, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
	{ 10, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
	{ 11, 0, 100, { 17, 17 }, { 0x64a, 0x100 }, 0 },
};

int bbmcLookupModeValue(int mode, SceUInt32 *output)
{
	int index;

	if (!output)
		return (int)0x80330002;
	if (mode == 1)
		index = 0;
	else if (mode == 2)
		index = 1;
	else if (mode == 3)
		index = 2;
	else if (mode == 4)
		index = 3;
	else if (mode == 12)
		index = 4;
	else if (mode == 5)
		index = mode;
	else if (mode == 6)
		index = mode;
	else if (mode == 7)
		index = mode;
	else if (mode == 8)
		index = 8;
	else if (mode == 9)
		index = mode;
	else if (mode == 10)
		index = mode;
	else if (mode == 11)
		index = mode;
	else
		return (int)0x80330002;
	*output = bbmcModeValues[index].value;
	return 0;
}

int bbmcLookupModeConfiguration(int mode,
	const BbmcModeConfiguration **output)
{
	if (!output)
		return (int)0x80330002;
	if (mode == 1)
		mode = 0;
	else if (mode == 2)
		mode = 1;
	else if (mode == 3)
		mode = 2;
	else if (mode == 4)
		mode = 3;
	else if (mode == 12)
		mode = 4;
	else if (mode == 5)
		;
	else if (mode == 6)
		;
	else if (mode == 7)
		;
	else if (mode == 8)
		mode = 8;
	else if (mode == 9)
		;
	else if (mode == 10)
		;
	else if (mode == 11)
		;
	else
		return (int)0x80330002;
	*output = &bbmcModeConfigurations[mode];
	return 0;
}

STUB(SceBbmcForDriver_009C73D2)
STUB(SceBbmcForDriver_07ADDA90)
STUB(SceBbmcForDriver_24EE6E9C)
STUB(SceBbmcForDriver_2930DB51)
STUB(SceBbmcForDriver_399EF002)
STUB(SceBbmcForDriver_3D06FDB0)
STUB(SceBbmcForDriver_3D27000A)
STUB(SceBbmcForDriver_4281E476)
STUB(SceBbmcForDriver_503C7E0B)
STUB(SceBbmcForDriver_683D19BF)
STUB(SceBbmcForDriver_6AD4053A)
STUB(SceBbmcForDriver_739274F3)
STUB(SceBbmcForDriver_75FA68D3)
STUB(SceBbmcForDriver_7EA964B6)
STUB(SceBbmcForDriver_864E6CA0)
STUB(SceBbmcForDriver_92D7C12C)
STUB(SceBbmcForDriver_93E5345A)
STUB(SceBbmcForDriver_A9CF664F)
STUB(SceBbmcForDriver_B17A056F)
STUB(SceBbmcForDriver_B7E650A9)
STUB(SceBbmcForDriver_BC3C8942)
STUB(SceBbmcForDriver_BF550F4A)
STUB(SceBbmcForDriver_D288E771)
STUB(SceBbmcForDriver_E09808A2)
STUB(SceBbmcForDriver_EBC5D3DB)
STUB(SceBbmcForDriver_F4C8482D)
STUB(SceBbmcForDriver_FB3DA35E)
STUB(SceBbmcForDriver_FCF6A1EA)
STUB(sceBbmcVerifyPin)
STUB(sceBbmcNetworkScanStart)
STUB(sceBbmcSMSGetListDone)
STUB(sceBbmcDLQGetCurrentFwDone)
STUB(sceBbmcIsAbleToRunUnderCurrentVoltage)
STUB(sceBbmcUnblockPin)
STUB(sceBbmcIsAbleToRunUnderCurrentTemp)
STUB(sceBbmcDLQConfirmChangePartition)
STUB(sceBbmcSimGetPLMNModeBit)
STUB(sceBbmcSetupSetValue)
STUB(sceBbmcSetupGetValueDone)
STUB(sceBbmcDLQGetCurrentFwStart)
STUB(sceBbmcSMSReadAck)
STUB(sceBbmcDLUpdaterGetLatestArchiveVersion)
STUB(sceBbmcReadSimContentStart)
STUB(sceBbmcGetNetworkInfo)
STUB(sceBbmcGetComFirmVersion)
STUB(sceBbmcResumeSubscriberCallback)
STUB(sceBbmcDLQGetFwListStart)
STUB(sceBbmcDLUpdaterCheckComVersion)
STUB(sceBbmcDLQRestoreEFSNV)
STUB(sceBbmcGpsCellGetLocationInfoStart)
STUB(sceBbmcDepersonalizeCK)
STUB(sceBbmcSMSGetListStart)
STUB(sceBbmcDebugSelectOutMedia)
STUB(sceBbmcDLQBackupEFSNV)
STUB(sceBbmcSMSReadStart)
STUB(sceBbmcEnvelopeCmd)
STUB(sceBbmcPacketGetLastCallEndReason)
STUB(sceBbmcNetworkAttach)
STUB(sceBbmcDLUpdaterFin)
STUB(sceBbmcDebugOutString)
STUB(sceBbmcSMSReadDone)
STUB(sceBbmcClearEvent)
STUB(sceBbmcGetFileRefreshDetail)
STUB(sceBbmcReserveOperationMode)
STUB(sceBbmcDLQDownloadSafeImage)
STUB(sceBbmcGetIdStorageFrom3GModule)
STUB(sceBbmcFileRefreshResponse)
STUB(sceBbmcGetFailureInfo)
STUB(sceBbmcGpsCellGetLocationInfoDone)
STUB(sceBbmcDLUpdaterStart)
STUB(sceBbmcSMSSend)
STUB(sceBbmcDLQDownloadSlotImage)
STUB(sceBbmcPacketReleasePDPContext)
STUB(sceBbmcGetDriverInfo)
STUB(sceBbmcPacketIfSetControlFlag)
STUB(sceBbmcSetLPMMode)
STUB(sceBbmcDLQGetFwListDone)
STUB(sceBbmcDLUpdaterGetSequence)
STUB(sceBbmcPacketIfClearControlFlag)
STUB(sceBbmcPacketIfGetControlFlag)
STUB(sceBbmcDLUpdaterContinue)
STUB(sceBbmcGetProactiveCmd)
STUB(sceBbmcNetworkScanDone)
STUB(sceBbmcUnblockCK)
STUB(sceBbmcSetupGetValueStart)
STUB(sceBbmcDLGetSwdlError)
STUB(sceBbmcIsPowerConfigForbiddenMode)
STUB(sceBbmcUnsubscribeFeature)
STUB(sceBbmcTerminalResponseCmd)
STUB(sceBbmcDLUpdaterGetPkgId)
STUB(sceBbmcSubscribeFeatureId)
STUB(sceBbmcNetGetRejectCause)
STUB(sceBbmcReadSimContentDone)
STUB(sceBbmcGetPinStatus)
STUB(sceBbmcNetworkScanAbort)
STUB(sceBbmcPacketGetCurrentBearerTech)
STUB(sceBbmcDLUpdaterGetArchiveVersion)
STUB(sceBbmcTurnOff)
STUB(sceBbmcTurnOn)
STUB(sceBbmcDLQAbort)
STUB(sceBbmcGetCKStatus)
