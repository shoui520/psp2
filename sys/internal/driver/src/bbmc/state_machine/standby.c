#include <sysmem/debug.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_gps.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int (*seqSuspendSelectHandler(int currentState, int targetState))(BbmcControl *,
	BbmcMessage *);

SceUInt32 standbyTransitionStep;
SceUInt32 standbySuspendStep;

/* name unknown: forget the SIM's information after it was removed or locked */
static inline void standbyClearSimInformation(BbmcControl *control)
{
	memset(&control->smscAddress, 0, sizeof(control->smscAddress));
	memset(&control->profile, 0, sizeof(control->profile));
	memset(control->deviceInformation.msisdn, 0, 36);
	memset(control->deviceInformation.iccid, 0, 24);
	memset(control->deviceInformation.imsi, 0, 16);
	memset(&control->deviceInformation.homeNetwork, 0,
	       sizeof(control->deviceInformation.homeNetwork));
	control->deviceInformation.messageProtocol = 0;
	control->deviceInformation.prlVersion = 0;
	control->pinStatus.pin1VerifyRetries = 0;
	control->pinStatus.pin1UnblockRetries = 0;
	control->pinStatus.pin1Status = 3;
	control->currentState = 4;
	control->targetState = 4;
}

/* name unknown: the state handler of the transition from Standby to Authorized */
int stmStandby_Authorized(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument output[13] = {{0}};
	SceUInt32 flags = control->flags;
	SceUInt32 command, step, busy, sysconRefresh, simRefresh;
	unsigned int send;
	int result;

	if (flags & 0xa) {
		control->currentState = 2;
		result = bbmcSendSimpleMessage(control, 22);
		standbyTransitionStep = 5;
		goto finish;
	}
	if (flags & 4) {
		bbmcEnterCS(&control->mutex);
		if (control->suspendState == 1) {
			step = standbyTransitionStep;
			if (message->command == 35 && step == 2) {
				if (message->body.pinMessage.status.result != 0) {
					standbyTransitionStep = 0;
					step = 0;
				} else {
					control->currentState = 3;
				}
			}
			control->suspendState = 2;
			control->suspendStep = step;
			result = bbmcSendSimpleMessage(control, 42);
			control->suspendMessageHandler = seqSuspendSelectHandler(
				control->currentState, control->targetState);
		} else {
			result = 0;
		}
		bbmcLeaveCS(&control->mutex);
		standbyTransitionStep = 5;
		goto finish;
	}

	command = message->command;
	if (command == 36 || command == 10) {
		standbyTransitionStep = 0;
		control->currentState = 2;
		control->targetState = 3;
	}
	if (command == 11) {
		standbyTransitionStep = 0;
		goto checkSim;
	}
	switch (standbyTransitionStep) {
	case 0:
		if (command == 10 || command == 21 || command == 36)
			goto checkSim;
		return 1;
	case 1:
		if (command != 5)
			return 1;
		if (message->body.commandResult & 0x20) {
			result = bbmcSendSimpleMessage(control, 34);
			if (result >= 0) {
				standbyTransitionStep = 2;
				return 1;
			}
			goto failed;
		}
		standbyTransitionStep = 3;
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			goto failed;
		break;
	case 2:
		if (command != 35)
			return 1;
		result = message->body.pinMessage.status.result;
		if (result < 0) {
			control->transitionStep = 2;
			standbyTransitionStep = 5;
			goto finish;
		}
		if (control->simRefreshPending == 1)
			goto readResponses;
		switch (result) {
		case 0:
			bbmcEnterCS(&control->mutex);
			control->currentState = 3;
			bbmcLeaveCS(&control->mutex);
			break;
		case 1:
			bbmcEnterCS(&control->mutex);
			standbyClearSimInformation(control);
			bbmcLeaveCS(&control->mutex);
			break;
		case 2:
			bbmcNotifyFeatureEvent(control, 4, 0x1400000);
			bbmcEnterCS(&control->mutex);
			standbyClearSimInformation(control);
			bbmcLeaveCS(&control->mutex);
			break;
		case 3:
			bbmcNotifyFeatureEvent(control, 4, 2);
			goto readResponses;
		case 4:
			bbmcNotifyFeatureEvent(control, 4, 0x8000001);
			bbmcEnterCS(&control->mutex);
			standbyClearSimInformation(control);
			bbmcLeaveCS(&control->mutex);
			break;
		}
		send = 21;
		standbyTransitionStep = 5;
		goto sendState;
	readResponses:
		send = 20;
		standbyTransitionStep = 3;
	sendState:
		result = bbmcSendSimpleMessage(control, send);
		break;
	case 3:
		if (command != 20)
			return 1;
		bbmcEnterCS(&control->mutex);
		memset(control->indexedResponses, 0, sizeof(control->indexedResponses));
		output[0].pointer = control->indexedResponses;
		bbmcLeaveCS(&control->mutex);
		standbyTransitionStep = 4;
		result = bbcmdExecute(0, 49, NULL, output, bbcmdManagerCompletion, control);
		if (result >= 0)
			break;
		standbyTransitionStep = 5;
		goto finish;
	case 4:
		if (command != 181)
			return 1;
		result = message->body.commandResult;
		if (result == 0) {
			bbmcNotifyFeatureEvent(control, 4, 0x400000);
			bbmcEnterCS(&control->mutex);
			standbyClearSimInformation(control);
			bbmcLeaveCS(&control->mutex);
		}
		standbyTransitionStep = 5;
		goto finish;
	default:
		result = (int)0x80330001;
		standbyTransitionStep = 5;
		goto finish;
	}
	goto check;

checkSim:
	bbmcEnterCS(&control->mutex);
	busy = control->simCommandBusy;
	sysconRefresh = control->sysconRefreshPending;
	simRefresh = control->simRefreshPending;
	control->simRefreshPending = 0;
	control->simCommandBusy = busy == 1 ? 0 : busy;
	bbmcLeaveCS(&control->mutex);
	if (sysconRefresh == 1) {
		bbmcNotifyFeatureEvent(control, 4, 0x8000001);
		bbmcEnterCS(&control->mutex);
		standbyClearSimInformation(control);
		bbmcLeaveCS(&control->mutex);
		send = 21;
		standbyTransitionStep = 5;
	} else if (simRefresh == 0 && busy == 1) {
		send = 34;
		standbyTransitionStep = 2;
	} else {
		send = 4;
		standbyTransitionStep = 1;
	}
	result = bbmcSendSimpleMessage(control, send);
	if (result < 0)
		goto failed;
check:
	if (standbyTransitionStep != 5)
		return 1;
	goto finish;

failed:
	step = standbyTransitionStep;
	standbyTransitionStep = 5;
	control->transitionStep = step;
finish:
	bbmcEnterCS(&control->mutex);
	control->simRefreshPending = 0;
	control->sysconRefreshPending = 0;
	bbmcLeaveCS(&control->mutex);
	standbyTransitionStep = 0;
	return result;
}

