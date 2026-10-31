#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
void bbmcSaveTransitionState(BbmcControl *control, SceInt32 status,
	SceUInt32 step);
SceUInt32 bbmcSelectOperationMode(BbmcControl *control, SceUInt32 operationMode);
SceInt32 udSelectBootTarget(BbmcControl *control, SceInt32 targetState);

int stmInitial_ComBoot(BbmcControl *control, BbmcMessage *message);
int stmComBoot_TestMode(BbmcControl *control, BbmcMessage *message);
int stmComBoot_Mode9(BbmcControl *control, BbmcMessage *message);
int stmComBoot_Standby(BbmcControl *control, BbmcMessage *message);
int bbmcPowerOffStateHandler(BbmcControl *control, BbmcMessage *message);
int stmStandby_ComBoot(BbmcControl *control, BbmcMessage *message);
int stmStandby_Authorized(BbmcControl *control, BbmcMessage *message);
int stmAuthorized_Standby(BbmcControl *control, BbmcMessage *message);
int stmAuthorized_Activated(BbmcControl *control, BbmcMessage *message);
int stmActivated_LowPowerMode(BbmcControl *control, BbmcMessage *message);
int stmActivated_Connected(BbmcControl *control, BbmcMessage *message);
int stmActivated_Authorized(BbmcControl *control, BbmcMessage *message);
int stmLowPowerMode_Connected(BbmcControl *control, BbmcMessage *message);
int stmLowPowerMode_Activated(BbmcControl *control, BbmcMessage *message);
int stmConnected_LowPowerMode(BbmcControl *control, BbmcMessage *message);
int stmConnected_TestMode(BbmcControl *control, BbmcMessage *message);
int stmTestMode_Authorized(BbmcControl *control, BbmcMessage *message);
int stmConnected_Activated(BbmcControl *control, BbmcMessage *message);
int suspendSteadyTestMode(BbmcControl *control, BbmcMessage *message);

int testModeSysconPending;
SceUInt32 testModeStep;
SceUID testModeTimer;
SceUInt32 testModeEvents;
static SceInt32 stmReportedState = -1;

int testModeSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	if (packet->request.command[0] == 0x8d &&
	    packet->request.command[1] == 8) {
		message = msgAllocMsgPacket(control, 39);
		if (message)
			msgSendMsgPacket(control, message);
	}
	testModeSysconPending = 0;
	return 0;
}

void testModeTimeoutHandler(void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;

	if (testModeStep != 5)
		return;
	message = msgAllocMsgPacket(control, 187);
	if (!message)
		return;
	message->body.timedOutCommand = 9;
	msgSendMsgPacket(control, message);
}

static const SceKernelDebugInfo dbg_stmTestMode_Initial_L205
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/test_mode.c",
	.line = 205,
	.func = "stmTestMode_Initial",
#endif
};

static const SceKernelDebugInfoSource dbg_stmTestMode_Initial_L205Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmTestMode_Initial_L205, "state_machine/test_mode.c", 205, "stmTestMode_Initial",
};

#if FW_DEBUG
static const SceKernelDebugInfo dbg_stmTestMode_Initial_L292
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "state_machine/test_mode.c",
	.line = 292,
	.func = "stmTestMode_Initial",
};

static const SceKernelDebugInfoSource dbg_stmTestMode_Initial_L292Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmTestMode_Initial_L292, "state_machine/test_mode.c", 292, "stmTestMode_Initial",
};
#endif

