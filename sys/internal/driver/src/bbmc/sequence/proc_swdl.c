#include <stddef.h>
#include <string.h>
#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbmc_netif.h"
#include "../bbcmd.h"
#include "../updater/updater.h"
#include <power/power.h>
#include <gps/gps.h>

int bbmcSendSysconByteCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *packet, void *argp),
	SceSysconPacket *packet, SceUInt8 value);
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcSysconEventReply(SceSysconPacket *packet, void *argp);
int bbmcSysconStatusReply(SceSysconPacket *packet, void *argp);

void seqClearSimDetected(void);

extern SceUInt32 sysconEventStep;
extern int sysconEventPending;
extern int sysconStatusPending;

SceUInt32 cellLocationStep;
SceUInt32 modemLinkStep;
SceUInt32 disconnectStep;
SceUInt32 sysconStatusStep;
SceUInt32 simRefreshStep;
SceUInt32 channelRatesStep;
SceUInt32 proactiveStep;
SceUInt32 pinStatusStep;
SceUInt32 gpsRequestStep;
SceUInt32 smsListStep;
SceUInt32 attachChangeStep;
SceUInt32 networkScanStep;
SceUInt32 controlFlagStep;
SceUInt32 setupStep;
SceUInt32 pinUnlockStep;
SceUInt32 firmwarePreferenceStep;
SceUInt32 firmwareImageStep;
SceUInt32 setFirmwarePreferenceStep;
SceUInt32 storedImagesStep;
SceUInt32 sysconEventDeferred;
SceUInt32 simDetectFlags;
int proactivePending;
int modemWakeupPending;
SceUInt32 pinStatusRequeued;
SceUInt32 controlFlagMask;
SceUInt32 controlFlagSet;
SceUID swdlTimerId;
SceUInt32 swdlNextStep;

/* name unknown: the download request being run, and _waitTransition's counts */
typedef struct SwdlWork {
	BbmcMessage request;
	SceUInt32 carrierFlags[3];
	SceUInt32 stableCount;
	SceUInt32 unstableCount;
} SwdlWork;

static SwdlWork swdlWork;

#if FW_DEBUG
static const SceKernelDebugInfo dbg__waitTransition_L1410
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/proc_swdl.c",
	.line = 1410,
	.func = "_waitTransition",
};

static const SceKernelDebugInfoSource dbg__waitTransition_L1410Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__waitTransition_L1410, "sequence/proc_swdl.c", 1410,
	"_waitTransition",
};
#endif

/* name unknown: command 163 reads the serving cell into control->cellLocation */
int seqGetCellLocation(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument output[6] = {{0}};
	SceUInt32 queryState, event;
	int result;

	switch (cellLocationStep) {
	case 0:
		if (message->command != 163)
			return 1;
		output[0].pointer = &control->cellLocation;
		result = bbcmdExecute(0, 0x22, NULL, output, bbcmdManagerCompletion,
				      control);
		if (result >= 0) {
			cellLocationStep = 1;
			return 1;
		}
		break;
	case 1:
		if (message->command != 164)
			return 1;
		if (message->body.commandResult == 0) {
			queryState = 2;
			event = 2;
		} else {
			queryState = 0;
			event = 4;
		}
		bbmcEnterCS(&control->mutex);
		control->cellQueryState = queryState;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 3, event);
		cellLocationStep = 0;
		return 0;
	default:
		result = (int)0x80330001;
		cellLocationStep = 2;
		break;
	}
	if (cellLocationStep == 2) {
		cellLocationStep = 0;
		bbmcEnterCS(&control->mutex);
		control->cellQueryState = 0;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 3, 4);
	}
	return result;
}

/* name unknown: syscon accepted the modem link on, the sequence goes on with 51 */
static int modemLinkOnCallback(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;

	if (packet->request.command[0] != 0x82)
		goto out;
	if (packet->request.command[1] != 10)
		goto out;
	if (modemLinkStep != 1)
		goto out;
	message = msgAllocMsgPacket(control, 51);
	if (message != NULL && msgSendMsgPacket(control, message) < 0)
		msgFreeMsgPacket(control, message);
out:
	return 0;
}

/* name unknown: syscon accepted the modem link off, the sequence goes on with 52 */
static int modemLinkOffCallback(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;

	if (packet->request.command[0] != 0x82)
		goto out;
	if (packet->request.command[1] != 10)
		goto out;
	if (modemLinkStep != 1)
		goto out;
	message = msgAllocMsgPacket(control, 52);
	if (message != NULL && msgSendMsgPacket(control, message) < 0)
		msgFreeMsgPacket(control, message);
out:
	return 0;
}

/* name unknown: commands 49 (on) and 50 (off) set the syscon's modem link byte */
int seqSetModemLink(BbmcControl *control, BbmcMessage *message)
{
	int result;

	switch (modemLinkStep) {
	case 0:
		if (message->command - 49 > 1)
			return 1;
		result = bbmcSendSysconByteCommand(control,
			message->command == 49 ? modemLinkOnCallback : modemLinkOffCallback,
			&control->bytePacket, message->command == 49);
		if (result >= 0) {
			modemLinkStep = 1;
			return 1;
		}
		break;
	case 1:
		if (message->command - 51 > 1)
			return 1;
		modemLinkStep = 0;
		return 0;
	default:
		result = (int)0x80330001;
		break;
	}
	modemLinkStep = 0;
	return result;
}

/* name unknown: command 114 drops the network link */
int seqDisconnectNetwork(BbmcControl *control, BbmcMessage *message)
{
	if (disconnectStep == 0) {
		if (message->command != 114)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->operationBusy = 1;
		bbmcLeaveCS(&control->mutex);
		DisconnectNetwork();
	}
	disconnectStep = 0;
	return 0;
}

/* name unknown: command 155 reads the modem's stored firmware images */
int seqGetStoredImages(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument output[6];
	int result;

	switch (storedImagesStep) {
	case 0:
		if (message->command != 155)
			return 1;
		output[0].pointer = message->body.firmwareMessage.storedImages;
		output[1].value = 0;
		output[2].value = 0;
		output[3].value = 0;
		output[4].value = 0;
		output[5].value = 0;
		result = bbcmdExecute(0, 0x28, NULL, output, bbcmdManagerCompletion,
				      control);
		if (result >= 0) {
			storedImagesStep = 1;
			return 1;
		}
		bbmcEnterCS(&control->mutex);
		control->storedImagesResult = result;
		control->storedImagesState = 2;
		bbmcLeaveCS(&control->mutex);
		break;
	case 1:
		if (message->command != 156)
			return 1;
		result = 0;
		bbmcEnterCS(&control->mutex);
		control->storedImagesState = 2;
		control->storedImagesResult =
			message->body.firmwareMessage.storedImagesResult;
		bbmcLeaveCS(&control->mutex);
		break;
	default:
		result = (int)0x80330001;
		control->storedImagesState = 2;
		break;
	}
	storedImagesStep = 0;
	bbmcNotifyFeatureEvent(control, 7, 0x10000);
	return result;
}

/* name unknown: command 157 reads the modem's firmware preference */
int seqGetFirmwarePreference(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument output[6];
	int result;

	switch (firmwarePreferenceStep) {
	case 0:
		if (message->command != 157)
			return 1;
		output[0].value = 0;
		output[1].pointer = message->body.firmwareMessage.firmwarePreference;
		output[2].value = 0;
		output[3].value = 0;
		output[4].value = 0;
		output[5].value = 0;
		result = bbcmdExecute(0, 0x27, NULL, output, bbcmdManagerCompletion,
				      control);
		if (result >= 0) {
			firmwarePreferenceStep = 1;
			return 1;
		}
		bbmcEnterCS(&control->mutex);
		control->firmwarePreferenceResult = result;
		control->firmwarePreferenceState = 2;
		bbmcLeaveCS(&control->mutex);
		break;
	case 1:
		if (message->command != 158)
			return 1;
		result = 0;
		bbmcEnterCS(&control->mutex);
		control->firmwarePreferenceState = 2;
		control->firmwarePreferenceResult =
			message->body.firmwareMessage.firmwarePreferenceResult;
		bbmcLeaveCS(&control->mutex);
		break;
	default:
		result = (int)0x80330001;
		control->firmwarePreferenceState = 2;
		break;
	}
	firmwarePreferenceStep = 0;
	bbmcNotifyFeatureEvent(control, 7, 0x20000);
	return result;
}

/* name unknown: command 159 hands the modem one firmware image description */
int seqSetFirmwareImage(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	int result;

	switch (firmwareImageStep) {
	case 0:
		if (message->command != 159)
			return 1;
		input[0].value =
			(SceUInt32)message->body.firmwareMessage.firmwareImage;
		input[1].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		result = bbcmdExecute(0, 0x29, input, NULL, bbcmdManagerCompletion,
				      control);
		if (result >= 0) {
			firmwareImageStep = 1;
			return 1;
		}
		bbmcEnterCS(&control->mutex);
		control->firmwareImageResult = result;
		control->firmwareImageState = 2;
		bbmcLeaveCS(&control->mutex);
		break;
	case 1:
		if (message->command != 160)
			return 1;
		result = 0;
		bbmcEnterCS(&control->mutex);
		control->firmwareImageState = 2;
		control->firmwareImageResult =
			message->body.firmwareMessage.firmwareImageResult;
		bbmcLeaveCS(&control->mutex);
		break;
	default:
		result = (int)0x80330001;
		control->firmwareImageState = 2;
		break;
	}
	firmwareImageStep = 0;
	bbmcNotifyFeatureEvent(control, 7, 0x40000);
	return result;
}

