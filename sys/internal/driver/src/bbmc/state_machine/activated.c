/* The Activated state (5): its transitions to Authorized, Low Power Mode and Connected,
 * and its suspend handlers. Sony's file name is not recorded; named after the state
 * like the other state_machine files.
 */
#include <syscon/syscon.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int (*seqSuspendSelectHandler(int currentState, int targetState))(BbmcControl *,
	BbmcMessage *);
int gpsStopOneShotTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopPositionTracking(BbmcControl *control, BbmcMessage *message);
int suspendStopNetworkService(BbmcControl *control, BbmcMessage *message);

SceUInt32 activatedTransitionStep;
SceUInt32 activatedSuspendStep;
int activatedTransitionTimer;
int activatedSysconPending;

int activatedSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt32 step = activatedTransitionStep;
	int result = 0;

	if (packet->request.command[0] != 0x80)
		goto done;
	if (packet->request.command[1] != 0x0a)
		goto done;
	if (step != 2 && step != 3)
		goto done;
	message = msgAllocMsgPacket(control, 23);
	message->body.sysconReplyHalfword = packet->reply.data[0] |
		(packet->reply.data[1] << 8);
	result = msgSendMsgPacket(control, message);
done:
	activatedSysconPending = 0;
	return result;
}

void activatedTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (activatedTransitionStep == 2) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 67;
			msgSendMsgPacket(control, message);
		}
	}
}

/* a suspend request arriving during a transition hands the transition to the
 * suspend handler of the current and target states */
static inline void activatedCheckSuspend(BbmcControl *control)
{
	bbmcEnterCS(&control->mutex);
	if (control->suspendState == 1) {
		control->suspendState = 2;
		control->suspendStep = activatedTransitionStep;
		control->suspendMessageHandler = seqSuspendSelectHandler(control->currentState,
			control->targetState);
	}
	bbmcLeaveCS(&control->mutex);
}

int stmActivated_LowPowerMode(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	SceUInt16 reply;
	int result;

	if (control->flags & 0xa) {
		control->currentState = 5;
		activatedTransitionStep = 0;
		return bbmcSendSimpleMessage(control, 22);
	}
	if (control->flags & 4)
		activatedCheckSuspend(control);
	if (message->command == 11) {
		control->currentState = 5;
		result = bbmcSendSimpleMessage(control, 36);
		if (result >= 0)
			goto finish;
		goto waiting;
	}
	switch (activatedTransitionStep) {
	case 0:
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		input[0].value = 1;
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		activatedTransitionStep = 1;
		return 1;
	case 1:
		if (message->command != 72)
			return 1;
		if (message->body.commandResult != 0 &&
		    message->body.commandResult != (int)0x8033501a) {
			control->transitionStep = 1;
			result = message->body.commandResult;
			goto finish;
		}
		activatedTransitionStep = 2;
		activatedSysconPending = 1;
		result = bbmcSendSysconCommand(control, activatedSysconReply,
			&control->transitionPacket, 6, 2, 0);
		if (result < 0) {
			activatedSysconPending = 0;
			goto finish;
		}
		goto waiting;
	case 2:
		if (message->command != 23)
			return 1;
		reply = message->body.sysconReplyHalfword;
		if (reply != 0) {
			result = (int)0x80330001;
			goto finish;
		}
		bbmcEnterCS(&control->mutex);
		control->currentState = 6;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->powerPolicy = reply;
		bbmcLeaveCS(&control->mutex);
		result = control->flags & 4;
		if (result)
			result = bbmcSendSimpleMessage(control, 42);
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}
waiting:
	if (activatedTransitionStep != 3)
		return 1;
finish:
	activatedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &activatedSysconPending);
	return result;
}

