#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_netif.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int connectedRequestLowPowerMode(BbmcControl *control);
int bbmcPrepareUsbShutdown(BbmcControl *control);
int gpsStopOneShotTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopPositionTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopNetworkService(BbmcControl *control, BbmcMessage *message);

SceUInt32 connectedTransitionStep;
SceUID connectedTransitionTimer;
int connectedSysconPending;
SceUInt32 connectedRestartRequested;
#if FW_DEBUG
SceUInt32 connectedSuspendCompletion;
SceUInt32 connectedSuspendStep;
#else
SceUInt32 connectedSuspendStep;
SceUInt32 connectedSuspendCompletion;
#endif

int connectedSuspendSelectWork(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitDisconnect(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitAttach(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitSmsSend(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitSmsList(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitSmsRead(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendWaitSmsReadCompletion(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendStopTracking(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendCheckFirstCondition(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendSendNotifications(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendCheckSecondCondition(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendCheckCompletion(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);
int connectedSuspendEnterLowPower(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep);

static const SceKernelDebugInfo dbg_suspendSteadyConnected_L1335
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/connected.c",
	.line = 1335,
	.func = "suspendSteadyConnected",
#endif
};

static const SceKernelDebugInfoSource dbg_suspendSteadyConnected_L1335Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendSteadyConnected_L1335,
	"state_machine/connected.c", 1335, "suspendSteadyConnected",
};

static const SceKernelDebugInfo dbg_suspendGotoLPM_L1373
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/connected.c",
	.line = 1373,
	.func = "suspendGotoLPM",
#endif
};

static const SceKernelDebugInfoSource dbg_suspendGotoLPM_L1373Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendGotoLPM_L1373,
	"state_machine/connected.c", 1373, "suspendGotoLPM",
};

typedef int (*ConnectedSuspendHandler)(BbmcControl *, BbmcMessage *,
				      SceUInt32 *);

static ConnectedSuspendHandler const connectedSuspendHandlers[13] = {
	connectedSuspendSelectWork,
	connectedSuspendWaitDisconnect,
	connectedSuspendWaitAttach,
	connectedSuspendWaitSmsSend,
	connectedSuspendWaitSmsList,
	connectedSuspendWaitSmsRead,
	connectedSuspendWaitSmsReadCompletion,
	connectedSuspendStopTracking,
	connectedSuspendCheckFirstCondition,
	connectedSuspendSendNotifications,
	connectedSuspendCheckSecondCondition,
	connectedSuspendCheckCompletion,
	connectedSuspendEnterLowPower,
};

int connectedSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	SceUInt32 step = connectedTransitionStep;
	BbmcMessage *message;

	if (packet->request.command[0] != 0x80)
		goto done;
	if (packet->request.command[1] != 0x0a)
		goto done;
	if (step != 3)
		goto done;
	message = msgAllocMsgPacket(control, 23);
	message->body.sysconReplyHalfword = packet->reply.data[0] |
		(packet->reply.data[1] << 8);
	msgSendMsgPacket(control, message);
done:
	connectedSysconPending = 0;
	return 0;
}

void connectedTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (connectedTransitionStep == 2) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 67;
			msgSendMsgPacket(control, message);
		}
	}
}

/*
 * The suspend work of the Connected state, one step per handler: each sets the
 * step to run next (13 ends the work) and returns 1 to wait for a message.
 */
int connectedSuspendWaitSmsRead(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 state;

	*nextStep = 5;
	bbmcEnterCS(&control->mutex);
	state = control->smsReadState;
	if (state == 1) {
		if (message->command == 92)
			goto next;
	} else if (state != 3) {
next:
		*nextStep = 7;
	} else {
		*nextStep = 6;
	}
	bbmcLeaveCS(&control->mutex);
	return 1;
}

int connectedSuspendWaitSmsSend(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 state;

	*nextStep = 3;
	bbmcEnterCS(&control->mutex);
	if (control->smsSendBusy == 0 || message->command == 91) {
		if (control->smsListBusy == 1) {
			*nextStep = 4;
		} else {
			state = control->smsReadState;
			if (state == 1)
				*nextStep = 5;
			else
				*nextStep = state == 3 ? 6 : 7;
		}
	}
	bbmcLeaveCS(&control->mutex);
	return 1;
}

int connectedSuspendWaitSmsReadCompletion(BbmcControl *control,
		BbmcMessage *message, SceUInt32 *nextStep)
{
	int result;

	*nextStep = 6;
	bbmcEnterCS(&control->mutex);
	if (control->smsReadState != 3 || message->command == 89) {
		*nextStep = 7;
		result = bbmcSendSimpleMessage(control, 188);
		bbmcLeaveCS(&control->mutex);
		return result;
	}
	bbmcLeaveCS(&control->mutex);
	return 1;
}

int connectedSuspendWaitSmsList(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 state;
	int result;

	*nextStep = 4;
	bbmcEnterCS(&control->mutex);
	if (control->smsListBusy != 1 || message->command == 85) {
		state = control->smsReadState;
		if (state == 1)
			*nextStep = 5;
		else
			*nextStep = state == 3 ? 6 : 7;
		result = bbmcSendSimpleMessage(control, 188);
		bbmcLeaveCS(&control->mutex);
		return result;
	}
	bbmcLeaveCS(&control->mutex);
	return 1;
}

/* the first SMS step still to wait for: send, list, read */
static inline unsigned int connectedSuspendSelectSmsStep(BbmcControl *control,
		SceUInt32 *nextStep)
{
	SceUInt32 state;

	if (control->smsSendBusy != 0) {
		*nextStep = 3;
	} else if (control->smsListBusy == 1) {
		*nextStep = 4;
	} else {
		state = control->smsReadState;
		if (state == 1)
			*nextStep = 5;
		else
			*nextStep = state == 3 ? 6 : 7;
	}
	return 188;
}

int connectedSuspendWaitAttach(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 command = message->command;

	*nextStep = 2;
	if (command != 150)
		return 1;
	bbmcEnterCS(&control->mutex);
	connectedSuspendSelectSmsStep(control, nextStep);
	bbmcLeaveCS(&control->mutex);
	return bbmcSendSimpleMessage(control, 188);
}

int connectedSuspendSelectWork(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 pendingRequest, initialized, online, started;
	unsigned int command;
	int result;

	(void)message;
	bbmcEnterCS(&control->mutex);
	pendingRequest = control->pendingPacketRequest;
	initialized = control->clientInitialized;
	online = control->clientOnline;
	started = control->networkStarted;
	bbmcLeaveCS(&control->mutex);
	*nextStep = 0;
	if (pendingRequest != 0) {
		result = DisconnectNetwork();
		if (result >= 0)
			goto disconnecting;
		if (result == (int)0x80330507) {
			*nextStep = 1;
			return 1;
		}
		if (result != (int)0x80330508)
			goto failed;
		if (*nextStep != 0)
			return result;
	} else if ((initialized | online) != 0 && started == 0) {
		result = bbmcSendSimpleMessage(control, 115);
		if (result < 0)
			goto failed;
disconnecting:
		*nextStep = 1;
		return result;
	}
	bbmcEnterCS(&control->mutex);
	if (control->networkScanState == 1) {
		command = 151;
		*nextStep = 2;
	} else {
		command = connectedSuspendSelectSmsStep(control, nextStep);
	}
	bbmcLeaveCS(&control->mutex);
	return bbmcSendSimpleMessage(control, command);
failed:
	*nextStep = 13;
	return result;
}

int connectedSuspendCheckCompletion(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	int result;

	(void)message;
	*nextStep = 11;
	result = bbmcPrepareUsbShutdown(control);
	if (result == 0)
		*nextStep = 13;
	return result;
}

int connectedSuspendCheckSecondCondition(BbmcControl *control,
		BbmcMessage *message, SceUInt32 *nextStep)
{
	int result;

	*nextStep = 10;
	result = suspendStopNetworkService(control, message);
	if (result == 0) {
		*nextStep = 11;
		result = bbmcSendSimpleMessage(control, 188);
		if (result >= 0)
			return 1;
	}
	return result;
}

int connectedSuspendCheckFirstCondition(BbmcControl *control,
		BbmcMessage *message, SceUInt32 *nextStep)
{
	int result;

	*nextStep = 8;
	result = suspendStopPositionTracking(control, message);
	if (result == 0) {
		*nextStep = 9;
		result = bbmcSendSimpleMessage(control, 188);
		if (result >= 0)
			return 1;
	}
	return result;
}

int connectedSuspendStopTracking(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	int result;

	*nextStep = 7;
	result = gpsStopOneShotTracking(control, message);
	if (result != 0)
		return result;
	if (control->suspendEligible == 0) {
		*nextStep = 8;
		result = bbmcSendSimpleMessage(control, 79);
		if (result < 0)
			return result;
		return 1;
	}
	*nextStep = 12;
	bbmcEnterCS(&control->mutex);
	control->targetState = 6;
	control->savedTargetState = 6;
	bbmcLeaveCS(&control->mutex);
	control->powerTransitionBusy = 1;
	if (bbmcSendSimpleMessage(control, 29) < 0)
		control->powerTransitionBusy = 0;
	return 1;
}

int connectedSuspendSendNotifications(BbmcControl *control,
		BbmcMessage *message, SceUInt32 *nextStep)
{
	int result;

	(void)message;
	*nextStep = 9;
	result = bbmcSendSimpleMessage(control, 50);
	if (result < 0)
		return result;
	result = bbmcSendSimpleMessage(control, 188);
	if (result < 0)
		return result;
	*nextStep = 10;
	return 1;
}

int connectedSuspendWaitDisconnect(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	SceUInt32 command = message->command;

	*nextStep = 1;
	if (command != 57)
		return 1;
	bbmcEnterCS(&control->mutex);
	if (control->networkScanState == 1) {
		command = 151;
		*nextStep = 2;
	} else {
		command = connectedSuspendSelectSmsStep(control, nextStep);
	}
	bbmcLeaveCS(&control->mutex);
	return bbmcSendSimpleMessage(control, command);
}

int connectedRequestLowPowerMode(BbmcControl *control)
{
	BbcmdArgument input[7];

	input[1].value = 0;
	input[2].value = 0;
	input[3].value = 0;
	input[4].value = 0;
	input[5].value = 0;
	input[6].value = 0;
	input[0].value = 1;
	return bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion, control);
}

int connectedSuspendEnterLowPower(BbmcControl *control, BbmcMessage *message,
		SceUInt32 *nextStep)
{
	BbcmdArgument input[7];
	int result = 1;
	int currentState;
	SceUInt16 reply;

	*nextStep = 12;
	if (message->command == 11 || message->command == 36) {
		connectedRestartRequested = 1;
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 1;
		bbmcLeaveCS(&control->mutex);
	}
	if (message->command == 67) {
		if (connectedTransitionStep == 2)
			goto sendSysconAfterTimer;
		goto sendSyscon;
	}

	switch (connectedTransitionStep) {
	case 0:
		if (message->command != 11 && message->command != 29 &&
		    message->command != 36)
			return 1;
		bbmcEnterCS(&control->mutex);
		currentState = control->currentState;
		bbmcLeaveCS(&control->mutex);
		if (currentState != 7) {
			result = (int)0x80330006;
			goto finish;
		}
		result = DisconnectNetwork();
		if (result < 0) {
			if (result == (int)0x80330508) {
				result = connectedRequestLowPowerMode(control);
				if (result < 0)
					goto finish;
				goto startTimer;
			}
			if (result != (int)0x80330507)
				goto finish;
		}
		connectedTransitionStep = 1;
		return 1;
	case 1:
		if (message->command != 57)
			return 1;
		if (message->body.commandResult != 0) {
			result = (int)0x80330001;
			control->transitionStep = 6;
			goto finish;
		}
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		input[0].value = 1;
		result = bbcmdExecute(0, 37, input, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
startTimer:
		connectedTransitionStep = 2;
		result = bbmcTimerSetHandler(60000000,
					    connectedTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = connectedTransitionStep;
			goto finish;
		}
		connectedTransitionTimer = result;
		goto checkTransition;
	case 2:
		if (message->command == 67)
			goto sendSysconAfterTimer;
		if (message->command == 72) {
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto finish;
			result = bbmcSendSimpleMessage(control, 67);
			goto checkTransition;
		}
		if (message->command == 187 &&
		    message->body.timedOutCommand == 67) {
			result = (int)0x80330001;
			control->transitionStep = 2;
			goto finish;
		}
		return 1;
	case 3:
		if (message->command != 23)
			return 1;
		reply = message->body.sysconReplyHalfword;
		if (reply != 0) {
			if (reply == 3) {
				control->targetState = 7;
			} else {
				bbmcNotifyFeatureEvent(control, 0, 1);
				result = (int)0x80330001;
			}
			goto finish;
		}
		bbmcEnterCS(&control->mutex);
		control->firstBootReply = 0;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->powerTransitionBusy = 0;
		memset(&control->networkInfo, 0, sizeof(control->networkInfo));
		control->networkInfo.signalLevel = -1;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, 4);
		if (connectedRestartRequested != 1) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 6;
			bbmcLeaveCS(&control->mutex);
			goto finish;
		}
		result = bbmcSendSimpleMessage(control, 4);
		if (result < 0)
			goto finish;
		connectedRestartRequested = 0;
		connectedTransitionStep = 4;
		return 1;
	case 4:
		if (message->command != 5)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->currentState = 6;
		bbmcLeaveCS(&control->mutex);
		if ((message->body.sysconReplyFlags & 0x20) != 0 &&
		    control->pinStatus.pin1Status == 1)
			result = bbmcSendSimpleMessage(control, 36);
		else
			result = bbmcSendSimpleMessage(control, 11);
		goto finish;
	case 5:
		if (message->command != 57 && message->command != 108)
			return 1;
		/* Both completions reach the failure cleanup. */
	default:
		result = (int)0x80330001;
		goto finish;
	}

sendSysconAfterTimer:
	bbmcTimerCancelHandler(connectedTransitionTimer);
	connectedTransitionTimer = 0;
sendSyscon:
	connectedTransitionStep = 3;
	connectedSysconPending = 1;
	result = bbmcSendSysconCommand(control, connectedSysconReply,
				      &control->transitionPacket, 6, 2, 0);
	if (result < 0) {
		connectedSysconPending = 0;
		goto finish;
	}
checkTransition:
	if (connectedTransitionStep != 6)
		return 1;
finish:
	connectedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket,
			       &connectedSysconPending);
	if (result < 0) {
		if (DisconnectNetwork() == 0) {
			connectedTransitionStep = 5;
			bbmcNotifyPacketInterfaceEvent(16, result);
			connectedRestartRequested = 0;
			return 1;
		}
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 0;
		bbmcLeaveCS(&control->mutex);
		connectedRestartRequested = 0;
		goto notify;
	}
	if ((control->flags & 4) != 0) {
		result = bbmcSendSimpleMessage(control, 42);
		connectedRestartRequested = 0;
		if (result < 0)
			goto notify;
	} else {
		result = 0;
		connectedRestartRequested = 0;
	}
	if (result == 0) {
		result = 2;
		*nextStep = 13;
	}
	return result;
notify:
	*nextStep = 9;
	result = bbmcSendSimpleMessage(control, 188);
	if (result >= 0)
		return 1;
	return result;
}