int stmTestMode_Initial(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7] = {{0}};
	SceUInt32 command, events, flags;
	SceUInt16 bootReply;
	int result;

	if ((control->flags & 0xa) && testModeStep == 0 &&
	    bbmcSendSimpleMessage(control, 22) < 0)
		goto failed;

	switch (testModeStep) {
	case 0:
		if (message->command != 22)
			goto check;
		input[0].value = 0;
		if (bbcmdExecute(0, 0, input, NULL, bbcmdManagerCompletion, control) < 0)
			goto failed;
		testModeStep = 1;
		return 1;
	case 1:
		if (message->command != 55)
			goto check;
		bootReply = control->secondBootReply;
		if (bootReply == 1 || bootReply == 6)
			goto operationMode;
		flags = control->flags;
		if (flags & 8) {
			if (bbcmdExecute(0, 10, NULL, NULL, bbcmdManagerCompletion, control) < 0)
				goto check;
			testModeStep = 2;
			return 1;
		}
		if (!(flags & 2))
			goto check;
		if (bbcmdExecute(0, 48, NULL, NULL, NULL, control) < 0)
			SceDebugForDriver_391B5B74(&dbg_stmTestMode_Initial_L205,
						   __builtin_return_address(0));
		goto powerDown;
	case 2:
		if (message->command != 56)
			goto check;
		result = message->body.commandResult;
		if (result != 0)
			goto result;
	powerDown:
		if (bbcmdExecute(0, 6, NULL, NULL, bbcmdManagerCompletion, control) < 0)
			goto failed;
		testModeStep = 3;
		return 1;
	case 3:
		if (message->command != 57)
			goto check;
		result = message->body.commandResult;
		if (result != 0) {
	result:
			if (result >= 0)
				goto check;
			goto failed;
		}
	operationMode:
		if (bbmcSendSimpleMessage(control, 50) < 0)
			goto failed;
		testModeStep = 4;
		return 1;
	case 4:
		if (message->command != 52)
			goto check;
		testModeSysconPending = 1;
		if (bbmcSendSysconPowerCommand(control, testModeSysconReply,
					       &control->transitionPacket,
					       control->defaultProfile == 1 ? 128 : 0) < 0) {
			testModeSysconPending = 0;
			goto failed;
		}
		result = bbmcTimerSetHandler(60000000, testModeTimeoutHandler, control);
		if (result >= 0) {
			testModeTimer = result;
			testModeEvents = 0;
			testModeStep = 5;
			return 1;
		}
		bbmcSaveTransitionState(control, result, testModeStep);
		testModeStep = 6;
#if FW_DEBUG
		sceKernelAssert(0, &dbg_stmTestMode_Initial_L292, __builtin_return_address(0));
#endif
		goto failed;
	case 5:
		command = message->command;
		if (command == 39 || command == 9) {
			if (command == 9)
				events = testModeEvents |= 1;
			else
				events = testModeEvents |= 2;
		} else {
			if (command != 187 || message->body.timedOutCommand != 9)
				goto check;
			testModeTimer = 0;
			events = testModeEvents |= 3;
		}
		if (events == 3 || ((control->flags & 2) && events != 0)) {
			if (testModeTimer) {
				bbmcTimerCancelHandler(testModeTimer);
				testModeTimer = 0;
			}
			testModeStep = 6;
			bbmcEnterCS(&control->mutex);
			flags = control->flags;
			control->currentState = 0;
			control->references = 0;
			control->flags = flags & ~0xau;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, 0, 0x20);
		}
		goto check;
	default:
		goto failed;
	}

check:
	if (testModeStep != 6)
		return 1;
	goto finish;
failed:
	testModeStep = 6;
finish:
	bbmcCancelSysconCommand(&control->transitionPacket, &testModeSysconPending);
	testModeStep = 0;
	return 0;
}

/*
 * name unknown: picks the target state a message asks for, then runs the handler of
 * the transition from the current state to it
 */
