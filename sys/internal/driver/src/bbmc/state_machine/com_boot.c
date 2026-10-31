#include <sysmem/debug.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_gps.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcUnregisterNetworkInterface(void);
void bbmcSaveTransitionState(BbmcControl *control, SceInt32 status,
	SceUInt32 step);

SceUInt32 comBootTransitionStep;
SceUID comBootTimer;
int comBootSysconPending;
SceUInt32 comBootRetryCount;
SceUInt32 comBootPowerOffEvents;
SceUInt32 comBootSuspendStep;

int comBootSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt32 step = comBootTransitionStep;
	int result = 0;

	if (packet->request.command[0] != 0x80)
		goto done;
	if (packet->request.command[1] != 0x0a)
		goto done;
	if (step != 7 && step != 8 && step != 3)
		goto done;
	message = msgAllocMsgPacket(control, 23);
	message->body.sysconReplyHalfword = packet->reply.data[0] |
		(packet->reply.data[1] << 8);
	result = msgSendMsgPacket(control, message);
done:
	comBootSysconPending = 0;
	return result;
}

void comBootTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (comBootTransitionStep == 1 || comBootTransitionStep == 3) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 67;
			msgSendMsgPacket(control, message);
		}
	}
}

int comBootPowerReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	int result;

	if (packet->request.command[0] == 0x8d &&
	    packet->request.command[1] == 0x08 &&
	    (message = msgAllocMsgPacket(control, 39)) != NULL)
		result = msgSendMsgPacket(control, message);
	else
		result = 0;
	comBootSysconPending = 0;
	return result;
}

void comBootPowerOffTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	if (comBootTransitionStep == 2) {
		message = msgAllocMsgPacket(control, 187);
		if (message) {
			message->body.timedOutCommand = 9;
			msgSendMsgPacket(control, message);
		}
	}
}

int stmComBoot_TestMode(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	BbcmdArgument input[7] = {0};
	SceUInt32 step;
	SceUInt16 reply, firstBootReply;
	int result;

	if (flags & 0xa) {
		control->currentState = 1;
		step = comBootTransitionStep;
		if (step == 0) {
			result = bbmcSendSimpleMessage(control, 22);
			goto finish;
		}
		if (step != 5 && step > 1) {
			if (bbcmdExecute(0, 48, NULL, NULL, NULL, control) < 0)
				SCE_PANIC(390);
			bbcmdExecute(0, 6, NULL, NULL, bbcmdManagerCompletion, control);
			comBootTransitionStep = 5;
			return 1;
		}
	} else {
		step = comBootTransitionStep;
	}
	switch (step) {
	case 0:
		if (message->command != 21)
			return 1;
		reply = control->secondBootReply;
		if (reply == 0 || reply == 4 || reply == 5) {
			input[0].value = control->baudRate;
			result = bbcmdExecute(0, 5, input, NULL, bbcmdManagerCompletion,
				control);
			if (result < 0)
				goto finish;
			comBootTransitionStep = 1;
			return 1;
		}
		if (reply != 6 && reply != 1 && reply != 2) {
			result = (int)0x80330001;
			goto finish;
		}
		goto request_reply;
	case 1:
		if (message->command != 58)
			return 1;
		result = message->body.commandResult;
		if (result != 0) {
			control->transitionStep = 1;
			goto finish;
		}
		input[0].value = 5;
		result = bbcmdExecute(0, 9, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		comBootTransitionStep = 2;
		return 1;
	case 2:
		if (message->command != 59)
			return 1;
		result = message->body.commandResult;
		if (result == 0) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 8;
			bbmcLeaveCS(&control->mutex);
			goto finish;
		}
		control->lastError = result;
		bbmcNotifyFeatureEvent(control, 0, 1);
		bbmcNotifyFeatureEvent(control, 7, 0x200);
		result = bbcmdExecute(0, 48, NULL, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		result = bbcmdExecute(0, 6, NULL, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		comBootTransitionStep = 5;
		return 1;
	case 3:
		if (message->command != 23)
			return 1;
		reply = message->body.sysconReplyHalfword;
		firstBootReply = control->firstBootReply;
		if ((reply == 6 && firstBootReply == 6) ||
		    (reply == 1 && (firstBootReply == 1 || firstBootReply == 7))) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 8;
			control->secondBootReply = message->body.sysconReplyHalfword;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control,
				(control->flags & 1) ? 22 : 21);
			goto finish;
		}
		if (comBootRetryCount > 19) {
			result = (int)0x80330001;
			goto finish;
		}
		sceKernelDelayThread(500000);
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			goto finish;
		comBootTransitionStep = 4;
		return 1;
	case 4:
		comBootSysconPending = 1;
		comBootTransitionStep = 3;
		result = bbmcSendSysconCommand(control, comBootSysconReply,
			&control->transitionPacket, 6, 2, 0);
		if (result < 0) {
			comBootSysconPending = 0;
			goto finish;
		}
		comBootRetryCount++;
		goto waiting;
	case 5:
		if (message->command != 57)
			return 1;
		result = bbmcSendSimpleMessage(control, 22);
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}
request_reply:
	comBootSysconPending = 1;
	comBootTransitionStep = 3;
	result = bbmcSendSysconCommand(control, comBootSysconReply,
		&control->transitionPacket, 6, 2, 0);
	if (result < 0) {
		comBootSysconPending = 0;
		goto finish;
	}
waiting:
	if (comBootTransitionStep != 6)
		return 1;
finish:
	comBootTransitionStep = 0;
	comBootRetryCount = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &comBootSysconPending);
	return result;
}

