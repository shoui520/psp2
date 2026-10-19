#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_netif.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int message);

SceUInt32 gBbmcStopNetIfState;
SceUInt32 *gBbmcStoppingNetworkHandle;

/* Descriptive source name: no surviving Sony record names this callback. */
int seqStopNetIF(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	BbmcMessage *reply;
	SceUInt32 online, initialized, deferred, handleValue;
	SceUInt32 *handle;
	int result;

	if (message == NULL)
		return 0;
	if (message->command == 72)
		bbmcNotifyPacketInterfaceEvent(0x10000, 0);

	switch (gBbmcStopNetIfState) {
	case 0:
		if (message->command == 104) {
			bbmcEnterCS(&control->mutex);
			control->networkStopBusy = 1;
			if (control->networkStarted != 0 &&
			    control->networkInfo.signalLevel != -1 &&
			    control->suspendState == 0) {
				bbmcLeaveCS(&control->mutex);
				handle = message->body.networkStop.networkHandle;
				handleValue = *handle;
				gBbmcStoppingNetworkHandle = handle;
				input[0].value = handleValue;
				result = bbcmdExecute(1, 26, input, NULL,
						     bbcmdManagerCompletion, control);
				if (result < 0)
					goto resetState;
			} else {
				bbmcLeaveCS(&control->mutex);
				reply = msgAllocMsgPacket(control, 106);
				/* The original reads this unwritten allocated word. */
				handle = reply->body.networkStop.networkHandle;
				reply->body.networkStop.commandResult = 0;
				gBbmcStoppingNetworkHandle = handle;
				result = msgSendMsgPacket(control, reply);
				if (result < 0)
					goto releaseReply;
			}
			gBbmcStopNetIfState = 1;
			return 1;
		}
		if (message->command != 115)
			return 1;
		bbmcEnterCS(&control->mutex);
		online = control->clientOnline;
		control->networkStopBusy = 1;
		if (online != 0) {
			reply = msgAllocMsgPacket(control, 106);
			/* This second original path has the same unwritten read. */
			handle = reply->body.networkStop.networkHandle;
			reply->body.networkStop.commandResult = 0;
			gBbmcStoppingNetworkHandle = handle;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReplyLocked;
			gBbmcStopNetIfState = 1;
		} else if (control->clientInitialized != 0) {
			reply = msgAllocMsgPacket(control, 56);
			reply->body.commandResult = 0;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReplyLocked;
			gBbmcStopNetIfState = 2;
		} else {
			gBbmcStopNetIfState = 4;
		}
		bbmcLeaveCS(&control->mutex);
		goto complete;
	case 1:
		if (message->command != 106)
			return 1;
		if (message->body.commandResult != 0)
			return 0;
		bbmcEnterCS(&control->mutex);
		if (control->clientOnline != 0) {
			control->networkStarted = 0;
			control->pendingPacketRequest = 0;
			if (gBbmcStoppingNetworkHandle != NULL)
				gBbmcStoppingNetworkHandle = NULL;
			/* Eight byte stores are explicit in the original. */
			((SceUInt8 *)&control->currentBearer)[0] = 0;
			((SceUInt8 *)&control->currentBearer)[1] = 0;
			((SceUInt8 *)&control->currentBearer)[2] = 0;
			((SceUInt8 *)&control->currentBearer)[3] = 0;
			((SceUInt8 *)&control->bearerDetails)[0] = 0;
			((SceUInt8 *)&control->bearerDetails)[1] = 0;
			((SceUInt8 *)&control->bearerDetails)[2] = 0;
			((SceUInt8 *)&control->bearerDetails)[3] = 0;
			result = bbcmdExecute(1, 10, NULL, NULL,
					     bbcmdManagerCompletion, control);
			if (result < 0) {
				bbmcLeaveCS(&control->mutex);
				goto resetState;
			}
		} else {
			reply = msgAllocMsgPacket(control, 56);
			reply->body.commandResult = 0;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReplyLocked;
		}
		bbmcLeaveCS(&control->mutex);
		gBbmcStopNetIfState = 2;
		return 1;
	case 2:
		if (message->command != 56)
			return 1;
		if (message->body.commandResult != 0)
			return (int)0x80330001;
		initialized = control->clientInitialized;
		control->clientOnline = 0;
		if (initialized != 0) {
			result = bbcmdExecute(1, 6, NULL, NULL,
					     bbcmdManagerCompletion, control);
			if (result < 0)
				goto resetState;
		} else {
			reply = msgAllocMsgPacket(control, 57);
			reply->body.commandResult = 0;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				goto releaseReply;
		}
		gBbmcStopNetIfState = 3;
		return 1;
	case 3:
		if (message->command != 57)
			return 1;
		bbmcClearPacketInterfaceUsbState();
		if (message->body.commandResult != 0) {
			gBbmcStopNetIfState = 4;
			bbmcNotifyPacketInterfaceEvent(16,
					message->body.commandResult);
		} else {
			control->clientInitialized = 0;
			gBbmcStopNetIfState = 4;
			result = bbmcSendSimpleMessage(control, 154);
			if (result < 0)
				goto checkReset;
		}
		goto complete;
	default:
		result = (int)0x80330001;
		goto resetState;
	}

releaseReplyLocked:
	msgFreeMsgPacket(control, reply);
	bbmcLeaveCS(&control->mutex);
	goto checkReset;
releaseReply:
	msgFreeMsgPacket(control, reply);
checkReset:
	if (gBbmcStopNetIfState != 4)
		return result;
resetState:
	gBbmcStopNetIfState = 0;
	return result;
complete:
	if (gBbmcStopNetIfState != 4)
		return 1;
	bbmcEnterCS(&control->mutex);
	control->networkStopBusy = 0;
	deferred = control->deferredNetworkStart;
	if (deferred != 0 &&
	    control->networkInfo.serving.registrationState == 1 &&
	    control->targetState == 7 && control->currentState == 7) {
		control->pendingPacketRequest = 1;
		bbmcSendSimpleMessage(control, 109);
		control->deferredNetworkStart = 0;
	}
	bbmcLeaveCS(&control->mutex);
	gBbmcStopNetIfState = 0;
	return 0;
}

