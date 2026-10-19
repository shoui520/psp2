#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbcmd.h"
#include "../bbmc_netif.h"
#include "../usb_extif.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int message);
int bbmcFinalizeGps(void);

int seqResumeFromConnected(BbmcControl *control, BbmcMessage *message);
int seqResumeResetAuthorized(BbmcControl *control, BbmcMessage *message);
int seqResumeResetInitialized(BbmcControl *control, BbmcMessage *message);
int seqResumeResetActivated(BbmcControl *control, BbmcMessage *message);
int seqResumeResetState(BbmcControl *control, BbmcMessage *message);
int seqResumeConnectedToLowPowerMode(BbmcControl *control, BbmcMessage *message);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_seqResume_L1570
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/resume.c",
	.line = 1570,
	.func = "seqResume",
};

static const SceKernelDebugInfoSource dbg_seqResume_L1570Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResume_L1570, "sequence/resume.c", 1570, "seqResume",
};
#endif

static const SceKernelDebugInfo dbg_seqResumeGotoInitialize_L1476
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 1476,
	.func = "seqResumeGotoInitialize",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeGotoInitialize_L1476Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeGotoInitialize_L1476, "sequence/resume.c", 1476,
	"seqResumeGotoInitialize",
};

static const SceKernelDebugInfo dbg_seqResumeUartInterface_L1281
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 1281,
	.func = "seqResumeUartInterface",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeUartInterface_L1281Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeUartInterface_L1281, "sequence/resume.c", 1281,
	"seqResumeUartInterface",
};

static const SceKernelDebugInfo dbg_seqResumeFromActivated_L662
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 662,
	.func = "seqResumeFromActivated",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeFromActivated_L662Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeFromActivated_L662, "sequence/resume.c", 662,
	"seqResumeFromActivated",
};

#if FW_DEBUG
static const SceKernelDebugInfo dbg_seqResumeNasIndication_L1379
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 1379,
	.func = "seqResumeNasIndication",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeNasIndication_L1379Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeNasIndication_L1379, "sequence/resume.c", 1379,
	"seqResumeNasIndication",
};
#endif

static const SceKernelDebugInfo dbg_seqResumeFromLowPowerMode_L879
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 879,
	.func = "seqResumeFromLowPowerMode",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeFromLowPowerMode_L879Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeFromLowPowerMode_L879, "sequence/resume.c", 879,
	"seqResumeFromLowPowerMode",
};

static const SceKernelDebugInfo dbg_seqResumeFromAuthorized_L474
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 474,
	.func = "seqResumeFromAuthorized",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeFromAuthorized_L474Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeFromAuthorized_L474, "sequence/resume.c", 474,
	"seqResumeFromAuthorized",
};

static const SceKernelDebugInfo dbg_seqResumeGotoInitialize_L1427
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 1427,
	.func = "seqResumeGotoInitialize",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeGotoInitialize_L1427Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeGotoInitialize_L1427, "sequence/resume.c", 1427,
	"seqResumeGotoInitialize",
};

static const SceKernelDebugInfo dbg_seqResumeFromConnected_L1100
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/resume.c",
	.line = 1100,
	.func = "seqResumeFromConnected",
#endif
};

static const SceKernelDebugInfoSource dbg_seqResumeFromConnected_L1100Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqResumeFromConnected_L1100, "sequence/resume.c", 1100,
	"seqResumeFromConnected",
};

SceUInt32 resumeConnectedStep;
static SceUInt32 resumeAuthorizedStep;
static SceUInt32 resumeInitializeStep;
static SceUInt32 resumeActivatedStep;
static SceUInt32 resumeUartInterfaceStep;
SceUInt32 resumeLowPowerStep;
SceUInt32 resumeGpsCompletionState;
SceUInt32 resumeNasIndicationStep;

