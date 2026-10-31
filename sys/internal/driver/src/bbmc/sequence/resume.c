#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbcmd.h"
#include "../bbmc_netif.h"
#include "../usb_extif.h"
#include "../sw_dload/sw_dload.h"
#include <string.h>

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
		sceKernelAssert(0, &dbg_seqResumeNasIndication_L1379, __builtin_return_address(0));
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
	sceKernelAssert(handler != NULL, &dbg_seqResume_L1570,
			__builtin_return_address(0));
#endif
	bbmcNotifyFeatureEvent(control, 0, 0x100000);
	result = bbmcSendSimpleMessage(control, 46);
	bbmcLeaveCS(&control->mutex);
	return result;
}

SceUInt32 smsSendStep;
SceUInt32 controlKeyStep;
SceUInt32 operationModeStep;
SceUID operationModeTimerId;
SceUInt32 operationModeRegisteredDownload;

/* name unknown: sends a stored SMS (command 90) and reports its result (reply 91) */
int seqSendSms(BbmcControl *control, BbmcMessage *message)
{
	int result;

	if (smsSendStep == 0) {
		if (message->command == 90) {
			BbcmdArgument input[7];

			input[0].pointer = message->body.smsSendRequest.data;
			input[1].value = message->body.smsSendRequest.length;
			input[2].value = message->body.smsSendRequest.format;
			input[3].value = 0;
			input[4].value = 0;
			input[5].value = 0;
			input[6].value = 0;
			result = bbcmdExecute(0, 0x12, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				goto finish;
			smsSendStep = 1;
		}
		return 1;
	}
	if (smsSendStep != 1) {
		result = (int)0x80330001;
		goto finish;
	}
	if (message->command != 91)
		return 1;
	result = message->body.commandResult;
	if (result != 0)
		bbmcNotifyFeatureEvent(control, 2, 0x20);
	else
		bbmcNotifyFeatureEvent(control, 2, 8);
	if (result > 0)
		result = 0;
finish:
	smsSendStep = 0;
	bbmcEnterCS(&control->mutex);
	control->smsSendBusy = 0;
	bbmcLeaveCS(&control->mutex);
	return result;
}

/*
 * name unknown: depersonalizes (command 179) or unblocks (command 180) a control
 * key, and records the retries its reply (182 or 183) reports
 */
int seqControlKey(BbmcControl *control, BbmcMessage *message)
{
	QmiIndexedResponse *response;
	int result;

	if (controlKeyStep == 0) {
		BbcmdArgument output[6];
		BbcmdArgument input[7];

		if (message->command - 179 > 1)
			return 1;
		input[0].pointer = &control->controlKey;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		if (message->command != 179)
			input[1].value = 1;
		output[0].pointer = control->indexedResponses;
		output[1].value = 0;
		output[2].value = 0;
		output[3].value = 0;
		output[4].value = 0;
		output[5].value = 0;
		result = bbcmdExecute(0, 0x32, input, output,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		controlKeyStep = 1;
		return 1;
	}
	if (controlKeyStep != 1) {
		result = (int)0x80330001;
		goto failed;
	}
	if (message->command == 182) {
		result = message->body.controlKeyReply.result;
		if (result == (int)0x8033500C || result == 0 ||
		    result == (int)0x80335023) {
			bbmcEnterCS(&control->mutex);
			control->indexedResponses[message->body.controlKeyReply.featureId]
				.detailBytes[0] =
				message->body.controlKeyReply.remainingRetries;
			bbmcLeaveCS(&control->mutex);
		}
	} else if (message->command == 183) {
		result = message->body.controlKeyReply.result;
		response = &control->indexedResponses[
			message->body.controlKeyReply.featureId];
		if (result == 0) {
			bbmcEnterCS(&control->mutex);
			response->detailBytes[1] =
				message->body.controlKeyReply.remainingRetries;
			bbmcLeaveCS(&control->mutex);
			response->secondaryValue = result;
		} else if (result == (int)0x8033500C || result == (int)0x80335023) {
			bbmcEnterCS(&control->mutex);
			response->detailBytes[1] =
				message->body.controlKeyReply.remainingRetries;
			bbmcLeaveCS(&control->mutex);
			if (response->detailBytes[1] == 0)
				bbmcNotifyFeatureEvent(control, 4, 0x1000000);
		}
	} else {
		return 1;
	}
	bbmcNotifyFeatureEvent(control, 4, 0x400000);
	controlKeyStep = 0;
	memset(&control->controlKey, 0, sizeof(control->controlKey));
	return 0;

failed:
	controlKeyStep = 0;
	memset(&control->controlKey, 0, sizeof(control->controlKey));
	return result;
}

/*
 * name unknown: compares two modem firmware versions (family, major, minor,
 * patch, build), most significant first
 */
static inline int bbmcCompareModemVersion(const SceUInt32 *version,
	const SceUInt32 *reference)
{
	if (version[0] != reference[0])
		return version[0] > reference[0] ? 1 : -1;
	if (version[1] != reference[1])
		return version[1] > reference[1] ? 1 : -1;
	if (version[2] != reference[2])
		return version[2] > reference[2] ? 1 : -1;
	if (version[3] != reference[3])
		return version[3] > reference[3] ? 1 : -1;
	if (version[4] != reference[4])
		return version[4] > reference[4] ? 1 : -1;
	return 0;
}

/* name unknown: whether the modem runs a firmware that is switched to mode 6 by a download */
static inline int bbmcIsDownloadFirmware(BbmcControl *control)
{
	const SceUInt32 first[5] = { 1575, 0, 4, 4, 0 };
	const SceUInt32 second[5] = { 1600, 0, 2, 1, 0 };
	const SceUInt8 *revision = (const SceUInt8 *)&control->versionRevision;
	SceUInt32 version[5];

	version[0] = control->versionFamily;
	version[1] = revision[0];
	version[2] = revision[1] % 50;
	version[3] = control->versionPatch;
	version[4] = control->versionBuild;
	if (version[0] >= 1600)
		return bbmcCompareModemVersion(version, second) >= 0;
	if (version[0] <= 1499)
		return 0;
	return bbmcCompareModemVersion(version, first) >= 0;
}

/*
 * name unknown: moves the modem to the operation mode command 116 asks for,
 * through syscon or, for mode 6 on a download firmware, through the download mode
 */
int seqChangeOperationMode(BbmcControl *control, BbmcMessage *message)
{
	BbmcMessage *reply;
	SceUInt32 mode;
	int result;

	switch (operationModeStep) {
	case 0:
		if (message->command != 116)
			return 1;
		operationModeRegisteredDownload = 0;
		switch (message->body.operationMode) {
		case 6:
			if (control->currentState > 0 && bbmcIsDownloadFirmware(control)) {
				if (control->downloadNotificationRegistered == 0) {
					result = bbmcRegisterDownloadNotification(control);
					if (result < 0)
						goto finish;
					operationModeRegisteredDownload = 1;
				}
				result = bbmcAcquireGpio(NULL);
				if (result < 0)
					goto finish;
				result = swdlWaitAttach(0);
				if (result < 0)
					goto finish;
				operationModeStep = 2;
				return 1;
			}
			mode = 1;
			break;
		case 0:
			mode = 4;
			break;
		case 1:
			mode = 0;
			break;
		case 2:
			mode = 1;
			break;
		case 3:
			mode = 2;
			break;
		case 4:
			mode = 3;
			break;
		case 5:
			mode = 6;
			break;
		case 7:
			mode = 8;
			break;
		default:
			result = (int)0x80330002;
			goto finish;
		}
		break;
	case 1:
		if (message->command == 117) {
			bbmcTimerCancelHandler(operationModeTimerId);
			operationModeTimerId = 0;
			if (message->body.commandResult != 0)
				result = (int)0x80330001;
			else
				result = 0;
			goto finish;
		}
		if (message->command != 187 || message->body.timedOutCommand != 117)
			return 1;
		result = (int)0x80330001;
		goto finish;
	case 2:
		if (message->command == 135) {
			if (swdlRequestDownloadMode() < 0)
				goto detached;
			operationModeStep = 3;
			return 1;
		}
		if (message->command != 137)
			return 1;
		bbmcReleaseGpio(NULL);
		mode = 1;
		break;
	case 3:
		if (message->command == 141) {
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			operationModeStep = 4;
		} else if (message->command == 134) {
detached:
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			operationModeStep = 5;
		}
		return 1;
	case 4:
		if ((message->command & ~2U) != 136)
			return 1;
		if ((unsigned int)bbmcGetBaryonVersion() <= 0x904FF)
			mode = 1;
		else
			mode = 7;
		break;
	case 5:
		if ((message->command & ~2U) != 136)
			return 1;
		mode = 1;
		break;
	default:
		result = (int)0x80330001;
		goto finish;
	}

	modemWakeupPending = 1;
	result = bbmcSendSizedSysconCommand(control, modemWakeupCallback,
		&control->operationModePacket, 4, 2, 2, mode, 0);
	if (result < 0) {
		modemWakeupPending = 0;
		goto finish;
	}
	result = bbmcTimerSetHandler(60000000, operationModeTimeoutHandler, control);
	if (result < 0) {
		bbmcCancelSysconCommand(&control->operationModePacket,
			&modemWakeupPending);
		goto finish;
	}
	operationModeStep = 1;
	operationModeTimerId = result;
	return 1;

finish:
	operationModeStep = 0;
	bbmcCancelSysconCommand(&control->operationModePacket, &modemWakeupPending);
	if (operationModeRegisteredDownload == 1 &&
	    control->downloadNotificationRegistered == 1) {
		swdlTerminate();
		control->downloadNotificationRegistered = 0;
	}
	operationModeRegisteredDownload = 0;
	if (result >= 0) {
		bbmcNotifyFeatureEvent(control, 0, 0x1000);
		return 0;
	}
	bbmcNotifyFeatureEvent(control, 0, 0x2000);
	reply = msgAllocMsgPacket(control, 117);
	if (reply != NULL) {
		reply->body.commandResult = result;
		result = msgSendMsgPacket(control, reply);
		if (result < 0)
			msgFreeMsgPacket(control, reply);
	}
	return result;
}