int stmActivated_Connected(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	SceUInt32 busy;
	int result;

	if (control->flags & 0xa) {
		control->currentState = 5;
		activatedTransitionStep = 0;
		return bbmcSendSimpleMessage(control, 22);
	}
	if (control->flags & 4)
		activatedCheckSuspend(control);
	if (message->command == 11) {
		control->currentState = 5;
		result = bbmcSendSimpleMessage(control, 36);
		if (result < 0)
			goto waiting;
		goto finish;
	}
	switch (activatedTransitionStep) {
	case 0:
		input[0].value = 0;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		activatedTransitionStep = 2;
		result = bbmcTimerSetHandler(60000000, activatedTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = activatedTransitionStep;
			goto finish;
		}
		activatedTransitionTimer = result;
		goto waiting;
	case 2:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(activatedTransitionTimer);
			activatedTransitionTimer = 0;
			activatedSysconPending = 1;
			activatedTransitionStep = 3;
			result = bbmcSendSysconCommand(control, activatedSysconReply,
				&control->transitionPacket, 6, 2, 0);
			if (result < 0) {
				activatedSysconPending = 0;
				goto finish;
			}
			goto waiting;
		case 74:
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto finish;
			result = bbmcSendSimpleMessage(control, 67);
			goto waiting;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			result = (int)0x80330001;
			control->transitionStep = 2;
			goto finish;
		default:
			return 1;
		}
	case 3:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword != 4) {
			result = (int)0x80330001;
			goto finish;
		}
		bbmcEnterCS(&control->mutex);
		control->currentState = 7;
		result = 0;
		busy = control->powerTransitionBusy;
		control->secondBootReply = message->body.sysconReplyHalfword;
		if (busy == 0) {
			control->powerTransitionBusy = 1;
			result = bbmcSendSimpleMessage(control, 29);
		}
		bbmcLeaveCS(&control->mutex);
		if (control->flags & 4) {
			result = bbmcSendSimpleMessage(control, 42);
			goto waiting;
		}
		goto finish;
	case 1:
	default:
		if (activatedTransitionStep != 0) {
			result = (int)0x80330001;
			goto finish;
		}
		return 1;
	}
waiting:
	if (activatedTransitionStep != 4)
		return 1;
finish:
	activatedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &activatedSysconPending);
	return result;
}

int stmActivated_Authorized(BbmcControl *control, BbmcMessage *message)
{
	int result;

	(void)message;
	if (control->flags & 2) {
		control->currentState = 3;
		activatedTransitionStep = 1;
		result = bbmcSendSimpleMessage(control, 22);
		if (activatedTransitionStep != 1)
			return 1;
		goto finish;
	}
	result = activatedTransitionStep;
	if (result != 0)
		result = (int)0x80330001;
	else
		control->currentState = 3;
finish:
	activatedTransitionStep = 0;
	return result;
}

int suspendSteadyActivated(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = activatedSuspendStep;
	SceUInt32 next;
	unsigned int command;
	int result;

	control->suspendError = (int)(step | 0x80010f00);
	switch (step) {
	case 0:
		result = gpsStopOneShotTracking(control, message);
		if (result != 0)
			goto pending;
		next = 1;
		command = 79;
		goto send;
	case 1:
		result = suspendStopPositionTracking(control, message);
		if (result != 0)
			goto pending;
		result = bbmcSendSimpleMessage(control, 50);
		if (result < 0)
			goto finish;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto finish;
		activatedSuspendStep = 2;
		return 1;
	case 2:
		result = suspendStopNetworkService(control, message);
		if (result > 0)
			goto pending;
		next = 3;
		command = 188;
		goto send;
	case 3:
		result = bbmcPrepareUsbShutdown(control);
		if (result > 0)
			goto pending;
		if (control->suspendArgument == 0)
			goto finish;
		next = 4;
		command = 188;
send:
		activatedSuspendStep = next;
		result = bbmcSendSimpleMessage(control, command);
		if (result < 0)
			goto finish;
pending:
		step = activatedSuspendStep;
check:
		if (step != 6)
			return 1;
finish:
		result = result & (result >> 31);
		break;
	case 4:
		control->flags |= 8;
		SceSysconForDriver_DD16ABD9(0);
		bbmcEnterCS(&control->mutex);
		control->sysconPowerRequestPending = 1;
		bbmcLeaveCS(&control->mutex);
		activatedSuspendStep = 5;
		return 1;
	case 5:
		if (message->command != 9)
			return 1;
		result = 0;
		break;
	default:
		result = 1;
		goto check;
	}
	activatedSuspendStep = 0;
	control->suspendError = (int)0x80010f06;
	return result;
}

/* the suspend handlers of a transition finish it as their state handler would, then
 * return 2 when it completed and 0 (with the error flag) when it failed */
static inline int activatedSuspendResult(BbmcControl *control, int result)
{
	if (result != 1) {
		if (result >= 0) {
			result = 2;
		} else {
			result = 0;
			control->flags |= 2;
		}
	}
	return result;
}

