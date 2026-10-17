#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbmc_netif.h"
#include "../qmi_backend.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcTimerSetHandler(SceUInt32 delay, void (*callback)(void *), void *arg);
int bbmcTimerCancelHandler(int timerId);
int bbmcSendSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 operation, unsigned int byteCount, ...);
int bbmcSendSizedSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 payloadLength, unsigned int byteCount, ...);
int bbmcCancelSysconCommand(SceSysconPacket *packet, int *pending);

SceUInt32 initialComBootStep;
int initialSysconPending;
SceUID initialComBootTimer;

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
		ksceKernelPanic(&dbg__timeoutHandler_L675, __builtin_return_address(0));
}