int stmComBoot_Mode9(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7] = {0};
	SceUInt32 step;
	int result;
	int timer;

	if (control->flags & 0xa) {
		control->currentState = 1;
		comBootTransitionStep = 10;
		result = bbmcSendSimpleMessage(control, 22);
		if (comBootTransitionStep != 10)
			return 1;
		goto finish;
	}
	switch (comBootTransitionStep) {
	case 0:
		input[0].value = control->baudRate;
		result = bbcmdExecute(0, 5, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto fail;
		comBootTransitionStep = 5;
		return 1;
	case 1:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(comBootTimer);
			comBootTimer = 0;
			comBootSysconPending = 1;
			result = bbmcSendSysconCommand(control, comBootSysconReply,
				&control->transitionPacket, 6, 2, 0);
			if (result < 0)
				goto syscon_failed;
			comBootTransitionStep = 7;
			return 1;
		case 73:
			result = message->body.commandResult;
			if (result >= 0)
				return 1;
			if (result != (int)0x8033501a)
				goto fail;
			result = bbmcSendSimpleMessage(control, 67);
			if (comBootTransitionStep != 10)
				return 1;
			goto finish;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			control->transitionStep = 1;
			goto timed_out;
		default:
			return 1;
		}
	case 3:
		switch (message->command) {
		case 67:
			bbmcTimerCancelHandler(comBootTimer);
			comBootTimer = 0;
			comBootSysconPending = 1;
			result = bbmcSendSysconCommand(control, comBootSysconReply,
				&control->transitionPacket, 6, 2, 0);
			if (result < 0)
				goto syscon_failed;
			comBootTransitionStep = 8;
			return 1;
		case 75:
			if (message->body.commandResult >= 0)
				return 1;
			comBootTransitionStep = 10;
			result = 0;
			goto finish;
		case 187:
			if (message->body.timedOutCommand != 67)
				return 1;
			control->transitionStep = 3;
timed_out:
			bbmcEnterCS(&control->mutex);
			control->references = 0;
			result = (int)0x80330001;
			bbmcLeaveCS(&control->mutex);
			comBootTransitionStep = 10;
			goto finish;
		default:
			return 1;
		}
	case 5:
		if (message->command != 58)
			return 1;
		result = message->body.commandResult;
		if (result != 0) {
			control->transitionStep = 5;
			comBootTransitionStep = 10;
			goto finish;
		}
		input[0].value = 1;
		result = bbcmdExecute(0, 9, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto fail;
		comBootTransitionStep = 6;
		return 1;
	case 6:
		if (message->command != 59)
			return 1;
		if (message->body.commandResult != 0) {
			bbmcNotifyFeatureEvent(control, 0, 1);
			result = bbcmdExecute(0, 48, NULL, NULL,
				bbcmdManagerCompletion, control);
			if (result < 0)
				goto fail;
			result = bbcmdExecute(0, 6, NULL, NULL,
				bbcmdManagerCompletion, control);
			if (result < 0)
				goto fail;
			bbmcSendSimpleMessage(control, 22);
			comBootTransitionStep = 9;
			return 1;
		}
		input[0].value = 0;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto fail;
		timer = bbmcTimerSetHandler(60000000, comBootTimeoutHandler,
			control);
		result = timer;
		if (timer < 0)
			goto timer_failed;
		comBootTransitionStep = 1;
		comBootTimer = timer;
		return 1;
	case 7:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword != 4)
			goto bad_step;
		input[0].value = 2;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		bbmcEnterCS(&control->mutex);
		control->secondBootReply = message->body.sysconReplyHalfword;
		bbmcLeaveCS(&control->mutex);
		result = bbcmdExecute(0, 37, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto fail;
		timer = bbmcTimerSetHandler(60000000, comBootTimeoutHandler,
			control);
		result = timer;
		if (timer < 0)
			goto timer_failed;
		comBootTransitionStep = 3;
		comBootTimer = timer;
		return 1;
	case 8:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword == 3) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 9;
			control->secondBootReply = message->body.sysconReplyHalfword;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control,
				(control->flags & 1) ? 22 : 21);
		} else {
			result = (int)0x80330001;
		}
		goto fail;
	case 9:
		if (message->command != 57)
			return 1;
		result = bbmcSendSimpleMessage(control, 22);
		goto fail;
	default:
		goto bad_step;
	}
