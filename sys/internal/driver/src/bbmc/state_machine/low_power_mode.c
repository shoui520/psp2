#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int (*seqSuspendSelectHandler(int currentState, int targetState))(BbmcControl *,
	BbmcMessage *);
int gpsStopOneShotTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopPositionTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopNetworkService(BbmcControl *control, BbmcMessage *message);

SceUInt32 lowPowerModeStep;
int lowPowerSysconPending;
SceUID lowPowerTransitionTimer;
SceUInt32 lowPowerSuspendCompletion;
SceUInt32 lowPowerSteadySuspendStep;

int lowPowerSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	SceUInt32 step = lowPowerModeStep;
	BbmcMessage *message;

	if (packet->request.command[0] != 0x80)
		goto done;
	if (packet->request.command[1] != 0x0a)
		goto done;
	if (step != 2)
		goto done;
	message = msgAllocMsgPacket(control, 23);
	message->body.sysconReplyHalfword = packet->reply.data[0] |
		(packet->reply.data[1] << 8);
	msgSendMsgPacket(control, message);
done:
	lowPowerSysconPending = 0;
	return 0;
}

void lowPowerTimeoutHandler(void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;

	if (lowPowerModeStep != 1)
		return;
	message = msgAllocMsgPacket(control, 187);
	if (!message)
		return;
	message->body.timedOutCommand = 67;
	msgSendMsgPacket(control, message);
}

/*
 * name unknown: the body the transitions from Low Power Mode to Connected share. It
 * sets *finished when the transition ended, successfully or not.
 */
static inline int lowPowerConnectSequence(BbmcControl *control,
		BbmcMessage *message, int *finished)
{
	BbcmdArgument input[7];
	SceUInt32 step = lowPowerModeStep;
	SceUInt32 state;
	int result;

	*finished = 0;
	switch (step) {
	case 0:
		if (message->command != 30)
			return 1;
		bbmcEnterCS(&control->mutex);
		state = control->currentState;
		bbmcLeaveCS(&control->mutex);
		if (state != 6) {
			result = (int)0x80330006;
			goto cleanup;
		}
		memset(input, 0, sizeof(input));
		result = bbcmdExecute(0, 37, input, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto cleanup;
		lowPowerModeStep = 1;
		result = bbmcTimerSetHandler(60000000,
					    lowPowerTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = lowPowerModeStep;
			goto cleanup;
		}
		lowPowerTransitionTimer = result;
		goto checkStep;
	case 1:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(lowPowerTransitionTimer);
			lowPowerTransitionTimer = 0;
			lowPowerSysconPending = step;
			lowPowerModeStep = 2;
			result = bbmcSendSysconCommand(control,
				lowPowerSysconReply, &control->transitionPacket,
				6, 2, 0);
			if (result < 0) {
				lowPowerSysconPending = 0;
				goto cleanup;
			}
			goto checkStep;
		case 73:
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto cleanup;
			result = bbmcSendSimpleMessage(control, 67);
			goto checkStep;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			control->transitionStep = step;
			result = (int)0x80330001;
			goto cleanup;
		default:
			return 1;
		}
	case 2:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword != 4) {
			result = (int)0x80330001;
			bbmcNotifyFeatureEvent(control, 0, 1);
			goto cleanup;
		}
		bbmcEnterCS(&control->mutex);
		control->currentState = 7;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->trackingStartMode = 1;
		if (control->powerEventActive || control->powerEventTimer) {
			result = bbmcSendSimpleMessage(control, 29);
		} else {
			result = control->deferredNetworkStart;
			control->powerTransitionBusy = 0;
			if (result) {
				result = 0;
				bbmcScheduleDeferredNetworkRetry(control);
			}
		}
		bbmcLeaveCS(&control->mutex);
		goto cleanup;
	default:
		result = (int)0x80330001;
		goto cleanup;
	}

checkStep:
	if (lowPowerModeStep != 3)
		return 1;
cleanup:
	*finished = 1;
	lowPowerModeStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket,
				&lowPowerSysconPending);
	if (result < 0) {
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 0;
		bbmcLeaveCS(&control->mutex);
	}
	return result;
}