/* name unknown: command 161 sets the modem's firmware preference */
int seqSetFirmwarePreference(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument output[6];
	BbcmdArgument input[7];
	SceUInt32 imageCount, firstImageType, secondImageType;
	int result;

	switch (setFirmwarePreferenceStep) {
	case 0:
		if (message->command != 161)
			return 1;
		input[0].value = 0;
		input[1].value =
			(SceUInt32)message->body.firmwareMessage.setFirmwarePreference;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		output[0].value = 0;
		output[1].value = 0;
		output[2].value = 0;
		output[3].value = 0;
		output[4].value = 0;
		output[5].value = 0;
		result = bbcmdExecute(0, 0x26, input, output, bbcmdManagerCompletion,
				      control);
		if (result >= 0) {
			setFirmwarePreferenceStep = 1;
			return 1;
		}
		bbmcEnterCS(&control->mutex);
		control->setFirmwarePreferenceResult = result;
		control->setFirmwarePreferenceState = 2;
		bbmcLeaveCS(&control->mutex);
		break;
	case 1:
		if (message->command != 162)
			return 1;
		result = 0;
		bbmcEnterCS(&control->mutex);
		control->setFirmwarePreferenceState = 2;
		control->setFirmwarePreferenceResult =
			message->body.firmwareMessage.setFirmwarePreferenceResult;
		imageCount = message->body.firmwareMessage.imageCount;
		firstImageType = message->body.firmwareMessage.firstImageType;
		secondImageType = message->body.firmwareMessage.secondImageType;
		control->downloadImageCount = imageCount;
		control->firstDownloadImageType = firstImageType;
		control->secondDownloadImageType = secondImageType;
		bbmcLeaveCS(&control->mutex);
		break;
	default:
		result = (int)0x80330001;
		control->setFirmwarePreferenceState = 2;
		break;
	}
	setFirmwarePreferenceStep = 0;
	bbmcNotifyFeatureEvent(control, 7, 0x80000);
	return result;
}

/* name unknown: message 2 (or a deferred message 1) reads the syscon's event words */
int seqSysconEvent(BbmcControl *control, BbmcMessage *message)
{
	BbmcMessage *reply;
	SceUInt32 events, command;
	int result;

	switch (sysconEventStep) {
	case 0:
		if ((message->command == 1 || sysconEventDeferred == 1) &&
		    sysconEventPending == 0) {
			sysconEventPending = 1;
			result = bbmcSendSysconCommand(control, bbmcSysconEventReply,
				&control->eventPacket, 8, 8, 0);
			if (result < 0)
				goto fail;
			sysconEventStep = 1;
			sysconEventDeferred = 0;
		}
		return 1;
	case 1:
		if (control->flags & 2)
			goto cancel;
		command = message->command;
		if (command != 2)
			goto other;
		bbmcEnterCS(&control->mutex);
		events = message->body.sysconReplyWords.words[0];
		if ((events & 4) && control->sysconPowerRequestPending == 0) {
			bbmcSendSimpleMessage(control, 3);
			bbmcNotifyFeatureEvent(control, 0, 1);
			events = message->body.sysconReplyWords.words[0];
		}
		if (events & 2) {
			command = 3;
			if (control->sysconPowerRequestPending != 0) {
				command = 9;
				control->sysconPowerRequestPending = 0;
			}
			bbmcSendSimpleMessage(control, command);
		}
		bbmcLeaveCS(&control->mutex);
		if (control->suspendState == 4) {
			reply = msgAllocMsgPacket(control, 48);
			if (reply != NULL) {
				reply->body.commandResult = 0;
				if (message->body.sysconReplyWords.words[0] & 0x86)
					reply->body.commandResult = 1;
				if (msgSendMsgPacket(control, reply) < 0)
					msgFreeMsgPacket(control, reply);
			}
		}
		events = message->body.sysconReplyWords.words[0];
		if ((events & 8) && control->registrationPending != 0) {
			bbmcSendSimpleMessage(control, 69);
			control->registrationPending = 0;
			events = message->body.sysconReplyWords.words[0];
		}
		if (events & 0x20) {
			bbmcSendSimpleMessage(control, 4);
			events = message->body.sysconReplyWords.words[0];
		}
		if (events & 1) {
			seqClearSimDetected();
			bbmcSendSimpleMessage(control, 8);
			events = message->body.sysconReplyWords.words[0];
		}
		if (events & 0x40) {
			sysconEventPending = 1;
			result = bbmcSendSysconCommand(control, bbmcSysconEventReply,
				&control->eventPacket, 6, 2, 0);
			if (result < 0)
				goto fail;
			sysconEventStep = 2;
			return 1;
		}
		if ((control->suspendState & ~4) == 0 && (events & 0x10) &&
		    control->currentState > 1 && control->targetState > 1)
			bbmcSendSimpleMessage(control, 6);
		events = message->body.sysconReplyWords.words[1];
		if (events & 2) {
			bbmcSendSimpleMessage(control, 12);
			events = message->body.sysconReplyWords.words[1];
		}
		if (events & 1) {
			bbmcSendSimpleMessage(control, 13);
			events = message->body.sysconReplyWords.words[1];
		}
		if (events & 0x10) {
			if ((control->suspendState & ~4) != 0) {
				control->resumePending = 1;
				scePowerRequestResume();
			} else {
				bbmcNotifyFeatureEvent(control, 2, 1);
			}
			events = message->body.sysconReplyWords.words[1];
		}
		if (events & 0x20) {
			bbmcSendSimpleMessage(control, 17);
			events = message->body.sysconReplyWords.words[1];
		}
		if (events & 0x40) {
			bbmcSendSimpleMessage(control, 15);
			events = message->body.sysconReplyWords.words[1];
		}
		if (events & 0x80)
			bbmcSendSimpleMessage(control, 16);
		break;
	case 2:
		if (control->flags & 2)
			goto cancel;
		command = message->command;
		if (command != 23)
			goto other;
		if (control->secondBootReply != message->body.sysconReplyHalfword)
			bbmcSendSimpleMessage(control, 67);
		break;
	default:
		result = (int)0x80330001;
		goto fail;
	}
	sysconEventStep = 0;
	return 0;
cancel:
	bbmcCancelSysconCommand(&control->eventPacket, &sysconEventPending);
	sysconEventStep = 0;
	return 0;
other:
	if (command == 1)
		sysconEventDeferred = 1;
	return 1;
fail:
	sysconEventStep = 0;
	bbmcCancelSysconCommand(&control->eventPacket, &sysconEventPending);
	return result;
}

/* name unknown: forgets that the syscon reported a SIM card */
__attribute__((noinline)) void seqClearSimDetected(void)
{
	simDetectFlags &= ~0x20;
}

/* name unknown: message 4 reads the syscon's status word; bit 0x20 is the SIM card */
int seqSysconStatus(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 status;
	int result;

	switch (sysconStatusStep) {
	case 0:
		if (message->command != 4)
			return 1;
		sysconStatusPending = 1;
		result = bbmcSendSysconCommand(control, bbmcSysconStatusReply,
			&control->statusPacket, 0x18, 4, 0);
		if (result >= 0) {
			sysconStatusStep = 1;
			return 1;
		}
		break;
	case 1:
		if ((control->flags & ~4) != 0) {
			bbmcCancelSysconCommand(&control->statusPacket,
						&sysconStatusPending);
			simDetectFlags = 0;
		} else {
			if (message->command != 5)
				return 1;
			bbmcEnterCS(&control->mutex);
			control->sysconRefreshPending =
				(message->body.sysconReplyFlags & 0x40) != 0;
			bbmcLeaveCS(&control->mutex);
			status = message->body.sysconReplyFlags;
			if (simDetectFlags & 0x20) {
				if ((status & 0x20) == 0) {
					simDetectFlags &= ~0x20;
					if (control->suspendState == 0 &&
					    (control->currentState != 0 ||
					     control->targetState != 0) &&
					    control->simCommandBusy == 0) {
						bbmcNotifyFeatureEvent(control, 4, 2);
						bbmcEnterCS(&control->mutex);
						control->simRefreshPending = 1;
						bbmcLeaveCS(&control->mutex);
						bbmcSendSimpleMessage(control, 11);
						bbcmdExecute(0, 0x21, NULL, NULL,
							     bbcmdManagerCompletion, control);
					}
				}
			} else if (status & 0x20) {
				simDetectFlags |= 0x20;
				bbmcSendSimpleMessage(control, 10);
				bbmcNotifyFeatureEvent(control, 4, 1);
			}
		}
		sysconStatusStep = 0;
		return 0;
	default:
		result = (int)0x80330001;
		break;
	}
	sysconStatusStep = 0;
	bbmcCancelSysconCommand(&control->statusPacket, &sysconStatusPending);
	return result;
}