int suspendSteadyConnected(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = connectedSuspendStep;
	SceUInt32 nextStep;
	int result;

	control->suspendError = (int)(step | 0x80011700);
	if (step > 12)
		SceDebugForDriver_391B5B74(&dbg_suspendSteadyConnected_L1335,
				__builtin_return_address(0));
	result = connectedSuspendHandlers[step](control, message, &nextStep);
	if (result >= 0) {
		connectedSuspendStep = nextStep;
		if (nextStep != 13)
			return 1;
	}
	connectedSuspendStep = 0;
	control->suspendError = (int)0x8001170d;
	return result;
}

int suspendGotoLPM(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	int result = 1;
	int currentState, stopResult;
	SceUInt16 reply;

	control->suspendError = (int)(connectedSuspendCompletion | 0x80011900);
	if (connectedSuspendCompletion != 0)
		SceDebugForDriver_391B5B74(&dbg_suspendGotoLPM_L1373,
				__builtin_return_address(0));
	if (message->command == 11 || message->command == 36) {
		connectedRestartRequested = 1;
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 1;
		bbmcLeaveCS(&control->mutex);
	}
	if (message->command == 67) {
		if (connectedTransitionStep == 2)
			goto sendSysconAfterTimer;
		goto sendSyscon;
	}

	switch (connectedTransitionStep) {
	case 0:
		if (message->command != 11 && message->command != 29 &&
		    message->command != 36)
			goto waiting;
		bbmcEnterCS(&control->mutex);
		currentState = control->currentState;
		bbmcLeaveCS(&control->mutex);
		if (currentState != 7) {
			result = (int)0x80330006;
			goto finish;
		}
		result = bbmcRequestNetworkStop();
		if (result < 0) {
			if (result == (int)0x80330508) {
				result = connectedRequestLowPowerMode(control);
				if (result < 0)
					goto finish;
				goto startTimer;
			}
			if (result != (int)0x80330507)
				goto finish;
		}
		connectedTransitionStep = 1;
		goto waiting;
	case 1:
		if (message->command != 57)
			goto waiting;
		if (message->body.commandResult != 0) {
			result = (int)0x80330001;
			control->transitionStep = 6;
			goto finish;
		}
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		input[0].value = 1;
		result = bbcmdExecute(0, 37, input, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
startTimer:
		connectedTransitionStep = 2;
		result = bbmcTimerSetHandler(60000000,
					    connectedTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = connectedTransitionStep;
			goto finish;
		}
		connectedTransitionTimer = result;
		goto checkTransition;
	case 2:
		if (message->command == 67)
			goto sendSysconAfterTimer;
		if (message->command == 72) {
			result = message->body.commandResult;
			if (result >= 0)
				goto waiting;
			if (result != (int)0x8033501a)
				goto finish;
			result = bbmcSendSimpleMessage(control, 67);
			goto checkTransition;
		}
		if (message->command == 187 &&
		    message->body.timedOutCommand == 67) {
			result = (int)0x80330001;
			control->transitionStep = 2;
			goto finish;
		}
		goto waiting;
	case 3:
		if (message->command != 23)
			goto waiting;
		reply = message->body.sysconReplyHalfword;
		if (reply != 0) {
			if (reply == 3) {
				control->targetState = 7;
			} else {
				result = (int)0x80330001;
				bbmcNotifyFeatureEvent(control, 0, 1);
			}
			goto finish;
		}
		bbmcEnterCS(&control->mutex);
		control->firstBootReply = 0;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->powerTransitionBusy = 0;
		memset(&control->networkInfo, 0, sizeof(control->networkInfo));
		control->networkInfo.signalLevel = -1;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, 4);
		if (connectedRestartRequested != 1) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 6;
			bbmcLeaveCS(&control->mutex);
			goto finish;
		}
		result = bbmcSendSimpleMessage(control, 4);
		if (result < 0)
			goto finish;
		connectedRestartRequested = 0;
		connectedTransitionStep = 4;
		goto waiting;
	case 4:
		if (message->command != 5)
			goto waiting;
		bbmcEnterCS(&control->mutex);
		control->currentState = 6;
		bbmcLeaveCS(&control->mutex);
		if ((message->body.sysconReplyFlags & 0x20) != 0 &&
		    control->pinStatus.pin1Status == 1)
			result = bbmcSendSimpleMessage(control, 36);
		else
			result = bbmcSendSimpleMessage(control, 11);
		goto finish;
	case 5:
		if (message->command != 57 && message->command != 108)
			goto waiting;
		/* Both completions reach the original failure cleanup. */
	default:
		result = (int)0x80330001;
		goto finish;
	}