/* name unknown: the state handler of the transition from Low Power Mode to Connected */
int stmLowPowerMode_Connected(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	int finished;
	int result;

	if (flags & 4) {
		bbmcEnterCS(&control->mutex);
		if (control->suspendState == 1) {
			control->suspendState = 2;
			control->suspendStep = lowPowerModeStep;
			control->suspendMessageHandler = seqSuspendSelectHandler(
				control->currentState, control->targetState);
		}
		bbmcLeaveCS(&control->mutex);
		return 0;
	}
	if (flags & 0xa) {
		bbmcEnterCS(&control->mutex);
		control->currentState = 5;
		control->powerTransitionBusy = 0;
		bbmcLeaveCS(&control->mutex);
		bbmcCancelSysconCommand(&control->transitionPacket,
					&lowPowerSysconPending);
		result = bbmcSendSimpleMessage(control, 22);
		lowPowerModeStep = 0;
		return result;
	}
	result = lowPowerConnectSequence(control, message, &finished);
	if (!finished)
		return 1;
	if (result < 0)
		return result;
	if (control->flags & 4)
		return bbmcSendSimpleMessage(control, 42);
	return 0;
}

/* name unknown: the state handler of the transition from Low Power Mode to Activated */
int stmLowPowerMode_Activated(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	int result;

	if (flags & 2) {
		control->currentState = 5;
		lowPowerModeStep = 1;
		result = bbmcSendSimpleMessage(control, 22);
	} else {
		if (lowPowerModeStep != 0) {
			result = (int)0x80330001;
			goto finish;
		}
		control->currentState = 5;
		lowPowerModeStep = 1;
		control->powerPolicy = 0;
		if (flags & 4) {
			result = 0;
			goto finish;
		}
		result = bbmcSendSimpleMessage(control, 22);
	}
	if (lowPowerModeStep != 1)
		return 1;
finish:
	lowPowerModeStep = 0;
	return result;
}

int suspendSteadyLowPowerMode(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = lowPowerSteadySuspendStep;
	SceUInt32 initialized, online, deferred;
	SceUInt32 next;
	unsigned int command;
	int result;

	control->suspendError = (int)(step | 0x80011300);
	switch (step) {
	case 0:
		bbmcEnterCS(&control->mutex);
		initialized = control->clientInitialized;
		online = control->clientOnline;
		deferred = control->deferredNetworkStart;
		bbmcLeaveCS(&control->mutex);
		if (initialized || online) {
			lowPowerSteadySuspendStep = 1;
			result = bbmcSendSimpleMessage(control, 115);
		} else if (deferred) {
			result = bbmcSendSimpleMessage(control, 107);
		} else {
			lowPowerSteadySuspendStep = 2;
			result = bbmcSendSimpleMessage(control, 188);
		}
		if (result < 0)
			goto finish;
		goto pending;
	case 1:
		if (message->command != 57)
			return 1;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto finish;
		lowPowerSteadySuspendStep = 2;
		return 1;
	case 2:
		result = gpsStopOneShotTracking(control, message);
		if (result != 0)
			goto pending;
		next = 3;
		command = 79;
		goto send;
	case 3:
		result = suspendStopPositionTracking(control, message);
		if (result != 0)
			goto pending;
		next = 4;
		command = 188;
		goto send;
	case 4:
		result = bbmcSendSimpleMessage(control, 50);
		if (result < 0)
			goto finish;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto finish;
		lowPowerSteadySuspendStep = 5;
		return 1;
	case 5:
		result = suspendStopNetworkService(control, message);
		if (result > 0)
			goto pending;
		next = 6;
		command = 188;
		goto send;
	case 6:
		result = bbmcPrepareUsbShutdown(control);
		if (result > 0)
			goto pending;
		if (control->suspendArgument == 0)
			goto finish;
		next = 7;
		command = 188;
send:
		lowPowerSteadySuspendStep = next;
		result = bbmcSendSimpleMessage(control, command);
		if (result < 0)
			goto finish;
pending:
		step = lowPowerSteadySuspendStep;
check:
		if (step != 9)
			return 1;
finish:
		result = result & (result >> 31);
		break;
	case 7:
		control->flags |= 8;
		SceSysconForDriver_DD16ABD9(0);
		bbmcEnterCS(&control->mutex);
		control->sysconPowerRequestPending = 1;
		bbmcLeaveCS(&control->mutex);
		lowPowerSteadySuspendStep = 8;
		return 1;
	case 8:
		if (message->command != 9)
			return 1;
		result = 0;
		break;
	default:
		result = 1;
		goto check;
	}
	lowPowerSteadySuspendStep = 0;
	control->suspendError = (int)0x80011309;
	return result;
}