static inline int seqResumeNasIndication(BbmcControl *control, BbmcMessage *message)
{
	int result = 1;
	switch (resumeNasIndicationStep) {
	case 0:
		if (message->command != 80)
			return 1;
		{
			BbcmdArgument input[7] = {{ .value = 1 }};
			result = bbcmdExecute(0, 30, input, NULL, bbcmdManagerCompletion, control);
		}
		resumeNasIndicationStep = result < 0 ? 2 : 1;
		break;
	case 1:
		if (message->command != 81)
			return 1;
		result = message->body.commandResult;
		if (result >= 0)
			result = 1;
		goto complete;
	default:
#if FW_DEBUG
		SceDebugForDriver_35A35322(0, &dbg_seqResumeNasIndication_L1379, __builtin_return_address(0));
#endif
		break;
	}
	if (resumeNasIndicationStep == 2) {
complete:
		resumeNasIndicationStep = 0;
		if (result >= 0)
			result = 0;
	}
	return result;
}

int seqResumeUartInterface(BbmcControl *control, const SceUInt32 *command)
{
	int result;

	switch (resumeUartInterfaceStep) {
	case 0:
		if (*command != 47)
			return 1;
		{
			BbcmdArgument input[7] = {
				{ .value = control->baudRate },
			};

			result = bbcmdExecute(0, 8, input, NULL,
					      bbcmdManagerCompletion, control);
		}
		if (result >= 0) {
			resumeUartInterfaceStep = 1;
			return 1;
		}
		break;
	case 1:
		if (*command == 62) {
			bbmcEnterCS(&control->mutex);
			control->targetState = control->savedTargetState;
			bbmcLeaveCS(&control->mutex);
			resumeUartInterfaceStep = 0;
			return 0;
		}
		if (*command != 63)
			return 1;
		break;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeUartInterface_L1281,
				__builtin_return_address(0));
	}
	resumeUartInterfaceStep = 0;
	return 0x80330001;
}

int seqResumeGotoInitialize(BbmcControl *control, BbmcMessage *message)
{
	int result;

	switch (resumeInitializeStep) {
	case 0:
		result = bbmcSendSimpleMessage(control, 47);
		if (result >= 0) {
			resumeInitializeStep = 1;
			return 0;
		}
		break;
	case 1:
		result = seqResumeUartInterface(control, &message->command);
		if (result < 0)
			break;
		if (result != 0) {
			if (resumeInitializeStep != 3)
				return 0;
			result = 1;
			break;
		}
		if (bbcmdExecute(0, 48, NULL, NULL, NULL, control) < 0)
			SceDebugForDriver_391B5B74(&dbg_seqResumeGotoInitialize_L1427,
					__builtin_return_address(0));
		result = bbcmdExecute(0, 6, NULL, NULL,
				      bbcmdManagerCompletion, control);
		if (result >= 0) {
			resumeInitializeStep = 2;
			return 0;
		}
		break;
	case 2:
		if (message->command != 57)
			return 0;
		result = message->body.commandResult;
		if (result == 0) {
			if (control->gpsProviderRouting != 0) {
				bbmcEnterCS(&control->mutex);
				control->gpsProviderRouting = 0;
				bbmcLeaveCS(&control->mutex);
			}
			bbmcFinalizeGps();
			result = bbmcSendSimpleMessage(control, 50);
			bbmcUnregisterNetworkInterface();
		}
		break;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeGotoInitialize_L1476,
				__builtin_return_address(0));
	}
	resumeInitializeStep = 0;
	if (result >= 0)
		result = 1;
	bbmcEnterCS(&control->mutex);
	control->currentState = 0;
	control->targetState = 0;
	control->secondBootReply = 0;
	control->references = 0;
	bbmcLeaveCS(&control->mutex);
	bbmcNotifyFeatureEvent(control, 0, 0x4000);
	return result;
}

static inline void seqResumeUpdatePowerState(BbmcControl *control,
					    unsigned int errorFlags)
{
	unsigned int inverted;
	unsigned int events;

	bbmcEnterCS(&control->mutex);
	control->suspendState = 0;
	control->flags |= errorFlags;
	bbmcLeaveCS(&control->mutex);
	inverted = SceSysconForDriver_63B14156() == 0;
	bbmcEnterCS(&control->mutex);
	if (control->sysconPowerInverted == inverted) {
		bbmcLeaveCS(&control->mutex);
	} else {
		control->sysconPowerInverted = inverted;
		events = inverted ? 0x400000 : 0x200000;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, events);
	}
}