static const SceKernelDebugInfo dbg_stmStandby_ComBoot_L547
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/standby.c",
	.line = 547,
	.func = "stmStandby_ComBoot",
#endif
};

static const SceKernelDebugInfoSource dbg_stmStandby_ComBoot_L547Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmStandby_ComBoot_L547, "state_machine/standby.c", 547, "stmStandby_ComBoot",
};

static const SceKernelDebugInfo dbg_stmStandby_ComBoot_L559
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/standby.c",
	.line = 559,
	.func = "stmStandby_ComBoot",
#endif
};

static const SceKernelDebugInfoSource dbg_stmStandby_ComBoot_L559Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmStandby_ComBoot_L559, "state_machine/standby.c", 559, "stmStandby_ComBoot",
};

int stmStandby_ComBoot(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7] = {{0}};
	BbmcMessage *reply;
	SceUInt32 flags;
	int result;

	switch (standbyTransitionStep) {
	case 0:
		result = bbcmdExecute(0, 0, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		standbyTransitionStep = 1;
		return 1;
	case 1:
		if (message->command != 55)
			return 1;
		if (bbmcFinalizeGps() < 0)
			SceDebugForDriver_391B5B74(&dbg_stmStandby_ComBoot_L547,
						   __builtin_return_address(0));
		if (!(control->flags & 2)) {
			result = bbcmdExecute(0, 10, NULL, NULL, bbcmdManagerCompletion, control);
			if (result < 0)
				goto finish;
			standbyTransitionStep = 2;
			return 1;
		}
		if (bbcmdExecute(0, 48, NULL, NULL, NULL, control) < 0)
			SceDebugForDriver_391B5B74(&dbg_stmStandby_ComBoot_L559,
						   __builtin_return_address(0));
		reply = msgAllocMsgPacket(control, 56);
		reply->body.commandResult = 0;
		result = msgSendMsgPacket(control, reply);
		if (result >= 0) {
			standbyTransitionStep = 2;
			return 1;
		}
		break;
	case 2:
		if (message->command != 56)
			return 1;
		result = message->body.commandResult;
		if (result != 0)
			goto finish;
		result = bbcmdExecute(0, 6, NULL, NULL, bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		standbyTransitionStep = 3;
		return 1;
	case 3:
		if (message->command != 57)
			return 1;
		flags = control->flags;
		control->currentState = 1;
		standbyTransitionStep = 4;
		if (flags & 4) {
			result = 0;
			goto finish;
		}
		result = bbmcSendSimpleMessage(control, 22);
		break;
	default:
		result = (int)0x80330001;
		goto finish;
	}
	if (standbyTransitionStep != 4)
		return 1;
finish:
	standbyTransitionStep = 0;
	return result;
}

static const SceKernelDebugInfo dbg_suspendDuringFromStandbyToAuthorized_L744
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/standby.c",
	.line = 744,
	.func = "suspendDuringFromStandbyToAuthorized",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromStandbyToAuthorized_L744Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromStandbyToAuthorized_L744,
	"state_machine/standby.c", 744, "suspendDuringFromStandbyToAuthorized",
};