syscon_failed:
	comBootSysconPending = 0;
	comBootTransitionStep = 10;
	goto finish;
timer_failed:
	step = comBootTransitionStep;
	comBootTransitionStep = 10;
	control->transitionStep = step;
	goto finish;
bad_step:
	result = (int)0x80330001;
fail:
	comBootTransitionStep = 10;
finish:
	bbmcCancelSysconCommand(&control->transitionPacket, &comBootSysconPending);
	comBootTransitionStep = 0;
	return result;
}

int stmComBoot_Standby(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	BbcmdArgument input[7] = {0};
	BbcmdArgument output[6] = {0};
	BbcmdArgument readInput[7];
	BbcmdArgument readOutput[6];
	int result = 0;
	int status;

	if (flags & 4) {
		bbmcEnterCS(&control->mutex);
		if (control->suspendState == 1) {
			control->suspendState = 2;
			control->suspendStep = comBootTransitionStep;
			result = bbmcSendSimpleMessage(control, 42);
		}
		bbmcLeaveCS(&control->mutex);
		goto finish;
	}
	if (flags & 2) {
		bbmcEnterCS(&control->mutex);
		control->currentState = 1;
		bbmcLeaveCS(&control->mutex);
		result = bbmcSendSimpleMessage(control, 22);
		goto finish;
	}
	if ((flags & 8) && comBootTransitionStep <= 1) {
		if (comBootTransitionStep != 1) {
			comBootTransitionStep = 5;
			bbmcEnterCS(&control->mutex);
			control->currentState = 1;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control, 22);
			if (comBootTransitionStep != 5)
				return 1;
			goto finish;
		}
		result = bbcmdExecute(0, 6, NULL, NULL, bbcmdManagerCompletion,
			control);
		goto stopping;
	}
	switch (comBootTransitionStep) {
	case 0:
		input[0].value = control->baudRate;
		result = bbcmdExecute(0, 5, input, NULL, bbcmdManagerCompletion,
			control);
		if (result < 0) {
			control->transitionStep = comBootTransitionStep;
			goto finish;
		}
		comBootTransitionStep = 1;
		return 1;
	case 1:
		if (message->command != 58)
			return 1;
		result = message->body.commandResult;
		if (result != 0) {
			control->transitionStep = 1;
			goto finish;
		}
		input[0].value = 63;
		output[0].pointer = &control->countedBytes;
		result = bbcmdExecute(0, 9, input, output, bbcmdManagerCompletion,
			control);
		if (result < 0)
			goto finish;
		comBootTransitionStep = 2;
		return 1;
	case 2:
		if (message->command != 59)
			return 1;
		if (message->body.commandResult != 0) {
			bbmcSaveTransitionState(control, message->body.commandResult, 2);
			bbmcNotifyFeatureEvent(control, 0, 1);
			goto stop;
		}
		readOutput[1].value = 0;
		readOutput[2].value = 0;
		readOutput[3].value = 0;
		readInput[1].value = 0;
		readInput[2].value = 0;
		readInput[3].value = 0;
		readInput[4].value = 0;
		readInput[5].value = 0;
		readInput[6].value = 0;
		bbmcEnterCS(&control->mutex);
		memset(&control->deviceInformation, 0,
			sizeof(control->deviceInformation));
		memset(&control->smscAddress, 0, sizeof(control->smscAddress));
		memset(&control->profile, 0, sizeof(control->profile));
		readInput[0].value = 2;
		readOutput[0].pointer = &control->deviceInformation;
		bbmcLeaveCS(&control->mutex);
		result = bbcmdExecute(0, 14, readInput, readOutput,
			bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		comBootTransitionStep = 3;
		return 1;
	case 3:
		if (message->command != 70)
			return 1;
		status = message->body.commandResult;
		if (status != 0) {
			bbmcSaveTransitionState(control, status, 3);
stop:
			result = bbcmdExecute(0, 48, NULL, NULL,
				bbcmdManagerCompletion, control);
			if (result < 0)
				goto finish;
			result = bbcmdExecute(0, 6, NULL, NULL,
				bbcmdManagerCompletion, control);
			goto stopping;
		}
		result = bbmcInitializeGps();
		if (result >= 0) {
			bbmcEnterCS(&control->mutex);
			control->currentState = 2;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control,
				(control->flags & 1) ? 22 : 21);
		}
		goto finish;
	case 4:
		if (message->command != 57)
			return 1;
		result = bbmcSendSimpleMessage(control, 22);
		if (result >= 0)
			result = (int)0x80330001;
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}
stopping:
	if (result < 0)
		goto finish;
	comBootTransitionStep = 4;
	return 1;