static inline int seqResumeConnectedSequence(BbmcControl *control,
		BbmcMessage *message, int resumeToLowPower)
{
	int result;
	int suppressCompletion = 0;
	unsigned int completionMessage;
	unsigned int errorFlags;
	int sendResult;

	switch (resumeConnectedStep) {
	case 0:
		if (message->command != 46)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->flags &= ~4u;
		bbmcLeaveCS(&control->mutex);
		bbmcResetGpio();
		if ((control->flags & 2) != 0) {
			result = 0x80330001;
			goto complete;
		}
		result = bbmcSendSimpleMessage(control, 1);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 1;
		return 1;
	case 1:
		if (message->command != 48)
			return 1;
		if (message->body.commandResult != 0) {
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto complete;
			resumeConnectedStep = 7;
			return 1;
		}
		if (control->backendShutdownRequested == 1) {
			bbmcUsbExtIfAcquireGpio();
			control->backendShutdownRequested = 0;
		}
		result = bbmcSendSimpleMessage(control, 47);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 2;
		return 1;
	case 2:
		result = seqResumeUartInterface(control, &message->command);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		resumeConnectedStep = 3;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 3;
		return 1;
	case 3:
		result = bbmcSendSimpleMessage(control, 49);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 4;
		return 1;
	case 4:
		if (message->command != 51)
			return 1;
		result = bbmcSendSimpleMessage(control, 80);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 5;
		return 1;
	case 5:
		result = seqResumeNasIndication(control, message);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		bbmcSendSimpleMessage(control, 6);
		result = control->gpsProviderRouting;
		if (result == 0)
			goto complete;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto complete;
		resumeConnectedStep = 6;
		return 1;
	case 6:
		resumeGpsCompletionState = 0;
		bbmcEnterCS(&control->mutex);
		control->gpsProviderRouting = 0;
		bbmcLeaveCS(&control->mutex);
		result = 0;
		goto complete;
	case 7:
		result = seqResumeGotoInitialize(control, message);
		if (result < 0)
			goto complete;
		if (result != 1)
			break;
		suppressCompletion = 1;
		goto complete;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeFromConnected_L1100,
				__builtin_return_address(0));
	}
	if (resumeConnectedStep != 8)
		return 1;
	result = 0;
complete:
	resumeConnectedStep = 0;
	if (result < 0) {
		errorFlags = 2;
		completionMessage = 22;
	} else {
		completionMessage = 184;
		if (control->deferredSysconEvent != 0) {
			bbmcSendSimpleMessage(control, 1);
			control->deferredSysconEvent = 0;
			control->resumePending = 0;
			errorFlags = 0;
			result = 0;
		} else {
			errorFlags = control->resumePending;
			result = errorFlags;
			if (errorFlags != 0) {
				bbmcNotifyFeatureEvent(control, 2, 1);
				control->deferredSysconEvent = 0;
				control->resumePending = 0;
				errorFlags = 0;
				result = 0;
			}
		}
	}
	seqResumeUpdatePowerState(control, errorFlags);
	if (suppressCompletion == 0) {
		sendResult = bbmcSendSimpleMessage(control, completionMessage);
		if (resumeToLowPower && sendResult < 0)
			goto disconnect;
		if (sendResult < 0 && result == 0)
			result = sendResult;
	}
	if (resumeToLowPower) {
		if (result != 0)
			goto disconnect;
		result = bbmcSendSimpleMessage(control, 29);
		return result < 0 ? result : 0;
	}
	return result;
disconnect:
	bbmcNotifyPacketInterfaceEvent(8, 0);
	return 0;
}

int seqResumeFromConnected(BbmcControl *control, BbmcMessage *message)
{
	return seqResumeConnectedSequence(control, message, 0);
}

static inline int seqResumeResetInactiveState(BbmcControl *control,
					    BbmcMessage *message)
{
	unsigned int flags;
	unsigned int inverted;
	unsigned int events;

	if (message->command != 46)
		return 1;
	bbmcResetGpio();
	if (control->backendShutdownRequested == 1) {
		bbmcUsbExtIfAcquireGpio();
		control->backendShutdownRequested = 0;
	}
	bbmcEnterCS(&control->mutex);
	flags = control->flags;
	control->stateHandler = NULL;
	control->suspendState = 0;
	control->targetState = 0;
	control->flags = flags & ~4u;
	control->currentState = 0;
	bbmcSendSimpleMessage(control, 18);
	bbmcLeaveCS(&control->mutex);
	inverted = SceSysconForDriver_63B14156() == 0;
	bbmcEnterCS(&control->mutex);
	if (control->sysconPowerInverted == inverted) {
		bbmcLeaveCS(&control->mutex);
	} else {
		control->sysconPowerInverted = inverted;
		events = inverted ? 0x400000 : 0x200000;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, events);
	}
	return 0;
}