/* name unknown: the step after each refresh stage, and the answer the modem gets for it */
static const SceUInt32 refreshStageNextStep[4] = { 3, 3, 5, 3 };
static const SceUInt32 refreshStageResponse[4] = { 1, 2, 0, 3 };

/* name unknown: message 17 runs a SIM refresh: read the refresh, answer it, reread the files */
int seqSimRefresh(BbmcControl *control, BbmcMessage *message)
{
	unsigned int completed;
	SceUInt32 index, stage, next;
	int result;

	switch (simRefreshStep) {
	case 0:
		if (message->command != 17)
			return 1;
		completed = 0;
		result = bbcmdGetCompletedMask(0, &completed);
		if (result < 0)
			break;
		if (completed & 4) {
			BbcmdArgument output[2];

			bbmcEnterCS(&control->mutex);
			memset(&control->refreshDetail, 0, sizeof(control->refreshDetail));
			bbmcLeaveCS(&control->mutex);
			output[0].pointer = &control->refreshDetail;
			output[1].pointer = &control->refreshCommandValue;
			result = bbcmdExecute(0, 0x2c, NULL, output,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				break;
			simRefreshStep = 1;
			return 1;
		}
		goto done;
	case 1:
		if (control->flags & 4) {
			BbcmdArgument input[2];

			control->pinCommandDeferred = 1;
			input[0].value = 1;
			input[1].value = 44;
			result = bbcmdExecute(0, 0, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result >= 0)
				goto finish;
			break;
		}
		if (message->command != 76)
			return 1;
		if (message->body.commandResult != 0) {
			result = 0;
			break;
		}
		for (index = 0; index < control->refreshDetail.fileCount; index++) {
			if (control->refreshDetail.files[index].fileKind - 18 > 3)
				continue;
			bbmcSendSimpleMessage(control, 6);
			stage = control->refreshDetail.stage;
			if (stage > 3) {
				next = 5;
			} else {
				next = refreshStageNextStep[stage];
				if (next != 5) {
					BbcmdArgument input[7] = {
						{ .value = refreshStageResponse[stage] }
					};

					if (bbcmdExecute(0, 0x2d, input, NULL,
							 bbcmdManagerCompletion, control) < 0)
						next = 5;
				}
			}
			simRefreshStep = next;
			goto done;
		}
		bbmcNotifyFeatureEvent(control, 4, 0x8000);
		simRefreshStep = 2;
		return 1;
	case 2:
		if (message->command == 171) {
			if (control->flags & 4) {
				bbmcNotifyFeatureEvent(control, 4, 0x20000);
				goto done;
			} else {
				BbcmdArgument input[7] = {
					{ .value = message->body.commandResult }
				};

				result = bbcmdExecute(0, 0x2d, input, NULL,
						      bbcmdManagerCompletion, control);
				if (result < 0)
					break;
				simRefreshStep = 3;
				return 1;
			}
		}
		if (message->command == 173) {
			BbcmdArgument input[7] = {
				{ .value = message->body.commandResult }
			};
			BbcmdArgument output[6] = {{0}};

			bbmcEnterCS(&control->mutex);
			memset(&control->simContent, 0, sizeof(control->simContent));
			bbmcLeaveCS(&control->mutex);
			output[0].pointer = &control->simContent;
			result = bbcmdExecute(0, 0x2f, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				break;
			simRefreshStep = 4;
		}
		return 1;
	case 3:
		if (message->command != 172)
			return 1;
		if (message->body.commandResult != 0)
			bbmcNotifyFeatureEvent(control, 4, 0x20000);
		else
			bbmcNotifyFeatureEvent(control, 4, 0x10000);
		goto finish;
	case 4:
		if (message->command == 174) {
			if (message->body.commandResult != 0)
				bbmcNotifyFeatureEvent(control, 4, 0x200000);
			else
				bbmcNotifyFeatureEvent(control, 4, 0x100000);
			simRefreshStep = 2;
		}
		return 1;
	default:
		result = (int)0x80330001;
		break;
	}
	simRefreshStep = 0;
	return result;
done:
	if (simRefreshStep != 5)
		return 1;
finish:
	simRefreshStep = 0;
	return 0;
}

/* name unknown: message 111 reads the packet channel's current and maximum rates */
int seqGetChannelRates(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7] = {{0}};
	BbcmdArgument output[6] = {{0}};
	int result;

	switch (channelRatesStep) {
	case 0:
		if (message->command != 111 || control->networkStarted != 1)
			return 1;
		output[0].pointer = message->body.channelRatesMessage.txRate;
		output[1].pointer = message->body.channelRatesMessage.rxRate;
		output[2].pointer = message->body.channelRatesMessage.maxTxRate;
		output[3].pointer = message->body.channelRatesMessage.maxRxRate;
		result = bbcmdExecute(1, 0x18, input, output, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			break;
		channelRatesStep = 1;
		return 1;
	case 1:
		if (message->command != 112)
			return channelRatesStep;
		result = message->body.channelRatesMessage.result;
		channelRatesStep = 2;
		if (result >= 0)
			return 0;
		break;
	default:
		channelRatesStep = 2;
		return (int)0x80330001;
	}
	channelRatesStep = 0;
	return result;
}

/* name unknown: syscon read the proactive word, message 165 hands it to the sequence */
int proactiveReplyCallback(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message = msgAllocMsgPacket(control, 165);
	SceUInt32 step = proactiveStep;

	if (message == NULL)
		goto send;
	if (packet->request.command[0] != 0x80)
		goto send;
	if (packet->request.command[1] != 0x0a)
		goto send;
	{
		if (step == 1) {
			message->body.proactiveReply.fetchRequested = 1;
			message->body.proactiveReply.sysconValue =
				((SceUInt32)packet->reply.data[2] << 16) |
				((SceUInt32)packet->reply.data[3] << 24) |
				packet->reply.data[0] |
				((SceUInt32)packet->reply.data[1] << 8);
		} else if (step == 2) {
			message->body.proactiveReply.fetchRequested = 0;
			message->body.proactiveReply.sysconValue =
				((SceUInt32)packet->reply.data[2] << 16) |
				((SceUInt32)packet->reply.data[3] << 24) |
				packet->reply.data[0] |
				((SceUInt32)packet->reply.data[1] << 8);
		}
	}
send:
	if (msgSendMsgPacket(control, message) < 0)
		msgFreeMsgPacket(control, message);
	proactivePending = 0;
	return 0;
}

/* name unknown: syscon accepted the modem wakeup, message 117 reports it */
int modemWakeupCallback(SceSysconPacket *packet, void *argp)
{
	BbmcControl *control = argp;
	BbmcMessage *message;
	SceUInt16 command;

	__builtin_memcpy(&command, packet->request.command, sizeof(command));
	if (command == 0x0a81) {
		message = msgAllocMsgPacket(control, 117);
		if (message != NULL) {
			message->body.commandResult = 0;
			if (msgSendMsgPacket(control, message) < 0)
				msgFreeMsgPacket(control, message);
		}
	}
	modemWakeupPending = 0;
	return 0;
}

/* name unknown: messages 15, 16 and 169 run a SIM toolkit proactive command or envelope */
int seqProactiveCommand(BbmcControl *control, BbmcMessage *message)
{
	unsigned int completed;
	int result;

	switch (proactiveStep) {
	case 0:
		if (message->command != 169 && message->command - 15 > 1)
			return 1;
		completed = 0;
		result = bbcmdGetCompletedMask(0, &completed);
		if (result < 0)
			goto cancel;
		if ((completed & 4) == 0)
			goto done;
		if (message->command == 169) {
			BbcmdArgument input[7];

			input[0].value = message->body.commandResult;
			result = bbcmdExecute(0, 0x2e, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				goto cancel;
			proactiveStep = 6;
			return 1;
		} else if (message->command == 15) {
			proactiveStep = 1;
		} else if (message->command == 16) {
			proactiveStep = 2;
		}
		proactivePending = 1;
		result = bbmcSendSysconCommand(control, proactiveReplyCallback,
			&control->proactivePacket, 0x10, 4, 0);
		if (result < 0)
			goto cancel;
		goto done;
	case 1:
		if (control->flags != 0) {
			bbmcCancelSysconCommand(&control->proactivePacket,
						&proactivePending);
			if (control->flags & 4)
				control->pinCommandInterrupted = 1;
			goto finish;
		}
		if (message->command != 165)
			return 1;
		{
			BbcmdArgument output[6] = {{0}};
			BbcmdArgument input[7] = {{0}};

			bbmcEnterCS(&control->mutex);
			memset(&control->proactiveCommand, 0,
			       sizeof(control->proactiveCommand));
			bbmcLeaveCS(&control->mutex);
			input[0].value = message->body.proactiveReply.sysconValue;
			output[0].pointer = &control->proactiveCommand;
			input[1].value = message->body.proactiveReply.fetchRequested;
			result = bbcmdExecute(0, 0x2a, input, output,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				proactiveStep = 3;
				return 1;
			}
		}
		goto cancel;
	case 2:
		result = 0;
		goto cancel;
	case 3:
		if (control->flags & 4) {
			BbcmdArgument input[2];

			control->pinCommandInterrupted = 1;
			input[0].value = 1;
			input[1].value = 42;
			result = bbcmdExecute(0, 0, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result >= 0)
				goto finish;
			goto cancel;
		}
		if (message->command != 166)
			return 1;
		if (message->body.commandResult == 0) {
			bbmcNotifyFeatureEvent(control, 4, 0x1000);
			proactiveStep = 4;
			return 1;
		}
		result = 0;
		goto cancel;
	case 4:
		if (message->command != 167)
			return 1;
		if ((control->flags & 4) == 0) {
			BbcmdArgument input[7];

			input[0].value = message->body.commandResult;
			result = bbcmdExecute(0, 0x2b, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				proactiveStep = 5;
				return 1;
			}
			goto cancel;
		}
		bbmcNotifyFeatureEvent(control, 4, 0x4000);
		goto done;
	case 5:
		if (message->command != 168)
			return 1;
		if (message->body.commandResult != 0) {
			bbmcNotifyFeatureEvent(control, 4, 0x4000);
			proactiveStep = 4;
			return 1;
		}
		bbmcNotifyFeatureEvent(control, 4, 0x2000);
		goto finish;
	case 6:
		if (message->command != 170)
			return 1;
		bbmcNotifyFeatureEvent(control, 4,
			message->body.commandResult != 0 ? 0x80000 : 0x40000);
		goto finish;
	default:
		result = (int)0x80330001;
		goto cancel;
	}
cancel:
	proactiveStep = 0;
	bbmcCancelSysconCommand(&control->proactivePacket, &proactivePending);
	return result;
done:
	if (proactiveStep != 7)
		return 1;
finish:
	proactiveStep = 0;
	return 0;
}

/* name unknown: message 184 rereads the card and PIN status, then runs held power requests */
int seqRefreshPinStatus(BbmcControl *control, BbmcMessage *message)
{
	unsigned int completed;
	SceUInt32 command;
	int ready = 0;
	int result;

	switch (pinStatusStep) {
	case 0:
		if (message->command != 184 || control->currentState <= 1)
			return 1;
		completed = 0;
		result = bbcmdGetCompletedMask(0, &completed);
		if (result < 0)
			goto failed;
		if ((completed & 5) == 0)
			goto check;
		result = bbcmdExecute(0, 0x21, NULL, NULL, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			goto failed;
		pinStatusStep = 1;
		return 1;
	case 1:
		command = message->command;
		if (command != 185) {
			if (command == 184)
				pinStatusRequeued = pinStatusStep;
			return 1;
		}
		if (message->body.pinMessage.status.result != 0) {
			result = 0;
			pinStatusStep = 3;
			goto requeue;
		}
		bbmcEnterCS(&control->mutex);
		if (control->simRefreshPending != 0) {
			result = 0;
		} else if ((SceInt32)message->body.pinMessage.detail.configuredServiceValue > 0) {
			if (control->currentState != 0 || control->targetState != 0) {
				bbmcNotifyFeatureEvent(control, 4, 2);
				seqClearSimDetected();
				control->simRefreshPending = 1;
			}
			result = bbmcSendSimpleMessage(control, 11);
			pinStatusStep = 3;
			bbmcLeaveCS(&control->mutex);
			goto requeue;
		} else {
			BbcmdArgument output[4] = {{0}};
			BbcmdArgument input[7] = { { .value = 1 } };

			__builtin_memset(&control->pinStatus, 0, sizeof(control->pinStatus));
			result = bbcmdExecute(0, 0x0b, input, output,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				pinStatusStep = 2;
				bbmcLeaveCS(&control->mutex);
				goto requeue;
			}
		}
		pinStatusStep = 3;
		bbmcLeaveCS(&control->mutex);
	requeue:
		if (pinStatusRequeued == 1)
			result = bbmcSendSimpleMessage(control, 184);
	check:
		if (pinStatusStep != 3)
			return 1;
		goto failed;
	case 2:
		if (message->command != 64)
			return 1;
		result = message->body.pinMessage.cardStatusResult;
		if (result != 0) {
			result = result < 0 ? result : 0;
		} else if (message->body.pinMessage.cardUnavailable == 1) {
			bbmcNotifyFeatureEvent(control, 4, 2);
			seqClearSimDetected();
			bbmcEnterCS(&control->mutex);
			control->simRefreshPending = 1;
			bbmcLeaveCS(&control->mutex);
			result = bbmcSendSimpleMessage(control, 11);
			result = result < 0 ? result : 0;
		} else if (message->body.pinMessage.pinStatusUnavailable == 1) {
			bbmcNotifyFeatureEvent(control, 4, 0x8000000);
			result = bbmcSendSimpleMessage(control, 11);
			result = result < 0 ? result : 0;
		} else {
			ready = 1;
			if (control->pinCommandInterrupted != 0) {
				control->pinCommandInterrupted = 0;
				command = 15;
			} else {
				result = 0;
				if (control->pinCommandDeferred == 0)
					break;
				control->pinCommandDeferred = 0;
				command = 17;
			}
			result = bbmcSendSimpleMessage(control, command);
			result = result < 0 ? result : 0;
		}
		break;
	default:
		result = (int)0x80330001;
		break;
	}
	goto finish;
failed:
	result = result < 0 ? result : 0;
finish:
	pinStatusRequeued = 0;
	pinStatusStep = 0;
	bbmcEnterCS(&control->mutex);
	if (control->pendingPowerDown != 0) {
		control->pendingPowerDown = 0;
		if (control->currentState == 6 && ready &&
		    control->powerTransitionBusy == 0 &&
		    control->powerEventActive == 0 &&
		    control->recentPowerEvent == 0) {
			control->powerTransitionBusy = 1;
			result = bbmcSendSimpleMessage(control, 30);
		}
	}
	if (control->pendingPowerUp != 0) {
		control->pendingPowerUp = 0;
		if (control->currentState == 7 && ready &&
		    control->powerTransitionBusy == 0) {
			control->powerTransitionBusy = 1;
			result = bbmcSendSimpleMessage(control, 29);
		}
	}
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* name unknown: message 101 runs one GPS request through bbcmd and tells the GPS driver */
int seqGpsRequest(BbmcControl *control, BbmcMessage *message)
{
	BbcmdArgument input[7];
	int result;

	switch (gpsRequestStep) {
	case 0:
		if (message->command != 101)
			return 1;
		input[0].value = message->body.gpsRequest.code;
		input[1].value = (SceUInt32)message->body.gpsRequest.input;
		input[2].pointer = message->body.gpsRequest.output;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		result = bbcmdExecute(0, 0x17, input, NULL, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			break;
		gpsRequestStep = 1;
		return result;
	case 1:
		if (message->command != 102)
			return gpsRequestStep;
		bbmcEnterCS(&control->mutex);
		result = (int)0x80330001;
		if (control->gpsIoctlPending != 0) {
			control->gpsIoctlPending = 0;
			result = 1;
			if (message->body.commandResult != 0)
				SceGpsForDriver_F2049C02(0, 0x60032);
			else
				SceGpsForDriver_F2049C02(0, 0x60000);
		}
		bbmcLeaveCS(&control->mutex);
		gpsRequestStep = 0;
		if (result >= 0)
			return 0;
		goto clear;
	default:
		result = (int)0x80330001;
		break;
	}
	gpsRequestStep = 0;
clear:
	bbmcEnterCS(&control->mutex);
	control->gpsIoctlPending = 0;
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* name unknown: message 84 lists the SMS indices stored on the SIM */
int seqListSms(BbmcControl *control, BbmcMessage *message)
{
	int result;

	switch (smsListStep) {
	case 0:
		if (message->command == 84) {
			BbcmdArgument output[6];
			BbcmdArgument input[7];

			output[0].pointer = message->body.smsListOutput.indices;
			output[1].pointer = message->body.smsListOutput.count;
			output[2].value = 0;
			output[3].value = 0;
			output[4].value = 0;
			output[5].value = 0;
			input[1].value = 0;
			input[2].value = 0;
			input[3].value = 0;
			input[4].value = 0;
			input[5].value = 0;
			input[6].value = 0;
			input[0].value = 64;
			result = bbcmdExecute(0, 0x0f, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0) {
				bbmcNotifyFeatureEvent(control, 2, 0x40);
				break;
			}
			smsListStep = 1;
		}
		return 1;
	case 1:
		if (message->command != 85)
			return 1;
		bbmcEnterCS(&control->mutex);
		control->smsListBusy = 2;
		bbmcLeaveCS(&control->mutex);
		if (message->body.smsMessage.completion.listResult != 0)
			bbmcNotifyFeatureEvent(control, 2, 0x40);
		else
			bbmcNotifyFeatureEvent(control, 2, 2);
		smsListStep = 0;
		return 0;
	default:
		result = (int)0x80330001;
		break;
	}
	smsListStep = 0;
	bbmcEnterCS(&control->mutex);
	control->smsListBusy = 2;
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* name unknown: message 152 changes the packet attach setting */
int seqChangeAttach(BbmcControl *control, BbmcMessage *message)
{
	int result;

	switch (attachChangeStep) {
	case 0:
		if (message->command != 152)
			return 1;
		{
			BbcmdArgument output[6] = {{0}};
			BbcmdArgument input[7] = {
				{ .value = message->body.commandResult }
			};

			result = bbcmdExecute(0, 0x1d, input, output,
					      bbcmdManagerCompletion, control);
		}
		if (result >= 0) {
			attachChangeStep = 1;
			return 1;
		}
		attachChangeStep = 2;
		break;
	case 1:
		if (message->command != 153)
			return 1;
		if (message->body.commandResult != 0)
			bbmcNotifyFeatureEvent(control, 0, 0x40000);
		else
			bbmcNotifyFeatureEvent(control, 0, 0x20000);
		bbmcEnterCS(&control->mutex);
		control->attachChangeBusy = 0;
		bbmcLeaveCS(&control->mutex);
		attachChangeStep = 0;
		return 0;
	default:
		result = (int)0x80330001;
		attachChangeStep = 2;
		break;
	}
	bbmcEnterCS(&control->mutex);
	control->attachChangeBusy = 0;
	bbmcLeaveCS(&control->mutex);
	bbmcNotifyFeatureEvent(control, 0, 0x40000);
	attachChangeStep = 0;
	return result;
}

/* name unknown: message 149 scans for networks, dropping the packet link first */
int seqScanNetworks(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 linkUp;
	int result;

	switch (networkScanStep) {
	case 0:
		if (message->command != 149)
			return 1;
		bbmcEnterCS(&control->mutex);
		linkUp = control->pendingPacketRequest;
		bbmcLeaveCS(&control->mutex);
		if (linkUp == 1) {
			result = DisconnectNetwork();
			if (result >= 0) {
				networkScanStep = 2;
				return 1;
			}
		} else {
			BbcmdArgument output[6] = {{0}};
			BbcmdArgument input[7] = {{0}};

			memset(&control->networkScan, 0, sizeof(control->networkScan));
			output[0].pointer = &control->networkScan;
			result = bbcmdExecute(0, 0x1c, input, output,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				networkScanStep = 1;
				return 1;
			}
		}
		networkScanStep = 3;
		goto failed;
	case 1:
		if (message->command - 150 > 1)
			return 1;
		if (message->command == 150) {
			SceUInt32 event = message->body.commandResult != 0 ?
				0x10000 : 0x8000;

			bbmcEnterCS(&control->mutex);
			control->networkScanState = 2;
			control->networkScanAbortBusy = 0;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, 0, event);
			goto finish;
		} else {
			BbcmdArgument input[2];

			input[0].value = 1;
			input[1].value = 28;
			result = bbcmdExecute(0, 0, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result < 0) {
				if (networkScanStep == 3)
					goto failed;
				return result;
			}
			if (networkScanStep == 3)
				goto finish;
			return 1;
		}
	case 2:
		if (message->command != 154)
			return 1;
		result = bbmcSendSimpleMessage(control, 149);
		if (result >= 0)
			goto finish;
		networkScanStep = 3;
		goto failed;
	default:
		result = (int)0x80330001;
		networkScanStep = 3;
		goto failed;
	}
failed:
	bbmcEnterCS(&control->mutex);
	control->networkScanState = 2;
	control->networkScanAbortBusy = 0;
	bbmcLeaveCS(&control->mutex);
	bbmcNotifyFeatureEvent(control, 0, 0x10000);
	networkScanStep = 0;
	return result;
finish:
	networkScanStep = 0;
	return 0;
}

/* name unknown: message 113 sets or clears packet interface control flags, cycling the link */
int seqChangeControlFlags(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 flags, newFlags, set, mask, initialized, command;
	int result;

	switch (controlFlagStep) {
	case 0:
		if (message->command != 113)
			return 0;
		set = message->body.controlFlag.set;
		mask = message->body.controlFlag.mask;
		controlFlagSet = set;
		controlFlagMask = mask;
		initialized = control->clientInitialized;
		flags = control->controlFlags;
		newFlags = set != 0 ? flags | mask : flags & ~mask;
		if (flags != newFlags && (newFlags & 3) == 1 && initialized != 0) {
			result = DisconnectNetwork();
			if (result >= 0 || result == (int)0x80330507) {
				controlFlagStep = 1;
				return 1;
			}
			if (result != (int)0x80330508)
				goto failed;
		}
		controlFlagStep = 2;
		result = bbmcSendSimpleMessage(control, 188);
		if (result < 0)
			goto failed;
		break;
	case 1:
		command = message->command;
		if (command == 106) {
			if (message->body.commandResult != 0) {
				result = 0;
				goto finish;
			}
		} else if (command != 57 && command != 108) {
			return 0;
		}
		controlFlagStep = 2;
		return 1;
	case 2:
		set = controlFlagSet;
		mask = controlFlagMask;
		bbmcEnterCS(&control->mutex);
		flags = control->controlFlags;
		newFlags = set != 0 ? flags | mask : flags & ~mask;
		control->controlFlags = newFlags;
		control->controlFlagsBusy = 0;
		control->normalizedControlState = (newFlags & 3) == 1;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 5, 4);
		result = 0;
		goto finish;
	default:
		result = (int)0x80330001;
		break;
	}
	if (controlFlagStep != 3)
		return result;
failed:
	result = result < 0 ? result : 0;
finish:
	controlFlagStep = 0;
	return result;
}

/* name unknown: messages 145 (read) and 147 (write) run one setup value through bbcmd */
int seqAccessSetupValue(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command, event, deferredEvent, kind;
	int result;

	switch (setupStep) {
	case 0:
		if (message->command == 145) {
			BbcmdArgument output[6];
			BbcmdArgument input[7];

			kind = message->body.setupMessage.type;
			bbmcEnterCS(&control->mutex);
			switch (kind) {
			case 0:
				if (control->currentState == 4)
					goto badRead;
				break;
			case 2:
			case 3:
			case 4:
			case 6:
				break;
			default:
			badRead:
				bbmcLeaveCS(&control->mutex);
				bbmcEnterCS(&control->mutex);
				control->setupReadState = 2;
				bbmcLeaveCS(&control->mutex);
				bbmcNotifyFeatureEvent(control, 0, 0x40);
				goto finish;
			}
			bbmcLeaveCS(&control->mutex);
			output[0].pointer = message->body.setupMessage.outputBuffer;
			input[1].value = message->body.setupMessage.valueSize;
			input[0].value = message->body.setupMessage.type;
			output[1].value = 0;
			output[2].value = 0;
			output[3].value = 0;
			output[4].value = 0;
			output[5].value = 0;
			input[2].value = 0;
			input[4].value = 0;
			input[5].value = 0;
			input[6].value = 0;
			input[3].pointer = message->body.setupMessage.outputBuffer;
			result = bbcmdExecute(0, 0x24, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0) {
				bbmcEnterCS(&control->mutex);
				control->setupReadState = 2;
				bbmcLeaveCS(&control->mutex);
				setupStep = 2;
				goto failed;
			}
		} else if (message->command == 147) {
			BbcmdArgument output[6] = {{0}};
			BbcmdArgument input[7] = {{0}};

			kind = message->body.setupMessage.type;
			input[0].value = kind;
			input[1].value = message->body.setupMessage.valueSize;
			input[2].value = 1;
			input[3].value = (SceUInt32)message->body.setupMessage.inputBuffer;
			bbmcEnterCS(&control->mutex);
			switch (kind) {
			case 1:
				output[0].pointer = &control->smscAddress;
				break;
			case 2:
			case 3:
				output[0].pointer = &control->pinStatus;
				break;
			case 4:
			case 6:
				break;
			case 5:
				output[0].pointer = &control->profile;
				break;
			case 7:
				output[0].pointer = &control->countedBytes;
				break;
			default:
				result = (int)0x80330002;
				bbmcLeaveCS(&control->mutex);
				bbmcEnterCS(&control->mutex);
				control->setupWriteState = 0;
				bbmcLeaveCS(&control->mutex);
				setupStep = 2;
				goto failed;
			}
			bbmcLeaveCS(&control->mutex);
			result = bbcmdExecute(0, 0x24, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0) {
				bbmcEnterCS(&control->mutex);
				control->setupWriteState = 0;
				bbmcLeaveCS(&control->mutex);
				setupStep = 2;
				goto failed;
			}
		} else {
			return 1;
		}
		setupStep = 1;
		return 1;
	case 1:
		if (message->command != 148 && message->command != 146)
			return 1;
		bbmcEnterCS(&control->mutex);
		if (message->command == 146)
			control->setupReadState = 2;
		else
			control->setupWriteState = 0;
		bbmcLeaveCS(&control->mutex);
		command = message->command;
		if (message->body.setupMessage.result >= 0) {
			if (command == 146) {
				event = 0x40;
				kind = message->body.setupMessage.type - 2;
				goto notify;
			}
			if (command == 148) {
				if (message->body.setupMessage.type - 2 > 1) {
					event = 0x100;
					goto notifyOperation;
				}
				event = 0x2000000;
				goto notifySim;
			}
			event = 0;
			kind = message->body.setupMessage.type - 2;
			goto notify;
		}
		if (command == 146) {
			event = 0x80;
			kind = message->body.setupMessage.type - 2;
			goto notify;
		}
		if (command != 148) {
			event = 0;
			kind = message->body.setupMessage.type - 2;
			goto notify;
		}
		kind = message->body.setupMessage.type - 2;
		if (kind >= 2) {
			deferredEvent = 0xa00;
			event = 0x200;
		} else {
			deferredEvent = 0x4000800;
			event = 0x4000000;
		}
		if (message->body.setupMessage.completionFlag != 1)
			goto notify;
		if (control->pinStatus.pin1VerifyRetries != 0) {
			event = deferredEvent;
			goto notify;
		}
		result = bbmcSendSimpleMessage(control, 36);
		if (result >= 0) {
			event = deferredEvent;
			kind = message->body.setupMessage.type - 2;
			goto notify;
		}
		setupStep = 2;
		goto failed;
	default:
		result = (int)0x80330001;
		setupStep = 2;
		goto failed;
	}
notify:
	if (kind <= 1)
		goto notifySim;
notifyOperation:
	bbmcNotifyFeatureEvent(control, 0, event);
	goto finish;
notifySim:
	bbmcNotifyFeatureEvent(control, 4, event);
	memset(&control->setupValue, 0, sizeof(control->setupValue));
finish:
	setupStep = 0;
	return 0;
failed:
	command = message->command;
	if (command - 147 <= 1)
		bbmcNotifyFeatureEvent(control, 4, 0x4000000);
	else if (command - 145 <= 1)
		bbmcNotifyFeatureEvent(control, 0, 0x80);
	setupStep = 0;
	return result;
}

/* name unknown: posts message 35 with one result word */
int seqSendResultMessage(BbmcControl *control, SceInt32 value)
{
	BbmcMessage *message;
	int result;

	if (control == NULL)
		return (int)0x80330002;
	message = msgAllocMsgPacket(control, 35);
	if (message == NULL)
		return (int)0x80330005;
	message->body.commandResult = value;
	result = msgSendMsgPacket(control, message);
	if (result < 0) {
		msgFreeMsgPacket(control, message);
		return result;
	}
	return result;
}

/* name unknown: message 34 unlocks the SIM: read the PIN state, then verify or unblock it */
int seqUnlockPin(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 step = pinUnlockStep;
	SceUInt32 flags, command, pinUnavailable;
	SceInt32 value, reported;
	unsigned int completed;
	SceUInt8 retries;
	BbmcMessage *reply;
	int result;

	switch (step) {
	case 0:
		if (message->command != 34)
			return 1;
		flags = control->flags;
		if (flags & 5) {
			value = (flags & 1) ? (int)0x80330006 : (int)0x8033000a;
			reply = msgAllocMsgPacket(control, 35);
			if (reply == NULL) {
				result = (int)0x80330005;
				goto check;
			}
			reply->body.commandResult = value;
			result = msgSendMsgPacket(control, reply);
			if (result < 0) {
				msgFreeMsgPacket(control, reply);
				goto check;
			}
			pinUnlockStep = 0;
			return 1;
		}
		completed = 0;
		result = bbcmdGetCompletedMask(0, &completed);
		if (result < 0)
			goto failed;
		if ((completed & 4) == 0)
			goto check;
		{
			BbcmdArgument output[6] = {{0}};
			BbcmdArgument input[7] = {{0}};

			bbmcEnterCS(&control->mutex);
			memset(&control->unblockPin, 0, sizeof(control->unblockPin));
			control->pinStatus.pin1UnblockRetries = 0;
			control->pinStatus.pin1VerifyRetries = 0;
			input[0].value = 1;
			output[1].pointer = &control->pinStatus.pin1VerifyRetries;
			output[2].pointer = &control->pinStatus.pin1UnblockRetries;
			bbmcLeaveCS(&control->mutex);
			result = bbcmdExecute(0, 0x0b, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				goto failed;
		}
		pinUnlockStep = 1;
		return 1;
	case 1:
		flags = control->flags;
		if (flags & 5) {
			value = (flags & 1) ? (int)0x80330006 : (int)0x8033000a;
			reply = msgAllocMsgPacket(control, 35);
			if (reply == NULL) {
				result = (int)0x80330005;
				goto failed;
			}
sendReply:
			reply->body.commandResult = value;
			result = msgSendMsgPacket(control, reply);
			if (result < 0)
				msgFreeMsgPacket(control, reply);
			goto failed;
		}
		command = message->command;
		if (command != 64)
			goto other;
		result = message->body.pinMessage.cardStatusResult;
		if (result != 0)
			goto failed;
		bbmcEnterCS(&control->mutex);
		if (message->body.pinMessage.cardUnavailable == 1) {
			control->pinStatus.pin1Status = 3;
			reported = 3;
			pinUnlockStep = 7;
			goto report;
		}
		pinUnavailable = message->body.pinMessage.pinStatusUnavailable;
		if (message->body.pinMessage.applicationNotReady == 1) {
			if (pinUnavailable == 1) {
				BbcmdArgument output[1];

				bbmcEnterCS(&control->mutex);
				memset(control->indexedResponses, 0,
				       sizeof(control->indexedResponses));
				output[0].pointer = control->indexedResponses;
				bbmcLeaveCS(&control->mutex);
				pinUnlockStep = 6;
				result = bbcmdExecute(0, 0x31, NULL, output,
						      bbcmdManagerCompletion, control);
				if (result < 0)
					goto failed;
				goto leave;
			}
		} else if (pinUnavailable == 1) {
			reported = 4;
			pinUnlockStep = 7;
			goto report;
		}
		if (message->body.pinMessage.status.flags.blocked == 1) {
			bbmcNotifyFeatureEvent(control, 4, 8);
			control->pinStatus.pin1Status = 1;
			pinUnlockStep = 3;
		} else if (message->body.pinMessage.status.flags.enabled != 1 ||
			   message->body.pinMessage.status.flags.verified != 0) {
			if (message->body.pinMessage.status.flags.enabled == 0 ||
			    message->body.pinMessage.status.flags.verified == 1) {
				bbmcNotifyFeatureEvent(control, 4, 0x40);
				control->pinStatus.pin1Status = 0;
				reported = 0;
				pinUnlockStep = 7;
			} else {
				reported = 1;
				bbmcNotifyFeatureEvent(control, 4, 0x80);
				control->pinStatus.pin1Status = 3;
				pinUnlockStep = 7;
			}
report:
			bbmcLeaveCS(&control->mutex);
			reply = msgAllocMsgPacket(control, 35);
			if (reply == NULL) {
				result = (int)0x80330005;
				goto failed;
			}
			reply->body.commandResult = reported;
			result = msgSendMsgPacket(control, reply);
			if (result < 0) {
				msgFreeMsgPacket(control, reply);
				goto failed;
			}
			goto check;
		} else {
			bbmcNotifyFeatureEvent(control, 4, 4);
			control->pinStatus.pin1Status = 2;
			pinUnlockStep = 2;
		}
leave:
		bbmcLeaveCS(&control->mutex);
		goto check;
	case 2:
	case 3:
		if (control->flags != 0) {
			bbmcNotifyFeatureEvent(control, 4, pinUnlockStep == 3 ? 32 : 16);
			reply = msgAllocMsgPacket(control, 35);
			if (reply == NULL) {
				result = (int)0x80330005;
				goto check;
			}
			reply->body.commandResult = 1;
			result = msgSendMsgPacket(control, reply);
			if (result >= 0)
				goto failed;
			msgFreeMsgPacket(control, reply);
			goto check;
		}
		switch (message->command) {
		case 11:
			goto cancelled;
		case 32: {
			BbcmdArgument input[7] = { { .value = control->flags } };
			SceUInt32 pukLength, pinLength;

			bbmcEnterCS(&control->mutex);
			pukLength = control->unblockPin.pukLength;
			input[1].value = pukLength;
			pinLength = control->unblockPin.pinLength;
			if (pukLength - 8 <= 25)
				input[0].pointer = control->unblockPin.puk;
			input[3].value = pinLength;
			if (pinLength - 4 <= 5)
				input[2].pointer = control->unblockPin.pin;
			bbmcLeaveCS(&control->mutex);
			result = bbcmdExecute(0, 0x0d, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				pinUnlockStep = 5;
				return 1;
			}
			break;
		}
		case 33: {
			BbcmdArgument input[7] = { { .value = control->flags } };
			SceUInt32 pinLength;

			bbmcEnterCS(&control->mutex);
			pinLength = control->unblockPin.pinLength;
			input[1].value = pinLength;
			if (pinLength <= 9)
				input[0].pointer = control->unblockPin.pin;
			bbmcLeaveCS(&control->mutex);
			result = bbcmdExecute(0, 0x0c, input, NULL,
					      bbcmdManagerCompletion, control);
			if (result >= 0) {
				pinUnlockStep = 4;
				return 1;
			}
			break;
		}
		default:
			return 1;
		}
		goto failed;
	case 4:
	case 5:
		command = message->command;
		if (command - 65 > 1)
			goto other;
		memset(&control->unblockPin, 0, sizeof(control->unblockPin));
		result = message->body.pinMessage.status.result;
		if (result < 0) {
			if (control->flags & 4) {
				result = seqSendResultMessage(control, (int)0x8033000a);
				if (result < 0)
					goto check;
				goto failed;
			}
			if (result != (int)0x8033500c &&
			    (SceUInt32)result - 0x80335023 > 1)
				goto failed;
			retries = message->body.pinMessage.detail.retries;
			if (step == 5) {
				control->pinStatus.pin1UnblockRetries = retries;
				if (message->body.pinMessage.detail.retries == 0) {
					bbmcNotifyFeatureEvent(control, 4, 0x880);
					bbmcEnterCS(&control->mutex);
					control->pinStatus.pin1Status = 3;
					bbmcLeaveCS(&control->mutex);
					result = seqSendResultMessage(control, 1);
					if (result < 0)
						goto check;
					goto failed;
				}
				bbmcNotifyFeatureEvent(control, 4, 0xa00);
				pinUnlockStep = 3;
			} else {
				control->pinStatus.pin1VerifyRetries = retries;
				if (message->body.pinMessage.detail.retries == 0) {
					bbmcNotifyFeatureEvent(control, 4, 0x808);
					control->pinStatus.pin1Status = 1;
					result = bbmcSendSimpleMessage(control, 20);
					if (result >= 0) {
						pinUnlockStep = 3;
						return 1;
					}
					goto failed;
				}
				bbmcNotifyFeatureEvent(control, 4, 0xc00);
				pinUnlockStep = 2;
			}
			result = bbmcSendSimpleMessage(control, 20);
			if (result >= 0)
				goto check;
		} else if (step == 5) {
			control->pinStatus.pin1Status = 2;
			control->pinStatus.pin1UnblockRetries = 10;
			flags = control->flags;
			control->pinStatus.pin1VerifyRetries =
				message->body.pinMessage.detail.retries;
			if (flags & 4) {
				reply = msgAllocMsgPacket(control, 35);
				if (reply == NULL) {
					result = (int)0x80330005;
					goto check;
				}
				reply->body.commandResult = (int)0x8033000a;
				result = msgSendMsgPacket(control, reply);
				if (result >= 0) {
					pinUnlockStep = 2;
					return 1;
				}
				msgFreeMsgPacket(control, reply);
				goto check;
			}
			bbmcNotifyFeatureEvent(control, 4, 4);
			result = bbmcSendSimpleMessage(control, 20);
			if (result >= 0) {
				pinUnlockStep = 2;
				return 1;
			}
		} else {
			bbmcEnterCS(&control->mutex);
			control->pinStatus.pin1Status = 4;
			retries = message->body.pinMessage.detail.retries;
			control->pinStatus.pin1UnblockRetries = 10;
			control->pinStatus.pin1VerifyRetries = retries;
			bbmcLeaveCS(&control->mutex);
			bbmcNotifyFeatureEvent(control, 4, 0x100);
			reply = msgAllocMsgPacket(control, 35);
			if (reply == NULL) {
				result = (int)0x80330005;
				goto check;
			}
			reply->body.commandResult = 0;
			result = msgSendMsgPacket(control, reply);
			if (result < 0) {
				msgFreeMsgPacket(control, reply);
				goto check;
			}
		}
		goto failed;
	case 6:
		if (message->command != 181)
			return 1;
		value = message->body.commandResult;
		if (value == 0)
			value = (bbmcCollectIndexedResponseEvents(control) & 0x1000000) ?
				2 : 4;
		if (control == NULL) {
			result = (int)0x80330002;
			goto failed;
		}
		reply = msgAllocMsgPacket(control, 35);
		if (reply == NULL) {
			result = (int)0x80330005;
			goto failed;
		}
		goto sendReply;
	default:
		result = (int)0x80330001;
		goto failed;
	}
other:
	if (command != 11)
		return 1;
cancelled:
	result = 0;
	goto failed;
check:
	if (pinUnlockStep != 7)
		return 1;
failed:
	pinUnlockStep = 0;
	memset(&control->unblockPin, 0, sizeof(control->unblockPin));
	if (result < 0) {
		if (control != NULL) {
			reply = msgAllocMsgPacket(control, 35);
			if (reply != NULL) {
				reply->body.commandResult = result;
				if (msgSendMsgPacket(control, reply) < 0)
					msgFreeMsgPacket(control, reply);
			}
		}
		return result;
	}
	return 0;
}

/* name unknown: the timer callbacks of the download sequences */
void swdlTransitionTimeoutHandler(void *arg)
{
	bbmcSendSimpleMessage(arg, 78);
}

void swdlComPowerTimeoutHandler(void *arg)
{
	bbmcSendSimpleMessage(arg, 178);
}

void swdlEdlComPowerTimeoutHandler(void *arg)
{
	bbmcSendSimpleMessage(arg, 178);
}

void swdlCarrierReplyTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	message = msgAllocMsgPacket(control, 187);
	if (message) {
		message->body.timedOutCommand = 59;
		msgSendMsgPacket(control, message);
	}
}

void swdlEdlReplyTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	message = msgAllocMsgPacket(control, 187);
	if (message) {
		message->body.timedOutCommand = 59;
		msgSendMsgPacket(control, message);
	}
}

void swdlCarrierPowerOffTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	message = msgAllocMsgPacket(control, 187);
	if (message) {
		message->body.timedOutCommand = 9;
		msgSendMsgPacket(control, message);
	}
}

void swdlEdlPowerOffTimeoutHandler(void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;

	message = msgAllocMsgPacket(control, 187);
	if (message) {
		message->body.timedOutCommand = 9;
		msgSendMsgPacket(control, message);
	}
}

/* name unknown: close the five update files the file-set download opened */
int swdlCloseUpdateFiles(BbmcControl *control)
{
	BbmcFileSetUpdateWork *work = &control->updateWork.fileSet;
	int result = 0, error;

	if (work->files[0] != 0) {
		error = udIoClose(work->files[0]);
		if (error < 0)
			result = error;
	}
	if (work->files[1] != 0) {
		error = udIoClose(work->files[1]);
		if (error < 0)
			result = error;
	}
	if (work->files[2] != 0) {
		error = udIoClose(work->files[2]);
		if (error < 0)
			result = error;
	}
	if (work->files[3] != 0) {
		error = udIoClose(work->files[3]);
		if (error < 0)
			result = error;
	}
	if (work->files[4] != 0) {
		error = udIoClose(work->files[4]);
		if (error < 0)
			result = error;
	}
	memset(&control->updateWork, 0, sizeof(control->updateWork));
	return result;
}

/* Wait for the modem to settle in a state at least `state`: two calls in a row
 * that find it there send command 78 and go on to `nextStep`, otherwise a
 * two-second timer calls again, at most ten times. */
int _waitTransition(BbmcControl *control, SceInt32 state, SceUInt32 nextStep)
{
	int result;

	if (control->currentState < state ||
	    bbmcIsStateTransitionPending(control)) {
		swdlWork.stableCount = 0;
		if (++swdlWork.unstableCount > 9)
			return (int)0x80330006;
	} else {
		swdlWork.unstableCount = 0;
		if (++swdlWork.stableCount > 1) {
			result = bbmcSendSimpleMessage(control, 78);
			if (result >= 0)
				swdlNextStep = nextStep;
			return result;
		}
	}
	result = bbmcTimerSetHandler(2000000, swdlTransitionTimeoutHandler,
				     control);
	if (result < 0) {
#if FW_DEBUG
		sceKernelAssert(0, &dbg__waitTransition_L1410,
				__builtin_return_address(0));
#endif
	} else {
		swdlTimerId = result;
	}
	return result;
}

/* name unknown: free the strings of the request copy and clear it */
void swdlFreeRequest(void)
{
	BbmcStringPairEntry *entry;

	switch (swdlWork.request.command) {
	case 119:
		if (swdlWork.request.body.stringPair.first != NULL)
			bbmcFree(swdlWork.request.body.stringPair.first);
		if (swdlWork.request.body.stringPair.second != NULL)
			bbmcFree(swdlWork.request.body.stringPair.second);
		break;
	case 122:
		if (swdlWork.request.body.stringPair.first != NULL)
			bbmcFree(swdlWork.request.body.stringPair.first);
		if (swdlWork.request.body.stringPair.second != NULL)
			bbmcFree(swdlWork.request.body.stringPair.second);
		break;
	case 120:
	case 125:
	case 126:
		if (swdlWork.request.body.stringSet.strings[0] != NULL)
			bbmcFree(swdlWork.request.body.stringSet.strings[0]);
		if (swdlWork.request.body.stringSet.strings[1] != NULL)
			bbmcFree(swdlWork.request.body.stringSet.strings[1]);
		if (swdlWork.request.body.stringSet.strings[2] != NULL)
			bbmcFree(swdlWork.request.body.stringSet.strings[2]);
		if (swdlWork.request.body.stringSet.strings[3] != NULL)
			bbmcFree(swdlWork.request.body.stringSet.strings[3]);
		if (swdlWork.request.body.stringSet.strings[4] != NULL)
			bbmcFree(swdlWork.request.body.stringSet.strings[4]);
		break;
	case 121:
		for (entry = swdlWork.request.body.stringPairs.entries;
		     entry != &swdlWork.request.body.stringPairs.entries[5]; entry++) {
			if (entry->strings.first != NULL)
				bbmcFree(entry->strings.first);
			if (entry->strings.second != NULL)
				bbmcFree(entry->strings.second);
		}
		memset(&swdlWork.request.body, 0, sizeof(swdlWork.request.body));
		break;
	}
	memset(&swdlWork.request, 0, sizeof(swdlWork.request));
}

/* name unknown: open the five update files a file-set request names and add up
 * their sizes */
int swdlOpenUpdateFiles(BbmcControl *control, const BbmcStringSet *request)
{
	BbmcFileSetUpdateWork *work = &control->updateWork.fileSet;
	const char *first = request->strings[0];
	const char *second = request->strings[1];
	const char *third = request->strings[2];
	const char *fourth = request->strings[3];
	const char *fifth = request->strings[4];
	SceOff size;
	int result;

	memset(&control->updateWork, 0, sizeof(control->updateWork));
	result = (int)first;
	if (first != NULL) {
		result = udIoOpen(first, UD_IO_OPEN_FLAGS, 0400);
		if (result < 0)
			goto failed;
		work->files[0] = result;
		size = udIoGetSize(result);
		if (size < 0)
			goto sizeFailed;
		work->sizes[0] = size;
	}
	if (second != NULL) {
		result = udIoOpen(second, UD_IO_OPEN_FLAGS, 0400);
		if (result < 0)
			goto failed;
		work->files[1] = result;
		size = udIoGetSize(result);
		if (size < 0)
			goto sizeFailed;
		work->sizes[1] = size;
	}
	if (third != NULL) {
		result = udIoOpen(third, UD_IO_OPEN_FLAGS, 0400);
		if (result < 0)
			goto failed;
		work->files[2] = result;
		size = udIoGetSize(result);
		if (size < 0)
			goto sizeFailed;
		work->sizes[2] = size;
	}
	if (fourth != NULL) {
		result = udIoOpen(fourth, UD_IO_OPEN_FLAGS, 0400);
		if (result < 0)
			goto failed;
		work->files[3] = result;
		size = udIoGetSize(result);
		if (size < 0)
			goto sizeFailed;
		work->sizes[3] = size;
	}
	if (fifth != NULL) {
		result = udIoOpen(fifth, UD_IO_OPEN_FLAGS, 0400);
		if (result < 0)
			goto failed;
		work->files[4] = result;
		size = udIoGetSize(result);
		if (size < 0)
			goto sizeFailed;
		work->sizes[4] = size;
	}
	work->forwardedOptions[0] = request->forwardedValues[0];
	work->forwardedOptions[1] = request->forwardedValues[1];
	work->forwardedOptions[2] = request->forwardedValues[2];
	work->completedBytes = 0;
	work->totalBytes = work->sizes[0] + work->sizes[1] + work->sizes[2] +
			   work->sizes[3] + work->sizes[4];
	return result;

sizeFailed:
	result = (int)size;
failed:
	if (work->files[0] != 0)
		udIoClose(work->files[0]);
	if (work->files[1] != 0)
		udIoClose(work->files[1]);
	if (work->files[2] != 0)
		udIoClose(work->files[2]);
	if (work->files[3] != 0)
		udIoClose(work->files[3]);
	if (work->files[4] != 0)
		udIoClose(work->files[4]);
	memset(&control->updateWork, 0, sizeof(control->updateWork));
	return result;
}

/* name unknown: copy a string of a request into the bbmc heap (as the API does) */
static inline char *swdlCopyString(const char *source, SceSize length)
{
	SceSize copySize = length + 1, remainder, allocationSize;
	char *copy;

	if (source == NULL || copySize == 0)
		return NULL;
	remainder = copySize % 124;
	allocationSize = remainder == 0 ? copySize : length + 125 - remainder;
	copy = bbmcAlloc(allocationSize, 0);
	if (copy == NULL)
		return NULL;
	memcpy(copy, source, copySize);
	return copy;
}

/* name unknown: keep a copy of a download request, with its strings, while the
 * download runs */
int swdlCopyRequest(const BbmcMessage *message)
{
	BbmcStringPairEntry *entry;
	const BbmcStringPairEntry *source;
	char *copy;
	int i;

	if (message == NULL)
		return (int)0x80330002;
	memset(&swdlWork.request, 0, sizeof(swdlWork.request));
	switch (message->command) {
	case 119:
		swdlWork.request.command = 119;
		if (message->body.stringPair.firstLength != 0) {
			copy = swdlCopyString(message->body.stringPair.first,
					      message->body.stringPair.firstLength);
			swdlWork.request.body.stringPair.first = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringPair.firstLength =
				message->body.stringPair.firstLength;
		}
		if (message->body.stringPair.secondLength != 0) {
			copy = swdlCopyString(message->body.stringPair.second,
					      message->body.stringPair.secondLength);
			swdlWork.request.body.stringPair.second = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringPair.secondLength =
				message->body.stringPair.secondLength;
		}
		return 0;
	case 120:
	case 125:
	case 126:
		swdlWork.request.command = message->command;
		swdlWork.request.body.stringSet.forwardedValues[0] =
			message->body.stringSet.forwardedValues[0];
		swdlWork.request.body.stringSet.forwardedValues[1] =
			message->body.stringSet.forwardedValues[1];
		swdlWork.request.body.stringSet.forwardedValues[2] =
			message->body.stringSet.forwardedValues[2];
		if (message->body.stringSet.lengths[0] != 0) {
			copy = swdlCopyString(message->body.stringSet.strings[0],
					      message->body.stringSet.lengths[0]);
			swdlWork.request.body.stringSet.strings[0] = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringSet.lengths[0] =
				message->body.stringSet.lengths[0];
		}
		if (message->body.stringSet.lengths[1] != 0) {
			copy = swdlCopyString(message->body.stringSet.strings[1],
					      message->body.stringSet.lengths[1]);
			swdlWork.request.body.stringSet.strings[1] = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringSet.lengths[1] =
				message->body.stringSet.lengths[1];
		}
		if (message->body.stringSet.lengths[2] != 0) {
			copy = swdlCopyString(message->body.stringSet.strings[2],
					      message->body.stringSet.lengths[2]);
			swdlWork.request.body.stringSet.strings[2] = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringSet.lengths[2] =
				message->body.stringSet.lengths[2];
		}
		if (message->body.stringSet.lengths[3] != 0) {
			copy = swdlCopyString(message->body.stringSet.strings[3],
					      message->body.stringSet.lengths[3]);
			swdlWork.request.body.stringSet.strings[3] = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringSet.lengths[3] =
				message->body.stringSet.lengths[3];
		}
		if (message->body.stringSet.lengths[4] != 0) {
			copy = swdlCopyString(message->body.stringSet.strings[4],
					      message->body.stringSet.lengths[4]);
			swdlWork.request.body.stringSet.strings[4] = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringSet.lengths[4] =
				message->body.stringSet.lengths[4];
		}
		return 0;
	case 121:
		swdlWork.request.body.stringPairs.count =
			message->body.stringPairs.count;
		swdlWork.request.body.stringPairs.selectedIndex =
			message->body.stringPairs.selectedIndex;
		swdlWork.request.command = 121;
		for (i = 0; i < 5; i++) {
			entry = &swdlWork.request.body.stringPairs.entries[i];
			source = &message->body.stringPairs.entries[i];
			if (source->strings.firstLength != 0) {
				copy = swdlCopyString(source->strings.first,
						      source->strings.firstLength);
				if (copy == NULL) {
					swdlWork.request.body.stringPairs.entries[i].strings.first = NULL;
					goto failed;
				}
				entry->strings.first = copy;
				entry->strings.firstLength = source->strings.firstLength;
				entry->forwardedValue = source->forwardedValue;
			}
			if (source->strings.secondLength != 0) {
				copy = swdlCopyString(source->strings.second,
						      source->strings.secondLength);
				if (copy == NULL) {
					swdlWork.request.body.stringPairs.entries[i].strings.second = NULL;
					goto failed;
				}
				entry->strings.second = copy;
				entry->strings.secondLength = source->strings.secondLength;
			}
		}
		return 0;
	case 122:
		swdlWork.request.command = 122;
		if (message->body.stringPair.firstLength != 0) {
			copy = swdlCopyString(message->body.stringPair.first,
					      message->body.stringPair.firstLength);
			swdlWork.request.body.stringPair.first = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringPair.firstLength =
				message->body.stringPair.firstLength;
		}
		if (message->body.stringPair.secondLength != 0) {
			copy = swdlCopyString(message->body.stringPair.second,
					      message->body.stringPair.secondLength);
			swdlWork.request.body.stringPair.second = copy;
			if (copy == NULL)
				goto failed;
			swdlWork.request.body.stringPair.secondLength =
				message->body.stringPair.secondLength;
		}
		return 0;
	default:
		memcpy(&swdlWork.request, message, sizeof(swdlWork.request));
		return 0;
	}

failed:
	swdlFreeRequest();
	memset(&swdlWork.request, 0, sizeof(swdlWork.request));
	return (int)0x80330005;
}
