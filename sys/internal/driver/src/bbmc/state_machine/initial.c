#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <string.h>
#include <stddef.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbmc_netif.h"
#include "../qmi_backend.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
void bbmcSaveTransitionState(BbmcControl *control, SceInt32 status,
	SceUInt32 step);

SceUInt32 initialComBootStep;
int initialSysconPending;
SceUID initialComBootTimer;
SceUInt32 initialSuspendStep;

typedef struct BbmcBootVersionRequirement {
	SceUInt32 enabled;
	SceUInt16 family;
	SceUInt16 build;
	SceUInt8 revision[3];
} BbmcBootVersionRequirement;

_Static_assert(sizeof(BbmcBootVersionRequirement) == 12,
	       "boot version requirement size");
_Static_assert(offsetof(BbmcBootVersionRequirement, family) == 4,
	       "boot version family offset");
_Static_assert(offsetof(BbmcBootVersionRequirement, build) == 6,
	       "boot version build offset");
_Static_assert(offsetof(BbmcBootVersionRequirement, revision) == 8,
	       "boot version revision offset");

#if !FW_DEBUG
static const SceUInt8 initialOperationModes[5] = {0, 1, 3, 4, 2};
#endif

static const BbmcBootVersionRequirement initialBootVersionRequirements[4] = {
	{1, 1500, 0, {0, 0, 0}},
	{1, 1600, 0, {0, 0, 0}},
	{1, 2077, 0, {0, 0, 1}},
	{1, 2085, 0, {0, 0, 5}},
};

#if FW_DEBUG
static const SceUInt8 initialOperationModes[5] = {0, 1, 3, 4, 2};

static const SceKernelDebugInfo dbg_stmInitial_ComBoot_L927
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "state_machine/initial.c",
	.line = 927,
	.func = "stmInitial_ComBoot",
};

static const SceKernelDebugInfoSource dbg_stmInitial_ComBoot_L927Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmInitial_ComBoot_L927, "state_machine/initial.c", 927, "stmInitial_ComBoot",
};

static const SceKernelDebugInfo dbg_stmInitial_ComBoot_L1259
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "state_machine/initial.c",
	.line = 1259,
	.func = "stmInitial_ComBoot",
};

static const SceKernelDebugInfoSource dbg_stmInitial_ComBoot_L1259Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_stmInitial_ComBoot_L1259, "state_machine/initial.c", 1259, "stmInitial_ComBoot",
};
#endif

static const SceKernelDebugInfo dbg__timeoutHandler_L675
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/initial.c",
	.line = 675,
	.func = "_timeoutHandler",
#endif
};

static const SceKernelDebugInfoSource dbg__timeoutHandler_L675Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__timeoutHandler_L675, "state_machine/initial.c", 675, "_timeoutHandler",
};

int initialSysconReply(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt32 step = initialComBootStep;
	SceUInt32 command;
	int result = 0;

	if (packet->request.command[0] == 0x8d) {
		if (packet->request.command[1] != 8)
			goto done;
		switch (step) {
		case 5:
			goto done;
		case 13:
		case 14:
			message = msgAllocMsgPacket(control, 39);
			if (message)
				goto send;
		default:
			goto done;
		}
	} else if (packet->request.command[0] == 0x80) {
		if (packet->request.command[1] != 0x0a)
			goto done;
		if (step == 10) {
			command = 23;
			goto halfword;
		} else if (step == 9) {
			command = 24;
		halfword:
			message = msgAllocMsgPacket(control, command);
			message->body.sysconReplyHalfword = packet->reply.data[0] |
				(packet->reply.data[1] << 8);
			goto send;
		} else if (step == 6) {
			message = msgAllocMsgPacket(control, 26);
			message->body.sysconReplyWords.words[0] = packet->reply.word[0];
			message->body.sysconReplyWords.words[1] = packet->reply.word[1];
			if (message)
				goto send;
		} else if (step == 7) {
			message = msgAllocMsgPacket(control, 27);
			message->body.sysconDecodedReply.replyBytes[0] = packet->reply.data[0];
			message->body.sysconDecodedReply.replyBytes[1] = packet->reply.data[1];
			message->body.sysconDecodedReply.decodedValue =
				(SceUInt32)packet->reply.data[3] |
				((SceUInt32)packet->reply.data[4] << 8) |
				((SceUInt32)packet->reply.data[5] << 16) |
				((SceUInt32)packet->reply.data[6] << 24);
			goto send;
		} else if (step == 8) {
			message = msgAllocMsgPacket(control, 28);
			message->body.sysconReplyByte = packet->reply.data[0];
			goto send;
		} else if (step == 2) {
			message = msgAllocMsgPacket(control, 25);
			message->body.sysconReplyBytes[0] = packet->reply.data[0];
			message->body.sysconReplyBytes[1] = packet->reply.data[1];
			goto send;
		}
	} else if (packet->request.command[0] == 0x81) {
		if (packet->request.command[1] != 0x0a)
			goto done;
		if (step == 4)
			message = msgAllocMsgPacket(control, 37);
		else if (step == 3)
			message = msgAllocMsgPacket(control, 38);
		else
			goto done;
		if (message)
			goto send;
	}
	goto done;

send:
	result = msgSendMsgPacket(control, message);
done:
	initialSysconPending = 0;
	return result;
}