int suspendDuringFromActivatedToConnected(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	SceUInt32 busy;
	int result;

	control->suspendError = (int)0x80011200;
	if (message->command == 11) {
		control->currentState = 5;
		result = bbmcSendSimpleMessage(control, 36);
		if (result < 0)
			goto waiting;
		goto finish;
	}
	switch (activatedTransitionStep) {
	case 0:
		input[0].value = 0;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		activatedTransitionStep = 2;
		result = bbmcTimerSetHandler(60000000, activatedTimeoutHandler, control);
		if (result < 0) {
			control->transitionStep = activatedTransitionStep;
			goto finish;
		}
		activatedTransitionTimer = result;
		goto waiting;
	case 2:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(activatedTransitionTimer);
			activatedTransitionTimer = 0;
			activatedTransitionStep = 3;
			activatedSysconPending = 1;
			result = bbmcSendSysconCommand(control, activatedSysconReply,
				&control->transitionPacket, 6, 2, 0);
			if (result < 0) {
				activatedSysconPending = 0;
				goto finish;
			}
			goto waiting;
		case 74:
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto finish;
			result = bbmcSendSimpleMessage(control, 67);
			goto waiting;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			result = (int)0x80330001;
			control->transitionStep = activatedTransitionStep;
			goto finish;
		default:
			return 1;
		}
	case 3:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword != 4) {
			result = (int)0x80330001;
			goto finish;
		}
		result = 0;
		bbmcEnterCS(&control->mutex);
		control->currentState = 7;
		busy = control->powerTransitionBusy;
		control->secondBootReply = message->body.sysconReplyHalfword;
		if (busy == 0) {
			control->powerTransitionBusy = 1;
			result = bbmcSendSimpleMessage(control, 29);
		}
		bbmcLeaveCS(&control->mutex);
		if ((control->flags & 4) == 0)
			goto finish;
		result = bbmcSendSimpleMessage(control, 42);
		goto waiting;
	default:
		result = (int)0x80330001;
		goto finish;
	}
waiting:
	if (activatedTransitionStep != 4)
		return 1;
finish:
	activatedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &activatedSysconPending);
	return activatedSuspendResult(control, result);
}

int suspendDuringFromActivatedToLowPowerMode(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	SceUInt16 reply;
	int result;

	control->suspendError = (int)0x80011100;
	if (message->command == 11) {
		control->currentState = 5;
		result = bbmcSendSimpleMessage(control, 36);
		if (result >= 0)
			goto finish;
		goto waiting;
	}
	switch (activatedTransitionStep) {
	case 0:
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		input[0].value = 1;
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		activatedTransitionStep = 1;
		return 1;
	case 1:
		if (message->command != 72)
			return 1;
		if (message->body.commandResult != 0 &&
		    message->body.commandResult != (int)0x8033501a) {
			control->transitionStep = activatedTransitionStep;
			result = message->body.commandResult;
			goto finish;
		}
		activatedSysconPending = activatedTransitionStep;
		activatedTransitionStep = 2;
		result = bbmcSendSysconCommand(control, activatedSysconReply,
			&control->transitionPacket, 6, 2, 0);
		if (result < 0) {
			activatedSysconPending = 0;
			goto finish;
		}
		goto waiting;
	case 2:
		if (message->command != 23)
			return 1;
		reply = message->body.sysconReplyHalfword;
		if (reply != 0) {
			result = (int)0x80330001;
			goto finish;
		}
		bbmcEnterCS(&control->mutex);
		control->currentState = 6;
		control->secondBootReply = message->body.sysconReplyHalfword;
		control->powerPolicy = reply;
		bbmcLeaveCS(&control->mutex);
		result = control->flags & 4;
		if (result)
			result = bbmcSendSimpleMessage(control, 42);
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}
waiting:
	if (activatedTransitionStep != 3)
		return 1;
finish:
	activatedTransitionStep = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &activatedSysconPending);
	return activatedSuspendResult(control, result);
}

int suspendActivatedToStandbyOrAuthorized(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 pending;

	(void)message;
	control->suspendError = (int)0x80011000;
	bbmcEnterCS(&control->mutex);
	pending = control->simRefreshPending;
	control->currentState = 3;
	if (pending == 0)
		control->targetState = 3;
	bbmcLeaveCS(&control->mutex);
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}