sendSysconAfterTimer:
	bbmcTimerCancelHandler(connectedTransitionTimer);
	connectedTransitionTimer = 0;
sendSyscon:
	connectedSysconPending = 1;
	connectedTransitionStep = 3;
	result = bbmcSendSysconCommand(control, connectedSysconReply,
				      &control->transitionPacket, 6, 2, 0);
	if (result < 0) {
		connectedSysconPending = 0;
		goto finish;
	}
checkTransition:
	if (connectedTransitionStep != 6)
		goto waiting;
finish:
	connectedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket,
			       &connectedSysconPending);
	if (result < 0) {
		stopResult = bbmcRequestNetworkStop();
		if (stopResult == 0) {
			connectedTransitionStep = 5;
			bbmcNotifyPacketInterfaceEvent(16, result);
			connectedRestartRequested = 0;
			goto waiting;
		}
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 0;
		bbmcLeaveCS(&control->mutex);
		connectedRestartRequested = 0;
		goto complete;
	}
	if ((control->flags & 4) == 0) {
		connectedRestartRequested = 0;
		goto success;
	}
	result = bbmcSendSimpleMessage(control, 42);
	connectedRestartRequested = 0;
	if (result == 1)
		goto waiting;
complete:
	if (result == 0)
		goto success;
	connectedSuspendCompletion = 0;
	if (result < 0)
		control->flags |= 2;
	return 0;
success:
	connectedSuspendCompletion = 0;
	control->suspendError = (int)0x80011901;
	return 0;
waiting:
	if (connectedSuspendCompletion == 1) {
		connectedSuspendCompletion = 0;
		return 0;
	}
	return 1;
}