finish:
	comBootTransitionStep = 0;
	return result;
}

/* ComBoot -> Initial: power the modem off through the syscon and wait for both
 * the power-off event and the syscon reply (or the timeout), then clear the
 * modem state. Returns 1 while in progress, 0 when done. */
static inline int comBootPowerOff(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = comBootTransitionStep;
	SceUInt32 profile;
	unsigned int mode;
	int timer;

	if (step == 1) {
		if (message->command != 52)
			return 1;
		profile = control->defaultProfile;
		bbmcEnterCS(&control->mutex);
		mode = profile == 1 ? 0x80 : 0;
		memset(&control->networkInfo, 0, sizeof(control->networkInfo));
		control->networkInfo.signalLevel = -1;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, 4);
		comBootSysconPending = 1;
		if (bbmcSendSysconPowerCommand(control, comBootPowerReply,
				&control->transitionPacket, mode) < 0) {
			comBootTransitionStep = 3;
			comBootSysconPending = 0;
			goto finish;
		}
		comBootTransitionStep = 2;
		if ((control->flags & 2) || (SceInt16)control->secondBootReply < 0)
			return 1;
		timer = bbmcTimerSetHandler(60000000, comBootPowerOffTimeoutHandler,
			control);
		if (timer < 0) {
			step = comBootTransitionStep;
			comBootTransitionStep = 3;
			control->transitionStep = step;
			goto finish;
		}
		comBootTimer = timer;
		if (comBootTransitionStep != 3)
			return 1;
		if (timer != 0)
			goto finish;
		goto restore_flags;
	}
	if (step == 0) {
		if (bbmcSendSimpleMessage(control, 50) >= 0) {
			comBootTransitionStep = 1;
			return 1;
		}
	} else if (step == 2) {
		switch (message->command) {
		case 9:
			comBootPowerOffEvents |= 1;
			bbmcTimerCancelHandler(comBootTimer);
			comBootTimer = 0;
			break;
		case 39:
			comBootPowerOffEvents |= 2;
			if ((control->flags & 2) ||
			    (SceInt16)control->secondBootReply < 0) {
				bbmcTimerCancelHandler(comBootTimer);
				comBootTransitionStep = 3;
				comBootTimer = 0;
			}
			break;
		case 187:
			if (message->body.timedOutCommand == 9)
				comBootTransitionStep = 3;
			break;
		default:
			return 1;
		}
		if ((comBootPowerOffEvents & 3) == 3) {
			comBootTransitionStep = 3;
			goto restore_flags;
		}
		if (comBootTransitionStep == 3)
			goto restore_flags;
		return 1;
	}
	comBootTransitionStep = 3;
	goto finish;
restore_flags:
	control->flags &= ((control->flags & 6) == 6 &&
		control->savedTargetState == 0) ? ~8u : ~0xau;
