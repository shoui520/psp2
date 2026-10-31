/*
 * SceBbmc manager helpers and exported GPIO reference operations.
 * The remaining exports are explicit skeleton placeholders.
 */

#include <fw.h>
#include <skeleton.h>
#include <bbmc/bbmc.h>
#include <sysmem/debug.h>
#include <threadmgr/fast_mutex_object.h>
#include <power/power.h>
#include <syscon/syscon.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>
#include <modulemgr/modulemgr.h>
#include <idstorage/idstorage.h>
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
#include "usb_extif.h"
#include "usb_device.h"
#include "sw_dload/sw_dload.h"
#include "bbmc_netif.h"
#include "bbcmd.h"
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/pmmgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <sceerror.h>

static const BbmcSetupDescriptor bbmcSetupDescriptors[8];

extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
	const char *name, int attr, void *opt)
	__asm__("sceKernelInitializeFastMutex");
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
void bbmcPowerEventTimeout(void *argp);
int bbmcPowerOffStateHandler(BbmcControl *control, BbmcMessage *message);
int stmInitial_ComBoot(BbmcControl *control, BbmcMessage *message);

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

int bbmcLookupModeValue(int mode, SceUInt32 *output);
int bbmcLookupModeConfiguration(int mode,
	const BbmcModeConfiguration **output);
int _managerThread(SceSize argumentSize, void *arguments);
BbmcControl *g_bbmcControl;
static BbmcControl bbmcMainControl;
int bbmcProcessKillHandler(SceUID processId,
	ProcEventParam *parameters, void *common);

const SceKernelProcEventHandler bbmcProcessEventHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.kill = bbmcProcessKillHandler,
};

/* the entry guard of every API call: nothing is accepted while suspending or suspended */
static inline int bbmcCheckSuspendState(BbmcControl *control)
{
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
	return 0;
}

/* the subscriber slot of an active subscriber ID, or -1 */
static inline int bbmcFindSubscriber(BbmcControl *control, int subscriberId)
{
	if (control->subscribers[0].active == 1 &&
	    subscriberId == control->subscribers[0].parameters.subscriberId)
		return 0;
	if (control->subscribers[1].active == 1 &&
	    subscriberId == control->subscribers[1].parameters.subscriberId)
		return 1;
	if (control->subscribers[2].active == 1 &&
	    subscriberId == control->subscribers[2].parameters.subscriberId)
		return 2;
	if (control->subscribers[3].active == 1 &&
	    subscriberId == control->subscribers[3].parameters.subscriberId)
		return 3;
	if (control->subscribers[4].active == 1 &&
	    subscriberId == control->subscribers[4].parameters.subscriberId)
		return 4;
	if (control->subscribers[5].active == 1 &&
	    subscriberId == control->subscribers[5].parameters.subscriberId)
		return 5;
	if (control->subscribers[6].active == 1 &&
	    subscriberId == control->subscribers[6].parameters.subscriberId)
		return 6;
	if (control->subscribers[7].active == 1 &&
	    subscriberId == control->subscribers[7].parameters.subscriberId)
		return 7;
	if (control->subscribers[8].active == 1 &&
	    subscriberId == control->subscribers[8].parameters.subscriberId)
		return 8;
	return -1;
}

/* name unknown: the slot of indexedResponses holding a personalization feature,
 * or -1 */
static inline int bbmcFindIndexedResponse(BbmcControl *control, SceUInt32 featureId)
{
	if (featureId == control->indexedResponses[0].index)
		return 0;
	if (featureId == control->indexedResponses[1].index)
		return 1;
	if (featureId == control->indexedResponses[2].index)
		return 2;
	if (featureId == control->indexedResponses[3].index)
		return 3;
	if (featureId == control->indexedResponses[4].index)
		return 4;
	return -1;
}


int bbmcProcessKillHandler(SceUID processId,
	ProcEventParam *parameters, void *common)
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
	bbmcEnterCS(&control->mutex);
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
	bbmcLeaveCS(&control->mutex);
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

/* every message goes to the state machine (or the suspend handler), then to each sequence */
static int (*const bbmcSequences[32])(BbmcControl *, BbmcMessage *) = {
	seqSysconEvent, seqUpdateNetworkInfo, seqSetModemLink, seqSysconStatus,
	seqListSms, seqReadSms, seqSendSms, seqUpdateGPS,
	seqGpsRequest, seqStartNetIF, seqStopNetIF, seqGetChannelRates,
	seqChangeControlFlags, seqChangeOperationMode, seqAccessSetupValue,
	seqScanNetworks, seqChangeAttach, seqGetStoredImages,
	seqGetFirmwarePreference, seqSetFirmwareImage, seqSetFirmwarePreference,
	seqGetCellLocation, _swdlSeqCarrierQmi, _udSeqChkComPower,
	seqSimRefresh, seqProactiveCommand, seqSuspend, seqResume,
	seqUnlockPin, seqControlKey, seqRefreshPinStatus, seqDisconnectNetwork,
};