int seqResumeResetAuthorized(BbmcControl *control, BbmcMessage *message)
{
	return seqResumeResetInactiveState(control, message);
}

int seqResumeResetInitialized(BbmcControl *control, BbmcMessage *message)
{
	return seqResumeResetInactiveState(control, message);
}

int seqResumeResetActivated(BbmcControl *control, BbmcMessage *message)
{
	return seqResumeResetInactiveState(control, message);
}

int seqResumeResetState(BbmcControl *control, BbmcMessage *message)
{
	unsigned int flags;
	unsigned int inverted;
	unsigned int events;

	if (message->command != 46)
		return 1;
	bbmcTimerResetHook();
	bbmcResetGpio();
	if (control->backendShutdownRequested == 1) {
		bbmcUsbExtIfAcquireGpio();
		control->backendShutdownRequested = 0;
	}
	bbmcEnterCS(&control->mutex);
	flags = control->flags;
	control->suspendState = 0;
	control->flags = flags & ~4u;
	if ((flags & 2) != 0) {
		bbmcNotifyFeatureEvent(control, 0, 1);
		control->flags &= ~2u;
	}
	bbmcLeaveCS(&control->mutex);
	inverted = SceSysconForDriver_63B14156() == 0;
	bbmcEnterCS(&control->mutex);
	if (control->sysconPowerInverted == inverted) {
		bbmcLeaveCS(&control->mutex);
	} else {
		control->sysconPowerInverted = inverted;
		events = inverted ? 0x400000 : 0x200000;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, events);
	}
	return 0;
}

int seqResumeFromActivated(BbmcControl *control, BbmcMessage *message)
{
	int result;
	int suppressCompletion = 0;
	unsigned int errorFlags;
	unsigned int completionMessage;
	int sendResult;

	switch (resumeActivatedStep) {
	case 0:
		if (message->command != 46)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->flags &= ~4u;
		bbmcLeaveCS(&control->mutex);
		bbmcResetGpio();
		if (control->flags & 2) {
			result = 0x80330001;
			goto complete;
		}
		result = bbmcSendSimpleMessage(control, 1);
		if (result < 0)
			goto complete;
		resumeActivatedStep = 1;
		return 1;
	case 1:
		if (message->command != 48)
			return 1;
		if (message->body.commandResult != 0) {
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto complete;
			resumeActivatedStep = 5;
			return 1;
		}
		if (control->backendShutdownRequested == 1) {
			bbmcUsbExtIfAcquireGpio();
			control->backendShutdownRequested = 0;
		}
		result = bbmcSendSimpleMessage(control, 47);
		if (result < 0)
			goto complete;
		resumeActivatedStep = 2;
		return 1;
	case 2:
		result = seqResumeUartInterface(control, &message->command);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		result = bbmcSendSimpleMessage(control, 49);
		if (result < 0)
			goto complete;
		resumeActivatedStep = 3;
		return 1;
	case 3:
		if (message->command != 51)
			return 1;
		result = bbmcSendSimpleMessage(control, 80);
		if (result < 0)
			goto complete;
		resumeActivatedStep = 4;
		return 1;
	case 4:
		result = seqResumeNasIndication(control, message);
		if (result <= 0)
			goto complete;
		break;
	case 5:
		result = seqResumeGotoInitialize(control, message);
		if (result < 0)
			goto complete;
		if (result == 1) {
			suppressCompletion = 1;
			goto complete;
		}
		break;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeFromActivated_L662,
				__builtin_return_address(0));
	}
	if (resumeActivatedStep != 6)
		return 1;
