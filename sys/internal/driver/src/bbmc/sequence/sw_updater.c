/*
 * The modem software update sequence: take the update package (175, 176), bring the
 * modem's power and transport up (_udSeqPrepareComPower), then hand every message to
 * the updater sequence of the selected handler.
 */
#include <stddef.h>
#include <string.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"
#include "../updater/updater.h"

#if FW_DEBUG
static const SceKernelDebugInfo dbg__udSeqChkComPower_L241
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/sw_updater.c",
	.line = 241,
	.func = "_udSeqChkComPower",
};

static const SceKernelDebugInfoSource dbg__udSeqChkComPower_L241Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__udSeqChkComPower_L241, "sequence/sw_updater.c", 241,
	"_udSeqChkComPower",
};

static const SceKernelDebugInfo dbg__udSeqPrepareComPower_L491
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/sw_updater.c",
	.line = 491,
	.func = "_udSeqPrepareComPower",
};

static const SceKernelDebugInfoSource dbg__udSeqPrepareComPower_L491Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__udSeqPrepareComPower_L491, "sequence/sw_updater.c", 491,
	"_udSeqPrepareComPower",
};
#endif

/* the updater sequence of each update handler */
static int (*const udSequences[2])(BbmcControl *, BbmcMessage *) = {
	_udSeqEdlPrepare,
	_udSeqEdlPrepare,
};

SceUInt32 udSeqStep;
SceUInt32 udPrepareStep;
SceUID udComPowerTimerId;
SceUInt32 udSavedMessagePending;
SceUInt32 udComPowerRetryLimit;
SceUInt32 udComPowerRetryCount;
BbmcMessage udSavedMessage;

/* name unknown: the updater's request for the next package chunk (message 177) */
static int udRequestChunk(SceUInt32 chunkIndex)
{
	BbmcControl *control = bbmcGetControl();
	int result;

	if (control == NULL)
		return (int)0x80330003;
	result = bbmcSendSimpleMessage(control, 177);
	if (sceKernelGetThreadId() == control->threadId)
		return (int)0x8033070D;
	return result;
}