static const SceKernelDebugInfo dbg_suspendDuringFromStandbyToAuthorized_L786
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/standby.c",
	.line = 786,
	.func = "suspendDuringFromStandbyToAuthorized",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromStandbyToAuthorized_L786Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromStandbyToAuthorized_L786,
	"state_machine/standby.c", 786, "suspendDuringFromStandbyToAuthorized",
};

int suspendDuringFromStandbyToAuthorized(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = standbySuspendStep;
	int result;

	control->suspendError = (int)(step | 0x80010700);
	switch (step) {
	case 0:
		switch (control->suspendStep) {
		case 0:
		case 1:
		case 3:
		case 4:
			goto restore;
		case 2:
			if (message->command != 35) {
				standbySuspendStep = 1;
				return 1;
			}
			goto reply;
		default:
			SceDebugForDriver_391B5B74(&dbg_suspendDuringFromStandbyToAuthorized_L744,
						   __builtin_return_address(0));
		}
	case 1:
		if (message->command != 35)
			return 1;
		goto reply;
	default:
		SceDebugForDriver_391B5B74(&dbg_suspendDuringFromStandbyToAuthorized_L786,
					   __builtin_return_address(0));
	}

reply:
	if (message->body.pinMessage.status.result == 0) {
		bbmcEnterCS(&control->mutex);
		control->pinStatus.pin1Status = 4;
		control->pinStatus.pin1VerifyRetries = message->body.pinMessage.detail.retries;
		control->pinStatus.pin1UnblockRetries = 10;
		control->currentState = 3;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 4, 0x100);
		result = bbmcSendSimpleMessage(control, 42);
	} else {
restore:
		bbmcEnterCS(&control->mutex);
		control->targetState = 2;
		bbmcLeaveCS(&control->mutex);
		result = bbmcSendSimpleMessage(control, 42);
	}
	standbySuspendStep = 0;
	if (result >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}

int suspendDuringFromStandbyToComBoot(BbmcControl *control, BbmcMessage *message)
{
	control->suspendError = (int)0x80010800;
	bbmcEnterCS(&control->mutex);
	control->currentState = 1;
	control->targetState = 1;
	bbmcLeaveCS(&control->mutex);
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}

int suspendSteadyTestMode(BbmcControl *control, BbmcMessage *message)
{
	control->suspendError = (int)0x80010c00;
	bbmcEnterCS(&control->mutex);
	control->currentState = 2;
	control->targetState = 2;
	bbmcLeaveCS(&control->mutex);
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		return 2;
	control->flags |= 2;
	return 0;
}