void _timeoutHandler(void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;

	switch (initialComBootStep) {
	case 5:
		message = msgAllocMsgPacket(control, 187);
		if (!message)
			return;
		message->body.timedOutCommand = 8;
		break;
	case 11:
		message = msgAllocMsgPacket(control, 187);
		if (!message)
			return;
		message->body.timedOutCommand = 51;
		break;
	case 13:
	case 15:
		message = msgAllocMsgPacket(control, 187);
		if (!message)
			return;
		message->body.timedOutCommand = 9;
		break;
	default:
		return;
	}
	if (msgSendMsgPacket(control, message) < 0)
		SceDebugForDriver_391B5B74(&dbg__timeoutHandler_L675, __builtin_return_address(0));
}

int stmInitial_ComBoot(BbmcControl *control, BbmcMessage *message)
{
	const BbmcBootVersionRequirement *requirement;
	const SceUInt8 *reply;
	SceUInt8 *revision;
	SceUInt32 flags = control->flags;
	SceUInt32 command, step, rate, family, build, index;
	SceUInt16 bootReply;
	int result;

	if (flags & 4) {
		bbmcCancelSysconCommand(&control->transitionPacket, &initialSysconPending);
		bbmcEnterCS(&control->mutex);
		if (control->suspendState == 1) {
			control->suspendState = 2;
			control->suspendStep = initialComBootStep;
			bbmcSendSimpleMessage(control, 42);
		}
		bbmcLeaveCS(&control->mutex);
		initialComBootStep = 1;
		return 0;
	}

	if ((flags & 2) && initialComBootStep != 1) {
		bbmcEnterCS(&control->mutex);
		control->targetState = control->currentState;
		bbmcLeaveCS(&control->mutex);
		result = 0;
		initialComBootStep = 16;
		goto finish;
	}

	switch (initialComBootStep) {
	case 0:
		if (message->command != 18)
			return 1;
		if (SceSysrootForDriver_4373AC96() && SceSysconForDriver_99A254A9())
			initialComBootStep = 12;
		else
			initialComBootStep = 1;
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			initialComBootStep = 16;
		break;
	case 1:
		command = message->command;
		if (command != 18 && command != 20)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->secondBootReply = 0;
		control->flags = 0;
		control->registrationPending = 0;
		control->simCommandBusy = 1;
		bbmcLeaveCS(&control->mutex);
		initialSysconPending = 1;
		initialComBootStep = 2;
		result = bbmcSendSysconCommand(control, initialSysconReply,
					       &control->transitionPacket, 0, 2, 0);
		if (result < 0)
			goto failed;
		break;
	case 2:
		if (message->command != 25)
			return 1;
		reply = message->body.sysconReplyBytes;
		bbmcEnterCS(&control->mutex);
		if (reply[0] == 0 && reply[1] <= 1) {
			result = (int)0x80330001;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, reply[0], 0x810);
			initialComBootStep = 16;
			bbmcSaveTransitionState(control, (int)0x80330001, 16);
			break;
		}
		control->replyInterface = reply[0];
		control->uartMode = reply[1];
		bbmcLeaveCS(&control->mutex);
		initialComBootStep = 3;
		if (reply[1] > 1) {
			result = bbmcSendSimpleMessage(control, 38);
			if (result < 0)
				goto abort;
			break;
		}
		initialSysconPending = 1;
		result = bbmcSendSizedSysconCommand(control, initialSysconReply,
						    &control->transitionPacket, 1, 1, 2, 2, 48);
		if (result < 0)
			goto failed;
		break;
	case 3:
		if (message->command != 38)
			return 1;
		control->uartMode = 2;
		switch (control->baudRate) {
		case 19200:
			rate = 0;
			break;
		case 38400:
			rate = 1;
			break;
		case 57600:
			rate = 2;
			break;
		case 115200:
			rate = 3;
			break;
		default:
			result = (int)0x80330002;
			step = 3;
			goto saved;
		}
		initialSysconPending = 1;
		result = bbmcSendSizedSysconCommand(control, initialSysconReply,
						    &control->transitionPacket, 0x24, 1, 2, rate, 48);
		if (result >= 0) {
			initialComBootStep = 4;
			return 1;
		}
		goto unsent;
	case 4:
		if (message->command != 37)
			return 1;
		initialSysconPending = 1;
		result = bbmcSendSysconPowerCommand(control, initialSysconReply,
						    &control->transitionPacket,
						    control->defaultProfile == 1 ? 129 : 1);
		if (result < 0)
			goto failed;
		result = bbmcTimerSetHandler(60000000, _timeoutHandler, control);
		if (result >= 0) {
			initialComBootStep = 5;
			initialComBootTimer = result;
			return 1;
		}
		bbmcSaveTransitionState(control, result, initialComBootStep);
		initialComBootStep = 16;
#if FW_DEBUG
		sceKernelAssert(0, &dbg_stmInitial_ComBoot_L927, __builtin_return_address(0));
#endif
		break;
	case 5:
		command = message->command;
		if (command == 8) {
			bbmcTimerCancelHandler(initialComBootTimer);
			initialComBootTimer = 0;
			initialSysconPending = 1;
			initialComBootStep = 8;
			result = bbmcSendSysconCommand(control, initialSysconReply,
						       &control->transitionPacket, 0x14, 1, 0);
			if (result < 0) {
				step = initialComBootStep;
				initialSysconPending = 0;
				initialComBootStep = 16;
				control->transitionStep = step;
			}
			break;
		}
		if (command == 187) {
			if (message->body.timedOutCommand != 8)
				return 1;
			step = 5;
			goto restart;
		}
		if (command != 9 && command != 3)
			return 1;
		result = (int)0x80330001;
		bbmcTimerCancelHandler(initialComBootTimer);
		initialComBootTimer = 0;
		goto reset;
	case 6:
		if (message->command != 26)
			return 1;
		reply = message->body.sysconReplyWords.bytes;
		revision = (SceUInt8 *)&control->versionRevision;
		bbmcEnterCS(&control->mutex);
		if (control->firstBootReply == 8) {
			revision[0] = reply[3];
			revision[1] = reply[4];
			revision[2] = reply[5];
			control->versionBuild = reply[6] + (reply[7] << 8);
			bbmcLeaveCS(&control->mutex);
			goto configuration;
		}
		family = (SceUInt16)(reply[1] + (reply[2] << 8));
		if (family >= 1500 && family < 1600)
			index = 0;
		else if (family >= 1600 && family < 1700)
			index = 1;
		else if (family >= 2077 && family < 2177)
			index = 2;
		else if (family >= 2085 && family < 2185)
			index = 3;
		else if (family <= 2085)
			goto unsupported;
		else
			goto supported;
		control->versionFamily = family;
		requirement = &initialBootVersionRequirements[index];
		if (requirement->enabled > 1)
			goto unsupported;
		control->compatibleVersion = 1;
		if (reply[0] != 85)
			goto unsupported;
		build = (SceUInt16)(reply[6] + (reply[7] << 8));
		if (reply[3] < requirement->revision[0])
			goto unsupported;
		if (reply[3] == requirement->revision[0]) {
			if (reply[4] < requirement->revision[1])
				goto unsupported;
			if (reply[4] == requirement->revision[1] &&
			    (reply[5] < requirement->revision[2] || requirement->build > build))
				goto unsupported;
		}
		revision[0] = reply[3];
		revision[1] = reply[4];
		revision[2] = reply[5];
		control->versionBuild = build;
		if (family - 1575 <= 124 ||
		    (family == 1574 && (reply[3] != 0 || reply[4] % 50 > 1)))
			qmiSetConfiguredService(227);
		else
			qmiSetConfiguredService(224);
		family = control->versionFamily;
		control->versionCapability = family - 1602 <= 97 ||
			(family == 1601 && (revision[0] != 0 || revision[1] > 3 ||
					    (revision[1] == 3 && revision[2] > 4)));
	supported:
		bbmcLeaveCS(&control->mutex);
	configuration:
		initialSysconPending = 1;
		initialComBootStep = 7;
		result = bbmcSendSysconCommand(control, initialSysconReply,
					       &control->transitionPacket, 0x25, 7, 0);
		if (result < 0)
			goto failed;
		break;
	unsupported:
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 0, 0x410);
		bbmcSaveTransitionState(control, (int)0x80330002, initialComBootStep);
		initialComBootStep = 12;
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			goto abort;
		break;
	case 7:
		if (message->command != 27)
			return 1;
		control->firstConfigurationReply = message->body.sysconDecodedReply.replyBytes[0];
		initialSysconPending = 1;
		control->secondConfigurationReply = message->body.sysconDecodedReply.replyBytes[1];
		control->configurationReplyValid = 1;
		initialComBootStep = 10;
		control->decodedReply = message->body.sysconDecodedReply.decodedValue;
		control->configurationReplySeen = 1;
		result = bbmcSendSysconCommand(control, initialSysconReply,
					       &control->transitionPacket, 6, 2, 0);
		if (result < 0)
			goto failed;
		break;
	case 8:
		if (message->command != 28)
			return 1;
		control->replyVersionValid = 1;
		control->replyVersion = message->body.sysconReplyByte;
		if (message->body.sysconReplyByte > 4) {
			bbmcSaveTransitionState(control, (int)0x80330002, 8);
			result = (int)0x80330002;
			initialComBootStep = 16;
			goto finish;
		}
		step = initialOperationModes[message->body.sysconReplyByte];
		bbmcEnterCS(&control->mutex);
		control->completionPending = 0;
		control->replyOperationMode = step;
		bbmcLeaveCS(&control->mutex);
		initialSysconPending = 1;
		initialComBootStep = 9;
		result = bbmcSendSysconCommand(control, initialSysconReply,
					       &control->transitionPacket, 4, 2, 0);
		if (result < 0)
			goto failed;
		break;
	case 9:
		if (message->command != 24)
			return 1;
		bbmcEnterCS(&control->mutex);
		bootReply = message->body.sysconReplyHalfword;
		control->firstBootReply = bootReply;
		if (bootReply - 1 > 1u && bootReply - 6 > 1u) {
			bbmcLeaveCS(&control->mutex);
			initialSysconPending = 1;
			initialComBootStep = 6;
			result = bbmcSendSysconCommand(control, initialSysconReply,
						       &control->transitionPacket, 0x1c, 8, 0);
		} else {
			bbmcLeaveCS(&control->mutex);
			initialSysconPending = 1;
			initialComBootStep = 10;
			result = bbmcSendSysconCommand(control, initialSysconReply,
						       &control->transitionPacket, 6, 2, 0);
		}
		if (result < 0)
			goto failed;
		break;
	case 10:
		if (message->command != 23)
			return 1;
		if (message->body.sysconReplyHalfword & 0x8000) {
			bbmcSaveTransitionState(control, 0, 10);
			initialComBootStep = 12;
			result = bbmcSendSimpleMessage(control, 20);
			if (result < 0)
				initialComBootStep = 16;
			break;
		}
		bbmcEnterCS(&control->mutex);
		control->secondBootReply = message->body.sysconReplyHalfword;
		bbmcLeaveCS(&control->mutex);
		result = bbmcSendSimpleMessage(control, 49);
		if (result < 0)
			goto abort;
		initialComBootStep = 11;
		result = bbmcTimerSetHandler(60000000, _timeoutHandler, control);
		if (result < 0) {
			bbmcSaveTransitionState(control, result, initialComBootStep);
			initialComBootStep = 16;
#if FW_DEBUG
			sceKernelAssert(0, &dbg_stmInitial_ComBoot_L1259,
					__builtin_return_address(0));
#endif
		} else {
			initialComBootTimer = result;
		}
		break;
	case 11:
		command = message->command;
		if (command == 51) {
			bbmcTimerCancelHandler(initialComBootTimer);
			initialComBootTimer = 0;
			if (!(control->controlFlags & 4))
				bbmcRegisterNetworkInterface();
			bbmcEnterCS(&control->mutex);
			control->currentState = 1;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, 0, 8);
			if (control->flags & 1)
				result = bbmcSendSimpleMessage(control, 22);
			else
				result = bbmcSendSimpleMessage(control, 21);
			if (result < 0)
				goto abort;
			initialComBootStep = 16;
			goto finish;
		}
		if (command != 187 || message->body.timedOutCommand != 51)
			return 1;
		step = 11;
	restart:
		bbmcSaveTransitionState(control, 0, step);
		bbmcEnterCS(&control->mutex);
		control->flags = 0;
		bbmcLeaveCS(&control->mutex);
		initialComBootStep = 12;
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			initialComBootStep = 16;
		break;
	case 12:
		initialSysconPending = 1;
		result = bbmcSendSysconPowerCommand(control, initialSysconReply,
						    &control->transitionPacket,
						    control->defaultProfile == 1 ? 128 : 0);
		if (result < 0)
			goto unsent;
		result = bbmcTimerSetHandler(60000000, _timeoutHandler, control);
		if (result >= 0) {
			initialComBootStep = 13;
			initialComBootTimer = result;
			return 1;
		}
		step = initialComBootStep;
		control->transitionStep = step;
		goto saved;
	case 13:
	case 14:
	case 15:
		command = message->command;
		if (command == 39 || command == 9 || command == 3) {
			bbmcTimerCancelHandler(initialComBootTimer);
			initialComBootTimer = 0;
		} else if (command != 187 || message->body.timedOutCommand != 9) {
			return 1;
		}
		step = initialComBootStep;
		if (step == 13) {
			if (message->command == 39) {
				initialComBootStep = 15;
				return 1;
			}
			if (message->command == 9) {
				initialComBootStep = 14;
				return 1;
			}
		} else if ((step == 14 && message->command == 9) ||
			   (step == 15 && message->command == 39)) {
			result = (int)0x80330001;
			bbmcSaveTransitionState(control, (int)0x80330001, step);
			initialComBootStep = 16;
			goto finish;
		}
		if (!SceSysrootForDriver_4373AC96()) {
			result = (int)0x80330001;
			goto reset;
		}
		result = bbmcSendSimpleMessage(control, 20);
		if (result < 0)
			goto abort;
		initialComBootStep = 1;
		return 1;
	default:
		result = (int)0x80330001;
		control->transitionStep = initialComBootStep;
		initialComBootStep = 16;
		goto finish;
	}

	if (initialComBootStep != 16)
		return 1;
	goto finish;