int _udSeqChkComPower(BbmcControl *control, BbmcMessage *message)
{
	BbmcMessage current;
	BbcmdArgument output[4];
	UdClient client;
	SceUInt32 needed;
	SceUInt32 reply;
	SceUInt32 cancelled;
	int state;
	int failed;
	int result;

	memcpy(&current, message, sizeof(current));
	if (message->command == 176) {
		result = udReceiveImageChunk(message->body.updatePackage.chunkCount,
					     message->body.updatePackage.chunkIndex,
					     message->body.updatePackage.buffer,
					     message->body.updatePackage.size);
		if (result < 0) {
			failed = result != (int)0x8033070D;
			goto finish;
		}
	}
	if (udSavedMessagePending == 1 && message->command == 176) {
		memcpy(&current, &udSavedMessage, sizeof(current));
		udSavedMessagePending = 0;
	}
	if (current.command == 177)
		bbmcNotifyFeatureEvent(control, 7, 0x800000);

	switch (udSeqStep) {
	case 0:
		if (current.command != 175)
			goto ignore;
		control->updateCompletedBytes = 0;
		control->updateTotalBytes = 0;
		/* the update files, the entry ID and the file pair */
		memset(control->updateFiles, 0, offsetof(BbmcControl, firstStatusValue) -
		       offsetof(BbmcControl, updateFiles));
		control->updateCancelled = 0;
		client.requestChunk = udRequestChunk;
		control->lastError = 0;
		udComPowerRetryCount = 0;
		udComPowerRetryLimit = 0;
		udComPowerTimerId = 0;
		result = udInitialize(4, &client);
		if (result < 0)
			goto error;
		result = udEnableRamIo();
		if (result < 0)
			goto error;
		result = udSetImageChunk(current.body.updatePackage.chunkCount,
					 current.body.updatePackage.chunkIndex,
					 current.body.updatePackage.buffer,
					 current.body.updatePackage.size);
		if (result < 0)
			goto error;
		result = bbmcSendSimpleMessage(control, 178);
		if (result < 0)
			goto error;
		udSeqStep = 1;
		udPrepareStep = 0;
		goto keep;
	case 1:
		if (current.command != 178) {
			if (current.command == 123) {
				control->updateCancelled = 1;
				udSeqStep = 4;
			}
			goto ignore;
		}
		reply = control->secondBootReply;
		if (reply == 6 || reply - 1 <= 1) {
			failed = 1;
			result = (int)0x80330006;
			goto finish;
		}
		if (control->flags == 0) {
			udComPowerRetryCount = 0;
			if (control->currentState <= 1) {
				result = bbmcRequestOperationMode(control, 7);
				if (result < 0)
					goto error;
				udSeqStep = 2;
				udPrepareStep = 0;
			} else {
				result = bbmcSendSimpleMessage(control, 178);
				if (result < 0)
					goto error;
				udSeqStep = 2;
				udPrepareStep = 2;
			}
			goto keep;
		}
		if (udComPowerRetryCount > 59) {
			result = (int)0x80330006;
			goto fail;
		}
		udComPowerRetryCount++;
		result = bbmcTimerSetHandler(1000000, swdlComPowerTimeoutHandler, control);
		if (result >= 0) {
			udComPowerTimerId = result;
			goto keep;
		}
#if FW_DEBUG
		sceKernelAssert(0, &dbg__udSeqChkComPower_L241, __builtin_return_address(0));
#endif
		goto error;
	case 2:
		if (current.command == 123) {
			bbmcTimerCancelHandler(udComPowerTimerId);
			udComPowerTimerId = 0;
			result = 0;
			control->updateCancelled = 1;
			udSeqStep = 4;
			goto keep;
		}
		break;
	case 3:
		result = udSequences[control->updateHandler](control, &current);
		if (result == 0)
			goto done;
		failed = result == (int)0x8033070D ? 0 : result < 0;
		goto finish;
	default:
		result = (int)0x80330001;
		goto fail;
	}

	/* _udSeqPrepareComPower */
	switch (udPrepareStep) {
	case 0:
		if (current.command != 117)
			goto ignore;
		result = current.body.commandResult;
		if (result < 0)
			goto error;
		if (control->currentState <= 0)
			goto acquire;
		result = bbmcReleasePowerReference(control);
		if (result < 0)
			goto error;
		udPrepareStep = 1;
		goto keep;
	case 1:
		if (bbmcIsTransportHandshakeComplete(control)) {
			result = (int)0x80330710;
			goto fail;
		}
		if (!bbmcIsTransportReady(control))
			goto ignore;
acquire:
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			goto error;
		result = bbmcSendSimpleMessage(control, 178);
		if (result < 0)
			goto error;
		udPrepareStep = 2;
		goto keep;
	case 2:
		if (current.command == 9) {
			result = (int)0x80330006;
			bbmcTimerCancelHandler(udComPowerTimerId);
			udComPowerTimerId = 0;
			goto fail;
		}
		if (current.command != 178)
			goto ignore;
		udComPowerTimerId = 0;
		if (control->flags != 0)
			goto retry;
		state = control->currentState;
		if (state <= 4) {
			if (state == 4)
				goto query;
			if (state <= 1)
				goto retry;
			if (control->pinStatus.pin1Status - 1 <= 1)
				goto query;
			if (state != 3)
				goto retry;
			if (bbmcCollectControlKeyEvents(control) & 0x1800000)
				goto query;
			result = 0;
			if (udPrepareStep != 2)
				goto keep;
		} else {
			if (state == 8)
				goto query;
			if (state != 9) {
				control->updateHandler = control->firmwareRevisionSelection;
				result = bbmcSendSimpleMessage(control, 178);
				if (result < 0)
					goto error;
				udPrepareStep = 4;
				goto keep;
			}
			if (control->pinStatus.pin1Status - 1 <= 1)
				goto query;
		}
retry:
		if (control->flags != 0 || control->currentState == control->targetState) {
			if (udComPowerRetryLimit != 5) {
				udComPowerRetryLimit = 5;
				udComPowerRetryCount = 0;
			}
		} else if (udComPowerRetryLimit != 50) {
			udComPowerRetryLimit = 50;
			udComPowerRetryCount = 0;
		}
		if (udComPowerRetryCount >= udComPowerRetryLimit) {
			result = (int)0x80330006;
			goto fail;
		}
		udComPowerRetryCount++;
		result = bbmcTimerSetHandler(1000000, swdlComPowerTimeoutHandler, control);
		if (result >= 0) {
			udComPowerTimerId = result;
			goto keep;
		}
#if FW_DEBUG
		sceKernelAssert(0, &dbg__udSeqPrepareComPower_L491,
				__builtin_return_address(0));
#endif
		goto error;
query:
		output[0].pointer = &control->firmwareBuildSelection;
		output[1].value = 0;
		output[2].value = 0;
		output[3].value = 0;
		result = bbcmdExecute(0, 0x33, NULL, output, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			goto error;
		udPrepareStep = 3;
		goto keep;
	case 3:
		if (current.command != 186)
			goto ignore;
		if (current.body.commandResult < 0) {
			result = (int)0x80330006;
			goto fail;
		}
		control->updateHandler = control->firmwareRevisionSelection;
		result = bbmcSendSimpleMessage(control, 178);
		if (result < 0)
			goto error;
		udPrepareStep = 4;
		goto keep;
	case 4:
		if (current.command != 178)
			goto ignore;
		needed = 0;
		result = udSetTarget(control->updateHandler, control->configurationMode);
		if (result < 0)
			goto error;
		result = udLoadImage();
		if (result < 0)
			goto error;
		result = udIsUpdateNeeded(&needed);
		if (result < 0)
			goto error;
		if (needed != 1) {
			udSeqStep = 4;
			goto keep;
		}
		result = bbmcSendSimpleMessage(control, 178);
		if (result < 0)
			goto error;
		udPrepareStep = 0;
		udSeqStep = 3;
		goto keep;
	default:
		result = (int)0x80330001;
		goto fail;
	}

ignore:
	result = 0;
	goto keep;
error:
	failed = result != (int)0x8033070D;
finish:
	if (failed) {
fail:
		bbmcEnterCS(&control->mutex);
		control->lastError = result;
		bbmcLeaveCS(&control->mutex);
		goto done;
	}
keep:
	if (result == (int)0x8033070D) {
		memcpy(&udSavedMessage, &current, sizeof(udSavedMessage));
		udSavedMessagePending = 1;
	}
	if (udSeqStep != 4)
		return 1;
done:
	cancelled = control->updateCancelled;
	udSeqStep = 0;
	udPrepareStep = 0;
	if (cancelled == 1)
		bbmcNotifyFeatureEvent(control, 7, 0x800);
	else
		bbmcNotifyFeatureEvent(control, 7,
				       control->lastError >= 0 ? 0x400000 : 0x200);
	udTerminate();
	return 0;
}
