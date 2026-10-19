#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_netif.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int message);

/* Independent original state and command-25 output destinations. */
SceUInt32 gBbmcStartNetIfState;
SceUInt32 *gBbmcCachedNetworkHandle;
SceUInt8 *gBbmcCachedHasIpv4Address;
SceUInt32 *gBbmcCurrentIpv4Address;
SceUInt8 *gBbmcCurrentHasIpv4Address;
SceUInt32 *gBbmcCurrentRuntimeValue;
BbmcCallEndReason gBbmcNetworkCallEndReason;
SceUInt32 *gBbmcCachedRuntimeValue;
SceUInt32 *gBbmcCurrentNetworkHandle;
SceUInt32 *gBbmcCachedIpv4Address;

static const SceKernelDebugInfo dbg_seqStartNetIF_L161
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "sequence/start_net_if.c",
	.line = 161,
	.func = "seqStartNetIF",
#endif
};

static const SceKernelDebugInfoSource dbg_seqStartNetIF_L161Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqStartNetIF_L161, "sequence/start_net_if.c", 161, "seqStartNetIF",
};

int seqStartNetIF(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7] = {0};
	BbcmdArgument output[6] = {0};
	BbmcMessage *reply;
	SceUInt32 online;
	int result;

	if (gBbmcStartNetIfState != 0 && gBbmcStartNetIfState != 11 &&
	    message->command == 107) {
		switch (gBbmcStartNetIfState) {
		case 1:
			if (control->clientOnline != 0) {
				bbmcSendSimpleMessage(control, 108);
				gBbmcStartNetIfState = 5;
				goto waitForDisconnect;
			}
			if (control->clientInitialized == 0) {
				gBbmcStartNetIfState = 8;
				goto pending;
			}
			online = control->clientOnline;
			reply = msgAllocMsgPacket(control, 56);
			if (reply == NULL)
				goto allocationFailed;
			reply->body.commandResult = online;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReply;
			gBbmcStartNetIfState = 7;
			goto waitForOffline;
		case 2:
			gBbmcStartNetIfState = 6;
			goto pending;
		case 3:
			gBbmcStartNetIfState = 4;
			goto cancelStart;
		case 10:
			bbmcEnterCS(&control->mutex);
			control->deferredNetworkStart = 0;
			control->networkRetryPending = 0;
			control->networkStartBusy = 0;
			control->pendingPacketRequest = 0;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyPacketInterfaceEvent(16, (int)0x80330505);
			gBbmcStartNetIfState = 0;
			return 0;
		default:
			SceDebugForDriver_391B5B74(&dbg_seqStartNetIF_L161,
					__builtin_return_address(0));
		}
	}

	switch (gBbmcStartNetIfState) {
	case 0:
		if (message->command == 103) {
			bbmcEnterCS(&control->mutex);
			control->networkStartBusy = 1;
			control->deferredNetworkStart =
				control->networkStopBusy != 0 ||
				(control->powerEventActive == 0 &&
				 control->recentPowerEvent != 0);
			bbmcLeaveCS(&control->mutex);
			if (control->deferredNetworkStart != 0) {
				gBbmcCachedNetworkHandle =
					message->body.networkStart.networkHandle;
				gBbmcCachedIpv4Address =
					message->body.networkStart.ipv4Address;
				gBbmcCachedHasIpv4Address =
					message->body.networkStart.hasIpv4Address;
				gBbmcCachedRuntimeValue =
					message->body.networkStart.runtimeValue;
				gBbmcStartNetIfState = 10;
				if (control->powerEventTimer > 0)
					goto pending;
				bbmcScheduleDeferredNetworkRetry(control);
				goto checkCompletion;
			}
		}
		goto dispatchStart;
	case 1:
		if (message->command != 58)
			goto pending;
		if (message->body.commandResult != 0) {
			bbmcEnterCS(&control->mutex);
			control->pendingPacketRequest = 0;
			bbmcLeaveCS(&control->mutex);
			result = (int)0x80330501;
			goto notifyFailure;
		}
		bbmcEnterCS(&control->mutex);
		control->clientInitialized = 1;
		online = control->clientOnline;
		bbmcLeaveCS(&control->mutex);
		if (online == 0) {
			input[0].value = 8;
			result = bbcmdExecute(1, 9, input, NULL,
					     bbcmdManagerCompletion, control);
			if (result < 0)
				goto failed;
		} else {
			reply = msgAllocMsgPacket(control, 59);
			if (reply == NULL)
				goto allocationFailed;
			reply->body.commandResult = 0;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReply;
		}
		gBbmcStartNetIfState = 2;
		goto pending;
	case 2:
		if (message->command != 59)
			goto pending;
		result = message->body.commandResult;
		if (result != 0)
			goto notifyFailure;
		bbmcEnterCS(&control->mutex);
		control->clientOnline = 1;
		bbmcLeaveCS(&control->mutex);
		input[0].value = 19;
		input[1].value = 30;
		input[2].bytes[0] = 1;
		input[3].bytes[0] = 1;
		input[4].value = 0;
		input[6].value = 1;
		output[0].pointer = gBbmcCurrentNetworkHandle;
		output[1].pointer = &gBbmcNetworkCallEndReason;
		output[2].pointer = gBbmcCurrentHasIpv4Address;
		output[3].pointer = gBbmcCurrentIpv4Address;
		output[4].pointer = gBbmcCurrentRuntimeValue;
		result = bbcmdExecute(1, 25, input, output,
				     bbcmdManagerCompletion, control);
		if (result < 0) {
			bbmcEnterCS(&control->mutex);
			control->networkStartBusy = 0;
			bbmcLeaveCS(&control->mutex);
			goto notifyFailure;
		}
		gBbmcStartNetIfState = 3;
		goto pending;
	case 3:
		if (message->command != 105)
			goto pending;
		bbmcEnterCS(&control->mutex);
		control->networkStartBusy = 0;
		bbmcLeaveCS(&control->mutex);
		if (message->body.commandResult < 0) {
			bbmcEnterCS(&control->mutex);
			control->pendingPacketRequest = 0;
			bbmcNotifyPacketInterfaceEvent(16,
					message->body.commandResult);
			if (gBbmcNetworkCallEndReason.type != 0 &&
			    gBbmcNetworkCallEndReason.reason != 0) {
				control->callEndReason = gBbmcNetworkCallEndReason;
				bbmcNotifyFeatureEvent(control, 5, 2);
			}
			bbmcLeaveCS(&control->mutex);
			result = message->body.commandResult;
			goto failed;
		}
		bbmcEnterCS(&control->mutex);
		if (control->networkStarted == 0)
			control->networkStarted = 1;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyPacketInterfaceEvent(1, 0);
		if (gBbmcNetworkCallEndReason.type != 0 &&
		    gBbmcNetworkCallEndReason.reason != 0) {
			bbmcEnterCS(&control->mutex);
			control->callEndReason = gBbmcNetworkCallEndReason;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, 5, 2);
		}
		gBbmcStartNetIfState = 0;
		return 0;
	case 4:
