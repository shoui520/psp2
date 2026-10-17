#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

void bbmcEnterCS(struct ThreadmgrFastMutex *mutex);
void bbmcLeaveCS(struct ThreadmgrFastMutex *mutex);
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcTimerSetHandler(SceUInt32 delay, void (*callback)(void *), void *arg);
int bbmcTimerCancelHandler(int timerId);
int bbmcSendSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 operation, unsigned int byteCount, ...);
int bbmcCancelSysconCommand(SceSysconPacket *packet, int *pending);

SceUInt32 lowPowerModeStep;
int lowPowerSysconPending;
SceUID lowPowerTransitionTimer;
SceUInt32 lowPowerSuspendCompletion;

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
		ksceKernelPanic(&dbg_suspendGotoConnected_L751,
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