int _managerThread(SceSize argumentSize, void *arguments)
{
	BbmcControl *control = *(BbmcControl **)arguments;
	int (*const *sequence)(BbmcControl *, BbmcMessage *);
	int (*handler)(BbmcControl *, BbmcMessage *);
	BbmcMessage *message = NULL;
	SceUInt32 events;
	int i, ret;

	ret = bbcmdInitialize(1);
	if (ret < 0)
		return ret;
	ret = bbcmdExecute(3, 1, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	if (ret < 0)
		goto finalize;
	ret = bbcmdExecute(0, 3, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	if (ret < 0)
		goto close_channel3;
	ret = bbcmdExecute(1, 3, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	if (ret < 0)
		goto close_channel0;

	control->sysconPowerInverted = SceSysconForDriver_63B14156() == 0;
	SceSysconForDriver_85E5DEBF(bbmcSysconPowerStateChanged, control);
	control->powerEventActive = ScePowerForDriver_5210A6FE(65569) > 0;
	ScePowerForDriver_B24263F0(1, bbmcPowerProcessFlagsChanged, control);

	for (;;) {
		if (msgReceiveMsgPacket(control, &message) < 0)
			return 0;
		if (message->command == 0)
			control->flags |= 1;
		if (control->suspendState <= 1) {
			stmProcessMessage(control, message);
		} else if (control->suspendMessageHandler != NULL) {
			ret = control->suspendMessageHandler(control, message);
			if (ret == 0) {
				bbmcEnterCS(&control->mutex);
				control->suspendMessageHandler = NULL;
				if (control->suspendState == 2) {
					bbmcGpioTransitionHook();
					bbmcFlushTimers();
					control->suspendState = 3;
				}
				bbmcLeaveCS(&control->mutex);
			} else if (ret == 2) {
				control->suspendMessageHandler = NULL;
			} else if (ret < 0) {
				int (*const suspendHandlers[24])(BbmcControl *, BbmcMessage *) = {
					suspendSteadyComBoot, suspendInitialToComBoot,
					suspendSteadyStandby, suspendDuringFromComBootToStandby,
					suspendDuringFromComBootToInitial,
					suspendDuringFromStandbyToAuthorized,
					suspendDuringFromStandbyToComBoot, suspendSteadyTestMode,
					suspendSteadyAuthorized,
					suspendDuringFromAuthorizedToActivated,
					suspendDuringFromAuthorizedToStandby,
					suspendSteadyActivated,
					suspendDuringFromActivatedToConnected,
					suspendDuringFromActivatedToLowPowerMode,
					suspendActivatedToStandbyOrAuthorized,
					suspendSteadyConnected,
					suspendConnectedToLowPowerModeOrStandby, suspendGotoLPM,
					suspendDuringFromLpmToConnected,
					suspendLowPowerModeToStandby, suspendSteadyLowPowerMode,
					suspendGotoConnected, suspendTestModeToAuthorized,
					suspendTestModeToStandby,
				};

				handler = control->suspendMessageHandler;
				for (i = 0; i < 24; i++)
					if (handler == suspendHandlers[i])
						break;
				if (i == 24)
					control->suspendError = 0x80030118;
				else
					control->suspendError = 0x80030100 | i;
				SCE_DBG_ASSERT_FUNC(0, 750, "_managerThread");
			}
		}
		for (sequence = bbmcSequences; sequence != &bbmcSequences[32]; sequence++)
			(*sequence)(control, message);
		msgFreeMsgPacket(control, message);

		bbmcEnterCS(&control->mutex);
		for (i = 0; i < 9; i++) {
			if (control->subscribers[i].active == 1 &&
			    control->subscribers[i].parameters.callbackId > 0) {
				events = (control->deliveredEvents[i] &
					  ~control->acknowledgedEvents[i]) |
					 (control->pendingEvents[i] &
					  control->subscribers[i].parameters.eventMask);
				if (events == 0 || control->callbackPending[i] != 0)
					continue;
				control->callbackPending[i] = 1;
				sceKernelNotifyCallback(
					control->subscribers[i].parameters.callbackId, events);
				control->deliveredEvents[i] = events;
			}
			control->acknowledgedEvents[i] = 0;
			control->pendingEvents[i] = 0;
		}
		bbmcLeaveCS(&control->mutex);

		if ((control->flags & 1) != 0 && control->currentState == 0 &&
		    control->targetState == 0)
			break;
	}
	control->flags &= ~1;
	ScePowerForDriver_B24263F0(1, NULL, NULL);
	SceSysconForDriver_85E5DEBF(NULL, NULL);
	bbcmdExecute(1, 4, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	bbcmdExecute(0, 4, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	bbcmdExecute(3, 2, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
	bbcmdFinalize();
	return 0;

close_channel0:
	bbcmdExecute(0, 4, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
close_channel3:
	bbcmdExecute(3, 2, NULL, NULL, bbcmdManagerCompletion, &bbmcMainControl);
finalize:
	bbcmdFinalize();
	return ret;
}

/*
 * Sony's 4.00 build inlines these into bbmc.c's callers; its 3.6x builds, where the asserts
 * are compiled out, call them whole.
 */
#if FW_SINCE(0x04000000)
#define BBMC_CS_INLINE
#else
#define BBMC_CS_INLINE	__attribute__((noinline))
#endif

BBMC_CS_INLINE void bbmcEnterCS(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcLockFastMutex(mutex);

	if (ret < 0)
		SCE_DBG_ASSERT(0, 1375);
}

BBMC_CS_INLINE void bbmcLeaveCS(struct ThreadmgrFastMutex *mutex)
{
	int ret = bbmcUnlockFastMutex(mutex);

	if (ret < 0)
		SCE_DBG_ASSERT(0, 1398);
}

/* name unknown: (re)arms the 15 s retry of a network start held back by a power event */
void bbmcScheduleDeferredNetworkRetry(BbmcControl *control)
{
	int timerId;

	if (control->deferredNetworkStart == 0)
		return;
	if (control->networkRetryTimer > 0) {
		bbmcTimerCancelHandler(control->networkRetryTimer);
		control->networkRetryTimer = 0;
	}
	timerId = bbmcTimerSetHandler(15000000, bbmcNetworkRetryTimeout, control);
	if (timerId > 0)
		control->networkRetryTimer = timerId;
	else
		control->networkRetryTimer = 0;
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
	if (sceIdStorageReadLeaf(0x113, &modemConfiguration) < 0 ||
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
	result = sceKernelCreateThread("SceBbmcMain", _managerThread, 96,
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
		&bbmcProcessEventHandlers, NULL);
	if (result < 0)
		goto unregisterSystemEvent;
	control->processEventHandler = result;
	if (scePowerSetWakeupCondition(256, 1) < 0)
		goto unregisterProcessEvent;
	if (SceSysconForDriver_4E88B4D9(bbmcSysconNotification, control) < 0)
		goto disablePower;
	g_bbmcControl = control;
	if (sceKernelStartThread(control->threadId, sizeof(g_bbmcControl),
		&g_bbmcControl) >= 0)
		return 0;
	SceSysconForDriver_4E88B4D9(NULL, NULL);
disablePower:
	scePowerSetWakeupCondition(256, 0);
unregisterProcessEvent:
	sceKernelUnregisterProcEventHandler(control->processEventHandler);
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

/* name unknown: posts the modem's new firmware preference (command 161) */
int bbmcRequestSetFirmwarePreference(BbmcControl *control,
	const QmiSetFirmwarePreference *preference)
{
	BbmcMessage *message;
	int result;

	message = msgAllocMsgPacket(control, 161);
	if (message == NULL)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->setFirmwarePreferenceState == 1) {
		result = (int)0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	memset(&control->setFirmwarePreference, 0, sizeof(control->setFirmwarePreference));
	control->downloadImageCount = 0;
	control->firstDownloadImageType = 0;
	control->secondDownloadImageType = 0;
	memcpy(&control->setFirmwarePreference, preference, sizeof(*preference));
	message->body.firmwareMessage.setFirmwarePreference = &control->setFirmwarePreference;
	control->setFirmwarePreferenceState = 1;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	control->setFirmwarePreferenceState = 0;
fail:
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: asks the modem for its stored images (command 155) */
int bbmcRequestStoredImages(BbmcControl *control)
{
	BbmcMessage *message;
	int result;

	message = msgAllocMsgPacket(control, 155);
	if (message == NULL)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->storedImagesState == 1) {
		result = (int)0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	memset(&control->storedImages, 0, sizeof(control->storedImages));
	message->body.firmwareMessage.storedImages = &control->storedImages;
	control->storedImagesState = 1;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	control->storedImagesState = 0;
fail:
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: posts one firmware image description to the modem (command 159) */
int bbmcRequestFirmwareImage(BbmcControl *control, const QmiFirmwareImage *image)
{
	BbmcMessage *message;
	int result;

	message = msgAllocMsgPacket(control, 159);
	if (message == NULL)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->firmwareImageState == 1) {
		result = (int)0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	control->firmwareImage = *image;
	message->body.firmwareMessage.firmwareImage = &control->firmwareImage;
	control->firmwareImageState = 1;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	control->firmwareImageState = 0;
fail:
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: the transport is up and has answered */
int bbmcIsTransportReady(BbmcControl *control)
{
	return control->transportReady == 1 && control->transportReplyPending == 1;
}

/* name unknown: a state change is under way, or the SIM still waits for its PIN */
int bbmcIsStateTransitionPending(BbmcControl *control)
{
	if (control->flags == 0 && control->targetState == control->currentState)
		return 0;
	if (control->currentState != 2)
		return 1;
	return control->pinStatus.pin1Status - 1 > 1;
}

/* name unknown: has the download module report to msgForwardNotifications, once */
int bbmcRegisterDownloadNotification(BbmcControl *control)
{
	SwdlClient client;
	int result = 0;

	if (control->downloadNotificationRegistered == 0) {
		client.notify = msgForwardNotifications;
		client.argument = control;
		result = swdlInitialize(8, &client);
		if (result >= 0) {
			result = 0;
			control->downloadNotificationRegistered = 1;
		}
	}
	return result;
}

/* name unknown: a forced operation mode overrides the requested one with 8 */
SceUInt32 bbmcSelectOperationMode(BbmcControl *control, SceUInt32 operationMode)
{
	if (control->forceOperationMode != 0)
		return 8;
	return operationMode;
}

/* name unknown: the subscriber events the five indexed control key replies raise */
SceUInt32 bbmcCollectControlKeyEvents(BbmcControl *control)
{
	SceUInt32 events = 0;
	int i;

	for (i = 0; i < 5; i++) {
		if (control->indexedResponses[i].primaryValue == 0)
			continue;
		bbmcEnterCS(&control->mutex);
		control->controlKeyReported = 1;
		bbmcLeaveCS(&control->mutex);
		if (control->indexedResponses[i].primaryValue == 2 &&
		    control->indexedResponses[0].detailBytes[1] == 0)
			events |= 0x1000000;
		if (control->indexedResponses[i].secondaryValue == 1)
			events |= 0x800000;
	}
	return events;
}

/* name unknown: remembers where a state transition stopped and why */
void bbmcSaveTransitionState(BbmcControl *control, SceInt32 status, SceUInt32 step)
{
	bbmcEnterCS(&control->mutex);
	control->previousCurrentState = control->currentState;
	control->previousTargetState = control->targetState;
	control->transitionStep = step;
	control->transitionStatus = status;
	bbmcLeaveCS(&control->mutex);
}

/* name unknown: the transport is up, has answered and finished its handshake */
int bbmcIsTransportHandshakeComplete(BbmcControl *control)
{
	return control->transportReady == 1 && control->transportReplyPending == 1 &&
	       control->transportHandshakePending == 1;
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

/* feature 7 (update): posts command 123 */
int SceBbmcForDriver_B7E650A9(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 7) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	bbmcLeaveCS(&control->mutex);
	message = msgAllocMsgPacket(control, 123);
	if (message == NULL)
		return (int)0x80330005;
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		msgFreeMsgPacket(control, message);
	return result;
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

/* name unknown: hands the subscriber the cell the modem last reported, GERAN before UMTS */
int bbmcGetCellLocationResult(int subscriberId, BbmcGpsCellInfo *info)
{
	BbmcControl *control = bbmcGetControl();
	const SceUInt8 *plmn;
	int index;
	int digit, mnc;

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
	else
		goto invalidSubscriber;
	if (control->subscribers[index].parameters.featureId != 3)
		goto invalidSubscriber;
	memset(info, 0, sizeof(*info));
	if (control->cellLocation.geran.cellId != 0xFFFFFFFF) {
		info->cellId = control->cellLocation.geran.cellId;
		info->radioInterface = 1;
		info->lac = control->cellLocation.geran.lac;
		plmn = control->cellLocation.geran.plmn;
		info->mcc = 100 * (plmn[0] & 0xF) + 10 * (plmn[0] >> 4) + (plmn[1] & 0xF);
		digit = plmn[1] >> 4;
		mnc = 100 * (plmn[2] & 0xF) + 10 * (plmn[2] >> 4);
		info->mnc = digit == 15 ? mnc / 10 : mnc + digit;
		info->timingAdvance = control->cellLocation.geran.timingAdvance;
		info->signalStrength = control->cellLocation.geran.rxLevel > 63 ?
			-48 : control->cellLocation.geran.rxLevel - 111;
		info->measurementPresent = 1;
		info->cellPresent = 1;
	} else if (control->cellLocation.umts.cellId != 0xFFFF) {
		info->cellId = control->cellLocation.umts.cellId;
		info->radioInterface = 2;
		info->lac = control->cellLocation.umts.lac;
		plmn = control->cellLocation.umts.plmn;
		info->mcc = 100 * (plmn[0] & 0xF) + 10 * (plmn[0] >> 4) + (plmn[1] & 0xF);
		digit = plmn[1] >> 4;
		mnc = 100 * (plmn[2] & 0xF) + 10 * (plmn[2] >> 4);
		info->mnc = digit == 15 ? mnc / 10 : mnc + digit;
		info->measurementPresent = 1;
		info->cellPresent = 1;
		info->signalStrength = control->cellLocation.umts.rscp > 0x8000 ?
			-0x8000 : -control->cellLocation.umts.rscp;
		info->primaryScramblingCode = control->cellLocation.umts.primaryScramblingCode;
	} else
		info->cellPresent = 0;
	control->cellQueryState = 0;
	bbmcLeaveCS(&control->mutex);
	return 0;
invalidSubscriber:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330002;
}

/* feature 0: starts a network scan (command 149) */
int SceBbmcForDriver_EBC5D3DB(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState != 7 || control->networkScanState != 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	message = msgAllocMsgPacket(control, 149);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->networkScanState = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->networkScanState = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: hands out the finished network scan and ends it */
int SceBbmcForDriver_503C7E0B(int subscriberId, BbmcNetworkScanInfo *scan)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0)
		result = (int)0x80330002;
	else if (control->currentState != 7 || control->networkScanState == 0)
		result = (int)0x80330006;
	else if (control->networkScanState == 1)
		result = (int)0x80330008;
	else {
		result = 0;
		if (scan != NULL)
			memcpy(scan, &control->networkScan, sizeof(*scan));
	}
	control->networkScanState = 0;
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: aborts the running network scan (command 151) */
int SceBbmcForDriver_739274F3(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState != 7 || control->networkScanState != 1 ||
	    control->networkScanAbortBusy == 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	control->networkScanAbortBusy = 1;
	message = msgAllocMsgPacket(control, 151);
	if (message == NULL) {
		result = (int)0x80330005;
		control->networkScanAbortBusy = 0;
		goto out;
	}
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->networkScanAbortBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: attaches to or detaches from the packet service (command 152) */
int SceBbmcForDriver_75FA68D3(int subscriberId, SceInt32 attach)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto out;
	}
	if (control->currentState != 7)
		goto busy;
	if (control->networkInfo.serving.psAttachState == 1) {
		if (attach == 1)
			goto busy;
	} else if (attach == 0)
		goto busy;
	message = msgAllocMsgPacket(control, 152);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	message->body.commandResult = attach;
	control->attachChangeBusy = 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->attachChangeBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
busy:
	bbmcLeaveCS(&control->mutex);
	return (int)0x80330006;
}

/* feature 5 (network interface): disconnects the network. As Sony's, a failed
 * lookup returns without leaving the critical section. */
int SceBbmcForDriver_3D27000A(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5)
		return (int)0x80330002;
	bbmcLeaveCS(&control->mutex);
	return DisconnectNetwork();
}

/* feature 5: the reason the last packet data call ended */
int SceBbmcForDriver_BF550F4A(int subscriberId, BbmcCallEndReason *reason)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	if (reason == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	*reason = control->callEndReason;
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* feature 5: sets packet interface control flags (command 113) */
int SceBbmcForDriver_FB3DA35E(int subscriberId, SceUInt32 flags)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5) {
		result = (int)0x80330002;
		goto out;
	}
	result = (int)0x80330503;
	if (control->controlFlagsBusy != 0)
		goto out;
	message = msgAllocMsgPacket(control, 113);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->controlFlagsBusy = 1;
	message->body.controlFlag.set = 1;
	message->body.controlFlag.mask = flags;
	control->normalizedControlState = ((control->controlFlags | flags) & 3) == 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->controlFlagsBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 5: clears packet interface control flags (command 113) */
int SceBbmcForDriver_7EA964B6(int subscriberId, SceUInt32 flags)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5) {
		result = (int)0x80330002;
		goto out;
	}
	result = (int)0x80330503;
	if (control->controlFlagsBusy != 0)
		goto out;
	message = msgAllocMsgPacket(control, 113);
	if (message == NULL) {
		result = (int)0x80330005;
		goto out;
	}
	control->controlFlagsBusy = 1;
	message->body.controlFlag.mask = flags;
	message->body.controlFlag.set = 0;
	control->normalizedControlState = (control->controlFlags & 3 & ~flags) == 1;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		control->controlFlagsBusy = 0;
	}
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 5: the packet interface control flags, once no change is pending */
int SceBbmcForDriver_BC3C8942(int subscriberId, SceUInt32 *flags)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (flags == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->controlFlagsBusy != 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330503;
	}
	*flags = control->controlFlags;
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* feature 5: the current bearer technology and its details. As Sony's, a failed
 * lookup returns without leaving the critical section. */
int SceBbmcForDriver_E09808A2(int subscriberId, SceUInt32 *bearer)
{
	BbmcControl *control = bbmcGetControl();
	SceUInt32 technology, details;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (bearer == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 5)
		return (int)0x80330002;
	technology = control->currentBearer;
	details = control->bearerDetails;
	bearer[0] = technology;
	bearer[1] = details;
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* feature 7: asks the modem for its stored images (command 155); the result is
 * taken with sceBbmcDLQGetFwListDoneForDriver */
int sceBbmcDLQGetFwListStartForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	message = msgAllocMsgPacket(control, 155);
	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (message == NULL)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->storedImagesState == 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 7) {
		result = (int)0x80330002;
		goto unlock;
	}
	memset(&control->storedImages, 0, sizeof(control->storedImages));
	message->body.firmwareMessage.storedImages = &control->storedImages;
	control->storedImagesState = 1;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	control->storedImagesState = 0;
	goto free;
unlock:
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 7: copies out the stored images once the modem has answered */
int sceBbmcDLQGetFwListDoneForDriver(int subscriberId, QmiStoredImages *storedImages)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (storedImages == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 7) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1 || control->storedImagesState != 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	result = control->storedImagesResult;
	if (result == 0)
		memcpy(storedImages, &control->storedImages, sizeof(*storedImages));
	control->storedImagesState = 0;
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 7: asks the modem for its firmware preference (command 157); the result is
 * taken with sceBbmcDLQGetCurrentFwDoneForDriver */
int sceBbmcDLQGetCurrentFwStartForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	message = msgAllocMsgPacket(control, 157);
	if (message == NULL)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->firmwarePreferenceState == 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 7) {
		result = (int)0x80330002;
		goto unlock;
	}
	memset(&control->firmwarePreference, 0, sizeof(control->firmwarePreference));
	message->body.firmwareMessage.firmwarePreference = &control->firmwarePreference;
	control->firmwarePreferenceState = 1;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	control->firmwarePreferenceState = 0;
	goto free;
unlock:
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 7: copies out the firmware preference once the modem has answered */
int sceBbmcDLQGetCurrentFwDoneForDriver(int subscriberId, QmiFirmwarePreference *firmwarePreference)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (firmwarePreference == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 7) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1 || control->firmwarePreferenceState != 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	result = control->firmwarePreferenceResult;
	if (result == 0)
		memcpy(firmwarePreference, &control->firmwarePreference, sizeof(*firmwarePreference));
	control->firmwarePreferenceState = 0;
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* the state the last transition left: current and target state, the step it
 * stopped at and its status (bbmcSaveTransitionState) */
int SceBbmcForDriver_A9CF664F(SceInt32 *transition)
{
	BbmcControl *control = bbmcGetControl();
	SceInt32 targetState, status;
	SceUInt32 step;

	if (control == NULL)
		return (int)0x80330003;
	if (transition == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	targetState = control->previousTargetState;
	step = control->transitionStep;
	status = control->transitionStatus;
	transition[0] = control->previousCurrentState;
	transition[1] = targetState;
	transition[2] = step;
	transition[3] = status;
	bbmcLeaveCS(&control->mutex);
	return 0;
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

/* feature 4: copies out the last proactive command of the SIM toolkit (sceBbmcGetProactiveCmd) */
int SceBbmcForDriver_399EF002(int subscriberId, BbmcProactiveCmd *command)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (command == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memcpy(command, &control->proactiveCommand, sizeof(*command));
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* feature 4: answers a proactive command (sceBbmcTerminalResponseCmd) (command 167) */
int SceBbmcForDriver_93E5345A(int subscriberId, const BbmcTerminalResponse *response)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (response == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	message = msgAllocMsgPacket(control, 167);
	if (message == NULL) {
		result = (int)0x80330005;
		goto unlock;
	}
	message->body.terminalResponse = memcpy(&control->terminalResponse, response,
		sizeof(control->terminalResponse));
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto free;
unlock:
	message = NULL;
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 4: sends a SIM toolkit envelope (sceBbmcEnvelopeCmd) (command 169) */
int SceBbmcForDriver_F4C8482D(int subscriberId, const QmiCat2CommandOptions *options)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (options == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	message = msgAllocMsgPacket(control, 169);
	if (message == NULL) {
		result = (int)0x80330005;
		goto unlock;
	}
	control->envelopeOptions = *options;
	message->body.envelopeOptions = &control->envelopeOptions;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto free;
unlock:
	message = NULL;
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 4: copies out the details of the last SIM file refresh (sceBbmcGetFileRefreshDetail) */
int SceBbmcForDriver_2930DB51(int subscriberId, BbmcFileRefreshDetail *detail)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (detail == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memcpy(detail, &control->refreshDetail, sizeof(*detail));
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* feature 4: answers a SIM file refresh (sceBbmcFileRefreshResponse) (command 171) */
int SceBbmcForDriver_009C73D2(int subscriberId, SceUInt32 response)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (response > 3)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	message = msgAllocMsgPacket(control, 171);
	if (message == NULL) {
		result = (int)0x80330005;
		goto unlock;
	}
	message->body.refreshResponse = response;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto free;
unlock:
	message = NULL;
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 4: reads a SIM file (command 173); the content is taken with
 * SceBbmcForDriver_24EE6E9C (sceBbmcReadSimContentStart) */
int SceBbmcForDriver_D288E771(int subscriberId, const BbmcSimFile *file)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (file == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState != 3 && control->currentState - 5U > 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	message = msgAllocMsgPacket(control, 173);
	if (message == NULL) {
		result = (int)0x80330005;
		goto unlock;
	}
	message->body.simFile = file;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result >= 0)
		return result;
	goto free;
unlock:
	message = NULL;
	bbmcLeaveCS(&control->mutex);
free:
	msgFreeMsgPacket(control, message);
	return result;
}

/* feature 4: the content of the SIM file read last (sceBbmcReadSimContentDone) */
int SceBbmcForDriver_24EE6E9C(int subscriberId, BbmcSimContentInfo *content)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (content == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState != 3 && control->currentState - 5U > 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	*content = control->simContent;
	result = 0;
unlock:
	bbmcLeaveCS(&control->mutex);
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

/* feature 4: lifts the SIM lock of one personalization feature with its control
 * key (command 179, sceBbmcDepersonalizeCK) */
int SceBbmcForDriver_683D19BF(int subscriberId, const BbmcControlKeyParam *param)
{
	BbmcControl *control = bbmcGetControl();
	int index, result, i;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (param == NULL || param->controlKeyLength > 21)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	i = bbmcFindIndexedResponse(control, param->featureId);
	if (i < 0) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->indexedResponses[i].primaryValue == 0) {
		result = (int)0x80330006;
		goto unlock;
	}
	control->controlKey.featureId = param->featureId;
	control->controlKey.controlKeyLength = param->controlKeyLength;
	memset(&control->controlKey.controlKey, 0, sizeof(control->controlKey.controlKey));
	control->controlKey.controlKey = param->controlKey;
	result = bbmcSendSimpleMessage(control, 179);
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 4: unblocks the SIM lock of one personalization feature with its control
 * key (command 180, sceBbmcUnblockCK) */
int SceBbmcForDriver_6AD4053A(int subscriberId, const BbmcControlKeyParam *param)
{
	BbmcControl *control = bbmcGetControl();
	int index, result, i;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (param == NULL || param->controlKeyLength > 21)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	i = bbmcFindIndexedResponse(control, param->featureId);
	if (i < 0) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->indexedResponses[i].primaryValue != 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	control->controlKey.featureId = param->featureId;
	control->controlKey.controlKeyLength = param->controlKeyLength;
	memset(&control->controlKey.controlKey, 0, sizeof(control->controlKey.controlKey));
	control->controlKey.controlKey = param->controlKey;
	result = bbmcSendSimpleMessage(control, 180);
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 4: the control key status of one personalization feature
 * (sceBbmcGetCKStatus) */
int SceBbmcForDriver_3D06FDB0(int subscriberId, SceUInt32 featureId, QmiIndexedResponse *status)
{
	BbmcControl *control = bbmcGetControl();
	int index, result, i;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (status == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
		goto unlock;
	}
	i = bbmcFindIndexedResponse(control, featureId);
	if (i < 0) {
		result = (int)0x80330002;
		goto unlock;
	}
	*status = control->indexedResponses[i];
	result = 0;
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: what the modem reported, in the layout of IdStorage leaf 0x113
 * (sceBbmcGetIdStorageFrom3GModule) */
int SceBbmcForDriver_07ADDA90(int subscriberId, BbmcModemConfigurationLeaf *leaf)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	if (leaf == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		result = (int)0x80330002;
		goto unlock;
	}
	if (control->currentState - 5U > 2) {
		result = (int)0x80330006;
		goto unlock;
	}
	leaf->imei = control->deviceInformation.imei;
	leaf->controlKeyReported = control->controlKeyReported;
	leaf->firmwareBuildSelection = control->firmwareBuildSelection;
	leaf->replyOperationMode = control->replyOperationMode;
	leaf->runningStorageIndex = control->runningStorageIndex;
	leaf->firmwareRevisionSelection = control->firmwareRevisionSelection;
	leaf->configurationReplySeen = control->configurationReplySeen;
	leaf->completionPending = control->completionPending;
	leaf->imeiSoftwareVersion = control->deviceInformation.imeiSoftwareVersion;
	result = 0;
unlock:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: the syscon reports the modem may run at the current temperature (sceBbmcIsAbleToRunUnderCurrentTemp) */
int sceBbmcIsAbleToRunUnderCurrentTempForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0)
		return (int)0x80330002;
	return control->sysconPowerInverted;
}

/* feature 0: the battery voltage lets the modem run (sceBbmcIsAbleToRunUnderCurrentVoltage) */
int sceBbmcIsAbleToRunUnderCurrentVoltageForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0)
		return (int)0x80330002;
	return control->powerAvailable;
}

/* feature 0: a power event forbids changing the power configuration now (sceBbmcIsPowerConfigForbiddenMode) */
int sceBbmcIsPowerConfigForbiddenModeForDriver(int subscriberId)
{
	BbmcControl *control = bbmcGetControl();
	int index;

	if (control == NULL)
		return (int)0x80330003;
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0)
		return (int)0x80330002;
	return control->powerEventActive != 0 || control->recentPowerEvent != 0;
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

/* features 7 and 0: whether the modem firmware is the one the configuration
 * mode expects (sceBbmcDLUpdaterCheckComVersion) */
int SceBbmcForDriver_4281E476(int subscriberId, SceUInt32 *matches)
{
	BbmcControl *control = bbmcGetControl();
	const BbmcModeConfiguration *configuration;
	int index, result, firstActive;

	if (matches == NULL)
		return (int)0x80330002;
	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
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
		goto checkVersion;
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
checkVersion:
	if (control->configurationReplyValid != 1) {
		result = (int)0x8033070F;
		goto out;
	}
	result = bbmcLookupModeConfiguration(control->configurationMode, &configuration);
	if (result >= 0)
		*matches = configuration->checkEnabled != 1 ||
			(control->firstConfigurationReply == configuration->expectedWords[0] &&
			 control->secondConfigurationReply == configuration->expectedWords[1] &&
			 control->versionFamily == configuration->expectedHalfwords[0] &&
			 control->versionRevision == configuration->expectedHalfwords[1] &&
			 control->versionPatch == configuration->expectedByte);
	goto out;
invalid:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* features 7 and 0: the firmware archive version the configuration mode
 * expects (sceBbmcDLUpdaterGetLatestArchiveVersion) */
int SceBbmcForDriver_FCF6A1EA(int subscriberId, SceUInt32 *version)
{
	BbmcControl *control = bbmcGetControl();
	const BbmcModeConfiguration *configuration;
	int index, result, firstActive;

	if (version == NULL)
		return (int)0x80330002;
	if (control == NULL)
		return (int)0x80330003;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
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
	result = bbmcLookupModeConfiguration(control->configurationMode, &configuration);
	if (result >= 0)
		*version = configuration->outputValue;
	goto out;
invalid:
	result = (int)0x80330002;
out:
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 0: the network's last reject cause (sceBbmcNetGetRejectCause) */
int SceBbmcForDriver_92D7C12C(int subscriberId, BbmcRejectCause *cause)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	if (cause == NULL)
		return (int)0x80330002;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->currentState <= 1) {
		result = (int)0x80330006;
	} else {
		__builtin_memset(cause, 0, sizeof(*cause));
		result = 0;
		cause->cause = control->nasCause;
		cause->reason = control->nasReason;
	}
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* feature 4: the value the last SIM refresh command carried
 * (sceBbmcSimGetPLMNModeBit) */
int SceBbmcForDriver_864E6CA0(int subscriberId, SceUInt32 *value)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	if (value == NULL)
		return (int)0x80330002;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 4) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	*value = control->refreshCommandValue;
	bbmcLeaveCS(&control->mutex);
	return 0;
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

/* feature 0: the modem firmware revision string (sceBbmcGetComFirmVersion) */
int SceBbmcForDriver_B17A056F(int subscriberId, char *revision, SceSize size)
{
	BbmcControl *control = bbmcGetControl();
	int index, result;

	if (control == NULL)
		return (int)0x80330003;
	if (revision == NULL || size - 1 > 103)
		return (int)0x80330002;
	result = bbmcCheckSuspendState(control);
	if (result < 0)
		return result;
	bbmcEnterCS(&control->mutex);
	index = bbmcFindSubscriber(control, subscriberId);
	if (index < 0 || control->subscribers[index].parameters.featureId != 0) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330002;
	}
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return (int)0x80330006;
	}
	memcpy(revision, control->deviceInformation.revision, size);
	bbmcLeaveCS(&control->mutex);
	return 0;
}

/* The user API: each syscall checks the caller (system programs, or any program on a
 * tool), copies its arguments in and out of user memory and calls the driver API.
 */

/* the user API is for system programs, or any program on a tool */
static inline __attribute__((always_inline)) int bbmcCheckUserPermission(void)
{
	uint8_t mode = 0;
	int ret;

	if (sceSblACMgrIsSystemProgram(0) || SceSblPostSsMgrForDriver_128FB35A(0) ||
	    SceQafMgrForDriver_BFD5E463())
		return 0;
	ret = sceSblPmMgrGetProductMode(&mode);
	if (ret < 0)
		return ret;
	if (mode != 1)
		return (int)SCE_ERROR_ERRNO_ENOSYS;
	return 0;
}

/* the user updater package mapped into the kernel between Start and Fin */
typedef struct BbmcUpdaterMapping {
	SceUID uid;
	const void *buffer;
	SceSize size;
} BbmcUpdaterMapping;

static BbmcUpdaterMapping bbmcUpdaterMapping = { .uid = -1 };

int sceBbmcGetNetworkInfo(BbmcNetworkInfo *info)
{
	BbmcNetworkInfo *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (info == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcGetNetworkInfoForDriver(buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(info, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSubscribeFeatureId(SceUInt32 featureId, SceUInt32 eventMask, SceUID callback)
{
	BbmcSubscribeFeatureIdParam parameters;
	SceUInt32 saved;
	int ret;

	parameters.featureId = featureId;
	parameters.eventMask = eventMask;
	parameters.callbackId = scePUIDtoGUID(0, callback);
	if (parameters.callbackId < 0)
		return parameters.callbackId;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret == 0) {
		ret = sceBbmcSubscribeFeatureIdForDriver(&parameters);
		if (ret < 0)
			parameters.subscriberId = ret;
		ret = parameters.subscriberId;
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcUnsubscribeFeature(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcUnsubscribeFeatureForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcClearEvent(int subscriberId, SceUInt32 eventMask)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcClearEventForDriver(subscriberId, eventMask);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcResumeSubscriberCallback(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcResumeSubscriberCallbackForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcTurnOn(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcTurnOnForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcTurnOff(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcTurnOffForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetPinStatus(int subscriberId, SceUInt32 *status, SceUInt16 *retries)
{
	SceUInt32 pinStatus;
	SceUInt16 pinRetries;
	SceUInt32 saved;
	int ret;

	if (status == NULL || retries == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = sceBbmcGetPinStatusForDriver(subscriberId, &pinStatus, &pinRetries);
	if (ret < 0)
		goto leave;
	ret = sceKernelCopyToUser(status, &pinStatus, sizeof(pinStatus));
	if (ret < 0)
		goto leave;
	ret = sceKernelCopyToUser(retries, &pinRetries, sizeof(pinRetries));
	if (ret >= 0)
		ret = 0;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcUnblockPin(int subscriberId, const BbmcUnblockPinParam *parameters)
{
	BbmcUnblockPinParam *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (parameters == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceKernelCopyFromUser(buffer, parameters, sizeof(*buffer));
	if (ret >= 0) {
		if (buffer->pukLength <= 33 && buffer->pinLength <= 9)
			ret = sceBbmcUnblockPinForDriver(subscriberId, buffer);
		else
			ret = (int)0x80330002;
	}
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcVerifyPin(int subscriberId, const void *pin, SceSize length)
{
	SceUInt32 saved;
	void *buffer;
	int ret;

	if (length > 33)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	if (pin != NULL) {
		buffer = __builtin_alloca(length);
		ret = sceKernelCopyFromUser(buffer, pin, length);
		if (ret < 0)
			goto leave;
		pin = buffer;
	}
	ret = sceBbmcVerifyPinForDriver(subscriberId, pin, length);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetDriverInfo(int subscriberId, BbmcDriverInfo *info)
{
	BbmcDriverInfo driverInfo;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (info == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = sceBbmcGetDriverInfoForDriver(subscriberId, &driverInfo);
	if (ret >= 0)
		ret = sceKernelCopyToUser(info, &driverInfo, sizeof(driverInfo));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcReserveOperationMode(int subscriberId, SceUInt32 operationMode)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcReserveOperationModeForDriver(subscriberId, operationMode);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSetLPMMode(int subscriberId, int mode)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcSetLPMModeForDriver(subscriberId, mode);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSGetListStart(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcSMSGetListStartForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSGetListDone(int subscriberId, SceUInt32 *count, SceUInt32 *indices,
	SceUInt32 maxCount)
{
	SceUInt32 listCount = 0;
	SceUInt32 *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (count == NULL || indices == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(maxCount * sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcSMSGetListDoneForDriver(subscriberId, &listCount, buffer, maxCount);
	if (ret >= 0) {
		ret = sceKernelCopyToUser(count, &listCount, sizeof(listCount));
		if (ret >= 0)
			ret = sceKernelCopyToUser(indices, buffer, listCount * sizeof(*buffer));
	}
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSReadStart(int subscriberId, SceUInt32 memoryIndex)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcSMSReadStartForDriver(subscriberId, memoryIndex);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSReadDone(int subscriberId, void *data, SceUInt32 *format)
{
	SceUInt32 messageFormat = 7;
	SceUInt32 saved;
	void *buffer;
	int ret;
	int copied;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (format == NULL || data == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(256, 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcSMSReadDoneForDriver(subscriberId, buffer, &messageFormat);
	if (ret >= 0) {
		copied = sceKernelCopyToUser(data, buffer, ret);
		if (copied >= 0)
			copied = sceKernelCopyToUser(format, &messageFormat, sizeof(messageFormat));
		if (copied < 0)
			ret = copied;
	}
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSReadAck(int subscriberId, SceUInt32 memoryIndex, SceUInt32 deleteMessage)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcSMSReadAckForDriver(subscriberId, memoryIndex, deleteMessage);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSMSSend(int subscriberId, const void *data, SceSize length, SceUInt32 format)
{
	SceUInt32 saved;
	void *buffer;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (length == 0 || data == NULL || format > 6)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(length, 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	memset(buffer, 0, length);
	ret = sceKernelCopyFromUser(buffer, data, length);
	if (ret >= 0)
		ret = sceBbmcSMSSendForDriver(subscriberId, buffer, length, format);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSetupSetValue(int subscriberId, SceUInt32 type, const void *input, SceSize size)
{
	SceUInt32 saved;
	void *buffer;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (input == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(size, 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	memset(buffer, 0, size);
	ret = sceKernelCopyFromUser(buffer, input, size);
	if (ret >= 0)
		ret = sceBbmcSetupSetValueForDriver(subscriberId, type, buffer, size);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSetupGetValueStart(int subscriberId, SceUInt32 type)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcSetupGetValueStartForDriver(subscriberId, type);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSetupGetValueDone(int subscriberId, SceUInt32 type, void *output, SceSize size)
{
	SceUInt32 saved;
	void *buffer;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (size == 0 || output == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(size, 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcSetupGetValueDoneForDriver(subscriberId, type, buffer, size);
	if (ret >= 0)
		ret = sceKernelCopyToUser(output, buffer, size);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcNetworkScanStart(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_EBC5D3DB(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcNetworkScanDone(int subscriberId, BbmcNetworkScanInfo *scan)
{
	BbmcNetworkScanInfo *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	if (scan == NULL) {
		ret = SceBbmcForDriver_503C7E0B(subscriberId, NULL);
		goto leave;
	}
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_503C7E0B(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(scan, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcNetworkScanAbort(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_739274F3(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcNetworkAttach(int subscriberId, SceInt32 attach)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_75FA68D3(subscriberId, attach);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcNetGetRejectCause(int subscriberId, BbmcRejectCause *cause)
{
	BbmcRejectCause rejectCause;
	SceUInt32 saved;
	int ret;

	if (cause == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = SceBbmcForDriver_92D7C12C(subscriberId, &rejectCause);
	if (ret >= 0)
		ret = sceKernelCopyToUser(cause, &rejectCause, sizeof(rejectCause));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketReleasePDPContext(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_3D27000A(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketGetLastCallEndReason(int subscriberId, BbmcCallEndReason *reason)
{
	BbmcCallEndReason endReason;
	SceUInt32 saved;
	int ret;

	if (reason == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = SceBbmcForDriver_BF550F4A(subscriberId, &endReason);
	if (ret >= 0)
		ret = sceKernelCopyToUser(reason, &endReason, sizeof(endReason));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketIfSetControlFlag(int subscriberId, SceUInt32 flags)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_FB3DA35E(subscriberId, flags);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketIfClearControlFlag(int subscriberId, SceUInt32 flags)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_7EA964B6(subscriberId, flags);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketIfGetControlFlag(int subscriberId, SceUInt32 *flags)
{
	SceUInt32 controlFlags;
	SceUInt32 saved;
	int ret;

	if (flags == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = SceBbmcForDriver_BC3C8942(subscriberId, &controlFlags);
	if (ret >= 0)
		ret = sceKernelCopyToUser(flags, &controlFlags, sizeof(controlFlags));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcPacketGetCurrentBearerTech(int subscriberId, SceUInt32 *bearer)
{
	SceUInt32 bearerTech[2];
	SceUInt32 saved;
	int ret;

	if (bearer == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = SceBbmcForDriver_E09808A2(subscriberId, bearerTech);
	if (ret >= 0)
		ret = sceKernelCopyToUser(bearer, bearerTech, sizeof(bearerTech));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQBackupEFSNV(int subscriberId, const BbmcStringPair *parameters)
{
	BbmcStringPair strings;
	SceUInt32 saved = sceSyscallEnter();
	char *first;
	char *second;
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (parameters == NULL)
		goto invalid;
	ret = sceKernelCopyFromUser(&strings, parameters, sizeof(strings));
	if (ret < 0)
		goto leave;
	if (strings.firstLength > 255 || strings.secondLength > 255)
		goto invalid;
	first = __builtin_alloca(strings.firstLength + 1);
	second = __builtin_alloca(strings.secondLength + 1);
	ret = sceKernelCopyFromUser(first, strings.first, strings.firstLength);
	if (ret < 0)
		goto leave;
	first[strings.firstLength] = '\0';
	ret = sceKernelCopyFromUser(second, strings.second, strings.secondLength);
	if (ret < 0)
		goto leave;
	second[strings.secondLength] = '\0';
	strings.first = first;
	strings.second = second;
	ret = sceBbmcDLQBackupEFSNVForDriver(subscriberId, &strings);
	goto leave;
invalid:
	ret = (int)0x80330002;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQRestoreEFSNV(int subscriberId, const BbmcStringPair *parameters)
{
	BbmcStringPair strings;
	SceUInt32 saved = sceSyscallEnter();
	char *first;
	char *second;
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (parameters == NULL)
		goto invalid;
	ret = sceKernelCopyFromUser(&strings, parameters, sizeof(strings));
	if (ret < 0)
		goto leave;
	if (strings.firstLength > 255 || strings.secondLength > 255)
		goto invalid;
	first = __builtin_alloca(strings.firstLength + 1);
	second = __builtin_alloca(strings.secondLength + 1);
	ret = sceKernelCopyFromUser(first, strings.first, strings.firstLength);
	if (ret < 0)
		goto leave;
	first[strings.firstLength] = '\0';
	ret = sceKernelCopyFromUser(second, strings.second, strings.secondLength);
	if (ret < 0)
		goto leave;
	second[strings.secondLength] = '\0';
	strings.first = first;
	strings.second = second;
	ret = sceBbmcDLQRestoreEFSNVForDriver(subscriberId, &strings);
	goto leave;
invalid:
	ret = (int)0x80330002;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQDownloadSafeImage(int subscriberId, const BbmcStringSet *parameters)
{
	BbmcStringSet set;
	SceUInt32 saved = sceSyscallEnter();
	char *strings[5];
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (parameters == NULL)
		goto invalid;
	ret = sceKernelCopyFromUser(&set, parameters, sizeof(set));
	if (ret < 0)
		goto leave;
	if (set.lengths[0] > 255 || set.lengths[1] > 255 || set.lengths[2] > 255 ||
	    set.lengths[3] > 255 || set.lengths[4] > 255)
		goto invalid;
	strings[0] = __builtin_alloca(set.lengths[0] + 1);
	strings[1] = __builtin_alloca(set.lengths[1] + 1);
	strings[2] = __builtin_alloca(set.lengths[2] + 1);
	strings[3] = __builtin_alloca(set.lengths[3] + 1);
	strings[4] = __builtin_alloca(set.lengths[4] + 1);
	ret = sceKernelCopyFromUser(strings[0], set.strings[0], set.lengths[0]);
	if (ret < 0)
		goto leave;
	strings[0][set.lengths[0]] = '\0';
	ret = sceKernelCopyFromUser(strings[1], set.strings[1], set.lengths[1]);
	if (ret < 0)
		goto leave;
	strings[1][set.lengths[1]] = '\0';
	ret = sceKernelCopyFromUser(strings[2], set.strings[2], set.lengths[2]);
	if (ret < 0)
		goto leave;
	strings[2][set.lengths[2]] = '\0';
	ret = sceKernelCopyFromUser(strings[3], set.strings[3], set.lengths[3]);
	if (ret < 0)
		goto leave;
	strings[3][set.lengths[3]] = '\0';
	ret = sceKernelCopyFromUser(strings[4], set.strings[4], set.lengths[4]);
	if (ret < 0)
		goto leave;
	strings[4][set.lengths[4]] = '\0';
	set.strings[0] = strings[0];
	set.strings[1] = strings[1];
	set.strings[2] = strings[2];
	set.strings[3] = strings[3];
	set.strings[4] = strings[4];
	ret = sceBbmcDLQDownloadSafeImageForDriver(subscriberId, &set);
	goto leave;
invalid:
	ret = (int)0x80330002;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQDownloadSlotImage(int subscriberId, const BbmcStringPairList *parameters)
{
	BbmcStringPairList list;
	BbmcStringPairEntry *entry;
	SceUInt32 saved = sceSyscallEnter();
	char *string;
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (parameters == NULL)
		goto invalid;
	ret = sceKernelCopyFromUser(&list, parameters, sizeof(list));
	if (ret < 0)
		goto leave;
	for (entry = &list.entries[0]; entry != &list.entries[5]; entry++) {
		if (entry->strings.firstLength > 255 || entry->strings.secondLength > 255)
			goto invalid;
		if (entry->strings.firstLength == 0) {
			entry->strings.first = NULL;
		} else {
			string = __builtin_alloca(entry->strings.firstLength + 1);
			if (sceKernelCopyFromUser(string, entry->strings.first,
					entry->strings.firstLength) < 0)
				goto invalid;
			entry->strings.first = string;
			string[entry->strings.firstLength] = '\0';
		}
		if (entry->strings.secondLength == 0) {
			entry->strings.second = NULL;
		} else {
			string = __builtin_alloca(entry->strings.secondLength + 1);
			if (sceKernelCopyFromUser(string, entry->strings.second,
					entry->strings.secondLength) < 0)
				goto invalid;
			entry->strings.second = string;
			string[entry->strings.secondLength] = '\0';
		}
	}
	ret = sceBbmcDLQDownloadSlotImageForDriver(subscriberId, &list);
	goto leave;
invalid:
	ret = (int)0x80330002;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQAbort(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_B7E650A9(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQConfirmChangePartition(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcDLQConfirmChangePartitionForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLGetSwdlError(int subscriberId, SceInt32 *error)
{
	SceInt32 swdlError;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = sceBbmcDLGetSwdlErrorForDriver(subscriberId, &swdlError);
		if (ret >= 0)
			ret = sceKernelCopyToUser(error, &swdlError, sizeof(swdlError));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetFailureInfo(SceInt32 *transition)
{
	SceInt32 failure[4];
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = SceBbmcForDriver_A9CF664F(failure);
		if (ret >= 0)
			ret = sceKernelCopyToUser(transition, failure, sizeof(failure));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQGetFwListStart(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcDLQGetFwListStartForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQGetFwListDone(int subscriberId, QmiStoredImages *storedImages)
{
	QmiStoredImages *buffer;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (storedImages == NULL) {
		ret = (int)0x80330002;
		goto leave;
	}
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcDLQGetFwListDoneForDriver(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(storedImages, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQGetCurrentFwStart(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = sceBbmcDLQGetCurrentFwStartForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLQGetCurrentFwDone(int subscriberId, QmiFirmwarePreference *firmwarePreference)
{
	QmiFirmwarePreference *buffer;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (firmwarePreference == NULL) {
		ret = (int)0x80330002;
		goto leave;
	}
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceBbmcDLQGetCurrentFwDoneForDriver(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(firmwarePreference, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterStart(int subscriberId, const BbmcUpdatePackage *package)
{
	BbmcUpdatePackage mapped;
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	SceUInt32 saved;
	int ret;

	if (package == NULL)
		return (int)0x80330002;
	if (bbmcUpdaterMapping.uid != -1)
		return (int)0x8033000C;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	bbmcUpdaterMapping.uid = -1;
	ret = sceKernelCopyFromUser(&mapped, package, sizeof(mapped));
	if (ret < 0)
		goto leave;
	ret = sceKernelUserMap("SceBbmcDLUpdaterStart", 1, mapped.buffer, mapped.size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (ret < 0)
		goto leave;
	bbmcUpdaterMapping.uid = ret;
	bbmcUpdaterMapping.buffer = mapped.buffer;
	bbmcUpdaterMapping.size = mapped.size;
	mapped.buffer = (char *)kernelPage + kernelOffset;
	ret = sceBbmcDLUpdaterStartForDriver(subscriberId, &mapped);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterContinue(int subscriberId, const BbmcUpdatePackage *package)
{
	BbmcUpdatePackage mapped;
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	SceUInt32 saved;
	int ret;

	if (package == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	if (bbmcUpdaterMapping.uid == -1) {
		ret = (int)0x8033000C;
		goto leave;
	}
	sceKernelUserUnmap(bbmcUpdaterMapping.uid, bbmcUpdaterMapping.buffer,
		bbmcUpdaterMapping.size);
	bbmcUpdaterMapping.uid = -1;
	ret = sceKernelCopyFromUser(&mapped, package, sizeof(mapped));
	if (ret < 0)
		goto leave;
	ret = sceKernelUserMap("SceBbmcDLUpdaterCont", 1, mapped.buffer, mapped.size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (ret < 0)
		goto leave;
	bbmcUpdaterMapping.uid = ret;
	bbmcUpdaterMapping.buffer = mapped.buffer;
	bbmcUpdaterMapping.size = mapped.size;
	mapped.buffer = (char *)kernelPage + kernelOffset;
	ret = sceBbmcDLUpdaterContinueForDriver(subscriberId, &mapped);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterFin(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	if (bbmcUpdaterMapping.uid == -1) {
		ret = (int)0x8033000C;
		goto leave;
	}
	ret = sceKernelUserUnmap(bbmcUpdaterMapping.uid, bbmcUpdaterMapping.buffer,
		bbmcUpdaterMapping.size);
	if (ret >= 0)
		bbmcUpdaterMapping.uid = -1;
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterGetSequence(int subscriberId, SceUInt32 *firstStatus,
	SceUInt32 *secondStatus, BbmcDriverInfo *info)
{
	SceUInt32 first;
	SceUInt32 second;
	BbmcDriverInfo driverInfo;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret)
		goto leave;
	ret = sceBbmcDLUpdaterGetSequenceForDriver(subscriberId, &first, &second, &driverInfo);
	if (ret < 0)
		goto leave;
	if (firstStatus != NULL) {
		ret = sceKernelCopyToUser(firstStatus, &first, sizeof(first));
		if (ret < 0)
			goto leave;
	}
	if (secondStatus != NULL) {
		ret = sceKernelCopyToUser(secondStatus, &second, sizeof(second));
		if (ret < 0)
			goto leave;
	}
	if (info != NULL)
		ret = sceKernelCopyToUser(info, &driverInfo, sizeof(driverInfo));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterGetArchiveVersion(int subscriberId, SceUInt32 *version)
{
	SceUInt32 archiveVersion;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = sceBbmcDLUpdaterGetArchiveVersionForDriver(subscriberId, &archiveVersion);
		if (ret >= 0 && version != NULL)
			ret = sceKernelCopyToUser(version, &archiveVersion, sizeof(archiveVersion));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterGetLatestArchiveVersion(int subscriberId, SceUInt32 *version)
{
	SceUInt32 latestVersion;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = SceBbmcForDriver_FCF6A1EA(subscriberId, &latestVersion);
		if (ret >= 0 && version != NULL)
			ret = sceKernelCopyToUser(version, &latestVersion, sizeof(latestVersion));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterCheckComVersion(int subscriberId, SceUInt32 *matches)
{
	SceUInt32 versionMatches;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = SceBbmcForDriver_4281E476(subscriberId, &versionMatches);
		if (ret >= 0 && matches != NULL)
			ret = sceKernelCopyToUser(matches, &versionMatches, sizeof(versionMatches));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDLUpdaterGetPkgId(int subscriberId, SceUInt32 *pkgId)
{
	SceUInt32 packageId;
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0) {
		ret = sceBbmcDLUpdaterGetPkgIdForDriver(subscriberId, &packageId);
		if (ret >= 0 && pkgId != NULL)
			ret = sceKernelCopyToUser(pkgId, &packageId, sizeof(packageId));
	}
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetProactiveCmd(int subscriberId, BbmcProactiveCmd *command)
{
	BbmcProactiveCmd *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (command == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_399EF002(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(command, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcTerminalResponseCmd(int subscriberId, const BbmcTerminalResponse *response)
{
	BbmcTerminalResponse *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (response == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	memset(buffer, 0, sizeof(*buffer));
	ret = sceKernelCopyFromUser(buffer, response, sizeof(*buffer));
	if (ret >= 0)
		ret = SceBbmcForDriver_93E5345A(subscriberId, buffer);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcEnvelopeCmd(int subscriberId, const QmiCat2CommandOptions *options)
{
	QmiCat2CommandOptions *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (options == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	memset(buffer, 0, sizeof(*buffer));
	ret = sceKernelCopyFromUser(buffer, options, sizeof(*buffer));
	if (ret >= 0)
		ret = SceBbmcForDriver_F4C8482D(subscriberId, buffer);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetFileRefreshDetail(int subscriberId, BbmcFileRefreshDetail *detail)
{
	BbmcFileRefreshDetail *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (detail == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_2930DB51(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(detail, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcFileRefreshResponse(int subscriberId, SceUInt32 response)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = SceBbmcForDriver_009C73D2(subscriberId, response);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcReadSimContentStart(int subscriberId, const BbmcSimFile *file)
{
	BbmcSimFile *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (file == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	__builtin_memset(buffer, 0, sizeof(*buffer));
	ret = sceKernelCopyFromUser(buffer, file, sizeof(*buffer));
	if (ret >= 0)
		ret = SceBbmcForDriver_D288E771(subscriberId, buffer);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcReadSimContentDone(int subscriberId, BbmcSimContentInfo *content)
{
	BbmcSimContentInfo *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (content == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_24EE6E9C(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(content, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGpsCellGetLocationInfoStart(int subscriberId)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = bbmcCheckUserPermission();

	if (ret == 0)
		ret = bbmcStartCellLocationQuery(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGpsCellGetLocationInfoDone(int subscriberId, BbmcGpsCellInfo *info)
{
	BbmcGpsCellInfo *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (info == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	memset(buffer, 0, sizeof(*buffer));
	ret = sceKernelCopyFromUser(buffer, info, sizeof(*buffer));
	if (ret >= 0) {
		ret = bbmcGetCellLocationResult(subscriberId, buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(info, buffer, sizeof(*buffer));
	}
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcDepersonalizeCK(int subscriberId, const BbmcControlKeyParam *param)
{
	BbmcControlKeyParam *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (param == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceKernelCopyFromUser(buffer, param, sizeof(*buffer));
	if (ret >= 0)
		ret = SceBbmcForDriver_683D19BF(subscriberId, buffer);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcUnblockCK(int subscriberId, const BbmcControlKeyParam *param)
{
	BbmcControlKeyParam *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (param == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = sceKernelCopyFromUser(buffer, param, sizeof(*buffer));
	if (ret >= 0)
		ret = SceBbmcForDriver_6AD4053A(subscriberId, buffer);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetCKStatus(int subscriberId, SceUInt32 featureId, QmiIndexedResponse *status)
{
	QmiIndexedResponse *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (status == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_3D06FDB0(subscriberId, featureId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(status, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcSimGetPLMNModeBit(int subscriberId, SceUInt32 *value)
{
	SceUInt32 modeBit = 0;
	SceUInt32 saved;
	int ret;

	if (value == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	ret = SceBbmcForDriver_864E6CA0(subscriberId, &modeBit);
	if (ret >= 0)
		ret = sceKernelCopyToUser(value, &modeBit, sizeof(modeBit));
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetIdStorageFrom3GModule(int subscriberId, BbmcModemConfigurationLeaf *leaf)
{
	BbmcModemConfigurationLeaf *buffer;
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (leaf == NULL)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(sizeof(*buffer), 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_07ADDA90(subscriberId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(leaf, buffer, sizeof(*buffer));
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcIsAbleToRunUnderCurrentTemp(int subscriberId)
{
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret == 0)
		ret = sceBbmcIsAbleToRunUnderCurrentTempForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcIsAbleToRunUnderCurrentVoltage(int subscriberId)
{
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret == 0)
		ret = sceBbmcIsAbleToRunUnderCurrentVoltageForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcIsPowerConfigForbiddenMode(int subscriberId)
{
	SceUInt32 saved;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret == 0)
		ret = sceBbmcIsPowerConfigForbiddenModeForDriver(subscriberId);
	sceSyscallLeave(saved);
	return ret;
}

int sceBbmcGetComFirmVersion(int subscriberId, char *revision, SceSize size)
{
	SceUInt32 saved;
	char *buffer;
	int ret;

	if (bbmcGetControl() == NULL)
		return (int)0x80330003;
	if (revision == NULL || size - 1 > 103)
		return (int)0x80330002;
	saved = sceSyscallEnter();
	ret = bbmcCheckUserPermission();
	if (ret)
		goto leave;
	buffer = bbmcAlloc(size, 0);
	if (buffer == NULL) {
		ret = (int)0x80330005;
		goto leave;
	}
	ret = SceBbmcForDriver_B17A056F(subscriberId, buffer, size);
	if (ret >= 0)
		ret = sceKernelCopyToUser(revision, buffer, size);
	bbmcFree(buffer);
leave:
	sceSyscallLeave(saved);
	return ret;
}

/* the debug output of the release build does nothing */
int sceBbmcDebugSelectOutMedia(void)
{
	return 0;
}

int sceBbmcDebugOutString(void)
{
	return 0;
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

int bbmcSendSysconPowerCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	unsigned int value)
{
	int ret;

	memset(packet, 0, sizeof(*packet));
	packet->request.length = 2;
	packet->request.command[1] = 0x08;
	packet->request.data[0] = value;
	packet->request.command[0] = 0x8d;
	bbmcEnterCS(&control->mutex);
	if ((value & 0x7f) == 0)
		control->sysconPowerRequestPending = 1;
	ret = SceSysconForDriver_C2224E82(packet, 0, callback, control);
	if (ret < 0)
		control->sysconPowerRequestPending = 0;
	bbmcLeaveCS(&control->mutex);
	return ret;
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