cancelStart:
		input[0].value = 1;
		input[1].value = 25;
		result = bbcmdExecute(1, 0, input, NULL,
				     bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		gBbmcStartNetIfState = 5;
		goto pending;
	case 5:
waitForDisconnect:
		if (message->command != 108)
			goto pending;
		result = bbcmdExecute(1, 10, NULL, NULL,
				     bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		gBbmcStartNetIfState = 7;
		goto pending;
	case 6:
		if (message->command != 59)
			goto pending;
		bbmcEnterCS(&control->mutex);
		control->clientOnline = 1;
		bbmcLeaveCS(&control->mutex);
		result = bbcmdExecute(1, 10, NULL, NULL,
				     bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		gBbmcStartNetIfState = 7;
		goto pending;
	case 7:
waitForOffline:
		if (message->command != 56)
			goto pending;
		result = message->body.commandResult;
		if (result != 0)
			goto failed;
		bbmcEnterCS(&control->mutex);
		control->clientOnline = 0;
		bbmcLeaveCS(&control->mutex);
		result = bbcmdExecute(1, 6, NULL, NULL,
				     bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		gBbmcStartNetIfState = 9;
		goto pending;
	case 8:
		if (message->command != 58)
			goto pending;
		result = bbcmdExecute(1, 6, NULL, NULL,
				     bbcmdManagerCompletion, control);
		if (result < 0)
			goto failed;
		gBbmcStartNetIfState = 9;
		goto pending;
	case 9:
		if (message->command != 57)
			goto pending;
		result = message->body.commandResult;
		if (result != 0)
			goto failed;
		bbmcEnterCS(&control->mutex);
		control->clientInitialized = 0;
		control->pendingPacketRequest = 0;
		control->networkStartBusy = 0;
		control->networkRetryPending = 0;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyPacketInterfaceEvent(16, (int)0x80330505);
		gBbmcStartNetIfState = 0;
		return 0;
	case 10:
dispatchStart:
		if (message->command == 103 || message->command == 109) {
			gBbmcNetworkCallEndReason = (BbmcCallEndReason){0};
			if (message->command == 103) {
				gBbmcCurrentNetworkHandle =
					message->body.networkStart.networkHandle;
				gBbmcCurrentIpv4Address =
					message->body.networkStart.ipv4Address;
				gBbmcCurrentHasIpv4Address =
					message->body.networkStart.hasIpv4Address;
				gBbmcCurrentRuntimeValue =
					message->body.networkStart.runtimeValue;
			} else {
				gBbmcCurrentNetworkHandle = gBbmcCachedNetworkHandle;
				gBbmcCurrentIpv4Address = gBbmcCachedIpv4Address;
				gBbmcCurrentHasIpv4Address = gBbmcCachedHasIpv4Address;
				gBbmcCurrentRuntimeValue = gBbmcCachedRuntimeValue;
			}
			bbmcEnterCS(&control->mutex);
			online = control->clientInitialized;
			bbmcLeaveCS(&control->mutex);
			if (online != 0) {
				reply = msgAllocMsgPacket(control, 58);
				if (reply == NULL)
					goto allocationFailed;
				reply->body.commandResult = 0;
				result = msgSendMsgPacket(control, reply);
				if (result < 0)
					goto releaseReply;
			} else {
				result = bbcmdExecute(1, 5, NULL, NULL,
						     bbcmdManagerCompletion, control);
				if (result < 0)
					goto notifyFailure;
			}
			gBbmcStartNetIfState = 1;
			goto pending;
		}
		if (message->command == 110) {
			result = control->powerEventActive != 0 ?
				(int)0x80330502 : (int)0x80330501;
			control->pendingPacketRequest = 0;
			goto notifyFailure;
		}
checkCompletion:
		if (gBbmcStartNetIfState != 11)
			goto pending;
		gBbmcStartNetIfState = 0;
		return 0;
	default:
		result = (int)0x80330001;
		goto failed;
	}

allocationFailed:
	bbmcNotifyPacketInterfaceEvent(16, (int)0x80330005);
	result = 0;
	goto failed;
releaseReply:
	msgFreeMsgPacket(control, reply);
notifyFailure:
	bbmcNotifyPacketInterfaceEvent(16, result);
failed:
	gBbmcStartNetIfState = 11;
	bbmcEnterCS(&control->mutex);
	control->networkStarted = 0;
	control->networkStartBusy = 0;
	bbmcLeaveCS(&control->mutex);
	gBbmcStartNetIfState = 0;
	return result;
pending:
	return 1;
}