reset:
	bbmcEnterCS(&control->mutex);
	control->currentState = 0;
	control->targetState = 0;
	control->references = 0;
	bbmcLeaveCS(&control->mutex);
	bbmcNotifyFeatureEvent(control, 0, 0x20);
	initialComBootStep = 16;
	goto finish;
abort:
	bbmcSaveTransitionState(control, result, initialComBootStep);
	initialComBootStep = 16;
	goto finish;
unsent:
	step = initialComBootStep;
	initialSysconPending = 0;
saved:
	bbmcSaveTransitionState(control, result, step);
	initialComBootStep = 16;
	goto finish;
failed:
	bbmcSaveTransitionState(control, result, initialComBootStep);
	initialSysconPending = 0;
	initialComBootStep = 16;
finish:
	bbmcCancelSysconCommand(&control->transitionPacket, &initialSysconPending);
	initialComBootStep = 1;
	return result;
}

int suspendSteadyComBoot(BbmcControl *control, BbmcMessage *message)
{
	int result = 0;

	control->suspendError = (int)0x80010300;
	bbmcUnregisterNetworkInterface();
	bbmcEnterCS(&control->mutex);
	control->currentState = 0;
	control->suspendStep = 10;
	if (bbmcSendSimpleMessage(control, 42) >= 0)
		result = 2;
	else
		control->flags |= 2;
	bbmcLeaveCS(&control->mutex);
	return result;
}

int suspendInitialToComBoot(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = initialSuspendStep;
	int result;

	if (step == 0) {
		control->suspendError = (int)0x80010200;
		result = SceSysconForDriver_99A254A9();
		if (result) {
			SceSysconForDriver_7BAFE083(0);
			control->flags |= 8;
			SceSysconForDriver_DD16ABD9(0);
			bbmcEnterCS(&control->mutex);
			control->sysconPowerRequestPending = 1;
			memset(&control->pinStatus, 0, offsetof(BbmcControl, smsListBusy) -
			       offsetof(BbmcControl, pinStatus));
			bbmcLeaveCS(&control->mutex);
			initialSuspendStep = 1;
			return 1;
		}
	} else if (step == 1) {
		control->suspendError = (int)0x80010201;
		if (message->command != 9)
			return 1;
		result = 0;
	} else {
		result = (int)0x80330001;
	}
	initialSuspendStep = 0;
	control->suspendError = (int)0x80010202;
	if (result == 0) {
		if (bbmcSendSimpleMessage(control, 42) >= 0) {
			bbmcEnterCS(&control->mutex);
			control->targetState = 0;
			bbmcLeaveCS(&control->mutex);
			return 2;
		}
		control->flags |= 2;
	}
	return result;
}
