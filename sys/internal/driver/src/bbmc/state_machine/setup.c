/* The Authorized state (3): modem setup towards Activated, the way back to
 * Standby and the suspend handlers. Sony's source filename is unavailable. */
#include <syscon/syscon.h>
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

SceUInt32 modemSetupStep;
SceUID modemSetupTimer;
SceUInt32 modemSetupRetryCount;
SceUInt32 authorizedSuspendStep;

void modemSetupTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (modemSetupStep == 1) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 181;
			msgSendMsgPacket(control, message);
		}
	} else if (modemSetupStep == 3) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 69;
			msgSendMsgPacket(control, message);
		}
	}
}

/* Authorized -> Activated: read the indexed responses, wait for the control key,
 * register (with the system selection first on modem firmware 1575..1699 past
 * 1575.0.1), read the device information and firmware selection, then report
 * the Activated state. Returns 1 while in progress; on finish it clears the step
 * and the retry count and returns the sequence result. */
static inline int modemSetupSequence(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument information[6];
	BbcmdArgument argument[7];
	SceUInt32 command = message->command;
	const SceUInt8 *revision;
	SceUInt32 events;
	int family;
	int result;

	if (command == 11) {
		result = modemSetupStep & ~8;
		if (result != 0) {
			if (modemSetupStep != 6)
				modemSetupStep = 6;
			goto check_reply;
		}
		if (bbmcCollectControlKeyEvents(control) & 0x1000000)
			goto finish;
		goto abort;
	}
	switch (modemSetupStep) {
	case 0:
		if (command != 21)
			return 1;
		bbmcEnterCS(&control->mutex);
		memset(control->indexedResponses, 0, sizeof(control->indexedResponses));
		argument[0].pointer = control->indexedResponses;
		bbmcLeaveCS(&control->mutex);
		result = bbcmdExecute(0, 49, NULL, argument, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		modemSetupStep = 1;
		return 1;
	case 1:
		if (command != 181) {
			if (command != 187 || message->body.timedOutCommand != 181)
				return 1;
			bbmcEnterCS(&control->mutex);
			memset(control->indexedResponses, 0,
				sizeof(control->indexedResponses));
			argument[0].pointer = control->indexedResponses;
			bbmcLeaveCS(&control->mutex);
			result = bbcmdExecute(0, 49, NULL, argument,
				bbcmdManagerCompletion, control);
			if (result < 0)
				goto finish;
			goto waiting;
		}
		result = message->body.commandResult;
		if (result != 0) {
			if (result == (int)0x80330306)
				goto finish;
			if (modemSetupRetryCount > 60) {
				if (result > 0)
					result = (int)0x80330001;
				goto finish;
			}
			result = bbmcTimerSetHandler(1000000, modemSetupTimeoutHandler,
				control);
			if (result < 0) {
				control->transitionStep = modemSetupStep;
				goto finish;
			}
			modemSetupTimer = result;
			modemSetupRetryCount++;
			goto waiting;
		}
		if (modemSetupTimer != 0) {
			bbmcTimerCancelHandler(modemSetupTimer);
			modemSetupTimer = 0;
		}
		modemSetupRetryCount = 0;
		bbmcEnterCS(&control->mutex);
		control->controlKeyReported = 0;
		bbmcLeaveCS(&control->mutex);
		events = bbmcCollectControlKeyEvents(control);
		bbmcNotifyFeatureEvent(control, 4, events | 0x400000);
		if (events & 0x1800000) {
			bbmcEnterCS(&control->mutex);
			control->targetState = 3;
			if (control->flags & 4) {
				control->savedTargetState = 3;
				result = bbmcSendSimpleMessage(control, 42);
				if (result < 0) {
					modemSetupStep = 8;
					bbmcLeaveCS(&control->mutex);
					goto waiting;
				}
			}
			bbmcLeaveCS(&control->mutex);
			goto finish;
		}
		family = control->versionFamily;
		revision = (const SceUInt8 *)&control->versionRevision;
		if ((unsigned int)(family - 1575) <= 124 &&
		    (family != 1575 || revision[0] != 0 || revision[1] > 1)) {
			argument[0].value = 0;
			argument[1].value = 0;
			argument[2].value = 0;
			argument[3].value = 0;
			argument[4].value = 0;
			argument[5].value = 0;
			argument[6].value = 0;
			modemSetupStep = 2;
			result = bbcmdExecute(0, 32, argument, NULL,
				bbcmdManagerCompletion, control);
			if (result < 0)
				goto finish;
			goto waiting;
		}
		modemSetupStep = 3;
		goto register_modem;
	case 2:
		if (command != 77)
			return 1;
		result = message->body.commandResult;
		if (result != 0)
			goto finish;
register_modem:
		result = bbcmdExecute(0, 31, NULL, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		control->registrationPending = 1;
		modemSetupStep = 3;
		result = bbmcTimerSetHandler(60000000, modemSetupTimeoutHandler,
			control);
		if (result < 0) {
			control->transitionStep = modemSetupStep;
			goto finish;
		}
		modemSetupTimer = result;
		goto waiting;
	case 3:
		if (command == 69) {
			bbmcTimerCancelHandler(modemSetupTimer);
			modemSetupTimer = 0;
			goto read_information;
		}
		if (command == 68) {
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			goto finish;
		}
		if (command == 187 && message->body.timedOutCommand == 69) {
			modemSetupTimer = 0;
			result = bbmcSendSimpleMessage(control, 1);
			if (result < 0)
				goto finish;
			modemSetupStep = 7;
		}
		return 1;
	case 4:
		if (command != 70)
			return 1;
		if (control->flags & 1) {
			result = bbmcSendSimpleMessage(control, 22);
			goto finish;
		}
		result = message->body.commandResult;
		if (result < 0)
			goto finish;
		argument[0].pointer = &control->firmwareBuildSelection;
		argument[1].pointer = &control->firmwareRevisionSelection;
		argument[2].pointer = &control->runningStorageIndex;
		argument[3].value = 0;
		argument[4].value = 0;
		argument[5].value = 0;
		result = bbcmdExecute(0, 51, NULL, argument, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		modemSetupStep = 5;
		return 1;
	case 5:
		if (command != 186)
			return 1;
		result = message->body.commandResult;
		if (result == 0) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 5;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control,
				(control->flags & 4) ? 42 : 21);
		}
		goto finish;
	case 6:
		if (command == 186 || command == 77 || command == 181)
			goto abort;
check_reply:
		if (command - 68 > 2 && command != 2)
			return 1;
		goto abort;
	case 7:
		if (command != 2)
			return 1;
		if ((message->body.commandResult & 8) == 0) {
			result = (int)0x80330001;
			control->transitionStep = 7;
			goto finish;
		}
read_information:
		information[3].value = 0;
		information[4].value = 0;
		information[5].value = 0;
		argument[1].value = 0;
		argument[2].value = 0;
		argument[3].value = 0;
		argument[4].value = 0;
		argument[5].value = 0;
		argument[6].value = 0;
		memset(&control->smscAddress, 0, sizeof(control->smscAddress));
		memset(&control->profile, 0, sizeof(control->profile));
		information[0].pointer = &control->deviceInformation;
		information[1].pointer = &control->smscAddress;
		information[2].pointer = &control->profile;
		argument[0].value = 1;
		result = bbcmdExecute(0, 14, argument, information,
			bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		modemSetupStep = 4;
		return 1;
	default:
		result = (int)0x80330001;
		goto finish;
	}
abort:
	control->currentState = 3;
	result = bbmcSendSimpleMessage(control, 36);
	if (result >= 0)
		goto finish;
waiting:
	if (modemSetupStep != 8)
		return 1;
finish:
	modemSetupStep = 0;
	modemSetupRetryCount = 0;
	return result;
}

int stmAuthorized_Activated(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;

	if (flags & 0xa) {
		control->currentState = 3;
		modemSetupStep = 0;
		return bbmcSendSimpleMessage(control, 22);
	}
	if (flags & 4) {
		bbmcEnterCS(&control->mutex);
		if (control->suspendState == 1) {
			control->suspendState = 2;
			control->suspendStep = modemSetupStep;
			control->suspendMessageHandler = seqSuspendSelectHandler(
				control->currentState, control->targetState);
		}
		bbmcLeaveCS(&control->mutex);
		return 0;
	}
	return modemSetupSequence(control, message);
}

int stmAuthorized_Standby(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	int result;

	(void)message;
	if ((flags & 2) == 0) {
		if (flags & 4) {
			bbmcEnterCS(&control->mutex);
			if (control->suspendState == 1) {
				control->suspendState = 2;
				control->suspendStep = modemSetupStep;
				bbmcSendSimpleMessage(control, 42);
			}
			bbmcLeaveCS(&control->mutex);
			modemSetupStep = 8;
			return 1;
		}
		if (modemSetupStep != 0) {
			result = (int)0x80330001;
			goto finish;
		}
	}
	control->currentState = 2;
	modemSetupStep = 1;
	result = bbmcSendSimpleMessage(control, 22);
	if (modemSetupStep != 1)
		return 1;
finish:
	modemSetupStep = 0;
	return result;
}

int suspendSteadyAuthorized(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = authorizedSuspendStep;
	SceUInt32 next;
	unsigned int command;
	int result;

	control->suspendError = (int)(step | 0x80010900);
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
		authorizedSuspendStep = 2;
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
		authorizedSuspendStep = next;
		result = bbmcSendSimpleMessage(control, command);
		if (result < 0)
			goto finish;
pending:
		step = authorizedSuspendStep;
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
		authorizedSuspendStep = 5;
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
	authorizedSuspendStep = 0;
	control->suspendError = (int)0x80010906;
	return result;
}

int suspendDuringFromAuthorizedToActivated(BbmcControl *control,
	BbmcMessage *message)
{
	int result;

	control->suspendError = (int)0x80010a00;
	result = modemSetupSequence(control, message);
	if (result == 1)
		return 1;
	if (result >= 0)
		return 2;
	control->currentState = 2;
	control->targetState = 2;
	control->savedTargetState = 1;
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}

int suspendDuringFromAuthorizedToStandby(BbmcControl *control,
	BbmcMessage *message)
{
	(void)message;
	control->suspendError = (int)0x80010b00;
	bbmcEnterCS(&control->mutex);
	control->currentState = 2;
	control->targetState = 2;
	bbmcLeaveCS(&control->mutex);
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}