complete:
	resumeActivatedStep = 0;
	errorFlags = result < 0 ? 2 : 0;
	completionMessage = result < 0 ? 22 : 21;
	seqResumeUpdatePowerState(control, errorFlags);
	if (suppressCompletion == 0) {
		sendResult = bbmcSendSimpleMessage(control, 184);
		if (sendResult < 0) {
			if (result >= 0)
				result = sendResult;
			bbmcSendSimpleMessage(control, completionMessage);
		} else {
			sendResult = bbmcSendSimpleMessage(control, completionMessage);
			if (sendResult < 0 && result >= 0)
				result = sendResult;
		}
	}
	return result;
}

int seqResumeFromAuthorized(BbmcControl *control, BbmcMessage *message)
{
	int result;
	int suppressCompletion = 0;
	unsigned int errorFlags;
	unsigned int completionMessage;
	int sendResult;

	switch (resumeAuthorizedStep) {
	case 0:
		if (message->command != 46)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->flags &= ~4u;
		bbmcLeaveCS(&control->mutex);
		bbmcResetGpio();
		if (control->flags & 2) {
			result = 0x80330001;
			goto complete;
		}
		result = bbmcSendSimpleMessage(control, 1);
		if (result < 0)
			goto complete;
		resumeAuthorizedStep = 1;
		return 1;
	case 1:
		if (message->command == 48) {
			if (message->body.commandResult == 0)
				return 1;
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto complete;
			resumeAuthorizedStep = 5;
			return 1;
		}
		if (control->backendShutdownRequested == 1) {
			bbmcUsbExtIfAcquireGpio();
			control->backendShutdownRequested = 0;
		}
		result = bbmcSendSimpleMessage(control, 47);
		if (result < 0)
			goto complete;
		resumeAuthorizedStep = 2;
		return 1;
	case 2:
		result = seqResumeUartInterface(control, &message->command);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		result = bbmcSendSimpleMessage(control, 49);
		if (result < 0)
			goto complete;
		resumeAuthorizedStep = 3;
		return 1;
	case 3:
		if (message->command != 51)
			return 1;
		result = bbmcSendSimpleMessage(control, 80);
		if (result < 0)
			goto complete;
		resumeAuthorizedStep = 4;
		return 1;
	case 4:
		result = seqResumeNasIndication(control, message);
		if (result <= 0)
			goto complete;
		break;
	case 5:
		result = seqResumeGotoInitialize(control, message);
		if (result < 0)
			goto complete;
		if (result == 1) {
			suppressCompletion = 1;
			goto complete;
		}
		break;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeFromAuthorized_L474,
				__builtin_return_address(0));
	}
	if (resumeAuthorizedStep != 6)
		return 1;
	result = 0;
complete:
	resumeAuthorizedStep = 0;
	if (result < 0) {
		errorFlags = 2;
		bbmcEnterCS(&control->mutex);
		completionMessage = 22;
		bbmcLeaveCS(&control->mutex);
	} else {
		errorFlags = 0;
		completionMessage = 21;
		result = 0;
	}
	seqResumeUpdatePowerState(control, errorFlags);
	if (suppressCompletion == 0) {
		sendResult = bbmcSendSimpleMessage(control, 184);
		if (sendResult < 0 && result == 0)
			result = sendResult;
		if (control->currentState != control->savedTargetState ||
		    bbmcCollectIndexedResponseEvents(control) == 0)
			bbmcSendSimpleMessage(control, completionMessage);
	}
	return result;
}

