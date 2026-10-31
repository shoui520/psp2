#include <stddef.h>
#include <string.h>
#include <syscon/syscon.h>
#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbmc_netif.h"
#include "../bbcmd.h"
#include <power/power.h>

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
SceUInt32 firmwarePreferenceStep;
SceUInt32 firmwareImageStep;
SceUInt32 setFirmwarePreferenceStep;
SceUInt32 storedImagesStep;
SceUInt32 sysconEventDeferred;
SceUInt32 simDetectFlags;
int proactivePending;
int modemWakeupPending;

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

	if (*(SceUInt16 *)packet->request.command == 0x0a81) {
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