finish:
	bbmcEnterCS(&control->mutex);
	control->currentState = 0;
	control->references = 0;
	control->secondBootReply = 0;
	control->sysconPowerRequestPending = 0;
	bbmcLeaveCS(&control->mutex);
	bbmcNotifyFeatureEvent(control, 0, 0x20);
	bbmcUnregisterNetworkInterface();
	control->decodedReply = 0;
	control->configurationReplyValid = 0;
	control->replyVersionValid = 0;
	comBootPowerOffEvents = 0;
	bbmcCancelSysconCommand(&control->transitionPacket, &comBootSysconPending);
	comBootTransitionStep = 0;
	if (control->flags & 4)
		control->suspendState = 3;
	return 0;
}

int bbmcPowerOffStateHandler(BbmcControl *control, BbmcMessage *message)
{
	return comBootPowerOff(control, message);
}

int suspendSteadyStandby(BbmcControl *control, BbmcMessage *message)
{
	int result;

	(void)message;
	control->suspendError = (int)0x80010600;
	bbmcEnterCS(&control->mutex);
	control->suspendStep = 5;
	control->currentState = 1;
	if (bbmcSendSimpleMessage(control, 42) >= 0) {
		result = 2;
	} else {
		result = 0;
		control->flags |= 2;
	}
	bbmcLeaveCS(&control->mutex);
	return result;
}

static const SceKernelDebugInfo dbg_suspendDuringFromComBootToStandby_L1664
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/com_boot.c",
	.line = 1664,
	.func = "suspendDuringFromComBootToStandby",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromComBootToStandby_L1664Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromComBootToStandby_L1664,
	"state_machine/com_boot.c", 1664, "suspendDuringFromComBootToStandby",
};

static const SceKernelDebugInfo dbg_suspendDuringFromComBootToStandby_L1679
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/com_boot.c",
	.line = 1679,
	.func = "suspendDuringFromComBootToStandby",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromComBootToStandby_L1679Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromComBootToStandby_L1679,
	"state_machine/com_boot.c", 1679, "suspendDuringFromComBootToStandby",
};

int suspendDuringFromComBootToStandby(BbmcControl *control,
				     BbmcMessage *message)
{
	BbcmdArgument input[7] = {0};
	SceUInt32 step = comBootSuspendStep;
	SceUInt32 suspendStep;
	int result;

	control->suspendError = (int)(step | 0x80010400);
	switch (step) {
	case 0:
	case 1:
		suspendStep = control->suspendStep;
		if (suspendStep > 2) {
			if (suspendStep == 3)
				goto requestZero;
			if (suspendStep == 5) {
				result = bbmcFinalizeGps();
				if (result < 0 && result != (int)0x80390005)
					SceDebugForDriver_391B5B74(
						&dbg_suspendDuringFromComBootToStandby_L1664,
						__builtin_return_address(0));
				input[0].value = 0;
				goto requestZero;
			}
			SceDebugForDriver_391B5B74(
				&dbg_suspendDuringFromComBootToStandby_L1679,
				__builtin_return_address(0));
		}
		switch (message->command) {
		case 43:
			comBootSuspendStep = 1;
			return 1;
		case 59:
			goto requestZero;
		case 58:
			goto requestSix;
		default:
			return 1;
		}
	case 2:
		if (message->command != 55)
			return 1;
		result = bbcmdExecute(0, 10, NULL, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		comBootSuspendStep = 3;
		return 1;
	case 3:
		if (message->command != 56 || message->body.commandResult != 0)
			return 1;
requestSix:
		result = bbcmdExecute(0, 6, NULL, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		comBootSuspendStep = 4;
		return 1;
	case 4:
		if (message->command != 57)
			return 1;
		bbmcPrepareUsbShutdown(control);
		bbmcEnterCS(&control->mutex);
		control->targetState = 1;
		bbmcLeaveCS(&control->mutex);
		result = bbmcSendSimpleMessage(control, 42);
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}

requestZero:
	result = bbcmdExecute(0, 0, input, NULL,
			      bbcmdManagerCompletion, control);
	if (result >= 0) {
		comBootSuspendStep = 2;
		return 1;
	}
	step = comBootSuspendStep;
	comBootTransitionStep = 5;
	if (step <= 4)
		return 1;
finish:
	control->suspendError = (int)0x80010405;
	if (result < 0) {
		SceUInt32 flags = control->flags;

		comBootSuspendStep = 0;
		control->flags = flags | 2;
		return 0;
	}
	comBootSuspendStep = 0;
	return 2;
}

int suspendDuringFromComBootToInitial(BbmcControl *control,
				      BbmcMessage *message)
{
	control->suspendError = (int)0x80010500;
	return comBootPowerOff(control, message);
}