int seqResumeFromLowPowerMode(BbmcControl *control, BbmcMessage *message)
{
	int result;
	int suppressCompletion = 0;
	unsigned int errorFlags;
	unsigned int completionMessage;
	int sendResult;

	switch (resumeLowPowerStep) {
	case 0:
		if (message->command != 46)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->flags &= ~4u;
		bbmcLeaveCS(&control->mutex);
		bbmcResetGpio();
		if (control->flags & 2) {
			result = 0x80330001;
			goto complete;
		}
		result = bbmcSendSimpleMessage(control, 1);
		if (result < 0)
			goto complete;
		resumeLowPowerStep = 1;
		return 1;
	case 1:
		if (message->command != 48)
			return 1;
		if (message->body.commandResult != 0) {
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto complete;
			resumeLowPowerStep = 7;
			return 1;
		}
		if (control->backendShutdownRequested == 1) {
			bbmcUsbExtIfAcquireGpio();
			control->backendShutdownRequested = 0;
		}
		resumeLowPowerStep = 2;
		result = bbmcSendSimpleMessage(control, 47);
		if (result < 0)
			goto complete;
		break;
	case 2:
		result = seqResumeUartInterface(control, &message->command);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto complete;
		resumeLowPowerStep = 3;
		return 1;
	case 3:
		result = bbmcSendSimpleMessage(control, 49);
		if (result < 0)
			goto complete;
		resumeLowPowerStep = 4;
		return 1;
	case 4:
		if (message->command != 51)
			return 1;
		result = bbmcSendSimpleMessage(control, 80);
		if (result < 0)
			goto complete;
		resumeLowPowerStep = 5;
		return 1;
	case 5:
		result = seqResumeNasIndication(control, message);
		if (result < 0)
			goto complete;
		if (result != 0)
			break;
		result = control->gpsProviderRouting;
		if (result == 0)
			goto complete;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto complete;
		resumeLowPowerStep = 6;
		return 1;
	case 6:
		result = 0;
		resumeGpsCompletionState = 0;
		bbmcEnterCS(&control->mutex);
		control->gpsProviderRouting = 0;
		bbmcLeaveCS(&control->mutex);
		goto complete;
	case 7:
		result = seqResumeGotoInitialize(control, message);
		if (result < 0)
			goto complete;
		if (result == 1) {
			suppressCompletion = 1;
			goto complete;
		}
		break;
	default:
		SceDebugForDriver_391B5B74(&dbg_seqResumeFromLowPowerMode_L879,
				__builtin_return_address(0));
	}
	if (resumeLowPowerStep != 8)
		return 1;
	result = 0;
complete:
	resumeLowPowerStep = 0;
	if (result < 0) {
		errorFlags = 2;
		completionMessage = 22;
	} else {
		result = 0;
		errorFlags = 0;
		completionMessage = 184;
	}
	seqResumeUpdatePowerState(control, errorFlags);
	if (suppressCompletion == 0) {
		sendResult = bbmcSendSimpleMessage(control, completionMessage);
		if (sendResult < 0 && result == 0)
			result = sendResult;
	}
	return result;
}
int seqResumeConnectedToLowPowerMode(BbmcControl *control, BbmcMessage *message)
{
	return seqResumeConnectedSequence(control, message, 1);
}

int seqResume(BbmcControl *control, BbmcMessage *message)
{
	int (*handler)(BbmcControl *, BbmcMessage *) = NULL;
	int currentState;
	int savedTargetState;
	int result;

	if (message->command != 45)
		return 0;
	bbmcEnterCS(&control->mutex);
	currentState = control->currentState;
	control->suspendState = 4;
	savedTargetState = control->savedTargetState;
	switch (currentState) {
	case 0:
		switch (savedTargetState) {
		case 0:
			handler = seqResumeResetState;
			break;
		case 1:
		case 4:
			handler = seqResumeResetInitialized;
			break;
		case 2:
			handler = seqResumeResetActivated;
			break;
		case 3:
			handler = seqResumeResetAuthorized;
			break;
		}
		break;
	case 3:
		if (savedTargetState == 2 || savedTargetState == 3)
			handler = seqResumeFromAuthorized;
		break;
	case 5:
		if (savedTargetState == 2 || savedTargetState == 5)
			handler = seqResumeFromActivated;
		break;
	case 6:
		if (savedTargetState == 6)
			handler = seqResumeFromLowPowerMode;
		break;
	case 7:
		if (savedTargetState == 6)
			handler = seqResumeConnectedToLowPowerMode;
		else if (savedTargetState == 7)
			handler = seqResumeFromConnected;
		break;
	}
	control->resumeHandler = handler;
#if FW_DEBUG
	SceDebugForDriver_35A35322(handler != NULL, &dbg_seqResume_L1570,
			__builtin_return_address(0));
#endif
	bbmcNotifyFeatureEvent(control, 0, 0x100000);
	result = bbmcSendSimpleMessage(control, 46);
	bbmcLeaveCS(&control->mutex);
	return result;
}