int suspendDuringFromLpmToConnected(BbmcControl *control, BbmcMessage *message)
{
	int finished;
	int result;

	control->suspendError = (int)0x80011400;
	result = lowPowerConnectSequence(control, message, &finished);
	if (!finished)
		return 1;
	if (result >= 0 && (control->flags & 4)) {
		result = bbmcSendSimpleMessage(control, 42);
		if (result == 1)
			return 1;
	}
	if (result < 0) {
		control->flags |= 2;
		return 0;
	}
	return 2;
}

static const SceKernelDebugInfo dbg_suspendGotoConnected_L751
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/low_power_mode.c",
	.line = 751,
	.func = "suspendGotoConnected",
#endif
};

static const SceKernelDebugInfoSource dbg_suspendGotoConnected_L751Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendGotoConnected_L751,
	"state_machine/low_power_mode.c", 751, "suspendGotoConnected",
};

int suspendGotoConnected(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	SceUInt32 completion = lowPowerSuspendCompletion;
	SceUInt32 step;
	SceUInt32 state;
	int result;

	control->suspendError = (int)(completion | 0x80011600);
	if (completion)
		SceDebugForDriver_391B5B74(&dbg_suspendGotoConnected_L751,
			       __builtin_return_address(0));
	step = lowPowerModeStep;
	switch (step) {
	case 0:
		if (message->command != 30)
			return 1;
		bbmcEnterCS(&control->mutex);
		state = control->currentState;
		bbmcLeaveCS(&control->mutex);
		if (state != 6) {
			result = (int)0x80330006;
			goto cleanup;
		}
		memset(input, 0, sizeof(input));
		result = bbcmdExecute(0, 37, input, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto cleanup;
		lowPowerModeStep = 1;
		result = bbmcTimerSetHandler(60000000,
					    lowPowerTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = lowPowerModeStep;
			goto cleanup;
		}
		lowPowerTransitionTimer = result;
		goto checkStep;
	case 1:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(lowPowerTransitionTimer);
			lowPowerTransitionTimer = 0;
			lowPowerSysconPending = step;
			lowPowerModeStep = 2;
			result = bbmcSendSysconCommand(control,
				lowPowerSysconReply, &control->transitionPacket,
				6, 2, 0);
			if (result < 0) {
				lowPowerSysconPending = 0;
				goto cleanup;
			}
			goto checkStep;
		case 73:
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto cleanup;
			result = bbmcSendSimpleMessage(control, 67);
			goto checkStep;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			control->transitionStep = step;
			result = (int)0x80330001;
			goto cleanup;
		default:
			return 1;
		}
	case 2:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword != 4) {
			result = (int)0x80330001;
			bbmcNotifyFeatureEvent(control, 0, 1);
			goto cleanup;
		}
		bbmcEnterCS(&control->mutex);
		control->currentState = 7;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->trackingStartMode = 1;
		if (control->powerEventActive || control->powerEventTimer) {
			result = bbmcSendSimpleMessage(control, 29);
		} else {
			result = control->deferredNetworkStart;
			control->powerTransitionBusy = 0;
			if (result) {
				result = 0;
				bbmcScheduleDeferredNetworkRetry(control);
			}
		}
		bbmcLeaveCS(&control->mutex);
		goto cleanup;
	default:
		result = (int)0x80330001;
		goto cleanup;
	}

checkStep:
	if (lowPowerModeStep == 3)
		goto cleanup;
checkCompletion:
	if (lowPowerSuspendCompletion != 1)
		return 1;
	lowPowerSuspendCompletion = 0;
	control->suspendError = (int)0x80011601;
	return 0;

cleanup:
	lowPowerModeStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket,
				&lowPowerSysconPending);
	if (result < 0) {
		bbmcEnterCS(&control->mutex);
		control->powerTransitionBusy = 0;
		bbmcLeaveCS(&control->mutex);
	} else {
		result = control->flags & 4;
		if (result) {
			result = bbmcSendSimpleMessage(control, 42);
			if (result == 1)
				goto checkCompletion;
		}
	}
	lowPowerSuspendCompletion = 0;
	control->suspendError = (int)0x80011601;
	if (result < 0)
		control->flags |= 2;
	return 0;
}

int suspendLowPowerModeToStandby(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 refreshPending;

	control->suspendError = (int)0x80011500;
	bbmcEnterCS(&control->mutex);
	refreshPending = control->simRefreshPending;
	control->currentState = 5;
	if (!refreshPending)
		control->targetState = 5;
	bbmcLeaveCS(&control->mutex);
	return bbmcSendSimpleMessage(control, 42);
}