int stmProcessMessage(BbmcControl *control, BbmcMessage *message)
{
	int (*handler)(BbmcControl *, BbmcMessage *);
	SceUInt32 flags = control->flags;
	SceUInt32 command = message->command;
	SceInt32 current, target;
	int result;

	if (flags & 1) {
		if (command == 11 || command == 36)
			goto leaveService;
		goto toInitial;
	}
	if (command == 3)
		goto toInitial;
	if (command == 11 || command == 36)
		goto leaveService;
	if (command == 19 || command == 22)
		goto toInitial;
	if (flags & 4)
		goto run;
	switch (control->currentState) {
	case 0:
		if (command == 18)
			control->targetState = 1;
		break;
	case 1:
		if (command != 21)
			break;
		if (control->secondBootReply == 5) {
			control->targetState = 8;
			break;
		}
		switch (control->firstBootReply) {
		case 0:
			target = udSelectBootTarget(control, 2);
			control->targetState = target;
			if (target != 8) {
				target = bbmcSelectOperationMode(control, 2);
				control->targetState = target;
				if (target != 8)
					break;
			}
			control->secondBootReply = 5;
			break;
		case 1:
		case 2:
		case 5:
		case 6:
		case 7:
		case 8:
			control->targetState = 8;
			break;
		case 3:
			control->targetState = 9;
			break;
		case 4:
			control->targetState = 2;
			break;
		}
		break;
	case 2:
		if (command == 21)
			control->targetState = 3;
		break;
	case 3:
		if (command == 21)
			control->targetState = 5;
		break;
	case 4:
		if (command == 10)
			control->targetState = 2;
		break;
	case 5:
		if (command != 21)
			break;
		if (control->firstBootReply == 0)
			control->targetState = 6;
		else if (control->firstBootReply == 4)
			control->targetState = control->powerEventActive ||
				control->recentPowerEvent ? 6 : 7;
		break;
	case 6:
		if (command == 30)
			control->targetState = 7;
		break;
	case 7:
		if ((command & ~2u) == 29 || command == 67)
			control->targetState = 6;
		break;
	case 8:
	case 9:
		break;
	default:
		return (int)0x80330001;
	}
	goto run;

leaveService:
	if (control->currentState > 2)
		control->targetState = 2;
	goto flagged;
toInitial:
	control->targetState = 0;
flagged:
	if (command == 19)
		control->flags = flags | 8;
	else if (command == 3 && control->currentState > 0)
		control->flags = flags | 2;

run:
	handler = control->stateHandler;
	if (!handler) {
		current = control->currentState;
		target = control->targetState;
		if (current == target)
			goto report;
		switch (current) {
		case 0:
			if (target <= 0)
				goto none;
			handler = stmInitial_ComBoot;
			break;
		case 1:
			if (target == 8)
				handler = stmComBoot_TestMode;
			else if (target == 9)
				handler = stmComBoot_Mode9;
			else if (target > 1)
				handler = stmComBoot_Standby;
			else if (target == 1)
				goto none;
			else
				handler = bbmcPowerOffStateHandler;
			break;
		case 2:
			handler = target > 2 ? stmStandby_Authorized : stmStandby_ComBoot;
			break;
		case 3:
			if (target > 3)
				handler = stmAuthorized_Activated;
			else if (target == 2)
				handler = stmStandby_Authorized;
			else
				handler = stmAuthorized_Standby;
			break;
		case 4:
			if (target == 2)
				handler = stmStandby_Authorized;
			else if (target > 3)
				goto none;
			else
				handler = stmTestMode_Authorized;
			break;
		case 5:
			if (target == 6)
				handler = stmActivated_LowPowerMode;
			else if (target == 7)
				handler = stmActivated_Connected;
			else if (target == 2)
				handler = stmStandby_Authorized;
			else if (target <= 4)
				handler = stmActivated_Authorized;
			else
				goto none;
			break;
		case 6:
			if (target == 7)
				handler = stmLowPowerMode_Connected;
			else if (target == 2)
				handler = stmStandby_Authorized;
			else if (target > 5)
				goto none;
			else
				handler = stmLowPowerMode_Activated;
			break;
		case 7:
			if (target == 2 || target == 6)
				handler = stmConnected_LowPowerMode;
			else if (target == 8)
				handler = stmConnected_TestMode;
			else if (target <= 6)
				handler = stmConnected_Activated;
			else
				goto none;
			break;
		case 8:
			if (target > 7)
				goto none;
			handler = stmTestMode_Initial;
			break;
		case 9:
			if (target > 8)
				goto none;
			handler = stmTestMode_Initial;
			break;
		default:
			goto none;
		}
		control->stateHandler = handler;
	}
	result = handler(control, message);
	if (result < 0) {
		bbmcEnterCS(&control->mutex);
		current = control->currentState;
		target = control->targetState;
		flags = control->flags | 2;
		control->transitionStatus = result;
		control->previousCurrentState = current;
		control->previousTargetState = target;
		control->lastError = result;
		control->flags = flags;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, 1);
		bbmcNotifyFeatureEvent(control, 7, 0x200);
	} else if (result == 0) {
		control->stateHandler = NULL;
	}
	current = control->currentState;
	goto reported;
none:
	result = 0;
report:
	current = control->currentState;
reported:
	if (stmReportedState != current) {
		stmReportedState = current;
		bbmcNotifyFeatureEvent(control, 0, 2);
		bbmcNotifyFeatureEvent(control, 7, 0x8000);
	}
	return result;
}

/* name unknown: the state handler of the transition from Test Mode to Authorized */
int stmTestMode_Authorized(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags = control->flags;
	int result;

	if (flags == 0)
		return 0;
	if (!(flags & 4))
		return stmAuthorized_Standby(control, message);
	if (control->suspendState != 1)
		return 0;
	bbmcEnterCS(&control->mutex);
	control->suspendStep = 0;
	control->suspendState = 2;
	result = bbmcSendSimpleMessage(control, 42);
	bbmcLeaveCS(&control->mutex);
	return result;
}

int suspendTestModeToAuthorized(BbmcControl *control, BbmcMessage *message)
{
	return suspendSteadyTestMode(control, message);
}

int suspendTestModeToStandby(BbmcControl *control, BbmcMessage *message)
{
	return suspendSteadyTestMode(control, message);
}
