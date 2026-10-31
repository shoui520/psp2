/* QMI command request completion callbacks. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <gps/gps.h>
#include "bbcmd.h"
#include "qmi_backend.h"
#include "qmi_gps_report.h"
#include "bbmc_control.h"
#include "bbmc_message.h"
#include "bbcmd_fp.h"
#include "bbmc_netif.h"
#include "bbmc_gps.h"
#include "qmi_custom_messages.h"
#include "qmi_network_operations.h"

static BbcmdProtocolContext g_qmiProtocolContexts[2];
static BbcmdProtocolContext *g_qmiPublishedContexts[2];
static QmiStoredImageGroup g_discardedStoredImages;
static SceUInt8 g_previousTrackingSessionState = 1;
static int g_trackingShortTimer;
static int g_trackingLongTimer;
static unsigned int g_trackingSatelliteAverage;
static unsigned int g_trackingSignalAverage;
static unsigned int g_trackingSampleCount;
static unsigned int g_trackingEnabled;
static unsigned int g_trackingShortPending;
static unsigned int g_trackingLongPending;
static unsigned int g_trackingStartRetryCount;
static unsigned int g_trackingStopRetryCount;

static const QmiTrackingMessageResult g_trackingStartMessageResults[4] = {
	{ 33, 0 }, { 1, 0 }, { 49, 0 }, { 42, 0 },
};

static const QmiTrackingMessageResult g_trackingOneShotMessageResults[3] = {
	{ 33, 0 }, { 1, 0 }, { 53, 0 },
};

static const int g_cat2TerminalCommandTypes[19] = {
	1, 2, 3, 4, 5, 6, 0, 0, 0, -1, -1, 7, 8, 9, 0, 13, 10, 11, 12,
};


const SceUInt16 g_qmiSmscParameterMessages[2] = { 53, 52 };
const SceUInt16 g_qmiSystemSelectionMessages[2] = { 51, 52 };
const SceUInt16 g_qmiPinProtectionMessages[2] = { 39, 43 };
const SceUInt16 g_qmiPinChangeMessages[2] = { 42, 43 };
const SceUInt16 g_qmiProfileParameterMessages[2] = { 40, 44 };
const SceUInt16 g_qmiCountedParameterMessages[2] = { 3, 3 };
const SceUInt16 g_qmiRegistrationParameterMessages[2] = { 34, 0 };
const SceUInt16 g_qmiMsisdnParameterMessages[2] = { 0, 36 };
const SceInt32 g_qmiPinStatusValues[5] = { 2, 4, 0, 1, 3 };

const QmiServiceParameterSet g_qmiServiceParameterSets[8] = {
	{ 1, g_qmiSmscParameterMessages, 2, 5 },
	{ 2, g_qmiPinProtectionMessages, 2, 2 },
	{ 3, g_qmiPinChangeMessages, 2, 2 },
	{ 4, g_qmiSystemSelectionMessages, 2, 3 },
	{ 5, g_qmiProfileParameterMessages, 2, 1 },
	{ 7, g_qmiCountedParameterMessages, 2, 227 },
	{ 6, g_qmiRegistrationParameterMessages, 2, 3 },
	{ 0, g_qmiMsisdnParameterMessages, 2, 2 },
};

const QmiUimFileSelection g_qmiUimFileSelections[21] = {
	{ 2, { 0x3f00, 0x7fff }, 0x6f07, 1 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f3b, 2 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f31, 3 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f7b, 4 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f7e, 5 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f20, 6 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f07, 7 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f30, 8 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f31, 9 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f7b, 10 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f7e, 11 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6f20, 12 },
	{ 2, { 0x3f00, 0x7f10 }, 0x6f3b, 13 },
	{ 2, { 0x3f00, 0x7f10 }, 0x6f40, 14 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f40, 15 },
	{ 2, { 0x3f00, 0x7fff }, 0x6fc7, 16 },
	{ 2, { 0x3f00, 0x7fff }, 0x6f15, 17 },
	{ 2, { 0x3f00, 0x7fff }, 0x6fc5, 18 },
	{ 2, { 0x3f00, 0x7fff }, 0x6fc6, 19 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6fc5, 20 },
	{ 2, { 0x3f00, 0x7f20 }, 0x6fc6, 21 },
};

extern void bbcmdRegisterQmiHandlers(BbcmdCommandEntry *commands);
const QmiBuildSuffixRule g_qmiBuildSuffixRules[11] = {
	{ 8, "STSUGNZ", 0 },
	{ 7, "UQSUAN", 1 },
	{ 7, "UQSUDN", 2 },
	{ 7, "UQSUVN", 3 },
	{ 7, "UQSUA2", 1 },
	{ 7, "UQSUD2", 2 },
	{ 7, "UQSUV2", 3 },
	{ 7, "UQSUG2", 0 },
	{ 7, "UQSUC2", 0 },
	{ 7, "UQSUM2", 0 },
	{ 7, "UQSUS2", 0 },
};

/* name unknown: secondary instance deactivation restores channel state one. */
int bbcmdSecondaryDeactivateCompleted(BbcmdRequest *request)
{
	BbcmdRequestNode *node;

	request->state = 3;
	bbcmdSetChannelState(1, 1);
	node = request->node;
	return sceKernelSetEventFlag(node->control->event, 1U << node->index);
}

/* name unknown: secondary instance activation reports failure before waking. */
int bbcmdSecondaryActivateCompleted(BbcmdRequest *request, int result)
{
	BbcmdRequestNode *node;

	if (result >= 0)
		bbcmdSetChannelState(1, 2);
	else
		request->result = result;
	node = request->node;
	request->state = 3;
	return sceKernelSetEventFlag(node->control->event, 1U << node->index);
}

int bbcmdActivateInstance(BbcmdRequest *request);
int bbcmdDeactivateInstance(BbcmdRequest *request);
int bbcmdSuspendPrimaryChannel(BbcmdRequest *request);
int bbcmdResumePrimaryChannel(BbcmdRequest *request);
int bbcmdReleaseClients(BbcmdRequest *request);
int bbcmdVerifyUimPin(BbcmdRequest *request);
int bbcmdUnblockUimPin(BbcmdRequest *request);
int bbcmdListSmsMessages(BbcmdRequest *request);
int bbcmdReadSmsMessage(BbcmdRequest *request);
int bbcmdDeleteSmsMessage(BbcmdRequest *request);
int bbcmdSendSmsMessage(BbcmdRequest *request);
int bbcmdStartTracking(BbcmdRequest *request);
int bbcmdStopTracking(BbcmdRequest *request);
int bbcmdSendGpsData(BbcmdRequest *request);
int bbcmdGetChannelRates(BbcmdRequest *request);
int bbcmdStopNetwork(BbcmdRequest *request);
int bbcmdGetNetworkInfo(BbcmdRequest *request);
int bbcmdAttachDetachPacketService(BbcmdRequest *request);
int bbcmdSetRegistrationRejectReport(BbcmdRequest *request);
int bbcmdResetConfiguredService(BbcmdRequest *request);
int bbcmdReadConfiguredServiceValue(BbcmdRequest *request);
int bbcmdConfigureServiceParameters(BbcmdRequest *request);
int bbcmdSetOperatingMode(BbcmdRequest *request);
int bbcmdSetFirmwarePreference(BbcmdRequest *request);
int bbcmdGetFirmwarePreference(BbcmdRequest *request);
int bbcmdListStoredImages(BbcmdRequest *request);
int bbcmdDeleteStoredImage(BbcmdRequest *request);
int bbcmdReadProactiveCommand(BbcmdRequest *request);
int bbcmdSendTerminalResponse(BbcmdRequest *request);
int bbcmdReadSimRefreshDetails(BbcmdRequest *request);
int bbcmdReplyUimRefresh(BbcmdRequest *request);
int bbcmdSendCat2Command(BbcmdRequest *request);
int bbcmdReadSimContent(BbcmdRequest *request);
int bbcmdGetUimFacilityStatus(BbcmdRequest *request);
int bbcmdDepersonalizeUim(BbcmdRequest *request);
int bbcmdReadFirmwareSelection(BbcmdRequest *request);

void bbcmdRegisterQmiHandlers(BbcmdCommandEntry *commands)
{
	unsigned int count;

	for (count = 52; count != 0; --count, ++commands) {
		__builtin_prefetch(commands + 4);
		switch (commands->command) {
		case 5:
			commands->handler = bbcmdActivateInstance;
			break;
		case 6:
			commands->handler = bbcmdDeactivateInstance;
			break;
		case 7:
			commands->handler = bbcmdSuspendPrimaryChannel;
			break;
		case 8:
			commands->handler = bbcmdResumePrimaryChannel;
			break;
		case 9:
			commands->handler = bbcmdInitializeClients;
			break;
		case 10:
			commands->handler = bbcmdReleaseClients;
			break;
		case 11:
			commands->handler = bbcmdGetUimCardStatus;
			break;
		case 12:
			commands->handler = bbcmdVerifyUimPin;
			break;
		case 13:
			commands->handler = bbcmdUnblockUimPin;
			break;
		case 14:
			commands->handler = bbcmdGetInformation;
			break;
		case 15:
			commands->handler = bbcmdListSmsMessages;
			break;
		case 16:
			commands->handler = bbcmdReadSmsMessage;
			break;
		case 17:
			commands->handler = bbcmdDeleteSmsMessage;
			break;
		case 18:
			commands->handler = bbcmdSendSmsMessage;
			break;
		case 20:
		case 21:
			commands->handler = bbcmdStartTracking;
			break;
		case 22:
			commands->handler = bbcmdStopTracking;
			break;
		case 23:
			commands->handler = bbcmdSendGpsData;
			break;
		case 24:
			commands->handler = bbcmdGetChannelRates;
			break;
		case 25:
			commands->handler = bbcmdStartNetwork;
			break;
		case 26:
			commands->handler = bbcmdStopNetwork;
			break;
		case 27:
			commands->handler = bbcmdGetNetworkInfo;
			break;
		case 28:
			commands->handler = bbcmdNetworkScan;
			break;
		case 29:
			commands->handler = bbcmdAttachDetachPacketService;
			break;
		case 30:
			commands->handler = bbcmdSetRegistrationRejectReport;
			break;
		case 31:
			commands->handler = bbcmdResetConfiguredService;
			break;
		case 32:
			commands->handler = bbcmdRegisterRefreshFiles;
			break;
		case 33:
			commands->handler = bbcmdReadConfiguredServiceValue;
			break;
		case 34:
			commands->handler = bbcmdGetCellLocationInfo;
			break;
		case 36:
			commands->handler = bbcmdConfigureServiceParameters;
			break;
		case 37:
			commands->handler = bbcmdSetOperatingMode;
			break;
		case 38:
			commands->handler = bbcmdSetFirmwarePreference;
			break;
		case 39:
			commands->handler = bbcmdGetFirmwarePreference;
			break;
		case 40:
			commands->handler = bbcmdListStoredImages;
			break;
		case 41:
			commands->handler = bbcmdDeleteStoredImage;
			break;
		case 42:
			commands->handler = bbcmdReadProactiveCommand;
			break;
		case 43:
			commands->handler = bbcmdSendTerminalResponse;
			break;
		case 44:
			commands->handler = bbcmdReadSimRefreshDetails;
			break;
		case 45:
			commands->handler = bbcmdReplyUimRefresh;
			break;
		case 46:
			commands->handler = bbcmdSendCat2Command;
			break;
		case 47:
			commands->handler = bbcmdReadSimContent;
			break;
		case 49:
			commands->handler = bbcmdGetUimFacilityStatus;
			break;
		case 50:
			commands->handler = bbcmdDepersonalizeUim;
			break;
		case 51:
			commands->handler = bbcmdReadFirmwareSelection;
			break;
		default:
			break;
		}
	}
}

/* name unknown: copy the bounded byte list carried by response parameter 16. */
int qmiReadCountedResponseBytes(const QmiResultResponse *response,
	QmiCountedBytes *output)
{
	unsigned int length, offset;

	if (output == NULL || response == NULL)
		return 0x80330002;
	length = response->message.lengthLow |
		(response->message.lengthHigh << 8);
	if (length <= 10)
		return 0x80330002;
	for (offset = sizeof(*response); offset < length;) {
		const QmiTlvHeader *tlv = (const QmiTlvHeader *)
			((const unsigned char *)response + offset);
		unsigned int size = tlv->lengthLow | (tlv->lengthHigh << 8);

		if (tlv->type == 16) {
			const QmiCountedByteParameter *value =
				(const QmiCountedByteParameter *)(tlv + 1);
			unsigned int count;

			output->length = 0;
			output->values[0] = 0;
			output->values[1] = 0;
			output->values[2] = 0;
			output->values[3] = 0;
			output->values[4] = 0;
			output->values[5] = 0;
			output->values[6] = 0;
			output->values[7] = 0;
			output->values[8] = 0;
			output->values[9] = 0;
			output->reserved = 0;
			count = value->length;
			if (count >= 10)
				count = 10;
			output->length = count;
			do {
				if (count == 0)
					break;
				output->values[0] = value->values[0];
				if (count == 1)
					break;
				output->values[1] = value->values[1];
				if (count == 2)
					break;
				output->values[2] = value->values[2];
				if (count == 3)
					break;
				output->values[3] = value->values[3];
				if (count == 4)
					break;
				output->values[4] = value->values[4];
				if (count == 5)
					break;
				output->values[5] = value->values[5];
				if (count == 6)
					break;
				output->values[6] = value->values[6];
				if (count == 7)
					break;
				output->values[7] = value->values[7];
				if (count == 8)
					break;
				output->values[8] = value->values[8];
				if (count == 9)
					break;
				output->values[9] = value->values[9];
			} while (0);
		}
		size += sizeof(*tlv);
		offset += size;
	}
	return 0;
}

/* name unknown: translate response result TLVs with service-specific exceptions. */
unsigned int qmiCheckResponseResults(BbcmdProtocolClient *client)
{
	const QmiMessageHeader *message;
	const QmiTlvHeader *tlv;
	const QmiResultTlv *result;
	unsigned int remaining;
	unsigned int length;
	unsigned int tlvLength;
	unsigned int tlvSize;
	unsigned int tlvRemaining;
	unsigned int messageId;
	unsigned int service;
	unsigned int error;

	if (client == NULL)
		goto invalid;
	remaining = client->response.length;
	if (remaining <= 6)
		goto invalid;
	message = (const QmiMessageHeader *)client->response.data;
	for (;;) {
		tlv = (const QmiTlvHeader *)(message + 1);
		length = message->lengthLow | (message->lengthHigh << 8);
		messageId = message->messageIdLow | (message->messageIdHigh << 8);
		if (length != 0) {
			tlvLength = tlv->lengthLow | (tlv->lengthHigh << 8);
			if (tlv->type != 2) {
				tlvSize = tlvLength + sizeof(*tlv);
				if (tlvSize > length)
					goto invalid;
				tlvRemaining = message->lengthLow | (message->lengthHigh << 8);
				for (;;) {
					tlv = (const QmiTlvHeader *)((const unsigned char *)tlv + tlvSize);
					tlvRemaining -= sizeof(*tlv) + tlvLength;
					if (tlvRemaining == 0)
						goto next;
					tlvLength = tlv->lengthLow | (tlv->lengthHigh << 8);
					tlvSize = tlvLength + sizeof(*tlv);
					if (tlv->type == 2)
						break;
					if (tlvSize > tlvRemaining)
						goto invalid;
				}
			}
			result = (const QmiResultTlv *)tlv;
			if ((result->resultLow | (result->resultHigh << 8)) != 0) {
				service = client->service;
				error = result->errorLow | (result->errorHigh << 8);
				switch (error) {
				case 16:
					if (messageId == 36 && service == 2)
						break;
					return 0x80335010;
				case 25:
					if (service == 2) {
						if (messageId == 48 || messageId == 67)
							break;
					} else if (service == 5 && messageId == 48) {
						break;
					}
					goto failed;
				case 26:
					if (messageId == 33 && service == 6)
						break;
					return 0x8033501a;
				case 37:
					if (messageId == 37 && service == 3)
						break;
					return 0x80335025;
				case 47:
					if (messageId == 52 && service == 5)
						break;
					return 0x8033502f;
				default:
failed:
					if (error != 0)
						return error | 0x80335000;
					break;
				}
			}
		}
next:
		if (length + sizeof(*message) > remaining)
			goto invalid;
		message = (const QmiMessageHeader *)((const unsigned char *)message +
			length + sizeof(*message));
		remaining -= sizeof(*message) + length;
		if (remaining == 0)
			return remaining;
	}
invalid:
	return 0x80330002;
}

/* name unknown: forward indexed values from command 49/50 responses. */
int qmiReadIndexedResponse(BbcmdRequest *request,
	BbcmdProtocolClient *client, unsigned int index)
{
	const QmiMessageHeader *message =
		(const QmiMessageHeader *)client->response.data;
	unsigned int remaining = client->response.length;

	if (remaining <= 10)
		return 0x80330302;
	do {
		unsigned int length = message->lengthLow | (message->lengthHigh << 8);
		const unsigned char *payload = (const unsigned char *)(message + 1);

		if (length > 7) {
			unsigned int messageId = message->messageIdLow |
				(message->messageIdHigh << 8);

			if (messageId == 64) {
				unsigned int offset = 0;
				int result = 0;

				do {
					const QmiTlvHeader *tlv =
						(const QmiTlvHeader *)(payload + offset);
					unsigned int size = tlv->lengthLow |
						(tlv->lengthHigh << 8);

					if (tlv->type == 1 || tlv->type == 16) {
						QmiIndexedResponse *output = request->output[0].pointer;

						if ((unsigned int)(request->command - 49) > 1 ||
						    output == NULL) {
							result = 0x80330002;
						} else if (tlv->type == 1) {
							const QmiIndexedValueTlv *value =
								(const QmiIndexedValueTlv *)tlv;

							output[index].primaryValue = value->value;
							output[index].detailBytes[0] = value->detailBytes[0];
							output[index].detailBytes[1] = value->detailBytes[1];
							output[index].index = index;
						} else {
							const QmiByteValueTlv *value =
								(const QmiByteValueTlv *)tlv;

							output[index].secondaryValue = value->value;
						}
					}
					offset += size + sizeof(*tlv);
				} while (offset < length);
				if (result != 0)
					return result;
			} else if (messageId == 68) {
				const QmiTlvHeader *tlv = (const QmiTlvHeader *)payload;
				unsigned int tlvRemaining = length;

				if (payload == NULL)
					return 0x80330002;
				do {
					unsigned int size = tlv->lengthLow |
						(tlv->lengthHigh << 8);

					if (tlv->type == 1) {
						const QmiByteValueTlv *value =
							(const QmiByteValueTlv *)tlv;

						if (value->value > 2)
							return 0x80330006;
						break;
					}
					size += sizeof(*tlv);
					tlvRemaining -= size;
					tlv = (const QmiTlvHeader *)
						((const unsigned char *)tlv + size);
				} while (tlvRemaining != 0);
			} else {
				return 0x80330002;
			}
		}
		message = (const QmiMessageHeader *)(payload + length);
		remaining -= length + sizeof(*message);
	} while (remaining != 0);
	return 0;
}

int qmiReadPacketServiceStatus(const QmiMessageHeader *message, unsigned int totalSize)
{
	SceUInt16 callEndReason = 0, verboseType = 0, verboseReason = 0;
	BbmcControl *control = bbmcGetControl();
	const QmiTlvHeader *parameter;
	unsigned int remaining, length, connectionStatus = 255;
	unsigned int reconfigurationRequired = 255;
	int hasCallEndReason = 0, hasVerboseReason = 0;
	int result = 0;

	if (totalSize <= 8)
		return 0x80330302;
	remaining = message->lengthLow | (message->lengthHigh << 8);
	if (remaining == 0)
		return 0;
	parameter = (const QmiTlvHeader *)(message + 1);
	do {
		const unsigned char *value = (const unsigned char *)(parameter + 1);

		length = parameter->lengthLow | (parameter->lengthHigh << 8);
		if (parameter->type == 16) {
			hasCallEndReason = 1;
			callEndReason = value[0] | (value[1] << 8);
		} else if (parameter->type == 17) {
			const QmiWdsVerboseCallEndReasonTlv *reason =
				(const QmiWdsVerboseCallEndReasonTlv *)parameter;

			hasVerboseReason = 1;
			verboseType = reason->reasonType[0] | (reason->reasonType[1] << 8);
			verboseReason = reason->reason[0] | (reason->reason[1] << 8);
		} else if (parameter->type == 1) {
			const QmiWdsPacketServiceStatusTlv *status =
				(const QmiWdsPacketServiceStatusTlv *)parameter;

			connectionStatus = status->connectionStatus;
			reconfigurationRequired = status->reconfigurationRequired;
		} else {
			result = 0x80330302;
		}
		parameter = (const QmiTlvHeader *)(value + length);
		remaining -= sizeof(*parameter) + length;
	} while (remaining != 0);
	if (connectionStatus != 255 && result >= 0) {
		if (reconfigurationRequired == 255) {
			result = remaining;
		} else {
			if (connectionStatus == 1) {
				bbmcEnterCS(&control->mutex);
				control->networkStarted = remaining;
				bbmcLeaveCS(&control->mutex);
			}
			result = bbmcSetPacketServiceStatus(connectionStatus, reconfigurationRequired);
		}
	}
	if (hasCallEndReason != 0 && callEndReason != 2)
		bbmcNotifyPacketInterfaceEvent(64, callEndReason | 0x80333000);
	if (hasVerboseReason != 0) {
		unsigned char *reason = (unsigned char *)&control->callEndReason;

		bbmcEnterCS(&control->mutex);
		__builtin_memcpy(reason, &verboseType, sizeof(verboseType));
		__builtin_memcpy(reason + sizeof(verboseType), &verboseReason, sizeof(verboseReason));
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 5, 2);
	}
	return result;
}

int bbcmdReadNetworkStartResponse(BbcmdRequest *request,
	BbcmdProtocolClient *client)
{
	const QmiMessageHeader *message;
	unsigned int remaining;

	if (request->result == (int)0x80330306)
		return request->result;
	remaining = client->response.length;
	message = (const QmiMessageHeader *)client->response.data;
	if (remaining <= 10)
		return 0x80330302;
	do {
		unsigned short messageId, length;
		const QmiTlvHeader *parameter = (const QmiTlvHeader *)(message + 1);
		unsigned int parameterRemaining;
		unsigned int callError = 0;
		unsigned int operationError = 0;

		__builtin_memcpy(&messageId, &message->messageIdLow, 2);
		__builtin_memcpy(&length, &message->lengthLow, 2);
		if (length <= 6)
			return 0x80330302;
		parameterRemaining = length;
		do {
			unsigned short parameterLength;
			const unsigned char *value = (const unsigned char *)(parameter + 1);

			__builtin_memcpy(&parameterLength, &parameter->lengthLow, 2);
			if (parameter->type == 2) {
				const QmiResultTlv *result = (const QmiResultTlv *)parameter;
				unsigned short status, error;

				__builtin_memcpy(&status, &result->resultLow, 2);
				__builtin_memcpy(&error, &result->errorLow, 2);
				if (status == 1) {
					operationError = error;
					if (error != 0)
						operationError = error | 0x80335000;
				} else if (status != 0) {
					return 0x80330302;
				}
			} else if (messageId == 32) {
				switch (parameter->type) {
				case 16: {
					unsigned short reason;

					__builtin_memcpy(&reason, value, 2);
					callError = (reason & 0xfff) | 0x80333000;
					break;
				}
				case 17:
					if (request->output[1].pointer != NULL) {
						unsigned short reason;

						__builtin_memcpy(&reason, value, 2);
						((unsigned short *)request->output[1].pointer)[0] = reason;
						__builtin_memcpy(&reason, value + 2, 2);
						((unsigned short *)request->output[1].pointer)[1] = reason;
					}
					break;
				case 1:
					__builtin_memcpy(request->output[0].pointer, value, 4);
					break;
				default:
					return 0x80330302;
				}
			} else if (messageId > 32) {
				if (messageId == 45) {
					unsigned int index = (unsigned short)(parameter->type - 16);
					unsigned int mask;

					if (index > 29)
						return 0x80330302;
					mask = 1U << index;
					if ((mask & 0x3c1faaf3) == 0) {
						if (mask & 0x02000000) {
							*(unsigned int *)request->output[4].pointer =
								value[0] | (value[1] << 8) |
								(value[2] << 16) | ((unsigned int)value[3] << 24);
						} else {
							if ((mask & 0x4000) == 0)
								return 0x80330302;
							*(unsigned int *)request->output[3].pointer =
								((unsigned int)value[0] << 24) |
								(value[1] << 16) | (value[2] << 8) | value[3];
							*(unsigned char *)request->output[2].pointer = 1;
						}
					}
				} else if (messageId == 68) {
					if (parameter->type == 1) {
						const QmiWdsBearerTlv *bearer =
							(const QmiWdsBearerTlv *)parameter;
						BbmcControl *control = bbmcGetControl();
						unsigned int details;
						unsigned int currentBearer = bearer->currentBearer;

						__builtin_memcpy(&details, bearer->bearerDetails, 4);
						bbmcEnterCS(&control->mutex);
						control->currentBearer = currentBearer;
						control->bearerDetails = details;
						bbmcLeaveCS(&control->mutex);
						bbmcNotifyFeatureEvent(control, 5, 8);
					}
				} else {
					return 0x80330302;
				}
			} else if (messageId != 1) {
				return 0x80330302;
			}
			parameter = (const QmiTlvHeader *)(value + parameterLength);
			parameterRemaining -= sizeof(*parameter) + parameterLength;
		} while (parameterRemaining != 0 && parameterRemaining <= length);
		if (callError != 0)
			return callError;
		if (operationError != 0)
			return operationError;
		message = (const QmiMessageHeader *)((const unsigned char *)(message + 1) + length);
		remaining -= sizeof(*message) + length;
	} while (remaining != 0);
	return remaining;
}

/* name unknown: mark the service completed by an initializer state. */
int bbcmdUpdateCompletedMaskForState(BbcmdRequest *request, int state)
{
	unsigned int mask;
	int result = bbcmdGetCompletedMask(request->channel, &mask);

	if (result >= 0) {
		switch (state) {
		case 2:
		case 10:
			break;
		case 4:
			mask |= 1;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 7:
			mask |= 2;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 12:
			mask |= 4;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 14:
			mask |= 8;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 16:
			mask |= 16;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 19:
			mask |= 32;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		case 21:
			mask |= 64;
			bbcmdSetCompletedMask(request->channel, mask);
			break;
		default:
			result = 0x80330102;
			break;
		}
	}
	return result;
}

/* The wire words are copied in the original halfword/byte access order. */
#define WRITE_QOS_WORD(member) do { \
	output->member.low = input->member; \
	output->member.middle = ((const unsigned char *)&input->member)[2]; \
	output->member.high = ((const unsigned char *)&input->member)[3]; \
} while (0)

/* name unknown: serialize either requested or minimum UMTS QoS. */
int qmiWriteUmtsQos(SceUInt8 type, const QmiUmtsQos *input,
	QmiUmtsQosTlv *output)
{
	output->header.type = type;
	output->header.lengthLow = 33;
	output->header.lengthHigh = 0;
	output->trafficClass = input->trafficClass;
	WRITE_QOS_WORD(maxUplinkBitrate);
	WRITE_QOS_WORD(maxDownlinkBitrate);
	WRITE_QOS_WORD(guaranteedUplinkBitrate);
	WRITE_QOS_WORD(guaranteedDownlinkBitrate);
	output->deliveryOrder = input->deliveryOrder;
	WRITE_QOS_WORD(maximumSduSize);
	output->sduErrorRatio = input->sduErrorRatio;
	output->residualBitErrorRatio = input->residualBitErrorRatio;
	output->deliveryErroneousSdu = input->deliveryErroneousSdu;
	WRITE_QOS_WORD(transferDelay);
	WRITE_QOS_WORD(trafficHandlingPriority);
	return sizeof(*output);
}

#undef WRITE_QOS_WORD

/* name unknown: restore the primary channel and report the command result. */
int bbcmdResumePrimaryChannel(BbcmdRequest *request)
{
	BbcmdCompletion completion;
	BbcmdRequestNode *node;
	BbcmdControl *control;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	if (request->command != 8)
		return (int)0x80330101;
	if (request->channel != 0)
		return (int)0x80330002;
	ret = qmiResumeChannel(0, NULL, request);
	if (ret >= 0)
		bbcmdSetChannelState(0, 3);
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	request->result = ret;
	request->state = 0;
	if (completion != NULL)
		completion(request->channel, 8, ret, request->input,
			   request->output, request->completionArg);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return ret;
}

/* name unknown: suspend the primary channel before completing command seven. */
int bbcmdSuspendPrimaryChannel(BbcmdRequest *request)
{
	BbcmdCompletion completion;
	BbcmdRequestNode *node;
	BbcmdControl *control;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	if (request->command != 7)
		return (int)0x80330101;
	if (request->channel != 0)
		return (int)0x80330002;
	ret = qmiSuspendChannel(0, NULL, NULL);
	if (ret >= 0) {
		bbcmdSetChannelState(0, 4);
		request->state = 0;
	}
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	request->result = ret;
	request->state = 0;
	if (completion != NULL)
		completion(request->channel, 7, ret, request->input,
			   request->output, request->completionArg);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return ret;
}

/* name unknown: deliver each command's completion to its manager message family. */
void bbcmdManagerCompletion(SceUInt32 channel, SceUInt32 command,
	int result, BbcmdArgument *input, BbcmdArgument *output, void *arg)
{
	BbmcControl *control = arg;
	BbmcMessage *message;
	SceUInt32 reply;
	SceUInt32 value;
	SceUInt8 retries;

	if (control == NULL)
		return;
	switch (command) {
	case 0:
		reply = input[1].value == 25 ? 108 : 55;
		goto allocate;
	case 7:
		reply = result != 0 ? 61 : 60;
		goto allocate;
	case 8:
		reply = result != 0 ? 63 : 62;
allocate:
		message = msgAllocMsgPacket(control, reply);
		if (message == NULL)
			return;
		break;
	case 5:
		message = msgAllocMsgPacket(control, 58);
		message->body.commandResult = result;
		break;
	case 6:
		message = msgAllocMsgPacket(control, 57);
		message->body.commandResult = result;
		break;
	case 9:
		message = msgAllocMsgPacket(control, 59);
		message->body.commandResult = result;
		break;
	case 10:
		message = msgAllocMsgPacket(control, 56);
		message->body.commandResult = result;
		break;
	case 14:
		message = msgAllocMsgPacket(control, 70);
		message->body.commandResult = result;
		break;
	case 18:
		message = msgAllocMsgPacket(control, 91);
		message->body.commandResult = result;
		break;
	case 19:
		message = msgAllocMsgPacket(control, 71);
		message->body.commandResult = result;
		break;
	case 20:
		message = msgAllocMsgPacket(control, 95);
		message->body.commandResult = result;
		break;
	case 21:
		message = msgAllocMsgPacket(control, 95);
		message->body.commandResult = result;
		break;
	case 22:
		message = msgAllocMsgPacket(control, 100);
		message->body.commandResult = result;
		break;
	case 23:
		message = msgAllocMsgPacket(control, 102);
		message->body.commandResult = result;
		break;
	case 25:
		message = msgAllocMsgPacket(control, 105);
		message->body.commandResult = result;
		break;
	case 26:
		message = msgAllocMsgPacket(control, 106);
		message->body.commandResult = result;
		break;
	case 27:
		message = msgAllocMsgPacket(control, 7);
		message->body.commandResult = result;
		break;
	case 28:
		message = msgAllocMsgPacket(control, 150);
		message->body.commandResult = result;
		break;
	case 29:
		message = msgAllocMsgPacket(control, 153);
		message->body.commandResult = result;
		break;
	case 30:
		message = msgAllocMsgPacket(control, 81);
		message->body.commandResult = result;
		break;
	case 31:
		message = msgAllocMsgPacket(control, 68);
		message->body.commandResult = result;
		break;
	case 32:
		message = msgAllocMsgPacket(control, 77);
		message->body.commandResult = result;
		break;
	case 34:
		message = msgAllocMsgPacket(control, 164);
		message->body.commandResult = result;
		break;
	case 42:
		message = msgAllocMsgPacket(control, 166);
		message->body.commandResult = result;
		break;
	case 43:
		message = msgAllocMsgPacket(control, 168);
		message->body.commandResult = result;
		break;
	case 44:
		message = msgAllocMsgPacket(control, 76);
		message->body.commandResult = result;
		break;
	case 45:
		message = msgAllocMsgPacket(control, 172);
		message->body.commandResult = result;
		break;
	case 46:
		message = msgAllocMsgPacket(control, 170);
		message->body.commandResult = result;
		break;
	case 47:
		message = msgAllocMsgPacket(control, 174);
		message->body.commandResult = result;
		break;
	case 49:
		message = msgAllocMsgPacket(control, 181);
		message->body.commandResult = result;
		break;
	case 51:
		message = msgAllocMsgPacket(control, 186);
		message->body.commandResult = result;
		break;
	case 11:
		message = msgAllocMsgPacket(control, 64);
		message->body.pinMessage.status.flags.blocked = output[0].bytes[0];
		message->body.pinMessage.status.flags.verified = output[0].bytes[2];
		message->body.pinMessage.status.flags.enabled = output[0].bytes[1];
		message->body.pinMessage.pinStatusUnavailable = output[3].value;
		message->body.pinMessage.cardStatusResult = result;
		message->body.pinMessage.cardUnavailable = output[4].value;
		message->body.pinMessage.applicationNotReady = output[5].value;
		break;
	case 12:
		message = msgAllocMsgPacket(control, 66);
		retries = output[0].bytes[2];
		message->body.pinMessage.status.result = result;
		message->body.pinMessage.detail.retries = retries;
		break;
	case 13:
		message = msgAllocMsgPacket(control, 65);
		retries = result != 0 ? output[0].bytes[3] : output[0].bytes[2];
		message->body.pinMessage.detail.retries = retries;
		message->body.pinMessage.status.result = result;
		break;
	case 15:
		message = msgAllocMsgPacket(control, 85);
		message->body.smsMessage.completion.listResult = result;
		break;
	case 16:
		message = msgAllocMsgPacket(control, 87);
		message->body.smsMessage.readResult = result;
		break;
	case 17:
		message = msgAllocMsgPacket(control, 89);
		message->body.smsMessage.completion.acknowledgementResult = result;
		break;
	case 24:
		message = msgAllocMsgPacket(control, 112);
		message->body.channelRatesMessage.result = result;
		break;
	case 33:
		message = msgAllocMsgPacket(control, 185);
		value = output[0].value;
		message->body.pinMessage.status.result = result;
		message->body.pinMessage.detail.configuredServiceValue = value;
		break;
	case 36:
		if (input[2].value == 1) {
			message = msgAllocMsgPacket(control, 148);
			message->body.setupMessage.result = result;
			value = input[0].value;
			if (value == 6 && result == (int)0x8033501a) {
				message->body.setupMessage.result = 0;
				value = input[0].value;
			}
			message->body.setupMessage.type = value;
		} else {
			message = msgAllocMsgPacket(control, 146);
			message->body.setupMessage.result = result;
		}
		message->body.setupMessage.completionFlag =
			result == (int)0x8033500c || result == (int)0x80335023;
		break;
	case 37:
		value = input[0].value;
		if (value == 2)
			reply = 75;
		else if (value != 0) {
			if (value != 1)
				return;
			reply = 72;
		} else
			reply = control->currentState == 5 ? 74 : 73;
		message = msgAllocMsgPacket(control, reply);
		message->body.commandResult = result;
		break;
	case 38:
		message = msgAllocMsgPacket(control, 162);
		message->body.firmwareMessage.setFirmwarePreferenceResult = result;
		message->body.firmwareMessage.imageCount = output[2].value;
		if (output[2].value != 0) {
			message->body.firmwareMessage.firstImageType = output[3].value;
			if (output[2].value > 1)
				message->body.firmwareMessage.secondImageType = output[4].value;
		}
		break;
	case 39:
		message = msgAllocMsgPacket(control, 158);
		message->body.firmwareMessage.firmwarePreferenceResult = result;
		break;
	case 40:
		message = msgAllocMsgPacket(control, 156);
		message->body.firmwareMessage.storedImagesResult = result;
		break;
	case 41:
		message = msgAllocMsgPacket(control, 160);
		message->body.firmwareMessage.firmwareImageResult = result;
		break;
	case 50:
		if (input[1].value != 0) {
			message = msgAllocMsgPacket(control, 183);
			retries = output[2].bytes[1];
		} else {
			message = msgAllocMsgPacket(control, 182);
			retries = output[2].bytes[0];
		}
		message->body.controlKeyReply.result = result;
		message->body.controlKeyReply.remainingRetries = retries;
		message->body.controlKeyReply.featureId = output[1].value;
		break;
	default:
		return;
	}
	msgSendMsgPacket(control, message);
}

/* Sony's service-selection switches use the protocol's binding order. */
#define SELECT_PARAMETER_BINDING(context, service, failure) do { \
	if ((service) > qmiGetConfiguredService()) \
		goto failure; \
	switch (service) { \
	case 1: binding = &(context)->bindings[2]; break; \
	case 2: binding = &(context)->bindings[0]; break; \
	case 3: binding = &(context)->bindings[1]; break; \
	case 4: binding = &(context)->bindings[5]; break; \
	case 5: binding = &(context)->bindings[3]; break; \
	case 6: binding = &(context)->bindings[4]; break; \
	case 8: binding = &(context)->bindings[6]; break; \
	case 10: binding = &(context)->bindings[8]; break; \
	case 11: binding = &(context)->bindings[7]; break; \
	default: \
		if ((service) != qmiGetConfiguredService()) \
			goto failure; \
		binding = &(context)->bindings[9]; \
	} \
} while (0)

#define PARAMETER_HEADER(tlv, tag, size) do { \
	(tlv)->header.type = (tag); \
	(tlv)->header.lengthLow = (size); \
	(tlv)->header.lengthHigh = (size) >> 8; \
} while (0)

#define PARAMETER_WORD(bytes, value) do { \
	SceUInt32 parameterWord = (value); \
	(bytes)[0] = parameterWord; \
	(bytes)[1] = parameterWord >> 8; \
	(bytes)[2] = parameterWord >> 16; \
	(bytes)[3] = parameterWord >> 24; \
} while (0)

#define PARAMETER_READ_WORD(bytes) ((SceUInt32)(bytes)[0] | \
	((SceUInt32)(bytes)[1] << 8) | ((SceUInt32)(bytes)[2] << 16) | \
	((SceUInt32)(bytes)[3] << 24))

#define APPEND_PARAMETER_WORD(tag, scalar) do { \
	QmiWordValueTlv *word = (QmiWordValueTlv *)(payload + payloadSize); \
	PARAMETER_HEADER(word, tag, 4); \
	PARAMETER_WORD(word->value, scalar); \
	payloadSize += sizeof(*word); \
} while (0)

#define APPEND_PARAMETER_BYTE(tag, scalar) do { \
	QmiByteValueTlv *byte = (QmiByteValueTlv *)(payload + payloadSize); \
	PARAMETER_HEADER(byte, tag, 1); \
	byte->value = (scalar); \
	payloadSize += sizeof(*byte); \
} while (0)

/* name unknown: serialize and read one service's setup parameter messages. */
int bbcmdConfigureServiceParameters(BbcmdRequest *request)
{
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	QmiServiceParameterContext *context;
	const QmiServiceParameterSet *parameters;
	BbmcControl *manager;
	QmiMessageHeader *message;
	const QmiMessageHeader *response;
	const QmiByteArrayTlv *tlv;
	unsigned char *payload;
	unsigned int payloadSize;
	unsigned int service;
	unsigned int messageId;
	unsigned int responseId;
	unsigned int responseSize;
	unsigned int position;
	unsigned int length;
	unsigned int index;
	int ret = 0;

	if (request == NULL)
		return (int)0x80330002;
	if (request->command != 36)
		return (int)0x80330101;
	if (request->channel > 1)
		return (int)0x80330002;
	control = request->node->control;
	protocol = control->protocols[request->channel];
	context = request->commandContext;
	if (control->cancelMask & (1u << request->node->index)) {
		control->cancelMask &= ~(1u << request->node->index);
		if (context == NULL || context->parameters == NULL)
			goto complete;
		service = context->parameters->service;
		SELECT_PARAMETER_BINDING(protocol, service, cancelInvalid);
		goto cancelSelected;
cancelInvalid:
		ret = (int)0x80330002;
		binding = NULL;
cancelSelected:
		context->messageIndex = 0;
		context->parameters = NULL;
		request->state = 0;
		bbcmdLockMutex(&request->node->control->mutex);
		request->node->state = 2;
		bbcmdUnlockMutex(&request->node->control->mutex);
		if (request->completion != NULL) {
			manager = bbmcGetControl();
			if (binding != NULL) {
				if (binding->requestBits >> 16)
					binding->requestBits &=
						~(1u << ((SceUInt8)request->node->index + 16));
				binding->requestBits &= ~(1u << request->node->index);
			}
			bbmcEnterCS(&manager->mutex);
			manager->setupWriteState = 0;
			manager->setupReadState = 0;
			bbmcLeaveCS(&manager->mutex);
			request->result = (int)0x80335003;
			if (request->node->control->cancelMask == 0)
				request->completion(request->channel, 0,
					(int)0x80335003, request->input,
					request->output, request->completionArg);
			request->completion(request->channel, 36, request->result,
				request->input, request->output, request->completionArg);
		}
		goto release;
	}

	bbcmdLockMutex(&control->mutex);
	protocol = request->node->control->protocols[request->channel];
	context = request->commandContext;
	switch (request->state) {
	case 0:
		if (context == NULL) {
			context = bbcmdAlloc(sizeof(*context));
			request->commandContext = context;
			if (context == NULL) {
				ret = (int)0x80330005;
				goto fail;
			}
			memset(context, 0, sizeof(*context));
		}
		switch (request->input[0].value) {
		case 1: index = 0; break;
		case 2: index = 1; break;
		case 3: index = 2; break;
		case 4: index = 3; break;
		case 5: index = 4; break;
		case 7: index = 5; break;
		case 6: index = 6; break;
		case 0: index = 7; break;
		default: goto selected;
		}
		context->parameters = &g_qmiServiceParameterSets[index];
		if (request->input[2].value == 0)
			++context->messageIndex;
		/* fall through */
	case 1:
	case 2:
selected:
		parameters = context->parameters;
		if (parameters == NULL)
			goto invalid;
		service = parameters->service;
		SELECT_PARAMETER_BINDING(protocol, service, invalid);
		client = binding->client;
		if (client == NULL) {
			ret = (int)0x80330305;
			goto fail;
		}
		if ((SceUInt16)binding->requestBits != 0) {
			binding->requestBits |= 1u << (request->node->index + 16);
			request->state = 1;
			bbcmdUnlockMutex(&request->node->control->mutex);
			goto finished;
		}
		binding->requestBits |= 1u << request->node->index;
		memset(client->request.data, 0, sizeof(client->request.data));
		client->request.length = 0;
		memset(&client->request, 0, sizeof(client->request));
		parameters = context->parameters;
		messageId = parameters->messages[context->messageIndex];
		payloadSize = 0;
		payload = NULL;
		switch (messageId) {
		case 53: {
			const BbmcSmscAddress *address = request->input[3].pointer;
			QmiByteArrayTlv *digits;
			QmiByteArrayTlv *type;

			if (parameters->service != 5)
				goto wrongMessage;
			if (address == NULL)
				goto invalidPayload;
			payloadSize = address->digitsLength + 3;
			if (address->typeLength != 0)
				payloadSize += address->typeLength + 3;
			payload = client->request.data + 2074 - payloadSize;
			digits = (QmiByteArrayTlv *)payload;
			PARAMETER_HEADER(digits, 1, address->digitsLength);
			memcpy(digits->data, address->digits, address->digitsLength);
			if (address->typeLength != 0) {
				type = (QmiByteArrayTlv *)(payload + address->digitsLength + 3);
				PARAMETER_HEADER(type, 16, address->typeLength);
				memcpy(type->data, address->type, address->typeLength);
			}
			break;
		}
		case 52:
			if (parameters->service != 5 && parameters->service != 3)
				goto wrongMessage;
			break;
		case 43:
		case 36:
			if (parameters->service != 2)
				goto wrongMessage;
			break;
		case 39: {
			const BbmcPinProtection *pin = request->input[3].pointer;
			QmiPinProtectionTlv *protection;

			if (parameters->service != 2)
				goto wrongMessage;
			if (pin == NULL)
				goto invalidPayload;
			payloadSize = pin->pinLength + 6;
			payload = client->request.data + 2074 - payloadSize;
			protection = (QmiPinProtectionTlv *)payload;
			PARAMETER_HEADER(protection, 1, pin->pinLength + 3);
			protection->pinId = pin->pinId;
			protection->protectionEnabled = pin->protectionEnabled;
			protection->pinLength = pin->pinLength;
			memcpy(protection->pin, pin->pin, pin->pinLength);
			break;
		}
		case 42: {
			const BbmcPinChange *pin = request->input[3].pointer;
			QmiPinChangeTlv *change;

			if (parameters->service != 2)
				goto wrongMessage;
			if (pin == NULL)
				goto invalidPayload;
			payloadSize = pin->oldPinLength + pin->newPinLength + 6;
			payload = client->request.data + 2074 - payloadSize;
			change = (QmiPinChangeTlv *)payload;
			PARAMETER_HEADER(change, 1, pin->oldPinLength + pin->newPinLength + 3);
			change->pinId = pin->pinId;
			change->oldPinLength = pin->oldPinLength;
			memcpy(change->pins, pin->oldPin, pin->oldPinLength);
			change->pins[pin->oldPinLength] = pin->newPinLength;
			memcpy(change->pins + pin->oldPinLength + 1,
				pin->newPin, pin->newPinLength);
			break;
		}
		case 51: {
			const BbmcSystemSelectionPreference *selection =
				request->input[3].pointer;

			if (parameters->service != 3)
				goto wrongMessage;
			payload = client->request.data + 2044;
			if (selection->validFields & 1)
				APPEND_PARAMETER_BYTE(16, selection->emergencyMode);
			if (selection->validFields & 2) {
				QmiHalfwordValueTlv *mode =
					(QmiHalfwordValueTlv *)(payload + payloadSize);
				PARAMETER_HEADER(mode, 17, 2);
				mode->value[0] = selection->modePreference;
				mode->value[1] = selection->modePreference >> 8;
				payloadSize += sizeof(*mode);
			}
			if (selection->validFields & 4) {
				QmiByteArrayTlv *band = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(band, 18, 8);
				PARAMETER_WORD(band->data, selection->bandPreferenceLow);
				PARAMETER_WORD(band->data + 4, selection->bandPreferenceHigh);
				payloadSize += 11;
			}
			if (selection->validFields & 8) {
				QmiHalfwordValueTlv *prl =
					(QmiHalfwordValueTlv *)(payload + payloadSize);
				PARAMETER_HEADER(prl, 19, 2);
				prl->value[0] = selection->prlPreference;
				prl->value[1] = selection->prlPreference >> 8;
				payloadSize += sizeof(*prl);
			}
			if (selection->validFields & 16) {
				QmiHalfwordValueTlv *roaming =
					(QmiHalfwordValueTlv *)(payload + payloadSize);
				PARAMETER_HEADER(roaming, 20, 2);
				roaming->value[0] = selection->roamingPreference;
				roaming->value[1] = selection->roamingPreference >> 8;
				payloadSize += sizeof(*roaming);
			}
			break;
		}
		case 40: {
			const BbmcProfileSettings *profile = request->input[3].pointer;
			QmiByteArrayTlv *part;

			if (parameters->service != 1)
				goto wrongMessage;
			payload = client->request.data + 1812;
			part = (QmiByteArrayTlv *)payload;
			PARAMETER_HEADER(part, 1, 2);
			part->data[0] = 0;
			part->data[1] = 1;
			payloadSize = 5;
			if (profile->validFields & 1) {
				if (profile->profileNameLength > 15)
					goto invalidPayload;
				part = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(part, 16, profile->profileNameLength);
				for (index = 0; index < profile->profileNameLength; ++index)
					part->data[index] = profile->profileName[index];
				payloadSize += profile->profileNameLength + 3;
			}
			if (profile->validFields & 2)
				APPEND_PARAMETER_BYTE(17, profile->pdpType);
			if (profile->validFields & 4) {
				if (profile->apnLength > 100)
					goto invalidPayload;
				part = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(part, 20, profile->apnLength);
				for (index = 0; index != profile->apnLength; ++index)
					part->data[index] = profile->apn[index];
				payloadSize += profile->apnLength + 3;
			}
			if (profile->validFields & 8)
				APPEND_PARAMETER_WORD(21, profile->primaryDnsAddress);
			if (profile->validFields & 16)
				APPEND_PARAMETER_WORD(22, profile->secondaryDnsAddress);
			if (profile->validFields & 32)
				payloadSize += qmiWriteUmtsQos(23, &profile->requestedUmtsQos,
					(QmiUmtsQosTlv *)(payload + payloadSize));
			if (profile->validFields & 64)
				payloadSize += qmiWriteUmtsQos(24, &profile->minimumUmtsQos,
					(QmiUmtsQosTlv *)(payload + payloadSize));
			if (profile->validFields & 128) {
				QmiGprsQosTlv *qos = (QmiGprsQosTlv *)(payload + payloadSize);
				PARAMETER_HEADER(qos, 25, 20);
				PARAMETER_WORD(qos->precedenceClass, profile->requestedGprsQos.precedenceClass);
				PARAMETER_WORD(qos->delayClass, profile->requestedGprsQos.delayClass);
				PARAMETER_WORD(qos->reliabilityClass, profile->requestedGprsQos.reliabilityClass);
				PARAMETER_WORD(qos->peakThroughputClass, profile->requestedGprsQos.peakThroughputClass);
				PARAMETER_WORD(qos->meanThroughputClass, profile->requestedGprsQos.meanThroughputClass);
				payloadSize += sizeof(*qos);
			}
			if (profile->validFields & 256) {
				QmiGprsQosTlv *qos = (QmiGprsQosTlv *)(payload + payloadSize);
				PARAMETER_HEADER(qos, 26, 20);
				PARAMETER_WORD(qos->precedenceClass, profile->minimumGprsQos.precedenceClass);
				PARAMETER_WORD(qos->delayClass, profile->minimumGprsQos.delayClass);
				PARAMETER_WORD(qos->reliabilityClass, profile->minimumGprsQos.reliabilityClass);
				PARAMETER_WORD(qos->peakThroughputClass, profile->minimumGprsQos.peakThroughputClass);
				PARAMETER_WORD(qos->meanThroughputClass, profile->minimumGprsQos.meanThroughputClass);
				payloadSize += sizeof(*qos);
			}
			if (profile->validFields & 512) {
				if (profile->usernameLength > 127)
					goto invalidPayload;
				part = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(part, 27, profile->usernameLength);
				for (index = 0; index != profile->usernameLength; ++index)
					part->data[index] = profile->username[index];
				payloadSize += profile->usernameLength + 3;
			}
			if (profile->validFields & 1024) {
				if (profile->passwordLength > 127)
					goto invalidPayload;
				part = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(part, 28, profile->passwordLength);
				for (index = 0; index != profile->passwordLength; ++index)
					part->data[index] = profile->password[index];
				payloadSize += profile->passwordLength + 3;
			}
			if (profile->validFields & 0x800)
				APPEND_PARAMETER_BYTE(29,
					(profile->papAuthentication == 1) |
					((profile->chapAuthentication == 1) ? 2 : 0));
			if (profile->validFields & 0x1000)
				APPEND_PARAMETER_WORD(30, profile->ipv4Address);
			if (profile->validFields & 0x2000)
				APPEND_PARAMETER_BYTE(1, profile->pcscfAddressUsingPco);
			if ((int)payloadSize < 0) {
				ret = payloadSize;
				goto failedRequest;
			}
			break;
		}
		case 44:
			if (parameters->service != 1)
				goto wrongMessage;
			payload = client->request.data + 2070;
			APPEND_PARAMETER_BYTE(1, 0);
			break;
		case 34: {
			const BbmcNetworkRegistration *registration = request->input[3].pointer;
			QmiByteArrayTlv *network;

			if (parameters->service != 3)
				goto wrongMessage;
			if (registration == NULL ||
			    (registration->action != 1 && registration->action != 2))
				goto invalidPayload;
			length = registration->action == 1 ? 8 : 16;
			payload = client->request.data + 2074 - length;
			APPEND_PARAMETER_BYTE(1, registration->action);
			if (registration->action == 2) {
				network = (QmiByteArrayTlv *)(payload + payloadSize);
				PARAMETER_HEADER(network, 16, 5);
				network->data[0] = registration->mcc;
				network->data[1] = registration->mcc >> 8;
				network->data[2] = registration->mnc;
				network->data[3] = registration->mnc >> 8;
				network->data[4] = registration->radioInterface;
				payloadSize += 8;
			}
			APPEND_PARAMETER_BYTE(17, 1);
			break;
		}
		case 3: {
			const QmiCountedBytes *bytes = request->input[3].pointer;
			QmiByteArrayTlv *part;

			if (parameters->service != 227 || request->input[2].value != 1)
				goto wrongMessage;
			if (context->messageIndex == 0) {
				payloadSize = bytes->length + 4;
				payload = client->request.data + 2074 - payloadSize;
				part = (QmiByteArrayTlv *)payload;
				PARAMETER_HEADER(part, 1, bytes->length + 1);
				part->data[0] = bytes->length;
				for (index = 0; index < 10 && index < bytes->length; ++index)
					part->data[index + 1] = bytes->values[index];
			} else {
				if (context->messageIndex != 1)
					goto wrongMessage;
				payload = client->request.data + 2070;
				APPEND_PARAMETER_BYTE(2, 1);
			}
			break;
		}
		default:
wrongMessage:
			ret = (int)0x80330101;
			goto failedRequest;
invalidPayload:
			ret = (int)0x80330002;
			goto failedRequest;
		}
		service = context->parameters->service;
		messageId = context->parameters->messages[context->messageIndex];
		if (client->service != service)
			goto invalidPayload;
		if (payloadSize + client->request.length + sizeof(*message) > 2075) {
			ret = (int)0x80330009;
			goto failedRequest;
		}
		message = (QmiMessageHeader *)(client->request.data + client->request.length);
		message->messageIdLow = messageId;
		message->messageIdHigh = messageId >> 8;
		message->lengthLow = payloadSize;
		message->lengthHigh = payloadSize >> 8;
		client->request.length += sizeof(*message);
		if (payloadSize != 0) {
			memmove(client->request.data + client->request.length, payload, payloadSize);
			client->request.length += payloadSize;
		}
		client->callbackContext = request;
		ret = qmiSendClientRequest(client);
		if (ret < 0)
			goto failedRequest;
		request->state = 3;
		bbcmdUnlockMutex(&request->node->control->mutex);
		goto finished;
failedRequest:
		binding->requestBits &= ~(1u << request->node->index);
		goto fail;
	case 3:
		parameters = context->parameters;
		service = parameters->service;
		SELECT_PARAMETER_BINDING(protocol, service, responseInvalid);
		client = binding->client;
		response = (const QmiMessageHeader *)client->response.data;
		messageId = parameters->messages[context->messageIndex];
		responseId = response->messageIdLow | (response->messageIdHigh << 8);
		responseSize = response->lengthLow | (response->lengthHigh << 8);
		switch (messageId) {
		case 52:
			if (responseId != 52)
				break;
			if (parameters->service == 5) {
				BbmcSmscAddress *address = request->output[0].pointer;
				const QmiSmscAddressValue *value;

				if (address == NULL)
					goto responseInvalid;
				memset(address, 0, sizeof(*address));
				length = responseSize - 7;
				for (position = 0; position < length; position += payloadSize + 3) {
					tlv = (const QmiByteArrayTlv *)(client->response.data + 11 + position);
					payloadSize = tlv->header.lengthLow | (tlv->header.lengthHigh << 8);
					if (tlv->header.type != 1) {
						ret = (int)0x80330002;
						continue;
					}
					value = (const QmiSmscAddressValue *)tlv->data;
					address->typeLength = 3;
					address->type[0] = value->type[0];
					address->type[1] = value->type[1];
					address->type[2] = value->type[2];
					index = value->digitsLength;
					if (index >= 20)
						index = 20;
					address->digitsLength = index;
					memcpy(address->digits, value->digits, index);
				}
			} else if (parameters->service == 3) {
				BbmcSystemSelectionPreference *selection = request->output[0].pointer;

				memset(selection, 0, sizeof(*selection));
				for (position = 11; position < responseSize + 3;
				     position += payloadSize + 3) {
					tlv = (const QmiByteArrayTlv *)(client->response.data + position);
					payloadSize = tlv->header.lengthLow | (tlv->header.lengthHigh << 8);
					switch (tlv->header.type) {
					case 16:
						selection->validFields |= 1;
						selection->emergencyMode = tlv->data[0];
						break;
					case 17:
						selection->validFields |= 2;
						selection->modePreference = tlv->data[0] | (tlv->data[1] << 8);
						break;
					case 18:
						selection->validFields |= 4;
						selection->bandPreferenceLow = PARAMETER_READ_WORD(tlv->data);
						selection->bandPreferenceHigh = PARAMETER_READ_WORD(tlv->data + 4);
						break;
					case 19:
						selection->validFields |= 8;
						selection->prlPreference = tlv->data[0] | (tlv->data[1] << 8);
						break;
					case 20:
						selection->validFields |= 16;
						selection->roamingPreference = tlv->data[0] | (tlv->data[1] << 8);
						break;
					}
				}
			}
			break;
		case 43: {
			BbmcPinStatus *status = request->output[0].pointer;
			int pinState;

			if (responseId != 43 || parameters->service != 2)
				break;
			if (status == NULL || responseSize <= 6)
				goto responseInvalid;
			for (position = 4; responseSize != 0; position += payloadSize + 3,
			     responseSize -= payloadSize + 3) {
				tlv = (const QmiByteArrayTlv *)(client->response.data + position);
				payloadSize = tlv->header.lengthLow | (tlv->header.lengthHigh << 8);
				if (tlv->header.type == 17) {
					status->pin1VerifyRetries = tlv->data[1];
					status->pin1UnblockRetries = tlv->data[2];
				} else if (tlv->header.type == 18) {
					status->pin2VerifyRetries = tlv->data[1];
					status->pin2UnblockRetries = tlv->data[2];
				} else
					continue;
				index = (SceUInt8)(tlv->data[0] - 1);
				if (index > 4) {
					ret = (int)0x80330002;
					continue;
				}
				pinState = g_qmiPinStatusValues[index];
				ret = pinState;
				if (pinState >= 0) {
					if (tlv->header.type == 17)
						status->pin1Status = pinState;
					else
						status->pin2Status = pinState;
				}
			}
			break;
		}
		case 51: {
			unsigned char *words = request->output[0].pointer;

			if (responseId != 51 || parameters->service != 5)
				break;
			if (words == NULL)
				goto responseInvalid;
			/* The original copies up to sixteen words after the first word. */
			length = client->response.data[15] | (client->response.data[14] << 8);
			if (length >= 16)
				length = 16;
			for (index = 0; index < length; ++index)
				memcpy(words + sizeof(SceUInt32) * (index + 1),
					client->response.data + 5 + sizeof(SceUInt32) * index,
					sizeof(SceUInt32));
			break;
		}
		case 44: {
			BbmcProfileSettings *profile = request->output[0].pointer;

			if (responseId != 44 || parameters->service != 1)
				break;
			memset(profile, 0, sizeof(*profile));
			for (position = 11; position < responseSize; position += payloadSize + 3) {
				tlv = (const QmiByteArrayTlv *)(client->response.data + position);
				payloadSize = tlv->header.lengthLow | (tlv->header.lengthHigh << 8);
				switch (tlv->header.type) {
				case 1:
					profile->pcscfAddressUsingPco = tlv->data[0];
					profile->validFields |= 0x2000;
					break;
				case 16:
					length = payloadSize < 15 ? payloadSize : 15;
					memcpy(profile->profileName, tlv->data, length);
					profile->profileNameLength = length;
					profile->validFields |= 1;
					break;
				case 17:
					profile->pdpType = tlv->data[0];
					profile->validFields |= 2;
					break;
				case 20:
					length = payloadSize < 100 ? payloadSize : 100;
					memcpy(profile->apn, tlv->data, length);
					profile->apnLength = length;
					profile->validFields |= 4;
					break;
				case 21:
					profile->validFields |= 8;
					profile->primaryDnsAddress = PARAMETER_READ_WORD(tlv->data);
					break;
				case 22:
					profile->validFields |= 16;
					profile->secondaryDnsAddress = PARAMETER_READ_WORD(tlv->data);
					break;
				case 23: {
					const QmiUmtsQosTlv *wire = (const QmiUmtsQosTlv *)tlv;
					QmiUmtsQos *qos = &profile->requestedUmtsQos;
					SceUInt32 validFields;
					SceUInt32 priority;
					qos->trafficClass = wire->trafficClass;
					qos->maxUplinkBitrate = wire->maxUplinkBitrate.low |
						((SceUInt32)wire->maxUplinkBitrate.middle << 16) |
						((SceUInt32)wire->maxUplinkBitrate.high << 24);
					qos->maxDownlinkBitrate = wire->maxDownlinkBitrate.low |
						((SceUInt32)wire->maxDownlinkBitrate.middle << 16) |
						((SceUInt32)wire->maxDownlinkBitrate.high << 24);
					qos->guaranteedUplinkBitrate = wire->guaranteedUplinkBitrate.low |
						((SceUInt32)wire->guaranteedUplinkBitrate.middle << 16) |
						((SceUInt32)wire->guaranteedUplinkBitrate.high << 24);
					qos->guaranteedDownlinkBitrate = wire->guaranteedDownlinkBitrate.low |
						((SceUInt32)wire->guaranteedDownlinkBitrate.middle << 16) |
						((SceUInt32)wire->guaranteedDownlinkBitrate.high << 24);
					qos->deliveryOrder = wire->deliveryOrder;
					qos->maximumSduSize = wire->maximumSduSize.low |
						((SceUInt32)wire->maximumSduSize.middle << 16) |
						((SceUInt32)wire->maximumSduSize.high << 24);
					qos->sduErrorRatio = wire->sduErrorRatio;
					qos->residualBitErrorRatio = wire->residualBitErrorRatio;
					qos->deliveryErroneousSdu = wire->deliveryErroneousSdu;
					validFields = profile->validFields | 32;
					qos->transferDelay = wire->transferDelay.low |
						((SceUInt32)wire->transferDelay.middle << 16) |
						((SceUInt32)wire->transferDelay.high << 24);
					priority = wire->trafficHandlingPriority.low |
						((SceUInt32)wire->trafficHandlingPriority.middle << 16) |
						((SceUInt32)wire->trafficHandlingPriority.high << 24);
					profile->validFields = validFields;
					qos->trafficHandlingPriority = priority;
					break;
				}
				case 24: {
					const QmiUmtsQosTlv *wire = (const QmiUmtsQosTlv *)tlv;
					QmiUmtsQos *qos = &profile->minimumUmtsQos;
					SceUInt32 validFields;
					SceUInt32 priority;
					qos->trafficClass = wire->trafficClass;
					qos->maxUplinkBitrate = wire->maxUplinkBitrate.low |
						((SceUInt32)wire->maxUplinkBitrate.middle << 16) |
						((SceUInt32)wire->maxUplinkBitrate.high << 24);
					qos->maxDownlinkBitrate = wire->maxDownlinkBitrate.low |
						((SceUInt32)wire->maxDownlinkBitrate.middle << 16) |
						((SceUInt32)wire->maxDownlinkBitrate.high << 24);
					qos->guaranteedUplinkBitrate = wire->guaranteedUplinkBitrate.low |
						((SceUInt32)wire->guaranteedUplinkBitrate.middle << 16) |
						((SceUInt32)wire->guaranteedUplinkBitrate.high << 24);
					qos->guaranteedDownlinkBitrate = wire->guaranteedDownlinkBitrate.low |
						((SceUInt32)wire->guaranteedDownlinkBitrate.middle << 16) |
						((SceUInt32)wire->guaranteedDownlinkBitrate.high << 24);
					qos->deliveryOrder = wire->deliveryOrder;
					qos->maximumSduSize = wire->maximumSduSize.low |
						((SceUInt32)wire->maximumSduSize.middle << 16) |
						((SceUInt32)wire->maximumSduSize.high << 24);
					qos->sduErrorRatio = wire->sduErrorRatio;
					qos->residualBitErrorRatio = wire->residualBitErrorRatio;
					qos->deliveryErroneousSdu = wire->deliveryErroneousSdu;
					validFields = profile->validFields | 64;
					qos->transferDelay = wire->transferDelay.low |
						((SceUInt32)wire->transferDelay.middle << 16) |
						((SceUInt32)wire->transferDelay.high << 24);
					priority = wire->trafficHandlingPriority.low |
						((SceUInt32)wire->trafficHandlingPriority.middle << 16) |
						((SceUInt32)wire->trafficHandlingPriority.high << 24);
					profile->validFields = validFields;
					qos->trafficHandlingPriority = priority;
					break;
				}
				case 25: {
					const QmiGprsQosTlv *wire = (const QmiGprsQosTlv *)tlv;
					QmiGprsQos *qos = &profile->requestedGprsQos;
					SceUInt32 validFields = profile->validFields | 128;
					SceUInt32 meanThroughput;
					qos->precedenceClass = PARAMETER_READ_WORD(wire->precedenceClass);
					qos->delayClass = PARAMETER_READ_WORD(wire->delayClass);
					qos->reliabilityClass = PARAMETER_READ_WORD(wire->reliabilityClass);
					if (qos->reliabilityClass == 1)
						qos->reliabilityClass = 2;
					qos->peakThroughputClass = PARAMETER_READ_WORD(wire->peakThroughputClass);
					meanThroughput = PARAMETER_READ_WORD(wire->meanThroughputClass);
					profile->validFields = validFields;
					qos->meanThroughputClass = meanThroughput;
					break;
				}
				case 26: {
					const QmiGprsQosTlv *wire = (const QmiGprsQosTlv *)tlv;
					QmiGprsQos *qos = &profile->minimumGprsQos;
					SceUInt32 validFields = profile->validFields | 256;
					SceUInt32 meanThroughput;
					qos->precedenceClass = PARAMETER_READ_WORD(wire->precedenceClass);
					qos->delayClass = PARAMETER_READ_WORD(wire->delayClass);
					qos->reliabilityClass = PARAMETER_READ_WORD(wire->reliabilityClass);
					if (qos->reliabilityClass == 1)
						qos->reliabilityClass = 2;
					qos->peakThroughputClass = PARAMETER_READ_WORD(wire->peakThroughputClass);
					meanThroughput = PARAMETER_READ_WORD(wire->meanThroughputClass);
					profile->validFields = validFields;
					qos->meanThroughputClass = meanThroughput;
					break;
				}
				case 27:
					length = payloadSize < 128 ? payloadSize : 128;
					memcpy(profile->username, tlv->data, length);
					profile->usernameLength = length;
					profile->validFields |= 0x200;
					break;
				case 28:
					length = payloadSize < 128 ? payloadSize : 128;
					memcpy(profile->password, tlv->data, length);
					profile->passwordLength = length;
					profile->validFields |= 0x400;
					break;
				case 29:
					profile->validFields |= 0x800;
					profile->papAuthentication = tlv->data[0] & 1;
					profile->chapAuthentication = (tlv->data[0] & 2) != 0;
					break;
				case 30:
					profile->validFields |= 0x1000;
					profile->ipv4Address = PARAMETER_READ_WORD(tlv->data);
					break;
				}
			}
			break;
		}
		case 3:
			if (responseId == 3 && parameters->service == 227 &&
			    context->messageIndex == 1)
				ret = qmiReadCountedResponseBytes((const QmiResultResponse *)response,
					request->output[0].pointer);
			break;
		case 36: {
			BbmcDeviceInformation *information = request->output[0].pointer;

			if (responseId != 36 || parameters->service != 2 ||
			    context->messageIndex != 1)
				break;
			if (responseSize <= 2)
				goto responseInvalid;
			for (position = 11; position < responseSize; position += payloadSize + 3) {
				tlv = (const QmiByteArrayTlv *)(client->response.data + position);
				payloadSize = tlv->header.lengthLow | (tlv->header.lengthHigh << 8);
				if (tlv->header.type == 1) {
					length = payloadSize < 33 ? payloadSize : 33;
					memset(information->msisdn, 0, 36);
					memcpy(information->msisdn, tlv->data, length);
				}
			}
			break;
		}
		}
		goto responseParsed;
responseInvalid:
		ret = (int)0x80330002;
responseParsed:
		if (ret < 0 && request->result == 0)
			request->result = ret;
		parameters = context->parameters;
		++context->messageIndex;
		index = parameters->parameterKind - 1;
		if (index > 6 || ((1u << index) & 0x57) == 0 ||
		    context->messageIndex == parameters->messageCount) {
			context->parameters = NULL;
			context->messageIndex = 0;
			request->state = 0;
		} else {
			request->state = 2;
			ret = sceKernelSetEventFlag(request->node->control->event,
				1u << request->node->index);
		}
		bbcmdUnlockMutex(&request->node->control->mutex);
		if (ret < 0) {
			request->result = ret;
			request->state = 0;
			context->messageIndex = 0;
		}
		goto finished;
	default:
		ret = (int)0x80330102;
		goto fail;
invalid:
		ret = (int)0x80330002;
fail:
		bbcmdUnlockMutex(&request->node->control->mutex);
		request->result = ret;
		request->state = 0;
		if (context != NULL)
			context->messageIndex = 0;
		break;
	}
finished:
	if (request->state != 0)
		return ret;
complete:
	bbcmdLockMutex(&request->node->control->mutex);
	request->node->state = 2;
	bbcmdUnlockMutex(&request->node->control->mutex);
	if (request->completion != NULL)
		request->completion(request->channel, 36, request->result,
			request->input, request->output, request->completionArg);
release:
	if (request->commandContext != NULL)
		bbcmdFree(request->commandContext);
	return ret;
}

#undef APPEND_PARAMETER_BYTE
#undef APPEND_PARAMETER_WORD
#undef PARAMETER_READ_WORD
#undef PARAMETER_WORD
#undef PARAMETER_HEADER
#undef SELECT_PARAMETER_BINDING

/* name unknown: activate an instance synchronously or await secondary completion. */
int bbcmdActivateInstance(BbcmdRequest *request)
{
	unsigned int channels[2] = { 0, 1 };
	BbcmdCompletion completion;
	int ret;

	if (request == NULL) {
		ret = (int)0x80330002;
		goto out;
	}
	if (request->command != 5) {
		ret = (int)0x80330101;
		goto out;
	}
	if (request->channel > 1) {
		ret = (int)0x80330002;
		goto out;
	}
	if (request->state <= 1)
		request->state = request->channel != 0;
	bbcmdLockMutex(&request->node->control->mutex);
	switch (request->state) {
	case 0:
		ret = qmiActivateChannel(channels[request->channel], NULL, request);
		if (ret < 0)
			goto failed;
		bbcmdSetChannelState(0, 2);
		request->state = 0;
		break;
	case 1:
		ret = qmiActivateChannel(channels[request->channel],
					 bbcmdSecondaryActivateCompleted, request);
		if (ret < 0)
			goto failed;
		request->state = 2;
		break;
	case 2:
		ret = 0;
		break;
	case 3:
		ret = 0;
		request->state = 0;
		break;
	default:
		ret = (int)0x80330102;
		goto failed;
	}
	bbcmdUnlockMutex(&request->node->control->mutex);
	if (request->state != 0)
		goto out;
	goto completed;
failed:
	bbcmdUnlockMutex(&request->node->control->mutex);
	request->result = ret;
	request->state = 0;
completed:
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 5, request->result, request->input,
			   request->output, request->completionArg);
	request->node->state = 2;
out:
	return ret;
}

/* name unknown: deactivate the instance and recycle its completed request. */
int bbcmdDeactivateInstance(BbcmdRequest *request)
{
	unsigned int channels[2] = { 0, 1 };
	BbcmdCompletion completion;
	BbcmdRequestNode *node;
	int ret;

	if (request == NULL) {
		ret = (int)0x80330002;
		goto out;
	}
	if (request->command != 6) {
		ret = (int)0x80330101;
		goto out;
	}
	if (request->channel > 1) {
		ret = (int)0x80330002;
		goto out;
	}
	if (request->state > 1) {
		switch (request->state) {
		case 2:
			ret = 0;
			goto locked;
		case 3:
			ret = 0;
			request->state = 0;
			goto locked;
		default:
			ret = (int)0x80330102;
			goto failed;
		}
	}
	if (request->channel != 0) {
		request->state = 1;
		ret = qmiDeactivateChannel(channels[request->channel],
					   bbcmdSecondaryDeactivateCompleted, request);
		request->result = ret;
		if (ret >= 0) {
			request->state = 2;
			goto locked;
		}
	} else {
		request->state = 0;
		ret = qmiDeactivateChannel(channels[0], NULL, NULL);
		if (ret >= 0) {
			bbcmdSetChannelState(0, 1);
			request->state = 0;
			goto locked;
		}
	}
failed:
	bbcmdLockMutex(&request->node->control->mutex);
	request->result = ret;
	request->state = 0;
	goto completed;
locked:
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->state != 0) {
		node = request->node;
		goto unlocked;
	}
completed:
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 6, request->result, request->input,
			   request->output, request->completionArg);
	node = request->node;
	node->state = 2;
unlocked:
	bbcmdUnlockMutex(&node->control->mutex);
out:
	return ret;
}

#define READ_CHANNEL_RATE(member) \
	((SceUInt32)response->rates.member[0] | \
	 ((SceUInt32)response->rates.member[2] << 16) | \
	 ((SceUInt32)response->rates.member[1] << 8) | \
	 ((SceUInt32)response->rates.member[3] << 24))

/* name unknown: command 24 gets the four WDS channel rates. */
int bbcmdGetChannelRates(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int cancelMask, bit;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 24)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int bits, remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[2];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[2];
	if (request->state <= 1) {
		unsigned int bits;
		SceUInt16 busy;
		QmiMessageHeader *message;

		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto error;
		}
		bits = binding->requestBits;
		busy = bits;
		if (busy != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 1;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = busy;
		if (client->service != 1) {
			result = 0x80330002;
			control = request->node->control;
			goto error;
		}
		message = (QmiMessageHeader *)client->request.data;
		message->messageIdHigh = busy;
		message->lengthLow = busy;
		message->lengthHigh = busy;
		message->messageIdLow = 35;
		client->request.length = sizeof(*message);
		client = binding->client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		control = request->node->control;
		if (result < 0)
			goto error;
		request->state = 2;
	} else if (request->state == 2) {
		result = request->result;
		if (result < 0)
			goto error;
		client = binding->client;
		if ((unsigned int)client->response.length > 11) {
			const QmiChannelRatesResponse *response =
				(const QmiChannelRatesResponse *)client->response.data;

			if (response->rates.header.type == 1) {
				*(SceUInt32 *)request->output[0].pointer = READ_CHANNEL_RATE(txRate);
				*(SceUInt32 *)request->output[1].pointer = READ_CHANNEL_RATE(rxRate);
				*(SceUInt32 *)request->output[2].pointer = READ_CHANNEL_RATE(maxTxRate);
				*(SceUInt32 *)request->output[3].pointer = READ_CHANNEL_RATE(maxRxRate);
			}
		}
		result = 0;
		request->state = 0;
	} else {
		result = 0x80330102;
		goto error;
	}

unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 24, request->result,
			request->input, request->output, request->completionArg);
	return result;

error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
	goto completed;
}

/* name unknown: retrieve the bounded DMS firmware preference records. */
int bbcmdGetFirmwarePreference(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, zero;
	QmiMessageHeader *message;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 39)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[0];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	if (request->state == 1) {
		const QmiMessageHeader *response;
		unsigned int remaining = client->response.length;

		result = request->result;
		if (result < 0)
			goto error;
		if (remaining <= 10)
			goto invalid_response;
		response = (const QmiMessageHeader *)client->response.data;
		do {
			unsigned int messageId = response->messageIdLow |
				(response->messageIdHigh << 8);
			unsigned int payloadLength = response->lengthLow |
				(response->lengthHigh << 8);
			unsigned int payloadRemaining = payloadLength;
			const QmiTlvHeader *parameter = (const QmiTlvHeader *)(response + 1);

			if (payloadLength <= 6)
				goto invalid_response;
			do {
				unsigned int size = parameter->lengthLow |
					(parameter->lengthHigh << 8);
				const unsigned char *payload = (const unsigned char *)(parameter + 1);

				if (parameter->type == 2) {
					const QmiResultTlv *operation = (const QmiResultTlv *)parameter;
					unsigned int status = operation->resultLow |
						(operation->resultHigh << 8);
					unsigned int errorCode = operation->errorLow |
						(operation->errorHigh << 8);

					if (status != 0 && status != 1)
						goto invalid_response;
					if (status == 1 && errorCode != 0) {
						result = errorCode | 0x80335000;
						goto failed_response;
					}
				} else if (messageId == 71 && size != 0 && parameter->type == 1) {
					const QmiFirmwareListWire *list = (const QmiFirmwareListWire *)payload;
					QmiFirmwarePreference *preference = request->output[1].pointer;
					unsigned int offset = sizeof(*list), index = 0;

					if (preference == NULL) {
						result = 0x80330002;
						goto failed_response;
					}
					preference->count = list->count;
					if (preference->count > 2)
						goto invalid_response;
					if (preference->count != 0) {
						if (size <= sizeof(QmiFirmwareImageWire))
							goto invalid_response;
						do {
							const QmiFirmwareImageWire *wire =
								(const QmiFirmwareImageWire *)(payload + offset);
							QmiFirmwareImage *image = &preference->images[index];
							unsigned int copyLength;

							image->imageType = wire->imageType;
							__builtin_memcpy(image->uniqueId, wire->uniqueId, 16);
							copyLength = wire->buildIdLength;
							offset += sizeof(*wire) + copyLength;
							if (size < offset)
								goto invalid_response;
							if (copyLength > 35)
								copyLength = 35;
							image->buildIdLength = copyLength;
							++index;
							memcpy(image->buildId, wire->buildId, copyLength);
							if (index >= preference->count)
								break;
							if (size < offset + sizeof(*wire))
								goto invalid_response;
						} while (1);
					}
				}
				parameter = (const QmiTlvHeader *)(payload + size);
				payloadRemaining -= sizeof(*parameter) + size;
			} while (payloadRemaining != 0);
			response = (const QmiMessageHeader *)((const unsigned char *)(response + 1) + payloadLength);
			remaining -= sizeof(*response) + payloadLength;
		} while (remaining != 0);
		result = 0;
		request->state = 0;
		control = request->node->control;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (client->service != 2) {
		result = 0x80330002;
		goto failed;
	}
	message = (QmiMessageHeader *)client->request.data;
	message->messageIdHigh = zero;
	message->lengthLow = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 71;
	client->request.length = 4;
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
invalid_response:
	result = 0x80330302;
failed_response:
	request->state = 0;
failed:
	node = request->node;
failed_node:
	request->result = result;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 39, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* name unknown: list stored DMS images by type, retaining at most five per type. */
int bbcmdListStoredImages(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, zero;
	QmiMessageHeader *message;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 40)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[0];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	if (request->state == 1) {
		const QmiMessageHeader *response;
		unsigned int remaining = client->response.length;

		result = request->result;
		if (result < 0)
			goto error;
		if (remaining <= 10)
			goto invalid_response;
		response = (const QmiMessageHeader *)client->response.data;
		do {
			unsigned int messageId = response->messageIdLow |
				(response->messageIdHigh << 8);
			unsigned int payloadLength = response->lengthLow |
				(response->lengthHigh << 8);
			unsigned int payloadRemaining = payloadLength;
			const QmiTlvHeader *parameter = (const QmiTlvHeader *)(response + 1);

			if (payloadLength <= 6)
				goto invalid_response;
			do {
				unsigned int size = parameter->lengthLow |
					(parameter->lengthHigh << 8);
				const unsigned char *payload = (const unsigned char *)(parameter + 1);

				if (parameter->type == 2) {
					const QmiResultTlv *operation = (const QmiResultTlv *)parameter;
					unsigned int status = operation->resultLow |
						(operation->resultHigh << 8);
					unsigned int errorCode = operation->errorLow |
						(operation->errorHigh << 8);

					if (status != 0 && status != 1)
						goto invalid_response;
					if (status == 1 && errorCode != 0) {
						result = errorCode | 0x80335000;
						goto failed_response;
					}
				} else if (messageId == 73 && size != 0 && parameter->type == 1) {
					QmiStoredImages *output = request->output[0].pointer;
					unsigned int offset = sizeof(QmiFirmwareListWire);

					if (output == NULL) {
						result = 0x80330002;
						goto failed_response;
					}
					if (size != sizeof(QmiFirmwareListWire)) {
						if (size <= sizeof(QmiStoredImageGroupWire))
							goto invalid_response;
						do {
							const QmiStoredImageGroupWire *groupWire =
								(const QmiStoredImageGroupWire *)(payload + offset);
							QmiStoredImageGroup *group;
							unsigned int imageCount, index = 0;
							unsigned int runningIndex;

							if (groupWire->imageType == 0)
								group = &output->groups[0];
							else if (groupWire->imageType == 1)
								group = &output->groups[1];
							else
								group = &g_discardedStoredImages;
							group->maximumImages = groupWire->maximumImages;
							imageCount = groupWire->count;
							runningIndex = groupWire->runningIndex;
							group->count = imageCount;
							offset += sizeof(*groupWire);
							if (imageCount != 0) {
								if (size < offset + sizeof(QmiStoredImageWire))
									goto invalid_response;
								do {
									const QmiStoredImageWire *wire =
										(const QmiStoredImageWire *)(payload + offset);

									if (index <= 4) {
										QmiStoredImage *image = &group->images[index];
										unsigned int copyLength;

										if (index == runningIndex)
											image->isRunning = 1;
										image->storageIndex = wire->storageIndex;
										image->failureCount = wire->failureCount;
										__builtin_memcpy(image->uniqueId, wire->uniqueId, 16);
										copyLength = wire->buildIdLength;
										offset += sizeof(*wire) + copyLength;
										if (size < offset)
											goto invalid_response;
										if (copyLength > 35)
											copyLength = 35;
										image->buildIdLength = copyLength;
										memcpy(image->buildId, wire->buildId, copyLength);
										imageCount = group->count;
									} else {
										offset += sizeof(*wire) + wire->buildIdLength;
										if (size < offset)
											goto invalid_response;
									}
									if (++index >= imageCount)
										break;
									if (size < offset + sizeof(*wire))
										goto invalid_response;
								} while (1);
								if (imageCount > 5)
									group->count = 5;
							}
							if (size <= offset)
								break;
							if (size < offset + sizeof(*groupWire))
								goto invalid_response;
						} while (1);
					}
				}
				parameter = (const QmiTlvHeader *)(payload + size);
				payloadRemaining -= sizeof(*parameter) + size;
			} while (payloadRemaining != 0);
			response = (const QmiMessageHeader *)((const unsigned char *)(response + 1) + payloadLength);
			remaining -= sizeof(*response) + payloadLength;
		} while (remaining != 0);
		result = 0;
		request->state = 0;
		control = request->node->control;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (client->service != 2) {
		result = 0x80330002;
		goto failed;
	}
	message = (QmiMessageHeader *)client->request.data;
	message->messageIdHigh = zero;
	message->lengthLow = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 73;
	client->request.length = 4;
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
invalid_response:
	result = 0x80330302;
failed_response:
	request->state = 0;
failed:
	node = request->node;
failed_node:
	request->result = result;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 40, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

void bbcmdClientRetryTimer(void *arg)
{
	BbcmdProtocolClient *client = arg;
	BbcmdRequest *request = client->callbackContext;
	int (*callback)(unsigned int, BbcmdProtocolClient *) = client->callback;
	BbcmdRequestNode *node = request->node;
	BbcmdControl *control = node->control;
	BbcmdProtocolContext *protocol = control->protocols[request->channel];
	SceUInt32 *bits;
	unsigned int service, pending, pendingBits, pendingMask;

	request->retryTimer = 0;
	if (callback == bbcmdDmsClientCallback) {
		bits = &protocol->bindings[2].requestBits;
		service = 2;
	} else if (callback == bbcmdConfiguredServiceClientCallback) {
		bits = &protocol->bindings[9].requestBits;
		service = qmiGetConfiguredService();
		node = request->node;
		control = node->control;
	} else if (callback == bbcmdNasClientCallback) {
		bits = &protocol->bindings[1].requestBits;
		service = 3;
	} else if (callback == bbcmdWdsClientCallback) {
		bits = &protocol->bindings[2].requestBits;
		service = 1;
	} else if (callback == bbcmdCat2ClientCallback) {
		bits = &protocol->bindings[8].requestBits;
		service = 10;
	} else if (callback == bbcmdUimClientCallback) {
		bits = &protocol->bindings[7].requestBits;
		service = 11;
	} else if (callback == bbcmdWmsClientCallback) {
		bits = &protocol->bindings[3].requestBits;
		service = 5;
	} else if (callback == bbcmdPdsClientCallback) {
		bits = &protocol->bindings[4].requestBits;
		service = 6;
	} else if (callback == bbcmdQosClientCallback) {
		bits = &protocol->bindings[5].requestBits;
		service = 4;
	} else if (callback == bbcmdAtClientCallback) {
		bits = &protocol->bindings[6].requestBits;
		service = 8;
	} else {
		return;
	}
	*bits &= ~(1U << node->index);
	sceKernelSetEventFlag(control->event, 1U << node->index);
	if (service == 3 && request->result == (int)0x80335004)
		return;
	pendingBits = *bits;
	pendingMask = pendingBits >> 16;
	if (pendingMask == 0)
		return;
	if ((int)(pendingMask << 31) < 0)
		pending = 0;
	else if ((int)(pendingBits << 14) < 0)
		pending = 1;
	else if ((int)(pendingBits << 13) < 0)
		pending = 2;
	else if ((int)(pendingBits << 12) < 0)
		pending = 3;
	else
		return;
	sceKernelSetEventFlag(request->node->control->event, 1U << pending);
	*bits &= ~(1U << (pending + 16));
}

/* name unknown: issue at most five empty messages on the DMS client. */
int bbcmdSendEmptyDmsMessages(BbcmdRequest *request, QmiEmptyMessageList messageList)
{
	BbcmdRequestNode *node = request->node;
	BbcmdProtocolContext *protocol = node->control->protocols[request->channel];
	BbcmdClientBinding *binding = &protocol->bindings[0];
	unsigned int bits = binding->requestBits;
	unsigned int zero = (unsigned short)bits;
	BbcmdProtocolClient *client;
	QmiMessageHeader *messages;
	QmiEmptyMessageList values;
	int result;

	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 10;
		return 0;
	}
	client = binding->client;
	binding->requestBits = bits | (1U << node->index);
	values = messageList;
	memset(client->request.data, 0, sizeof(client->request.data));
	client->request.length = zero;
	if (client->service != 2) {
		result = 0x80330002;
	} else {
		messages = (QmiMessageHeader *)client->request.data;
		messages[0].messageIdLow = values.messageIds[0];
		messages[0].messageIdHigh = values.messageIds[0] >> 8;
		messages[0].lengthLow = zero;
		messages[0].lengthHigh = zero;
		client->request.length = 4;
		if (messageList.count > 1) {
			messages[1].lengthLow = zero;
			messages[1].lengthHigh = zero;
			messages[1].messageIdLow = values.messageIds[1];
			messages[1].messageIdHigh = values.messageIds[1] >> 8;
			if (messageList.count <= 2) {
				client->request.length = 8;
			} else {
				messages[2].lengthLow = zero;
				messages[2].lengthHigh = zero;
				messages[2].messageIdLow = values.messageIds[2];
				messages[2].messageIdHigh = values.messageIds[2] >> 8;
				if (messageList.count <= 3) {
					client->request.length = 12;
				} else {
					messages[3].lengthLow = zero;
					messages[3].lengthHigh = zero;
					messages[3].messageIdLow = values.messageIds[3];
					messages[3].messageIdHigh = values.messageIds[3] >> 8;
					if (messageList.count <= 4) {
						client->request.length = 16;
					} else {
						client->request.length = 20;
						messages[4].lengthLow = zero;
						messages[4].lengthHigh = zero;
						messages[4].messageIdLow = values.messageIds[4];
						messages[4].messageIdHigh = values.messageIds[4] >> 8;
					}
				}
			}
		}
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
	}
	request->result = result;
	return result;
}

/* name unknown: select the next configured client for deactivation. */
int bbcmdSelectClientDeactivation(BbcmdRequest *request, unsigned int clearMask)
{
	unsigned int completed = 0;
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdProtocolClient *client;
	int result;

	result = bbcmdGetCompletedMask(request->channel, &completed);
	if (result < 0)
		return result;
	completed &= ~clearMask;
	if (clearMask != 0)
		bbcmdSetCompletedMask(request->channel, completed);

	node = request->node;
	control = node->control;
	protocol = control->protocols[request->channel];
	if (request->state > 16)
		return 0x80330002;
	client = protocol->bindings[0].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 1;
		goto notify;
	}
	client = protocol->bindings[9].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 3;
		goto notify;
	}
	client = protocol->bindings[1].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 5;
		goto notify;
	}
	client = protocol->bindings[8].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 7;
		goto notify;
	}
	client = protocol->bindings[4].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 15;
		goto notify;
	}
	client = protocol->bindings[7].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 9;
		goto notify;
	}
	client = protocol->bindings[2].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 11;
		goto notify;
	}
	client = protocol->bindings[3].client;
	if (client != NULL && client->clientId != 255) {
		request->state = 13;
		goto notify;
	}
	request->state = 0;
	return 0;
notify:
	return sceKernelSetEventFlag(control->event, 1U << node->index);
}

int bbcmdSendTerminalResponse(BbcmdRequest *request)
{
	BbcmdControl *control;
	BbcmdRequestNode *node;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	const BbmcTerminalResponse *input;
	unsigned char *parameters;
	QmiMessageHeader *message;
	unsigned int mask, bits, flags, index, size, offset, length;
	int result, commandType;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 43)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		node = request->node;
		control = node->control;
		result = 0;
		if (completion != NULL) {
			binding = &control->protocols[request->channel]->bindings[8];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			binding->requestBits = bits;
			if (control->cancelMask == 0) {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				node = request->node;
				control = node->control;
			}
		}
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		return result;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[8];
	if (request->state == 1) {
		result = request->result;
		request->state = 3;
		result &= result >> 31;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			request->result = result;
		goto checkCompleted;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	if ((unsigned short)bits != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = 0;
	input = request->input[0].pointer;
	index = input->command - 1;
	if (index > 18) {
		result = 0x80330002;
		goto reloadError;
	}
	commandType = g_cat2TerminalCommandTypes[index];
	if (commandType > 0) {
		QmiCat2TerminalResultPrefix *wire;
		unsigned char commandNumber;

		flags = input->validFields;
		size = input->additionalInfoLength + ((flags & 1) ? 16 : 11);
		if (flags & 2)
			size += input->text.length + 5;
		if (flags & 4)
			size += input->getInkeyExtra.length + 7;
		if (flags & 8)
			size += 4;
		parameters = client->request.data + sizeof(client->request.data) - 1 - size;
		memset(parameters, 0, size);
		wire = (QmiCat2TerminalResultPrefix *)parameters;
		wire->header.type = 1;
		input = request->input[0].pointer;
		wire->header.lengthLow = input->additionalInfoLength + 8;
		input = request->input[0].pointer;
		wire->header.lengthHigh = (input->additionalInfoLength + 8) >> 8;
		input = request->input[0].pointer;
		wire->reference[0] = input->referenceId;
		input = request->input[0].pointer;
		wire->reference[1] = input->referenceId >> 8;
		input = request->input[0].pointer;
		wire->reference[2] = input->referenceId >> 16;
		input = request->input[0].pointer;
		wire->reference[3] = input->referenceId >> 24;
		input = request->input[0].pointer;
		commandNumber = input->commandNumber;
		wire->commandType = commandType;
		wire->commandNumber = commandNumber;
		input = request->input[0].pointer;
		wire->generalResult = input->generalResult;
		input = request->input[0].pointer;
		wire->additionalInfoLength = input->additionalInfoLength;
		input = request->input[0].pointer;
		memcpy(wire->additionalInfo, (const unsigned char *)input +
			offsetof(BbmcTerminalResponse, additionalInfo), input->additionalInfoLength);
		input = request->input[0].pointer;
		length = input->additionalInfoLength;
		flags = input->validFields;
		offset = length + sizeof(*wire);
		if (flags & 1) {
			QmiHalfwordValueTlv *language = (QmiHalfwordValueTlv *)(parameters + offset);

			language->header.type = 19;
			offset = length + sizeof(*wire) + sizeof(*language);
			language->header.lengthLow = 2;
			language->header.lengthHigh = 0;
			input = request->input[0].pointer;
			language->value[0] = input->language;
			input = request->input[0].pointer;
			language->value[1] = input->language >> 8;
			input = request->input[0].pointer;
			flags = input->validFields;
		}
		if (flags & 2) {
			QmiCat2TextPrefix *text = (QmiCat2TextPrefix *)(parameters + offset);

			text->header.type = 16;
			input = request->input[0].pointer;
			text->header.lengthLow = input->text.length + 2;
			input = request->input[0].pointer;
			text->header.lengthHigh = (input->text.length + 2) >> 8;
			input = request->input[0].pointer;
			text->encoding = input->text.dataCodingScheme;
			input = request->input[0].pointer;
			text->length = input->text.length;
			input = request->input[0].pointer;
			memcpy(text->text, (const unsigned char *)input +
				offsetof(BbmcTerminalResponse, text.text), input->text.length);
			input = request->input[0].pointer;
			offset += input->text.length + sizeof(*text);
			flags = input->validFields;
		}
		if (flags & 4) {
			QmiCat2GetInkeyPrefix *extra = (QmiCat2GetInkeyPrefix *)(parameters + offset);

			extra->header.type = 18;
			input = request->input[0].pointer;
			extra->header.lengthLow = input->getInkeyExtra.length + 4;
			input = request->input[0].pointer;
			extra->header.lengthHigh = (input->getInkeyExtra.length + 4) >> 8;
			input = request->input[0].pointer;
			extra->units = input->getInkeyExtra.units;
			input = request->input[0].pointer;
			extra->interval = input->getInkeyExtra.interval;
			input = request->input[0].pointer;
			extra->dataCodingScheme = input->getInkeyExtra.dataCodingScheme;
			input = request->input[0].pointer;
			extra->length = input->getInkeyExtra.length;
			input = request->input[0].pointer;
			memcpy(extra->text, (const unsigned char *)input +
				offsetof(BbmcTerminalResponse, getInkeyExtra.text), input->getInkeyExtra.length);
			input = request->input[0].pointer;
			offset += input->getInkeyExtra.length + sizeof(*extra);
			flags = input->validFields;
		}
		if (flags & 8) {
			QmiByteValueTlv *item = (QmiByteValueTlv *)(parameters + offset);

			item->header.type = 17;
			item->header.lengthLow = 1;
			item->header.lengthHigh = 0;
			input = request->input[0].pointer;
			item->value = input->itemIdentifier;
		}
		client = binding->client;
		if (client->service != 10) {
			result = 0x80330002;
			goto reloadError;
		}
		length = client->request.length;
		if (size + length + sizeof(*message) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto reloadError;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->lengthLow = size;
		message->lengthHigh = size >> 8;
		message->messageIdLow = 36;
		message->messageIdHigh = 0;
		client->request.length = length + sizeof(*message);
		if (size != 0) {
			memmove(client->request.data + length + sizeof(*message), parameters, size);
			client->request.length += size;
		}
	} else {
		if (commandType != 0) {
			result = 0x80330002;
			goto reloadError;
		}
		flags = input->validFields;
		if (flags & 0x10)
			size = (flags & 0x20) ? 8 : 4;
		else if (flags & 0x20)
			size = 4;
		else {
			if (client->service != 10) {
				result = 0x80330002;
				goto reloadError;
			}
			message = (QmiMessageHeader *)client->request.data;
			message->messageIdHigh = 0;
			message->lengthLow = 0;
			message->lengthHigh = 0;
			message->messageIdLow = 38;
			client->request.length = sizeof(*message);
			goto send;
		}
		parameters = client->request.data + sizeof(client->request.data) - 1 - size;
		memset(parameters, 0, size);
		input = request->input[0].pointer;
		offset = 0;
		if (input->validFields & 0x10) {
			QmiByteValueTlv *confirmed = (QmiByteValueTlv *)parameters;

			confirmed->header.type = 16;
			offset = sizeof(*confirmed);
			confirmed->header.lengthLow = 1;
			confirmed->header.lengthHigh = 0;
			input = request->input[0].pointer;
			confirmed->value = input->userConfirmed;
			input = request->input[0].pointer;
		}
		if (input->validFields & 0x20) {
			QmiByteValueTlv *icon = (QmiByteValueTlv *)(parameters + offset);

			icon->header.type = 17;
			icon->header.lengthLow = 1;
			icon->header.lengthHigh = 0;
			input = request->input[0].pointer;
			icon->value = input->iconIsDisplayed;
		}
		client = binding->client;
		if (client->service != 10) {
			result = 0x80330002;
			goto reloadError;
		}
		length = client->request.length;
		if (size + length + sizeof(*message) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto reloadError;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->lengthLow = size;
		message->messageIdLow = 38;
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), parameters, size);
		client->request.length += size;
	}
send:
	sendClient = binding->client;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	control = request->node->control;
	if (result < 0)
		goto error;
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
checkCompleted:
	if (request->state != 3)
		return result;
	goto complete;
reloadError:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
complete:
	request->state = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 43, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

/* name unknown: send the reset message to the configured service client. */
int bbcmdResetConfiguredService(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, length, zero;
	QmiMessageHeader *message;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 31)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[9];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[9];
	if (request->state == 1) {
		request->state = 0;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330101;
		request->result = result;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (qmiGetConfiguredService() != client->service) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (length + 4 > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdLow = zero;
	message->messageIdHigh = zero;
	message->lengthLow = zero;
	message->lengthHigh = zero;
	client->request.length = length + 4;
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	request->result = result;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 31, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* name unknown: enable or disable NAS registration reject event reports. */
int bbcmdSetRegistrationRejectReport(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, length, zero;
	QmiMessageHeader *message;
	QmiByteValueTlv *parameter;
	BbcmdProtocolClient *sendClient;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 30)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[1];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[1];
	if (request->state == 1) {
		request->state = 0;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330101;
		request->result = result;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	parameter->header.type = 18;
	client->request.length = zero;
	parameter->value = request->input[0].value == 1;
	client = binding->client;
	if (client->service != 3) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 2;
	message->lengthLow = sizeof(*parameter);
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameter,
		sizeof(*parameter));
	sendClient = binding->client;
	client->request.length += sizeof(*parameter);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	client->callbackContext = request;
	node = request->node;
failed_node:
	request->result = result;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 30, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* name unknown: attach or detach the NAS packet service. */
int bbcmdAttachDetachPacketService(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, length, zero;
	QmiMessageHeader *message;
	QmiByteValueTlv *parameter;
	BbcmdProtocolClient *sendClient;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 29)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[1];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[1];
	if (request->state == 1) {
		result = request->result;
		if (result < 0)
			goto error;
		request->state = 0;
		result = 0;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0;
		request->result = 0x80330001;
		goto unlocked;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	parameter->header.type = 16;
	client->request.length = zero;
	parameter->value = request->input[0].value == 1 ? 1 : 2;
	client = binding->client;
	if (client->service != 3) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 35;
	message->lengthLow = sizeof(*parameter);
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameter,
		sizeof(*parameter));
	sendClient = binding->client;
	client->request.length += sizeof(*parameter);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 29, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* name unknown: query UIM state followed by all five facility lock states. */
int bbcmdGetUimFacilityStatus(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiMessageHeader *message;
	QmiByteValueTlv *parameter;
	unsigned char *facility;
	unsigned int bits, bit, cancelMask, length, zero, next;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 49)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 6;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		result = 0;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[0];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		goto release_context;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	switch (request->state) {
	case 0:
	case 2:
		client = binding->client;
		if (client == NULL)
			goto missing_client;
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 2;
			result = 0;
			goto unlocked;
		}
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = zero;
		if (client->service != 2) {
			node = request->node;
			result = 0x80330002;
			client->callbackContext = request;
			control = node->control;
			goto error;
		}
		message = (QmiMessageHeader *)client->request.data;
		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 68;
		sendClient = binding->client;
		client->request.length = 4;
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		node = request->node;
		control = node->control;
		if (result < 0)
			goto error;
		request->state = 1;
		goto unlocked;
	case 1:
		result = request->result;
		if (result < 0)
			goto error;
		result = qmiReadIndexedResponse(request, binding->client, 0);
		node = request->node;
		if (result < 0) {
			control = node->control;
			goto error;
		}
		goto next_facility;
	case 3:
	case 4:
		client = binding->client;
		if (client == NULL)
			goto missing_client;
		bits = binding->requestBits;
		if ((unsigned short)bits != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 4;
			result = 0;
			goto unlocked;
		}
		facility = request->commandContext;
		binding->requestBits = bits | (1U << node->index);
		if (facility == NULL) {
			facility = bbcmdAlloc(1);
			request->commandContext = facility;
			if (facility == NULL) {
				result = 0x80330005;
				goto failed;
			}
			*facility = (unsigned char)bits;
			client = binding->client;
		}
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		parameter = (QmiByteValueTlv *)(client->request.data +
			sizeof(client->request.data) - 1 - sizeof(*parameter));
		parameter->header.lengthHigh = 0;
		parameter->header.type = 1;
		parameter->header.lengthLow = 1;
		client->request.length = 0;
		parameter->value = *facility;
		client = binding->client;
		if (client->service != 2) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*parameter) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 64;
		message->lengthLow = sizeof(*parameter);
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message),
			parameter, sizeof(*parameter));
		sendClient = binding->client;
		client->request.length += sizeof(*parameter);
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		node = request->node;
		control = node->control;
		if (result < 0)
			goto error;
		request->state = 5;
		goto unlocked;
	case 5:
		result = request->result;
		if (result < 0)
			goto error;
		facility = request->commandContext;
		if (facility == NULL) {
			result = 0x80330002;
			goto error;
		}
		result = qmiReadIndexedResponse(request, binding->client, *facility);
		next = (unsigned char)(*facility + 1);
		*facility = next;
		if (next <= 4) {
			node = request->node;
next_facility:
			request->state = 3;
			result = sceKernelSetEventFlag(node->control->event,
				1U << node->index);
		} else {
			request->state = 6;
		}
		bbcmdUnlockMutex(&request->node->control->mutex);
		if (result < 0)
			goto result_error;
		goto check_completed;
	default:
		result = 0;
		request->result = 0x80330102;
		goto unlocked;
	}
missing_client:
	result = 0x80330305;
	goto error;
failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
result_error:
	request->result = result;
	request->state = 6;
	goto completed;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 6)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 49, request->result,
			request->input, request->output, request->completionArg);
release_context:
	if (request->commandContext != NULL)
		bbcmdFree(request->commandContext);
	return result;
}

int bbcmdDeleteSmsMessage(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiWmsDeleteParameters *parameters;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, index, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 17)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		BbmcControl *manager;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		manager = bbmcGetControl();
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[3];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&manager->mutex);
		manager->smsReadState = 0;
		bbmcLeaveCS(&manager->mutex);
		control = request->node->control;
		if (control->cancelMask == 0) {
			completion = request->completion;
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[3];
	if (request->state > 1) {
		if (request->state != 2) {
			result = 0x80330102;
			goto error;
		}
		request->state = 0;
		result = request->result & (request->result >> 31);
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed;
		goto unlocked;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 1;
		goto unlock;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	parameters = (QmiWmsDeleteParameters *)(client->request.data +
		sizeof(client->request.data) - sizeof(*parameters) - 1);
	index = request->input[0].value;
	parameters->storage.header.lengthHigh = 0;
	parameters->messageIndex.header.lengthHigh = 0;
	parameters->storage.value = index >> 16 ? zero : 1;
	parameters->messageIndex.value[0] = index;
	parameters->storage.header.type = 1;
	parameters->messageIndex.value[1] = index >> 8;
	parameters->storage.header.lengthLow = 1;
	parameters->messageIndex.value[2] = 0;
	parameters->messageIndex.value[3] = 0;
	parameters->messageIndex.header.type = 16;
	parameters->messageIndex.header.lengthLow = 4;
	client = binding->client;
	if (client->service != 5) {
		result = 0x80330002;
		goto failed_node;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameters) >
	    sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed_node;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = 0;
	message->lengthHigh = 0;
	message->lengthLow = sizeof(*parameters);
	message->messageIdLow = 36;
	client->request.length = length + sizeof(*message);
	memmove(message + 1, parameters, sizeof(*parameters));
	sendClient = binding->client;
	client->request.length += sizeof(*parameters);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto error;
	request->state = 2;
unlock:
	bbcmdUnlockMutex(&control->mutex);
unlocked:
	if (request->state != 0)
		return result;
	goto completed;
failed_node:
	node = request->node;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 17, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdSetOperatingMode(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiByteValueTlv *parameter;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 37)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		BbmcControl *manager;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		manager = bbmcGetControl();
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[0];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&manager->mutex);
		manager->powerTransitionBusy = 0;
		bbmcLeaveCS(&manager->mutex);
		control = request->node->control;
		if (control->cancelMask == 0) {
			completion = request->completion;
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	if (request->state == 1) {
		result = request->result;
		request->state = 0;
		result &= result >> 31;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlock;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - sizeof(*parameter) - 1);
	parameter->header.type = 1;
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	client->request.length = zero;
	parameter->value = request->input[0].value;
	client = binding->client;
	if (client->service != 2) {
		result = 0x80330002;
		goto failed_node;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) >
	    sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed_node;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 46;
	message->lengthLow = sizeof(*parameter);
	client->request.length = length + sizeof(*message);
	memmove(message + 1, parameter, sizeof(*parameter));
	sendClient = binding->client;
	client->request.length += sizeof(*parameter);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto error;
	request->state = 1;
unlock:
	bbcmdUnlockMutex(&control->mutex);
unlocked:
	if (request->state != 0)
		return result;
	goto completed;
failed_node:
	node = request->node;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 37, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* name unknown: read the word returned by configured-service command four. */
int bbcmdReadConfiguredServiceValue(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, length, zero;
	QmiMessageHeader *message;
	QmiByteValueTlv *parameter;
	BbcmdProtocolClient *sendClient;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 33)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[9];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[9];
	if (request->state == 1) {
		if (request->result >= 0) {
			const QmiResultResponse *response;
			unsigned int remaining, offset = 0;

			client = binding->client;
			response = (const QmiResultResponse *)client->response.data;
			remaining = (unsigned int)response->message.lengthLow -
				sizeof(response->result) + (response->message.lengthHigh << 8);
			if (remaining != 0) {
				do {
					const QmiTlvHeader *header;
					unsigned int size;

					response = (const QmiResultResponse *)client->response.data;
					header = (const QmiTlvHeader *)(response->parameters + offset);
					size = (header->lengthLow | (header->lengthHigh << 8)) + sizeof(*header);
					if (header->type == 16) {
						const QmiWordValueTlv *word = (const QmiWordValueTlv *)header;

						request->output[0].value = ((unsigned int)word->value[2] << 16) |
							((unsigned int)word->value[1] << 8) | word->value[0] |
							((unsigned int)word->value[3] << 24);
					}
					offset += size;
					if (remaining <= offset)
						break;
					client = binding->client;
				} while (1);
				control = node->control;
			}
		}
		result = 0;
		request->state = 0;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330101;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.type = 1;
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	parameter->value = 1;
	sendClient = binding->client;
	client->request.length = zero;
	client = sendClient;
	if (qmiGetConfiguredService() != client->service) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 4;
	message->lengthLow = sizeof(*parameter);
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameter,
		sizeof(*parameter));
	sendClient = binding->client;
	client->request.length += sizeof(*parameter);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 33, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* The original scratch headroom is smaller than a maximum-length PIN payload. */
int bbcmdVerifyUimPin(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiUimVerifyPinParameters *parameters;
	QmiMessageHeader *message;
	unsigned char *scratch;
	unsigned int bits, bit, cancelMask, zero, pinLength, payloadLength, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 12)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 3;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[7];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[7];
	if (request->state == 1) {
		if (request->result < 0) {
			const QmiResultResponse *response;

			client = binding->client;
			response = (const QmiResultResponse *)client->response.data;
			if ((unsigned int)client->response.length > 7 &&
				(response->message.lengthLow | (response->message.lengthHigh << 8)) > 7) {
				const QmiUimPinRetryTlv *retries =
					(const QmiUimPinRetryTlv *)response->parameters;

				request->output[0].bytes[2] = retries->verifyRetries;
				response = (const QmiResultResponse *)binding->client->response.data;
				retries = (const QmiUimPinRetryTlv *)response->parameters;
				request->output[0].bytes[3] = retries->unblockRetries;
				control = node->control;
			}
		} else {
			request->output[0].bytes[3] = 10;
			request->output[0].bytes[2] = 3;
			control = node->control;
		}
		result = 0;
		request->state = 3;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = 0;
	pinLength = request->input[1].bytes[0];
	/* Address through the complete owner: the original may touch response.length. */
	scratch = (unsigned char *)client + offsetof(BbcmdProtocolClient, request) +
		offsetof(QmiPacketStorage, data) + sizeof(client->request.data) - 1 - 14;
	parameters = (QmiUimVerifyPinParameters *)scratch;
	parameters->session.header.type = 1;
	parameters->session.header.lengthHigh = 0;
	parameters->session.sessionType = 0;
	parameters->session.aidLength = 0;
	parameters->pin.header.lengthHigh = zero;
	parameters->pin.pinId = 1;
	payloadLength = pinLength + sizeof(*parameters);
	parameters->pin.pinLength = pinLength;
	parameters->pin.header.lengthLow = pinLength + 2;
	parameters->session.header.lengthLow = 2;
	parameters->pin.header.type = 2;
	memcpy(scratch + sizeof(*parameters), request->input[0].pointer, pinLength);
	client = binding->client;
	if (client->service != 11) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 38;
	message->lengthLow = payloadLength;
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), scratch, payloadLength);
	sendClient = binding->client;
	client->request.length += payloadLength;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 3)
		return result;
	goto completed;
failed:
	node = request->node;
	request->result = result;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 3;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 12, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdUnblockUimPin(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiUimUnblockPinParameters *parameters;
	QmiMessageHeader *message;
	unsigned char *scratch;
	unsigned int bits, bit, cancelMask, zero, pukLength, pinLength, payloadLength, length;
	QmiLengthPrefixedBytes *newPin;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 13)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 3;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[7];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[7];
	if (request->state == 1) {
		if (request->result < 0) {
			const QmiResultResponse *response;

			client = binding->client;
			response = (const QmiResultResponse *)client->response.data;
			if ((unsigned int)client->response.length > 7) {
				const QmiUimPinRetryTlv *retries =
					(const QmiUimPinRetryTlv *)response->parameters;

				request->output[0].bytes[2] = retries->verifyRetries;
				response = (const QmiResultResponse *)binding->client->response.data;
				retries = (const QmiUimPinRetryTlv *)response->parameters;
				request->output[0].bytes[3] = retries->unblockRetries;
				control = node->control;
			}
		} else {
			request->output[0].bytes[3] = 10;
			request->output[0].bytes[2] = 3;
			control = node->control;
		}
		result = 0;
		request->state = 3;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = 0;
	pukLength = request->input[1].value;
	pinLength = request->input[3].value;
	payloadLength = pukLength + pinLength + sizeof(*parameters) + sizeof(*newPin);
	scratch = client->request.data + sizeof(client->request.data) - 1 - payloadLength;
	parameters = (QmiUimUnblockPinParameters *)scratch;
	parameters->session.header.type = 1;
	parameters->pin.pinId = 1;
	parameters->pin.pukLength = pukLength;
	parameters->pin.header.lengthLow = pukLength + pinLength + 3;
	parameters->session.header.lengthHigh = zero;
	parameters->session.sessionType = zero;
	parameters->session.aidLength = zero;
	parameters->pin.header.lengthHigh = zero;
	parameters->session.header.lengthLow = 2;
	parameters->pin.header.type = 2;
	memcpy(scratch + sizeof(*parameters), request->input[0].pointer, pukLength);
	newPin = (QmiLengthPrefixedBytes *)(scratch + sizeof(*parameters) + pukLength);
	newPin->length = pinLength;
	memcpy(newPin->value, request->input[2].pointer, pinLength);
	client = binding->client;
	if (client->service != 11) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 39;
	message->lengthLow = payloadLength;
	client->request.length = length + sizeof(*message);
	if (payloadLength != 0) {
		memmove(client->request.data + length + sizeof(*message), scratch, payloadLength);
		client->request.length += payloadLength;
	}
	sendClient = binding->client;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 3)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 3;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 13, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdSetFirmwarePreference(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, zero;
	QmiMessageHeader *message;
	QmiSetFirmwarePreference *options;
	QmiFirmwarePreference *preference;
	QmiFirmwarePreferenceTlv *parameters;
	unsigned char *cursor;
	unsigned int payloadLength, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 38)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[0];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	if (request->state == 1) {
		const QmiMessageHeader *response;
		unsigned int remaining = client->response.length;

		result = request->result;
		if (result < 0)
			goto error;
		if (remaining <= 10)
			goto invalid_response;
		response = (const QmiMessageHeader *)client->response.data;
		do {
			unsigned int messageId = response->messageIdLow |
				(response->messageIdHigh << 8);
			unsigned int payloadLength = response->lengthLow |
				(response->lengthHigh << 8);
			unsigned int payloadRemaining = payloadLength;
			const QmiTlvHeader *parameter = (const QmiTlvHeader *)(response + 1);

			if (payloadLength <= 6)
				goto invalid_response;
			do {
				unsigned int size = parameter->lengthLow |
					(parameter->lengthHigh << 8);
				const unsigned char *payload = (const unsigned char *)(parameter + 1);

				if (parameter->type == 2) {
					const QmiResultTlv *operation = (const QmiResultTlv *)parameter;
					unsigned int status = operation->resultLow;
					unsigned int errorCode = operation->errorLow |
						(operation->errorHigh << 8);

					if (status != 0 && status != 1)
						goto invalid_response;
					if (status == 1) {
						if (errorCode != 0) {
							result = errorCode | 0x80335000;
							goto failed_response;
						}
						break;
					}
				} else if (messageId == 72 && parameter->type == 1) {
					const QmiFirmwareListWire *list = (const QmiFirmwareListWire *)payload;

					request->output[2].value = list->count;
					if (list->count != 0) {
						request->output[3].value = list->images[0];
						if (list->count != 1)
							request->output[4].value = list->images[1];
					}
				}
				parameter = (const QmiTlvHeader *)(payload + size);
				payloadRemaining -= sizeof(*parameter) + size;
			} while (payloadRemaining != 0 && payloadRemaining <= payloadLength);
			response = (const QmiMessageHeader *)((const unsigned char *)(response + 1) + payloadLength);
			remaining -= sizeof(*response) + payloadLength;
		} while (remaining != 0 && remaining <= (unsigned int)client->response.length);
		result = 0;
		request->state = 0;
		control = request->node->control;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	options = request->input[1].pointer;
	preference = &options->preference;
	payloadLength = sizeof(*parameters);
	if (preference->count != 0) {
		payloadLength = preference->images[0].buildIdLength + sizeof(*parameters) +
			sizeof(QmiFirmwareImageWire);
		if (preference->count > 1)
			payloadLength += preference->images[1].buildIdLength + sizeof(QmiFirmwareImageWire);
	}
	if (options->downloadOverride == 1)
		payloadLength += sizeof(QmiByteValueTlv);
	if (options->hasStorageIndex == 1)
		payloadLength += sizeof(QmiByteValueTlv);
	parameters = (QmiFirmwarePreferenceTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - payloadLength);
	parameters->header.type = 1;
	parameters->count = preference->count;
	cursor = parameters->images;
	if ((int)preference->count > 0) {
		int index = 0;

		do {
			QmiFirmwareImage *image = &preference->images[index];
			QmiFirmwareImageWire *wire = (QmiFirmwareImageWire *)cursor;

			++index;
			wire->imageType = image->imageType;
			__builtin_memcpy(wire->uniqueId, image->uniqueId, 16);
			if (image->buildIdLength > sizeof(image->buildId))
				image->buildIdLength = sizeof(image->buildId);
			wire->buildIdLength = image->buildIdLength;
			cursor = memcpy(wire->buildId, image->buildId, image->buildIdLength);
			cursor += image->buildIdLength;
		} while (index < (int)preference->count);
	}
	length = cursor - (unsigned char *)parameters - sizeof(parameters->header);
	parameters->header.lengthLow = length;
	parameters->header.lengthHigh = length >> 8;
	if (options->downloadOverride == 1) {
		QmiByteValueTlv *value = (QmiByteValueTlv *)cursor;

		value->header.lengthLow = 1;
		cursor += sizeof(*value);
		value->header.type = 16;
		value->header.lengthHigh = 0;
		value->value = options->downloadOverride;
	}
	if (options->hasStorageIndex == 1) {
		QmiByteValueTlv *value = (QmiByteValueTlv *)cursor;

		value->header.lengthLow = 1;
		value->header.type = 17;
		value->header.lengthHigh = 0;
		value->value = options->storageIndex;
	}
	client = binding->client;
	if (client->service != 2) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->lengthLow = payloadLength;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 72;
	message->messageIdHigh = 0;
	client->request.length = length + sizeof(*message);
	if (payloadLength != 0) {
		memmove(client->request.data + length + sizeof(*message), parameters, payloadLength);
		client->request.length += payloadLength;
	}
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);

	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
invalid_response:
	result = 0x80330302;
failed_response:
	request->state = 0;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 38, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdDeleteStoredImage(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bits, bit, cancelMask, zero;
	QmiMessageHeader *message;
	QmiFirmwareImage *image;
	QmiFirmwareImageWire *wire;
	QmiTlvHeader *parameters;
	unsigned char *cursor;
	unsigned int payloadLength, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 41)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	result = cancelMask & bit;
	if (result != 0) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			unsigned int remaining;

			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[0];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			remaining = control->cancelMask;
			binding->requestBits = bits;
			if (remaining == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	if (request->state == 1) {
		const QmiMessageHeader *response;
		unsigned int remaining = client->response.length;

		result = request->result;
		if (result < 0)
			goto error;
		if (remaining <= 10)
			goto invalid_response;
		response = (const QmiMessageHeader *)client->response.data;
		do {
			unsigned int payloadLength = response->lengthLow |
				(response->lengthHigh << 8);
			unsigned int payloadRemaining = payloadLength;
			const QmiTlvHeader *parameter = (const QmiTlvHeader *)(response + 1);

			if (payloadLength <= 6)
				goto invalid_response;
			do {
				unsigned int size = parameter->lengthLow |
					(parameter->lengthHigh << 8);
				const unsigned char *payload = (const unsigned char *)(parameter + 1);

				if (parameter->type == 2) {
					const QmiResultTlv *operation = (const QmiResultTlv *)parameter;
					unsigned int status = operation->resultLow;
					unsigned int errorCode = operation->errorLow |
						(operation->errorHigh << 8);

					if (status != 0 && status != 1)
						goto invalid_response;
					if (status == 1) {
						if (errorCode != 0) {
							result = errorCode | 0x80335000;
							goto failed_response;
						}
						break;
					}
				}
				parameter = (const QmiTlvHeader *)(payload + size);
				payloadRemaining -= sizeof(*parameter) + size;
			} while (payloadRemaining != 0);
			response = (const QmiMessageHeader *)((const unsigned char *)(response + 1) + payloadLength);
			remaining -= sizeof(*response) + payloadLength;
		} while (remaining != 0);
		result = 0;
		request->state = 0;
		control = request->node->control;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	image = request->input[0].pointer;
	payloadLength = image->buildIdLength + sizeof(*parameters) + sizeof(*wire);
	parameters = (QmiTlvHeader *)(client->request.data +
		sizeof(client->request.data) - 1 - payloadLength);
	parameters->type = 1;
	wire = (QmiFirmwareImageWire *)(parameters + 1);
	wire->imageType = image->imageType;
	__builtin_memcpy(wire->uniqueId, image->uniqueId, 16);
	wire->buildIdLength = image->buildIdLength;
	cursor = memcpy(wire->buildId, image->buildId, image->buildIdLength);
	length = cursor + (unsigned short)image->buildIdLength - (unsigned char *)wire;
	parameters->lengthLow = length;
	parameters->lengthHigh = length >> 8;
	client = binding->client;
	if (client->service != 2) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->lengthLow = payloadLength;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 74;
	message->messageIdHigh = 0;
	client->request.length = length + sizeof(*message);
	if (payloadLength != 0) {
		memmove(client->request.data + length + sizeof(*message), parameters, payloadLength);
		client->request.length += payloadLength;
	}
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);

	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
invalid_response:
	result = 0x80330302;
failed_response:
	request->state = 0;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 41, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

#include "tracking_start.h"

/* name unknown: stop the WDS packet-data handle and complete under the mutex. */
#define PDS_LE16(value) ((unsigned int)(value)[0] | ((unsigned int)(value)[1] << 8))
#define PDS_LE32(value) (PDS_LE16(value) | ((unsigned int)(value)[2] << 16) | \
	((unsigned int)(value)[3] << 24))
#define PDS_COPY_BYTES(destination, source) do { \
	((unsigned char *)&(destination))[0] = (source)[0]; \
	((unsigned char *)&(destination))[1] = (source)[1]; \
	((unsigned char *)&(destination))[2] = (source)[2]; \
	((unsigned char *)&(destination))[3] = (source)[3]; \
	if (sizeof(destination) == 8) { \
		((unsigned char *)&(destination))[4] = (source)[4]; \
		((unsigned char *)&(destination))[5] = (source)[5]; \
		((unsigned char *)&(destination))[6] = (source)[6]; \
		((unsigned char *)&(destination))[7] = (source)[7]; \
	} \
} while (0)
#define PDS_OUTPUT_ADDRESS(member) ((unsigned char *)output + \
	offsetof(QmiPdsEventReport, member))

int bbcmdListSmsMessages(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient, *responseClient;
	BbcmdCompletion completion;
	QmiByteValueTlv *parameter;
	QmiMessageHeader *message;
	BbcmdArgument countBytes, indexBytes;
	unsigned int bits, bit, cancelMask, zero, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 15)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		BbmcControl *manager;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		manager = bbmcGetControl();
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[3];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&manager->mutex);
		manager->smsListBusy = 0;
		bbmcLeaveCS(&manager->mutex);
		control = request->node->control;
		if (control->cancelMask == 0) {
			completion = request->completion;
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[3];
	switch (request->state) {
	case 0:
	case 3:
		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto error;
		}
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			result = 0;
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 3;
			goto unlock;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		parameter = (QmiByteValueTlv *)(client->request.data +
			sizeof(client->request.data) - sizeof(*parameter) - 1);
		parameter->header.type = 1;
		parameter->header.lengthLow = 1;
		parameter->header.lengthHigh = zero;
		parameter->value = 1;
		sendClient = binding->client;
		client->request.length = zero;
		if (sendClient->service != 5) {
			result = 0x80330002;
			goto failed_node;
		}
		length = sendClient->request.length;
		if (length + sizeof(*message) + sizeof(*parameter) >
		    sizeof(sendClient->request.data)) {
			result = 0x80330009;
			goto failed_node;
		}
		message = (QmiMessageHeader *)(sendClient->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->lengthLow = sizeof(*parameter);
		message->messageIdLow = 49;
		sendClient->request.length = length + sizeof(*message);
		memmove(message + 1, parameter, sizeof(*parameter));
		client = binding->client;
		sendClient->request.length += sizeof(*parameter);
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		node = request->node;
		control = node->control;
		if (result < 0)
			goto error;
		request->state = 1;
		goto unlock;
	case 1:
	case 2: {
		const QmiMessageHeader *response;
		const unsigned char *parameters;
		unsigned int offset = 0, responseLength;

		result = request->result;
		if (result < 0)
			goto error;
		responseClient = binding->client;
		response = (const QmiMessageHeader *)responseClient->response.data;
		parameters = (const unsigned char *)(response + 1);
		responseLength = response->lengthLow | (response->lengthHigh << 8);
		if (responseLength <= 2) {
			result = 0x80330002;
			goto parsed;
		}
		client = binding->client;
		do {
			const QmiTlvHeader *header =
				(const QmiTlvHeader *)(parameters + offset);
			unsigned int size = header->lengthLow | (header->lengthHigh << 8);

			if (header->type == 1) {
				const QmiWmsMessageListTlv *list =
					(const QmiWmsMessageListTlv *)header;
				const QmiWmsMessageListEntry *entry;
				const QmiByteValueTlv *storage;
				unsigned int *count = request->output[1].pointer;
				unsigned int added, first, total, end, storageMask;

				countBytes.bytes[0] = list->count[0];
				countBytes.bytes[1] = list->count[1];
				countBytes.bytes[2] = list->count[2];
				countBytes.bytes[3] = list->count[3];
				if (countBytes.value >= 64)
					countBytes.value = 64;
				added = countBytes.value;
				first = *count;
				total = first + added;
				if (total >= 64)
					total = 64;
				if (64 - first < added)
					added = 64 - first;
				*count = total;
				storage = (const QmiByteValueTlv *)(client->request.data +
					sizeof(QmiMessageHeader));
				if (storage->value == 0)
					storageMask = 0xffff0000U;
				else if (storage->value == 1)
					storageMask = 0;
				else {
					result = 0x80330002;
					goto parsed;
				}
				if (added != 0) {
					entry = list->entries;
					end = first + added;
					do {
						indexBytes.bytes[0] = entry->memoryIndex[0];
						++entry;
						indexBytes.bytes[1] = entry[-1].memoryIndex[1];
						indexBytes.bytes[2] = entry[-1].memoryIndex[2];
						indexBytes.bytes[3] = entry[-1].memoryIndex[3];
						((unsigned int *)request->output[0].pointer)[first] =
							indexBytes.value | storageMask;
						++first;
					} while (first != end);
				}
			}
			offset += size + sizeof(*header);
		} while (offset < responseLength);
		result = 0;
parsed:
		if (request->state == 1) {
			binding->requestBits |= 1U << node->index;
			memset(responseClient->request.data, 0,
				sizeof(responseClient->request.data));
			client = binding->client;
			parameter = (QmiByteValueTlv *)(client->request.data +
				sizeof(client->request.data) - sizeof(*parameter) - 1);
			parameter->header.type = 1;
			parameter->header.lengthLow = 1;
			parameter->header.lengthHigh = 0;
			parameter->value = 0;
			sendClient = binding->client;
			client->request.length = 0;
			if (sendClient->service == 5) {
				length = sendClient->request.length;
				if (length + sizeof(*message) + sizeof(*parameter) <=
				    sizeof(sendClient->request.data)) {
					message = (QmiMessageHeader *)(sendClient->request.data + length);
					message->messageIdHigh = 0;
					message->lengthHigh = 0;
					message->messageIdLow = 49;
					message->lengthLow = sizeof(*parameter);
					sendClient->request.length = length + sizeof(*message);
					memmove(message + 1, parameter, sizeof(*parameter));
					client = binding->client;
					sendClient->request.length += sizeof(*parameter);
					sendClient = binding->client;
					client->callbackContext = request;
				}
			}
			result = qmiSendClientRequest(sendClient);
			node = request->node;
			control = node->control;
			if (result < 0) {
				request->result = result;
				goto error;
			}
			request->state = 2;
			goto unlock;
		}
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed;
		goto unlocked;
	}
	default:
		result = 0x80330102;
		goto error;
	}
unlock:
	bbcmdUnlockMutex(&control->mutex);
unlocked:
	if (request->state != 0)
		return result;
	goto completed;
failed_node:
	node = request->node;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 15, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdReadPdsEventReport(QmiPdsEventReport *output,
			  const QmiMessageHeader *message, unsigned int totalSize)
{
	const unsigned char *bytes = (const unsigned char *)message;
	unsigned int remaining, cursor, stringBytes = 0;
	int result;

	if (totalSize <= 3 || output == NULL)
		return 0x80330002;
	remaining = totalSize - sizeof(*message);
	cursor = sizeof(*message);
	while (remaining != 0 && totalSize > remaining) {
		const QmiTlvHeader *header = (const QmiTlvHeader *)(bytes + cursor);
		const unsigned char *payload = (const unsigned char *)(header + 1);
		BbmcControl *control = bbmcGetControl();
		unsigned int length = header->lengthLow | (header->lengthHigh << 8);
		unsigned int size = length + sizeof(*header);
		unsigned int index;

		result = 0;
		switch (header->type) {
		case 18:
			output->sessionStatus = payload[0];
			if (output->sessionStatus == 0 && g_trackingEnabled == 1) {
				if (g_trackingShortTimer != 0)
					bbmcTimerCancelHandler(g_trackingShortTimer);
				g_trackingShortTimer = 0;
				g_trackingShortPending = 0;
				if (g_trackingLongTimer != 0)
					bbmcTimerCancelHandler(g_trackingLongTimer);
				g_trackingLongTimer = 0;
				g_trackingLongPending = 0;
				g_trackingEnabled = 0;
				g_trackingSatelliteAverage = 0;
				g_trackingSignalAverage = 0;
				g_trackingSampleCount = 0;
			}
			if (control->gpsProviderRouting == 0)
				result = bbmcNotifyGpsSessionStatus(&output->sessionStatus);
			break;
		case 19: {
			const QmiPdsPositionWire *position = (const QmiPdsPositionWire *)payload;
			QmiPdsPosition *decoded = &output->position;

			if (length != sizeof(*position))
				return 0x80330002;
			decoded->validFields = PDS_LE32(position->validFields);
			decoded->year = PDS_LE16(position->year);
			decoded->month = position->month;
			decoded->dayOfWeek = position->dayOfWeek;
			decoded->dayOfMonth = position->dayOfMonth;
			decoded->hour = position->hour;
			decoded->minute = position->minute;
			decoded->second = position->second;
			decoded->millisecond = PDS_LE16(position->millisecond);
			decoded->leapSeconds = position->leapSeconds;
			PDS_COPY_BYTES(decoded->utcTimestamp, position->utcTimestamp);
			PDS_COPY_BYTES(decoded->timeUncertainty, position->timeUncertainty);
			bbcmdEncodeClampedFloat((BbcmdFloatEncoding *)&decoded->latitude,
				(const BbcmdDoubleEncoding *)position->latitude);
			bbcmdEncodeClampedFloat((BbcmdFloatEncoding *)&decoded->longitude,
				(const BbcmdDoubleEncoding *)position->longitude);
#define PDS_POSITION_FLOAT(member) PDS_COPY_BYTES(decoded->member, position->member)
			PDS_POSITION_FLOAT(altitudeEllipsoid);
			PDS_POSITION_FLOAT(altitudeSeaLevel);
			PDS_POSITION_FLOAT(horizontalSpeed);
			PDS_POSITION_FLOAT(verticalSpeed);
			PDS_POSITION_FLOAT(heading);
			PDS_POSITION_FLOAT(horizontalUncertaintyCircular);
			PDS_POSITION_FLOAT(horizontalUncertaintyEllipseMajor);
			PDS_POSITION_FLOAT(horizontalUncertaintyEllipseMinor);
			PDS_POSITION_FLOAT(horizontalUncertaintyEllipseAzimuth);
			PDS_POSITION_FLOAT(verticalUncertainty);
			PDS_POSITION_FLOAT(horizontalVelocityUncertainty);
			PDS_POSITION_FLOAT(verticalVelocityUncertainty);
			decoded->horizontalConfidence = position->horizontalConfidence;
			PDS_POSITION_FLOAT(positionDop);
			PDS_POSITION_FLOAT(horizontalDop);
			PDS_POSITION_FLOAT(verticalDop);
#undef PDS_POSITION_FLOAT
			*(unsigned char *)&decoded->operationMode = position->operationMode;
			if (control->gpsProviderRouting == 0)
				result = bbmcNotifyGpsPosition(decoded);
			break;
		}
		case 20:
		case 21: {
			QmiPdsNmeaReport *report = header->type == 20 ?
				&output->nmea : &output->extendedNmea;
			unsigned int count = payload[header->type == 20 ? 2 : 4];
			unsigned int prefix = header->type == 20 ? 3 : 5;

			report->operationMode = PDS_LE16(payload);
			if (count >= 16)
				count = 16;
			report->count = count;
			for (index = 0; index < count; ++index) {
				unsigned int textLength = payload[prefix + stringBytes + index];
				unsigned char *destination = (unsigned char *)output +
					(header->type == 20 ? offsetof(QmiPdsEventReport, nmea) :
					 offsetof(QmiPdsEventReport, extendedNmea)) +
					offsetof(QmiPdsNmeaReport, sentences) + index * sizeof(QmiPdsNmeaSentence);

				destination[0] = textLength;
				memcpy(destination + offsetof(QmiPdsNmeaSentence, text),
					payload + prefix + stringBytes + index + 1, textLength);
				stringBytes += textLength;
			}
			result = header->type == 20 ? bbmcNotifyGpsNmea(report) :
				bbmcNotifyGpsExtendedNmea(report);
			break;
		}
		case 22:
			output->wifiRequest.requestType = payload[0];
			output->wifiRequest.intervalMilliseconds = PDS_LE16(payload + 1);
			result = bbmcNotifyGpsWifiRequest(&output->wifiRequest);
			break;
		case 23: {
			QmiPdsSatelliteReport *report = &output->satelliteReport;
			const QmiPdsSatelliteWire *satellite = (const QmiPdsSatelliteWire *)(payload + 6);
			unsigned int sum = 0, average, count, samples, previous;

			if (size <= 7)
				return 0x80330002;
			report->validFields = PDS_LE32(payload);
			report->ionosphericCorrections = payload[4];
			report->count = payload[5];
			if (size < sizeof(*satellite) * report->count + 9)
				return 0x80330002;
			for (index = 0; index < report->count; ++index, ++satellite) {
				QmiPdsSatellite *decoded = (QmiPdsSatellite *)(PDS_OUTPUT_ADDRESS(satelliteReport) +
					offsetof(QmiPdsSatelliteReport, satellites) + index * sizeof(*decoded));

				decoded->validFields = PDS_LE32(satellite->validFields);
				decoded->system = satellite->system;
				decoded->prn = satellite->prn;
				decoded->health = satellite->health;
				decoded->processingStatus = satellite->processingStatus;
				decoded->ephemerisState = satellite->ephemerisState;
				decoded->almanacState = satellite->almanacState;
				PDS_COPY_BYTES(decoded->elevation, satellite->elevation);
				decoded->azimuth = PDS_LE16(satellite->azimuth);
				decoded->carrierToNoise = PDS_LE16(satellite->carrierToNoise);
				sum += decoded->carrierToNoise;
			}
			count = report->count;
			average = count == 0 ? 0 : (unsigned short)(sum / count);
			if (g_trackingEnabled != 0) {
				previous = g_trackingSampleCount;
				samples = ++g_trackingSampleCount;
				if (previous != 0) {
					if (samples > 19) {
						previous = 19;
						samples = 20;
					}
					g_trackingSignalAverage = (average + g_trackingSignalAverage * previous) / samples;
					g_trackingSatelliteAverage = (100 * count + g_trackingSatelliteAverage * previous) / samples;
					if ((g_trackingLongPending != 0 || g_trackingShortPending != 0) &&
					    g_trackingSatelliteAverage >= 500 && g_trackingSignalAverage > 199) {
						bbmcEnterCS(&control->mutex);
						control->trackingStartMode = g_trackingLongPending != 0 ? 2 : 1;
						bbmcLeaveCS(&control->mutex);
						result = bbmcNotifyTrackingStarted();
						if (g_trackingLongPending != 0) {
							if (g_trackingShortTimer != 0)
								bbmcTimerCancelHandler(g_trackingShortTimer);
							g_trackingShortTimer = 0;
							g_trackingShortPending = 0;
							if (g_trackingLongTimer != 0)
								bbmcTimerCancelHandler(g_trackingLongTimer);
							g_trackingLongTimer = 0;
							g_trackingLongPending = 0;
							g_trackingEnabled = 0;
							g_trackingSatelliteAverage = 0;
							g_trackingSignalAverage = 0;
							g_trackingSampleCount = 0;
						} else if (g_trackingShortPending != 0) {
							g_trackingShortPending = 0;
						}
					}
				} else {
					g_trackingSignalAverage = average;
					g_trackingSatelliteAverage = 100 * count;
				}
			}
			if (control->gpsProviderRouting == 0)
				result = bbmcNotifyGpsSatellites(report);
			break;
		}
		case 24: {
			const QmiPdsVxPromptWire *prompt = (const QmiPdsVxPromptWire *)payload;
			QmiPdsVxPrompt *decoded = &output->vxPrompt;

			decoded->validFields = PDS_LE32(prompt->validFields);
			decoded->privacy = prompt->privacy;
			decoded->qualityOfService = prompt->qualityOfService;
			decoded->positionCount = PDS_LE16(prompt->positionCount) |
				(prompt->positionCount[2] << 16) | (prompt->validFields[3] << 24);
			decoded->intervalSeconds = PDS_LE32(prompt->intervalSeconds);
			decoded->mode = prompt->mode;
			decoded->requestorCodingScheme = prompt->requestorCodingScheme;
			decoded->requestorLength = prompt->requestorLength;
			memcpy(PDS_OUTPUT_ADDRESS(vxPrompt.requestor), prompt->requestor, prompt->requestorLength);
			break;
		}
		case 25: {
			const QmiPdsSuplPromptWire *prompt = (const QmiPdsSuplPromptWire *)payload;
			const QmiPdsCountedStringWire *requestor, *client;
			const unsigned char *tail;
			QmiPdsSuplPrompt *decoded = &output->suplPrompt;

			decoded->validFields = PDS_LE32(prompt->validFields);
			decoded->privacy = prompt->privacy;
			memcpy(decoded->initHash, prompt->initHash, sizeof(decoded->initHash));
			decoded->mode = prompt->mode;
			decoded->sessionId = PDS_LE32(prompt->sessionId);
			decoded->ipv4Port = PDS_LE32(prompt->ipv4Port);
			decoded->ipv6Port = PDS_LE32(prompt->ipv6Port);
			PDS_COPY_BYTES(decoded->ipv4Address, prompt->ipv4Address);
			memcpy(decoded->ipv6Address, prompt->ipv6Address, sizeof(decoded->ipv6Address));
			decoded->serverUrlLength = prompt->serverUrlLength;
			memcpy(PDS_OUTPUT_ADDRESS(suplPrompt.serverUrl), prompt->serverUrl, prompt->serverUrlLength);
			tail = (const unsigned char *)prompt->serverUrl + prompt->serverUrlLength;
			decoded->requestCodingScheme = *tail++;
			requestor = (const QmiPdsCountedStringWire *)tail;
			decoded->requestorCodingScheme = requestor->codingScheme;
			decoded->requestorLength = requestor->length;
			memcpy(PDS_OUTPUT_ADDRESS(suplPrompt.requestor), requestor->text, requestor->length);
			client = (const QmiPdsCountedStringWire *)(requestor->text + requestor->length);
			decoded->clientCodingScheme = client->codingScheme;
			decoded->clientLength = client->length;
			memcpy(PDS_OUTPUT_ADDRESS(suplPrompt.client), client->text, client->length);
			tail = client->text + client->length;
			output->suplHorizontalAccuracy = tail[0];
			output->suplVerticalAccuracy = tail[1];
			break;
		}
		case 26: {
			const QmiPdsUmtsCpPromptWire *prompt = (const QmiPdsUmtsCpPromptWire *)payload;
			QmiPdsUmtsCpPrompt *decoded = &output->umtsCpPrompt;
			const QmiPdsCountedStringWire *requestor, *codeword;
			const unsigned char *tail;
			unsigned int clientLength;

			decoded->validFields = PDS_LE32(prompt->validFields);
			decoded->privacy = prompt->privacy;
			decoded->invokeId = prompt->invokeId;
			decoded->codewordCodingScheme = prompt->notificationCodingScheme;
			decoded->notificationLength = prompt->notificationLength;
			memcpy(PDS_OUTPUT_ADDRESS(umtsCpPrompt.notification), prompt->notification, prompt->notificationLength);
			tail = prompt->notification + prompt->notificationLength;
			clientLength = *tail++;
			decoded->clientAddressLength = clientLength;
			memcpy(PDS_OUTPUT_ADDRESS(umtsCpPrompt.clientAddress), tail, clientLength);
			tail += clientLength;
			decoded->locationType = *tail++;
			requestor = (const QmiPdsCountedStringWire *)tail;
			decoded->requestorCodingScheme = requestor->codingScheme;
			decoded->requestorLength = requestor->length;
			memcpy(PDS_OUTPUT_ADDRESS(umtsCpPrompt.requestor), requestor->text, requestor->length);
			codeword = (const QmiPdsCountedStringWire *)(requestor->text + requestor->length);
			decoded->codewordCodingScheme = codeword->codingScheme;
			decoded->codewordLength = codeword->length;
			memcpy(PDS_OUTPUT_ADDRESS(umtsCpPrompt.codeword), codeword->text, codeword->length);
			decoded->serviceType = codeword->text[codeword->text[0]];
			break;
		}
		case 27:
			output->communicationEvent = payload[0];
			output->communicationState = payload[1];
			break;
		}
		cursor += size;
		remaining -= size;
		if (result != 0)
			return result;
	}
	return 0;
}

#undef PDS_OUTPUT_ADDRESS
#undef PDS_COPY_BYTES
#undef PDS_LE32
#undef PDS_LE16

int bbcmdSendSmsMessage(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiWmsRawMessageTlv *parameter;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, length, format;
	unsigned int payloadLength, offset;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 18)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		BbmcControl *manager;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		manager = bbmcGetControl();
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[3];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&manager->mutex);
		manager->smsSendBusy = 0;
		bbmcLeaveCS(&manager->mutex);
		control = request->node->control;
		if (control->cancelMask == 0) {
			completion = request->completion;
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[3];
	if (request->state == 1) {
		result = request->result;
		request->state = 0;
		result &= result >> 31;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlock;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	length = request->input[1].value;
	format = request->input[2].value;
	if (format != 0 && format != 6) {
		result = 0x80330002;
		goto failed_node;
	}
	parameter = (QmiWmsRawMessageTlv *)(client->request.data +
		sizeof(client->request.data) - length - sizeof(*parameter) - 1);
	parameter->header.type = 1;
	parameter->format = format;
	parameter->lengthLow = length;
	parameter->header.lengthLow = length + 3;
	payloadLength = length + sizeof(*parameter);
	parameter->header.lengthHigh = (length + 3) >> 8;
	parameter->lengthHigh = length >> 8;
	memcpy(parameter->data, request->input[0].pointer, length);
	client = binding->client;
	if (client->service != 5) {
		result = 0x80330002;
		goto failed_node;
	}
	length = client->request.length;
	offset = length + sizeof(*message);
	if (offset + payloadLength > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed_node;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->lengthLow = payloadLength;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 32;
	message->messageIdHigh = 0;
	client->request.length = offset;
	if (payloadLength != 0) {
		memmove(message + 1, parameter, payloadLength);
		client->request.length += payloadLength;
	}
	sendClient = binding->client;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto error;
	request->state = 1;
unlock:
	bbcmdUnlockMutex(&control->mutex);
unlocked:
	if (request->state != 0)
		return result;
	goto completed;
failed_node:
	node = request->node;
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 18, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdStopTracking(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiByteValueTlv *parameter;
	QmiMessageHeader *message;
	unsigned int bits, bit, mask, zero, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 22)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = control->cancelMask;
	bit = 1U << node->index;
	result = mask & bit;
	if (result != 0) {
		control->cancelMask = mask & ~bit;
		request->state = 5;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		binding = &control->protocols[request->channel]->bindings[4];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		if (request->node->control->cancelMask != 0)
			return 0;
		completion(request->channel, 0, request->result,
			request->input, request->output, request->completionArg);
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[4];
	if (request->state > 1) {
		if (request->state != 2) {
			result = 0x80330102;
			goto error;
		}
		result = request->result;
		if (result < 0)
			goto error;
		if (g_trackingShortTimer != 0)
			bbmcTimerCancelHandler(g_trackingShortTimer);
		g_trackingShortTimer = 0;
		g_trackingShortPending = 0;
		if (g_trackingLongTimer != 0)
			bbmcTimerCancelHandler(g_trackingLongTimer);
		g_trackingLongTimer = 0;
		g_trackingLongPending = 0;
		g_trackingEnabled = 0;
		g_trackingSatelliteAverage = 0;
		g_trackingSignalAverage = 0;
		g_trackingSampleCount = 0;
		result = 0;
		if (((BbmcControl *)request->completionArg)->gpsProviderRouting == 0) {
			result = bbmcNotifyTrackingStopped();
			if (result < 0)
				goto failed;
		}
		request->state = 5;
		control = request->node->control;
		goto unlocked;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 1;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.type = 1;
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	parameter->value = zero;
	sendClient = binding->client;
	client->request.length = zero;
	if (sendClient->service != 6) {
		result = 0x80330002;
		goto failed;
	}
	length = sendClient->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(sendClient->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(sendClient->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->messageIdLow = 49;
	message->lengthLow = sizeof(*parameter);
	sendClient->request.length = length + sizeof(*message);
	memmove(sendClient->request.data + length + sizeof(*message), parameter, sizeof(*parameter));
	client = binding->client;
	sendClient->request.length += sizeof(*parameter);
	parameter = (QmiByteValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.type = 1;
	parameter->header.lengthLow = 1;
	parameter->header.lengthHigh = zero;
	parameter->value = zero;
	sendClient = binding->client;
	if (sendClient->service != 6) {
		result = 0x80330002;
		goto failed;
	}
	length = sendClient->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(sendClient->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(sendClient->request.data + length);
	message->messageIdLow = 33;
	message->messageIdHigh = 0;
	message->lengthHigh = 0;
	message->lengthLow = sizeof(*parameter);
	sendClient->request.length = length + sizeof(*message);
	memmove(sendClient->request.data + length + sizeof(*message), parameter, sizeof(*parameter));
	client = binding->client;
	sendClient->request.length += sizeof(*parameter);
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	if (result < 0)
		goto failed_node;
	control = node->control;
	request->state = 2;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 5)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 5;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 22, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdStopNetwork(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiWordValueTlv *parameter;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 26)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		context = control->protocols[request->channel];
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL) {
			node = request->node;
			control = node->control;
			result = 0;
			goto finish;
		}
		if (context->bindings != NULL) {
			binding = &context->bindings[2];
			bits = binding->requestBits;
			if (bits >> 16) {
				node = request->node;
				control = node->control;
				result = control->cancelMask;
				binding->requestBits = bits & ~(1U << (node->index + 16));
				if (result == 0)
					goto cancel_completion;
				result = 0;
				goto finish;
			}
		}
		node = request->node;
		control = node->control;
		result = control->cancelMask;
		if (result != 0) {
			result = 0;
			goto finish;
		}
cancel_completion:
		completion(request->channel, 0, request->result,
			request->input, request->output, request->completionArg);
		node = request->node;
		control = node->control;
		goto finish;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	context = control->protocols[request->channel];
	binding = &context->bindings[2];
	if (request->state == 1) {
		result = request->result;
		request->state = 0;
		if (result >= 0)
			result = 0;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto error_result;
		goto check_completed;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = (unsigned short)bits;
	if (zero != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (request->input[0].value == 0) {
		result = 0x80330002;
		control = request->node->control;
		goto error;
	}
	parameter = (QmiWordValueTlv *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*parameter));
	parameter->header.type = 1;
	parameter->header.lengthHigh = zero;
	parameter->header.lengthLow = 4;
	parameter->value[0] = request->input[0].value;
	parameter->value[1] = request->input[0].value >> 8;
	parameter->value[2] = request->input[0].halfwords[1];
	parameter->value[3] = request->input[0].bytes[3];
	client = binding->client;
	if (client->service != 1) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (length + sizeof(*message) + sizeof(*parameter) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = zero;
	message->lengthLow = sizeof(*parameter);
	message->messageIdLow = 33;
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameter, sizeof(*parameter));
	sendClient = binding->client;
	client->request.length += sizeof(*parameter);
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	request->state = 1;
	control = node->control;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 0)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
error_result:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 26, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
finish:
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

int bbcmdConfiguredServiceClientCallback(unsigned int event,
	BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int service = qmiGetConfiguredService();
	unsigned int bits, next;
	int result;

	if (event == 4 && service <= qmiGetConfiguredService() && incomingClient != NULL) {
		int indicationSize = incomingClient->indication.length;
		const QmiMessageHeader *message =
			(const QmiMessageHeader *)incomingClient->indication.data;
		unsigned int messageId;

		if (indicationSize < 0)
			goto complete;
		switch (service) {
		case 1:
			if (incomingClient->channel > 1)
				break;
			messageId = message->messageIdLow | (message->messageIdHigh << 8);
			if (messageId == 34) {
				qmiReadPacketServiceStatus(message, incomingClient->indication.length);
			} else if (messageId == 1) {
				BbmcControl *control = bbmcGetControl();
				unsigned int remaining, initialRemaining;
				const QmiTlvHeader *parameter;

				if ((unsigned int)indicationSize <= 4)
					break;
				remaining = message->lengthLow | (message->lengthHigh << 8);
				initialRemaining = remaining;
				parameter = (const QmiTlvHeader *)(message + 1);
				while (remaining != 0 && initialRemaining >= remaining) {
					unsigned int length = message->messageIdLow |
						(message->messageIdHigh << 8);

					if (parameter->type == 29) {
						const QmiWdsBearerTlv *bearer =
							(const QmiWdsBearerTlv *)parameter;
						unsigned int currentBearer = bearer->currentBearer;
						unsigned int details;

						memcpy(&details, bearer->bearerDetails, sizeof(details));
						bbmcEnterCS(&control->mutex);
						control->currentBearer = currentBearer;
						control->bearerDetails = details;
						bbmcLeaveCS(&control->mutex);
						bbmcNotifyFeatureEvent(control, 5, 8);
					}
					parameter = (const QmiTlvHeader *)
						((const unsigned char *)(parameter + 1) + length);
					remaining -= sizeof(*parameter) + length;
				}
			}
			break;
		case 3:
			if (incomingClient->channel > 1)
				break;
			messageId = message->messageIdLow | (message->messageIdHigh << 8);
			if (messageId == 2) {
				BbmcControl *control = bbmcGetControl();
				const QmiTlvHeader *parameter;
				unsigned int remaining;

				if ((unsigned int)indicationSize <= 4)
					break;
				remaining = message->lengthLow | (message->lengthHigh << 8);
				parameter = (const QmiTlvHeader *)(message + 1);
				while (remaining != 0) {
					unsigned int length = parameter->lengthLow |
						(parameter->lengthHigh << 8);

					if (parameter->type == 18) {
						const QmiNasCauseTlv *cause = (const QmiNasCauseTlv *)parameter;
						unsigned int causeValue = cause->cause;
						SceUInt16 encodedReason;
						unsigned int reason;

						memcpy(&encodedReason, cause->reason, sizeof(encodedReason));
						bbmcEnterCS(&control->mutex);
						control->nasCause = causeValue;
						reason = encodedReason;
						switch (reason) {
						case 0: case 2: case 3: case 4: case 5: case 6:
						case 11: case 12: case 13: case 15: case 17:
						case 20: case 21: case 22: case 32: case 33:
						case 34: case 38: case 95: case 96: case 97:
						case 98: case 99: case 100: case 101: case 111:
							break;
						default:
							if (reason - 48 <= 15)
								reason = 0;
							else if (reason <= 47)
								reason = 34;
							else
								reason = 111;
						}
						control->nasReason = reason;
						bbmcLeaveCS(&control->mutex);
						if (control->currentState == 7)
							bbmcNotifyFeatureEvent(control, 0, 0x2000000);
					}
					parameter = (const QmiTlvHeader *)
						((const unsigned char *)(parameter + 1) + length);
					remaining -= sizeof(*parameter) + length;
				}
			}
			break;
		case 6:
			if (incomingClient->channel > 1)
				break;
			messageId = message->messageIdLow | (message->messageIdHigh << 8);
			if (messageId == 1 && indicationSize != 0) {
				QmiPdsEventReport *report = bbcmdAlloc(sizeof(*report));

				if (report != NULL) {
					memset(report, 0, sizeof(*report));
					bbcmdReadPdsEventReport(report, message, incomingClient->indication.length);
					bbcmdFree(report);
				}
			} else if (messageId == 32 && (unsigned int)indicationSize > 4) {
				BbmcControl *control = bbmcGetControl();
				const QmiPdsServiceStateTlv *state =
					(const QmiPdsServiceStateTlv *)(message + 1);

				if (control->trackingState == 4 && state->header.type == 1 &&
				    g_previousTrackingSessionState == 2 && state->trackingSessionState == 1) {
					bbmcNotifyTrackingStopped();
					bbmcEnterCS(&control->mutex);
					control->trackingState = 0;
					bbmcLeaveCS(&control->mutex);
				}
				g_previousTrackingSessionState = state->trackingSessionState;
			}
			break;
		case 2: case 4: case 5: case 8: case 10: case 11:
			break;
		default:
			qmiGetConfiguredService();
		}
	}
complete:
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (service > qmiGetConfiguredService())
		return 0x80330002;
	switch (service) {
	case 1: binding = &context->bindings[2]; break;
	case 2: binding = &context->bindings[0]; break;
	case 3: binding = &context->bindings[1]; break;
	case 4: binding = &context->bindings[5]; break;
	case 5: binding = &context->bindings[3]; break;
	case 6: binding = &context->bindings[4]; break;
	case 8: binding = &context->bindings[6]; break;
	case 10: binding = &context->bindings[8]; break;
	case 11: binding = &context->bindings[7]; break;
	default:
		if (service != qmiGetConfiguredService())
			return 0x80330002;
		binding = &context->bindings[9];
	}
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		request->result = qmiCheckResponseResults(client);
		result = 0;
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (service > qmiGetConfiguredService()) {
			result = 0x80330002;
			goto out;
		}
		result = 0;
		if (client->indication.length < 0) {
			request->result = 0x80330002;
			break;
		}
		if (service - 1 <= 10) {
			bits = 1U << (service - 1);
			if (bits & 0x69f)
				break;
			if (bits & 0x20) {
				const QmiMessageHeader *message = (const QmiMessageHeader *)client->indication.data;
				unsigned int messageId = message->messageIdLow | (message->messageIdHigh << 8);

				if (messageId == 1 || messageId == 32)
					request->result = 0;
				break;
			}
		}
		if (service == qmiGetConfiguredService())
			break;
		result = 0x80330002;
		goto out;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	if (service == 3 && request->result == (int)0x80335004)
		goto out;
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

/* name unknown: complete requests and release queued UIM clients. */
int bbcmdUimClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 10)
		return 0x80330002;
	binding = &context->bindings[7];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 10) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdDmsClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 1)
		return 0x80330002;
	binding = &context->bindings[0];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 1) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdQosClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 3)
		return 0x80330002;
	binding = &context->bindings[5];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 3) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdAtClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 7)
		return 0x80330002;
	binding = &context->bindings[6];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 7) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdCat2ClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 9)
		return 0x80330002;
	binding = &context->bindings[8];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 9) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdPdsClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4 && qmiGetConfiguredService() > 5 && incomingClient != NULL &&
	    incomingClient->indication.length >= 0 && incomingClient->channel <= 1) {
		const QmiMessageHeader *message =
			(const QmiMessageHeader *)incomingClient->indication.data;
		unsigned int messageId = message->messageIdLow | (message->messageIdHigh << 8);

		if (messageId == 1) {
			if (incomingClient->indication.length != 0) {
				QmiPdsEventReport *report = bbcmdAlloc(sizeof(*report));

				if (report != NULL) {
					memset(report, 0, sizeof(*report));
					bbcmdReadPdsEventReport(report, message, incomingClient->indication.length);
					bbcmdFree(report);
				}
			}
		} else if (messageId == 32 && (unsigned int)incomingClient->indication.length > 4) {
			BbmcControl *control = bbmcGetControl();
			const QmiPdsServiceStateTlv *state = (const QmiPdsServiceStateTlv *)(message + 1);

			if (control->trackingState == 4 && state->header.type == 1 &&
			    g_previousTrackingSessionState == 2 && state->trackingSessionState == 1) {
				bbmcNotifyTrackingStopped();
				bbmcEnterCS(&control->mutex);
				control->trackingState = 0;
				bbmcLeaveCS(&control->mutex);
			}
			g_previousTrackingSessionState = state->trackingSessionState;
		}
	}
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 5)
		return 0x80330002;
	binding = &context->bindings[4];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 5) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0) {
			request->result = 0x80330002;
		} else {
			const QmiMessageHeader *message = (const QmiMessageHeader *)client->indication.data;
			unsigned int messageId = message->messageIdLow | (message->messageIdHigh << 8);

			if (messageId == 1 || messageId == 32)
				request->result = 0;
		}
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdWdsClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4 && qmiGetConfiguredService() != 0 && incomingClient != NULL) {
		int indicationSize = incomingClient->indication.length;
		const QmiMessageHeader *message =
			(const QmiMessageHeader *)incomingClient->indication.data;
		unsigned int messageId;

		if (indicationSize < 0 || incomingClient->channel > 1)
			goto complete;
		messageId = message->messageIdLow | (message->messageIdHigh << 8);

		if (messageId == 34) {
			qmiReadPacketServiceStatus(message, incomingClient->indication.length);
		} else if (messageId == 1) {
			BbmcControl *control = bbmcGetControl();
			unsigned int remaining, initialRemaining;
			const QmiTlvHeader *parameter;

			if ((unsigned int)indicationSize > 4) {
				remaining = message->lengthLow | (message->lengthHigh << 8);
				initialRemaining = remaining;
				parameter = (const QmiTlvHeader *)(message + 1);
				while (remaining != 0 && initialRemaining >= remaining) {
					unsigned int length = message->messageIdLow |
						(message->messageIdHigh << 8);

					if (parameter->type == 29) {
						const QmiWdsBearerTlv *bearer = (const QmiWdsBearerTlv *)parameter;
						unsigned int currentBearer = bearer->currentBearer;
						unsigned int details;

						__builtin_memcpy(&details, bearer->bearerDetails, sizeof(details));
						bbmcEnterCS(&control->mutex);
						control->currentBearer = currentBearer;
						control->bearerDetails = details;
						bbmcLeaveCS(&control->mutex);
						bbmcNotifyFeatureEvent(control, 5, 8);
					}
					parameter = (const QmiTlvHeader *)
						((const unsigned char *)(parameter + 1) + length);
					remaining -= sizeof(*parameter) + length;
				}
			}
		}
	}
complete:
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() == 0)
		return 0x80330002;
	binding = &context->bindings[2];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() == 0) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdNasClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4 && qmiGetConfiguredService() > 2 && incomingClient != NULL) {
		int indicationSize = incomingClient->indication.length;
		const QmiMessageHeader *message = (const QmiMessageHeader *)incomingClient->indication.data;

		if (indicationSize >= 0 && incomingClient->channel <= 1 &&
		    (message->messageIdLow | (message->messageIdHigh << 8)) == 2) {
			BbmcControl *control = bbmcGetControl();

			if ((unsigned int)indicationSize > 4) {
				unsigned int remaining = message->lengthLow | (message->lengthHigh << 8);
				const QmiTlvHeader *parameter = (const QmiTlvHeader *)(message + 1);

				while (remaining != 0) {
					unsigned int length = parameter->lengthLow | (parameter->lengthHigh << 8);

					if (parameter->type == 18) {
						const QmiNasCauseTlv *cause = (const QmiNasCauseTlv *)parameter;
						unsigned int causeValue = cause->cause;
						SceUInt16 encodedReason;
						unsigned int reason;

						__builtin_memcpy(&encodedReason, cause->reason, sizeof(encodedReason));
						bbmcEnterCS(&control->mutex);
						reason = encodedReason;
						control->nasCause = causeValue;
						switch (encodedReason) {
						case 0: case 2: case 3: case 4: case 5: case 6:
						case 11: case 12: case 13: case 15: case 17:
						case 20: case 21: case 22: case 32: case 33:
						case 34: case 38: case 95: case 96: case 97:
						case 98: case 99: case 100: case 101: case 111:
							break;
						default:
							if ((unsigned int)encodedReason - 48 > 15) {
								if (encodedReason <= 47)
									reason = 34;
								else
									reason = 111;
							} else {
								reason = 0;
							}
						}
						control->nasReason = reason;
						bbmcLeaveCS(&control->mutex);
						if (control->currentState == 7)
							bbmcNotifyFeatureEvent(control, 0, 0x2000000);
					}
					parameter = (const QmiTlvHeader *)
						((const unsigned char *)(parameter + 1) + length);
					remaining -= sizeof(*parameter) + length;
				}
			}
		}
	}
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 2)
		return 0x80330002;
	binding = &context->bindings[1];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 2) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	if (request->result == (int)0x80335004)
		goto out;
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdWmsClientCallback(unsigned int event, BbcmdProtocolClient *incomingClient)
{
	BbcmdControl *control;
	BbcmdRequest *request;
	BbcmdRequestNode *node;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	unsigned int bits, next;
	int result;

	if (event == 4)
		qmiGetConfiguredService();
	request = incomingClient->callbackContext;
	if (request == NULL || request->node == NULL)
		return 0x80330002;
	node = request->node;
	context = node->control->protocols[request->channel];
	if (qmiGetConfiguredService() <= 4)
		return 0x80330002;
	binding = &context->bindings[3];
	client = binding->client;
	if (client == NULL)
		return 0;
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->command == 10) {
		if (event != 1) {
			result = 0;
			goto out;
		}
		goto deactivated;
	}
	switch (event) {
	case 0:
		if (client->clientId != 0) {
			request->result = 0;
			result = 0;
			break;
		}
		result = 0;
		request->result = qmiCheckResponseResults(client);
		goto out;
	case 1:
deactivated:
		result = qmiCheckResponseResults(client);
		request->result = result;
		if (result == 0 && client->clientId != 0)
			client->clientId = 255;
		result = 0;
		break;
	case 2:
		result = 0;
		goto out;
	case 3:
		result = client->result;
		if (result == (int)0x80330306) {
			request->result = result;
			break;
		}
		if ((unsigned int)client->response.length <= 6) {
			request->result = 0x80330002;
			result = 0x80330002;
			goto out;
		}
		result = qmiCheckResponseResults(client);
		if (request->result == 0 || result != 0)
			request->result = result;
		break;
	case 4:
		if (qmiGetConfiguredService() <= 4) {
			result = 0x80330002;
			goto out;
		}
		if (client->indication.length < 0)
			request->result = 0x80330002;
		result = 0;
		break;
	default:
		result = 0x80330002;
		goto out;
	}
	if (request->state == 0)
		client->callbackContext = NULL;
	if (event > 1 && event != 3)
		goto out;
	node = request->node;
	control = node->control;
	binding->requestBits &= ~(1U << node->index);
	result = sceKernelSetEventFlag(control->event, 1U << node->index);
	bits = binding->requestBits;
	if ((bits >> 16) == 0)
		goto out;
	if (bits & 0x10000)
		next = 0;
	else if (bits & 0x20000)
		next = 1;
	else if (bits & 0x40000)
		next = 2;
	else if (bits & 0x80000)
		next = 3;
	else
		goto out;
	result = sceKernelSetEventFlag(request->node->control->event, 1U << next);
	binding->requestBits &= ~(1U << (next + 16));
out:
	bbcmdUnlockMutex(&request->node->control->mutex);
	return result;
}

int bbcmdReadSmsMessage(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiWmsMemoryIndexTlv *memory;
	QmiWmsModifyTagTlv *tag;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, index, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 16)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;

	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	zero = (cancelMask >> node->index) & 1;
	if (zero != 0) {
		BbmcControl *manager;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		if (request->completion == NULL)
			return 0;
		manager = bbmcGetControl();
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[3];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&manager->mutex);
		manager->smsReadState = 0;
		bbmcLeaveCS(&manager->mutex);
		control = request->node->control;
		result = control->cancelMask;
		if (result != 0)
			return 0;
		request->completion(request->channel, 0, request->result,
			request->input, request->output, request->completionArg);
		return result;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[3];
	if (request->state <= 1) {
		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto error;
		}
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 1;
			result = 0;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		memory = (QmiWmsMemoryIndexTlv *)(client->request.data +
			sizeof(client->request.data) - sizeof(*memory) - 1);
		memory->header.type = 1;
		client->request.length = zero;
		memory->header.lengthHigh = zero;
		memory->header.lengthLow = 5;
		index = request->input[0].value;
		memory->storageType = index >> 16 ? zero : 1;
		memory->memoryIndex[0] = index;
		memory->memoryIndex[2] = 0;
		memory->memoryIndex[1] = index >> 8;
		memory->memoryIndex[3] = 0;
		client = binding->client;
		if (client->service != 5) {
			result = 0x80330002;
			goto request_failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*memory) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto request_failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->lengthLow = sizeof(*memory);
		message->messageIdLow = 34;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), memory, sizeof(*memory));
		sendClient = binding->client;
		client->request.length += sizeof(*memory);
		sendClient->callbackContext = request;

		index = request->input[0].value;
		client = binding->client;
		tag = (QmiWmsModifyTagTlv *)(client->request.data +
			sizeof(client->request.data) - sizeof(*tag) - 1);
		tag->messageTag = 0;
		tag->memory.storageType = index >> 16 ? 0 : 1;
		tag->memory.memoryIndex[0] = index;
		tag->memory.memoryIndex[1] = index >> 8;
		tag->memory.header.lengthHigh = 0;
		tag->memory.memoryIndex[2] = 0;
		tag->memory.memoryIndex[3] = 0;
		tag->memory.header.type = 1;
		tag->memory.header.lengthLow = 6;
		tag->messageTag = request->input[2].bytes[0];
		client = binding->client;
		if (client->service != 5) {
			result = 0x80330002;
			goto request_failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*tag) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto request_failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->lengthLow = sizeof(*tag);
		message->messageIdLow = 35;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), tag, sizeof(*tag));
		sendClient = binding->client;
		client->request.length += sizeof(*tag);
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		control = request->node->control;
		if (result < 0)
			goto error;
		request->state = 2;
		goto unlocked;
	}
	if (request->state == 2) {
		unsigned int remaining, offset;
		int parseResult;
		const QmiResultResponse *response;

		result = request->result;
		if (result < 0) {
			request->state = 0;
			goto error;
		}
		client = binding->client;
		response = (const QmiResultResponse *)client->response.data;
		remaining = response->message.lengthLow |
			(response->message.lengthHigh << 8);
		if (remaining < sizeof(response->result)) {
			result = 0x80330302;
			goto error;
		}
		remaining -= sizeof(response->result);
		if (remaining == 0) {
			request->state = 0;
			bbcmdUnlockMutex(&control->mutex);
			result = 0;
			goto check_completed;
		}
		offset = zero;
		parseResult = zero;
		do {
			const QmiWmsReadMessageTlv *raw =
				(const QmiWmsReadMessageTlv *)(response->parameters + offset);
			unsigned int type = raw->header.type;
			unsigned int size;

			if (type != 1)
				parseResult = 0x80330002;
			size = raw->header.lengthLow | (raw->header.lengthHigh << 8);
			if (type == 1) {
				*(SceUInt32 *)request->output[0].pointer = raw->format;
				*(SceUInt32 *)request->output[2].pointer = size;
				memcpy(request->output[1].pointer, raw->data, size);
			}
			offset += size + sizeof(QmiTlvHeader);
			if (offset >= remaining)
				break;
			client = binding->client;
			response = (const QmiResultResponse *)client->response.data;
		} while (1);
		result = parseResult;
		request->state = 0;
		bbcmdUnlockMutex(&request->node->control->mutex);
		if (parseResult != 0)
			goto failed;
		goto check_completed;
	}
	result = 0x80330102;
	goto error;
request_failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 0;
	goto completed;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 0)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 16, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdDepersonalizeUim(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	const BbmcControlKeyParam *key;
	QmiUimDepersonalizationTlv *parameters;
	QmiByteValueTlv *value;
	QmiMessageHeader *message;
	unsigned int bits, bit, cancelMask, zero, keyLength, payloadLength, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 50)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;

	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		completion = request->completion;
		control->cancelMask = cancelMask & ~bit;
		result = 0;
		if (completion != NULL) {
			context = control->protocols[request->channel];
			if (request->state == 5)
				binding = &context->bindings[0];
			else
				binding = &context->bindings[7];
			bits = binding->requestBits & ~bit;
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			binding->requestBits = bits;
			if (control->cancelMask == 0) {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				control = request->node->control;
			}
		}
		goto reset_node;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	context = control->protocols[request->channel];

	switch (request->state) {
	case 0:
	case 1:
		binding = &context->bindings[7];
		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto error;
		}
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 1;
			result = 0;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = zero;
		key = request->input[0].pointer;
		keyLength = key->controlKeyLength;
		parameters = (QmiUimDepersonalizationTlv *)(client->request.data +
			sizeof(client->request.data) - keyLength - sizeof(*parameters) - 1);
		parameters->header.type = 1;
		parameters->header.lengthHigh = zero;
		parameters->header.lengthLow = keyLength + 3;
		payloadLength = keyLength + sizeof(*parameters);
		parameters->feature = key->featureId;
		parameters->controlKeyLength = keyLength;
		parameters->operation = request->input[1].value;
		key = request->input[0].pointer;
		memcpy(parameters->controlKey, key->controlKey.digits, keyLength);
		client = binding->client;
		if (client->service != 11) {
			result = 0x80330002;
			goto first_request_failed;
		}
		length = client->request.length;
		if (payloadLength + length + sizeof(*message) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto first_request_failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = payloadLength >> 8;
		message->messageIdLow = 41;
		message->lengthLow = payloadLength;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message),
			parameters, payloadLength);
		sendClient = binding->client;
		client->request.length += payloadLength;
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		node = request->node;
		control = node->control;
		if (result < 0)
			goto error;
		request->state = 2;
		goto unlocked;
	case 2:
		result = request->result;
		if (result >= 0) {
			request->output[2].bytes[0] = 10;
			request->output[2].bytes[1] = 32;
		} else {
			unsigned int offset, remaining;

			if (result != (int)0x80335023 && result != (int)0x8033500c)
				goto error;
			binding = &context->bindings[7];
			client = binding->client;
			request->state = 0;
			remaining = client->response.length;
			if (remaining > 11) {
				offset = 11;
				do {
					const QmiUimPinRetryTlv *retries =
						(const QmiUimPinRetryTlv *)(client->response.data + offset);
					unsigned int size = retries->header.lengthLow |
						(retries->header.lengthHigh << 8);

					if (retries->header.type == 16) {
						key = request->input[0].pointer;
						request->output[2].bytes[0] = retries->verifyRetries;
						client = binding->client;
						retries = (const QmiUimPinRetryTlv *)(
							client->response.data + offset);
						request->output[2].bytes[1] = retries->unblockRetries;
						request->output[1].value = key->featureId;
						client = binding->client;
						remaining = client->response.length;
					}
					offset += size + sizeof(QmiTlvHeader);
				} while (offset < remaining);
			}
		}
		control = node->control;
		request->state = 3;
		result = sceKernelSetEventFlag(control->event, 1U << node->index);
		goto response_unlocked;
	case 3:
	case 4:
		binding = &context->bindings[0];
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 4;
			result = 0;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		client = binding->client;
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		value = (QmiByteValueTlv *)(client->request.data +
			sizeof(client->request.data) - sizeof(*value) - 1);
		value->header.type = 1;
		value->header.lengthLow = 1;
		value->header.lengthHigh = zero;
		client->request.length = zero;
		key = request->input[0].pointer;
		value->value = key->featureId;
		client = binding->client;
		if (client->service != 2) {
			result = 0x80330002;
			goto request_failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*value) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto request_failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 64;
		message->lengthLow = sizeof(*value);
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), value, sizeof(*value));
		sendClient = binding->client;
		client->request.length += sizeof(*value);
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		node = request->node;
		control = node->control;
		if (result < 0)
			goto error;
		request->state = 5;
		goto unlocked;
	case 5:
		key = request->input[0].pointer;
		result = qmiReadIndexedResponse(request, context->bindings[0].client,
			(SceUInt8)key->featureId);
		request->state = 6;
response_unlocked:
		bbcmdUnlockMutex(&request->node->control->mutex);
		if (result < 0)
			goto failed;
		goto check_completed;
	default:
		result = 0x80330102;
		goto error;
	}
first_request_failed:
	request->result = result;
request_failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	request->state = 6;
	goto completed;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 6)
		return result;
completed:
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 50, request->result,
			request->input, request->output, request->completionArg);
	control = request->node->control;
reset_node:
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

int bbcmdSendCat2Command(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	const QmiCat2CommandOptions *options;
	QmiCat2CommandPrefix *parameters;
	QmiHalfwordValueTlv *halfword;
	QmiByteValueTlv *value;
	QmiMessageHeader *message;
	BbcmdCompletion completion;
	unsigned int mask, bits, length, payloadLength;
	unsigned short zero;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 46)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		node = request->node;
		if (completion != NULL) {
			control = node->control;
			context = control->protocols[request->channel];
			binding = &context->bindings[8];
			bits = binding->requestBits & ~(1U << node->index);
			if ((bits >> 16) != 0)
				bits &= ~(1U << (node->index + 16));
			result = control->cancelMask;
			binding->requestBits = bits;
			if (result != 0) {
				result = 0;
			} else {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				node = request->node;
				control = node->control;
			}
		} else {
			result = 0;
			control = node->control;
		}
		goto finish_locked;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	context = control->protocols[request->channel];
	if (request->state == 1) {
		result = request->result;
		request->state = 3;
		result &= result >> 31;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed_unlocked;
		goto check_completed;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	binding = &context->bindings[8];
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	options = request->input[0].pointer;
	switch (options->operation) {
	case 1:
		payloadLength = options->hasOptionalValue == 1 ? 12 : 8;
		break;
	case 2:
		payloadLength = 8;
		break;
	case 3:
	case 4:
		payloadLength = 4;
		break;
	default:
		result = 0x80330002;
		goto failed;
	}
	parameters = (QmiCat2CommandPrefix *)(client->request.data +
		sizeof(client->request.data) - 1 - payloadLength);
	parameters->operation.header.type = 1;
	parameters->operation.header.lengthLow = 1;
	parameters->operation.header.lengthHigh = 0;
	options = request->input[0].pointer;
	parameters->operation.value = options->operation;
	options = request->input[0].pointer;
	if (options->operation == 2) {
		halfword = (QmiHalfwordValueTlv *)parameters->parameters;
		halfword->header.lengthLow = 2;
		halfword->header.lengthHigh = 0;
		halfword->header.type = 18;
		halfword->value[0] = options->forwardedHalfword;
		options = request->input[0].pointer;
		halfword->value[1] = options->forwardedHalfword >> 8;
		options = request->input[0].pointer;
	}
	if (options->operation == 1) {
		value = (QmiByteValueTlv *)parameters->parameters;
		value->header.lengthLow = 1;
		value->header.type = 16;
		value->header.lengthHigh = 0;
		value->value = options->forwardedByte;
		options = request->input[0].pointer;
		if (options->hasOptionalValue == 1) {
			value++;
			value->header.lengthLow = 1;
			value->header.lengthHigh = 0;
			value->header.type = 17;
			value->value = options->optionalValue;
		}
	}
	client = binding->client;
	if (client->service != 10) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->lengthLow = payloadLength;
	message->messageIdLow = 37;
	message->messageIdHigh = 0;
	message->lengthHigh = 0;
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameters, payloadLength);
	sendClient = binding->client;
	client->request.length += payloadLength;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	node = request->node;
	if (result < 0)
		goto failed_node;
	control = node->control;
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 3)
		return result;
	goto completed;
failed:
	node = request->node;
failed_node:
	control = node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
completed:
	request->state = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 46, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
finish_locked:
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

int bbcmdGetNetworkInfo(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int mask, bits, offset, responseLength, length;
	unsigned short zero;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 27)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[1];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		result = control->cancelMask;
		binding->requestBits = bits;
		if (result != 0) {
			result = 0;
		} else {
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
			completion = request->completion;
		}
		completion(request->channel, 27, request->result,
			request->input, request->output, request->completionArg);
		return result;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[1];
	if (request->state == 1) {
		result = request->result;
		if (result < 0) {
			request->state = 0;
			goto error;
		}
		client = binding->client;
		if (client == NULL || (unsigned int)client->response.length <= 10) {
			request->state = 0;
			result = 0x80330002;
			goto error;
		}
		responseLength = client->response.length;
		offset = 0;
		result = 0;
		while (offset + 11 <= (unsigned int)client->response.length) {
			const QmiResultResponse *response =
				(const QmiResultResponse *)(client->response.data + offset);
			unsigned int messageLength = response->message.lengthLow |
				(response->message.lengthHigh << 8);
			unsigned int messageId = response->message.messageIdLow |
				(response->message.messageIdHigh << 8);

			if ((response->result.resultLow | (response->result.resultHigh << 8)) == 0 &&
			    messageLength > 7) {
				unsigned int cursor = 0;

				result = 0;
				if (client->service != 3)
					goto next_message;
				if (messageId == 36 || messageId == 32) do {
					const QmiTlvHeader *header =
						(const QmiTlvHeader *)(response->parameters + cursor);
					const unsigned char *value = (const unsigned char *)(header + 1);
					unsigned int size = header->lengthLow | (header->lengthHigh << 8);
					unsigned int count, index;

					if (messageId == 36) {
						BbmcServingSystemInfo *serving = request->output[0].pointer;

						switch (header->type) {
						case 1:
							serving->registrationState = value[0];
							serving->csAttachState = value[1];
							serving->psAttachState = value[2];
							serving->selectedNetwork = value[3];
							serving->radioInterfaceCount = value[4];
							serving = request->output[0].pointer;
							if (serving->radioInterfaceCount >= 8)
								serving->radioInterfaceCount = 8;
							serving = request->output[0].pointer;
							count = serving->radioInterfaceCount;
							for (index = 0; index < count; ++index)
								serving->radioInterfaces[index] = value[5 + index];
							break;
						case 16:
							serving->roamingIndicator = value[0];
							serving->validFlags |= 1;
							break;
						case 17:
							serving->dataCapabilityCount = value[0];
							serving = request->output[0].pointer;
							if (serving->dataCapabilityCount >= 8)
								serving->dataCapabilityCount = 8;
							serving = request->output[0].pointer;
							count = serving->dataCapabilityCount;
							for (index = 0; index < count; ++index)
								serving->dataCapabilities[index] = value[1 + index];
							serving->validFlags |= 2;
							break;
						case 18:
							serving->mcc = value[0] | (value[1] << 8);
							serving->mnc = value[2] | (value[3] << 8);
							serving->descriptionLength = value[4];
							serving = request->output[0].pointer;
							if (serving->descriptionLength >= 68)
								serving->descriptionLength = 68;
							serving = request->output[0].pointer;
							memcpy(serving->description, value + 5, serving->descriptionLength);
							serving = request->output[0].pointer;
							serving->validFlags |= 4;
							break;
						case 19:
							serving->cdmaSystemId = value[0] | (value[1] << 8);
							serving->cdmaNetworkId = value[2] | (value[3] << 8);
							serving->validFlags |= 8;
							break;
						case 20:
							serving->baseStationId = value[0] | (value[1] << 8);
							serving->baseStationLatitude = value[2] | (value[3] << 8) |
								(value[4] << 16) | ((unsigned int)value[5] << 24);
							serving->baseStationLongitude = value[6] | (value[7] << 8) |
								(value[8] << 16) | ((unsigned int)value[9] << 24);
							serving->validFlags |= 16;
							break;
						case 21:
							serving->roamingIndicatorCount = value[0];
							serving = request->output[0].pointer;
							if (serving->roamingIndicatorCount >= 8)
								serving->roamingIndicatorCount = 8;
							serving = request->output[0].pointer;
							count = serving->roamingIndicatorCount;
							/* Sony advances one byte for each two-byte pair. */
							for (index = 0; index < count; ++index) {
								serving->roamingIndicators[index].radioInterface = value[1 + index];
								serving->roamingIndicators[index].roamingIndicator = value[2 + index];
							}
							break;
						case 22:
							serving->defaultRoamingIndicator = value[0];
							break;
						case 23:
							serving->leapSeconds = value[0];
							serving = request->output[0].pointer;
							serving->localTimeOffset = value[1];
							serving = request->output[0].pointer;
							serving->daylightSavingTime = value[2];
							serving->validFlags |= 32;
							break;
						case 24:
							serving->cdmaProtocolRevision = value[0];
							break;
						default:
							result = 0x80330002;
							break;
						}
					} else {
						BbmcSignalStrengthInfo *signal = request->output[1].pointer;

						switch (header->type) {
						case 1:
							signal->strength = value[0];
							signal = request->output[1].pointer;
							signal->radioInterface = value[1];
							break;
						case 16:
						case 17:
							count = value[0] | (value[1] << 8);
							if (count >= 4)
								count = 4;
							if (header->type == 16)
								signal->strengthCount = count;
							else
								signal->rssiCount = count;
							for (index = 0; index < count; ++index) {
								unsigned int strength = value[2 + index];
								BbmcSignalStrength *entry;

								signal = request->output[1].pointer;
								entry = header->type == 16 ? &signal->strengths[index] : &signal->rssi[index];
								if (header->type == 17 && strength - 1 > 124)
									strength = 125;
								entry->strength = strength;
								signal = request->output[1].pointer;
								entry = header->type == 16 ? &signal->strengths[index] : &signal->rssi[index];
								entry->radioInterface = value[3 + index];
							}
							signal->validMask |= header->type == 16 ? 32 : 1;
							break;
						case 18:
							count = value[0] | (value[1] << 8);
							if (count >= 4)
								count = 4;
							signal->ecioCount = count;
							for (index = 0; index < count; ++index) {
								signal->ecio[index].value = value[2 + index];
								signal = request->output[1].pointer;
								signal->ecio[index].radioInterface = value[3 + index];
							}
							/* The original advances this TLV twice. */
							cursor += size + sizeof(*header);
							signal->validMask |= 2;
							break;
						case 19:
							signal->validMask |= 4;
							signal->io = value[0] | (value[1] << 8) |
								(value[2] << 16) | ((unsigned int)value[3] << 24);
							break;
						case 20:
							signal->sinr = header->lengthHigh;
							signal = request->output[1].pointer;
							signal->validMask |= 8;
							break;
						case 21:
							count = value[0] | (value[1] << 8);
							if (count >= 4)
								count = 4;
							signal->errorRateCount = count;
							/* Original tests ECIO count and overwrites ECIO radios. */
							for (index = 0; index < signal->ecioCount && index < 4; ++index) {
								signal->errorRates[index].value = value[2 + index];
								signal = request->output[1].pointer;
								signal->ecio[index].radioInterface = value[4 + index];
							}
							signal->validMask |= 16;
							break;
						case 22:
							if (value[1] == 8)
								signal->rsrq = value[0];
							break;
						default:
							result = 0x80330002;
							break;
						}
					}
					cursor += size + sizeof(*header);
				} while (messageLength - 7 > cursor);
				if (messageId == 36) {
					BbmcServingSystemInfo *serving = request->output[0].pointer;

					if (serving->selectedNetwork == 1) {
						unsigned int mcc = serving->mcc + 111;
						unsigned int mnc;

						if (mcc % 10 == 0)
							mcc -= 10;
						if (mcc % 100 <= 9)
							mcc -= 100;
						if (mcc >= 1000)
							mcc %= 1000;
						mnc = serving->mnc + 11;
						serving->mcc = mcc;
						if (mnc % 10 == 0)
							mnc -= 10;
						if (mnc > 99)
							mnc %= 100;
						serving->mnc = mnc;
					}
				}
			}
next_message:
			offset += messageLength + sizeof(response->message);
			if (responseLength <= offset)
				break;
		}
		request->state = 0;
		control = request->node->control;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed_unlocked;
		goto completed;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	zero = bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (client->service != 3) {
		result = 0x80330002;
		goto failed;
	}
	{
		QmiMessageHeader *message = (QmiMessageHeader *)client->request.data;

		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 36;
		client->request.length = sizeof(*message);
	}
	if (request->input[0].halfwords[0] != 0) {
		QmiHalfwordValueTlv *parameter;
		QmiMessageHeader *message;

		client = binding->client;
		parameter = (QmiHalfwordValueTlv *)(client->request.data +
			sizeof(client->request.data) - 1 - sizeof(*parameter));
		parameter->header.lengthHigh = zero;
		parameter->header.type = 16;
		parameter->header.lengthLow = 2;
		__builtin_memcpy(parameter->value, &request->input[0].halfwords[0], 2);
		client = binding->client;
		if (client->service != 3) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*parameter) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 32;
		message->lengthLow = sizeof(*parameter);
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), parameter, sizeof(*parameter));
		client->request.length += sizeof(*parameter);
	} else {
		QmiMessageHeader *message;

		client = binding->client;
		if (client->service != 3) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = 0;
		message->lengthLow = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 32;
		client->request.length = length + sizeof(*message);
	}
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto error;
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 27, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdSendGpsData(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	QmiMessageHeader *message;
	unsigned char *payload;
	unsigned int bits, mask, size, messageId, index, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 23)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = control->cancelMask;
	bits = 1U << node->index;
	if (mask & bits) {
		control->cancelMask = mask & ~bits;
		request->state = 0;
		node->state = 2;
		completion = request->completion;
		if (completion != NULL) {
			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[4];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			mask = control->cancelMask;
			binding->requestBits = bits;
			if (mask == 0)
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[4];
	if (request->state > 1) {
		if (request->state != 3) {
			result = 0x80330102;
			goto error;
		}
		result = request->result;
		if (result < 0)
			goto error;
		result = 0;
		request->state = 0;
		goto unlocked;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	if ((unsigned short)bits != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 1;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = 0;
	switch (request->input[0].value) {
	case 0x53001000:
		messageId = 0x43;
		payload = NULL;
		size = 0;
		break;
	case 0x5b001000: {
		const GpsXtraData *input = request->input[2].pointer;
		QmiPdsXtraDataTlv *output;

		size = request->input[1].value;
		if (size > sizeof(*input)) {
			result = 0x80330002;
			goto error;
		}
		payload = client->request.data + sizeof(client->request.data) - size - 9;
		output = (QmiPdsXtraDataTlv *)payload;
		output->header.type = 1;
		length = input->sequenceLength + 5;
		output->header.lengthLow = length;
		output->header.lengthHigh = length >> 8;
		output->sequenceNumber = input->sequenceNumber;
		output->totalLength[0] = input->totalLength;
		output->totalLength[1] = input->totalLength >> 8;
		output->sequenceLength[0] = input->sequenceLength;
		output->sequenceLength[1] = input->sequenceLength >> 8;
		if ((unsigned short)(input->sequenceLength - 1) <= 1499)
			for (index = 0; index < input->sequenceLength; index++)
				output->data[index] = input->data[index];
		size += sizeof(*output);
		messageId = 0x37;
		break;
	}
	case 0x5b001001: {
		const GpsAbsoluteTime *input = request->input[2].pointer;
		QmiPdsAbsoluteTimeTlv *output;

		payload = client->request.data + sizeof(client->request.data) - 18;
		output = (QmiPdsAbsoluteTimeTlv *)payload;
		output->header.type = 1;
		output->header.lengthLow = 14;
		output->header.lengthHigh = 0;
		output->timestamp[0] = input->timestampMilliseconds;
		output->timestamp[1] = input->timestampMilliseconds >> 8;
		output->timestamp[2] = input->timestampMilliseconds >> 16;
		output->timestamp[3] = input->timestampMilliseconds >> 24;
		output->timestamp[4] = input->timestampMilliseconds >> 32;
		output->timestamp[5] = input->timestampMilliseconds >> 40;
		output->timestamp[6] = input->timestampMilliseconds >> 48;
		output->timestamp[7] = input->timestampMilliseconds >> 54;
		output->uncertainty[0] = input->timeUncertaintyMilliseconds;
		output->uncertainty[1] = input->timeUncertaintyMilliseconds >> 8;
		output->uncertainty[2] = input->timeUncertaintyMilliseconds >> 16;
		output->uncertainty[3] = input->timeUncertaintyMilliseconds >> 24;
		output->timeBase = input->timeBase;
		output->forceAcceptance = input->forceAcceptance;
		size = sizeof(*output);
		messageId = 0x3d;
		break;
	}
	case 0x5b001002: {
		const GpsWifiPosition *input = request->input[2].pointer;
		QmiPdsWifiAssistance *output;

		payload = client->request.data + sizeof(client->request.data) - 676;
		output = (QmiPdsWifiAssistance *)payload;
		output->position.header.type = 0x11;
		output->position.header.lengthLow = 12;
		output->position.header.lengthHigh = 0;
		output->position.latitude[0] = input->latitude;
		output->position.latitude[1] = (unsigned int)input->latitude >> 8;
		output->position.latitude[2] = (unsigned int)input->latitude >> 16;
		output->position.latitude[3] = (unsigned int)input->latitude >> 24;
		output->position.longitude[0] = input->longitude;
		output->position.longitude[1] = (unsigned int)input->longitude >> 8;
		output->position.longitude[2] = (unsigned int)input->longitude >> 16;
		output->position.longitude[3] = (unsigned int)input->longitude >> 24;
		output->time.value[0] = input->timeCounterMilliseconds;
		output->time.value[1] = input->timeCounterMilliseconds >> 8;
		output->time.value[2] = input->timeCounterMilliseconds >> 16;
		output->time.value[3] = input->timeCounterMilliseconds >> 24;
		output->position.hepe[0] = input->hepeMeters;
		output->position.hepe[1] = input->hepeMeters >> 8;
		output->position.apCount = input->accessPointCount;
		output->position.errorCode = input->errorCode;
		output->time.header.type = 0x10;
		output->time.header.lengthLow = 4;
		output->time.header.lengthHigh = 0;
		output->accessPointHeader.type = 0x12;
		size = sizeof(*output);
		length = 1;
		output->accessPointCount = 0;
		if ((unsigned char)(input->recordCount - 1) <= 49) {
			output->accessPointCount = input->recordCount;
			for (index = 0; index < input->recordCount; index++) {
				const GpsWifiAccessPoint *source = &input->accessPoints[index];
				QmiPdsWifiAccessPoint *destination = &output->accessPoints[index];
				unsigned int byte;

				for (byte = 0; byte < 6; byte++)
					destination->mac[byte] = source->mac[byte];
				for (byte = 0; byte < 4; byte++)
					destination->signalStrength[byte] = source->receivedSignalStrength >> (8 * byte);
				destination->beaconChannel[0] = source->beaconChannel;
				destination->beaconChannel[1] = source->beaconChannel >> 8;
				destination->flags = source->flags;
			}
			length += input->recordCount * sizeof(QmiPdsWifiAccessPoint);
			size += input->recordCount * sizeof(QmiPdsWifiAccessPoint);
		}
		output->accessPointHeader.lengthLow = length;
		output->accessPointHeader.lengthHigh = length >> 8;
		messageId = 0x39;
		break;
	}
	case 0x5b001003: {
		const GpsResetMasks *input = request->input[2].pointer;
		QmiWordValueTlv *output;

		payload = client->request.data + sizeof(client->request.data) - 15;
		size = 0;
		mask = input->gpsDataMask & 0x1fff;
		if (mask != 0) {
			output = (QmiWordValueTlv *)payload;
			output->header.type = 0x10;
			output->header.lengthLow = 4;
			output->header.lengthHigh = 0;
			for (index = 0; index < 4; index++)
				output->value[index] = mask >> (8 * index);
			size = sizeof(*output);
		}
		mask = input->cellDataMask & 0x3ff;
		if (mask != 0) {
			output = (QmiWordValueTlv *)(payload + size);
			output->header.type = 0x11;
			output->header.lengthLow = 4;
			output->header.lengthHigh = 0;
			for (index = 0; index < 4; index++)
				output->value[index] = mask >> (8 * index);
			size += sizeof(*output);
		}
		messageId = 0x34;
		break;
	}
	default:
		result = 0x80330001;
		goto error;
	}
	client = binding->client;
	if (client->service != 6) {
		result = 0x80330002;
		goto error;
	}
	length = client->request.length;
	if (length + sizeof(*message) + size > sizeof(client->request.data)) {
		result = 0x80330009;
		goto error;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdLow = messageId;
	message->messageIdHigh = messageId >> 8;
	message->lengthLow = size;
	message->lengthHigh = size >> 8;
	client->request.length += sizeof(*message);
	if (size != 0) {
		memmove(client->request.data + client->request.length, payload, size);
		client->request.length += size;
	}
	binding->client->callbackContext = request;
	result = qmiSendClientRequest(binding->client);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto error;
	request->state = 3;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	node = request->node;
	node->state = 2;
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 23, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdStartTracking(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *backend, *unlockBackend;
	BbmcControl *control;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int command, completionCommand, bits;
	int clearTracking = 0, cancelled = 0;
	int result = 0;

	if (request == NULL)
		return 0x80330002;
	command = request->command;
	if (command == 21) {
		if (request->channel > 1)
			return 0x80330002;
	} else if (command != 20 || request->channel > 1) {
		return 0x80330101;
	}
	node = request->node;
	backend = node->control;
	control = request->completionArg;
	completionCommand = command;
	bits = 1U << node->index;
	if (backend->cancelMask & bits) {
		backend->cancelMask &= ~bits;
		request->state = 5;
		completionCommand = 0;
		clearTracking = 1;
		cancelled = 1;
		goto completed;
	}
	if ((control->flags & 4) != 0 && request->state != 3) {
		request->state = 5;
		completionCommand = 0;
		clearTracking = 1;
		cancelled = 1;
		goto completed;
	}
	bbcmdLockMutex(&backend->mutex);
	backend = request->node->control;
	context = backend->protocols[request->channel];
	binding = &context->bindings[4];
	switch (request->state) {
	case 0:
	case 1:
	case 2: {
		unsigned int mode;

		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto errorLocked;
		}
		bits = binding->requestBits;
		if ((SceUInt16)bits != 0) {
			binding->requestBits = bits | (1U << (request->node->index + 16));
			request->state = 1;
			unlockBackend = backend;
			goto unlockCompleted;
		}
		binding->requestBits = bits | (1U << request->node->index);
		client->request.length = 0;
		memset(client->request.data, 0, sizeof(client->request.data));
		mode = control->trackingStartMode;
		if (mode != 0 && request->command != 20) {
			QmiWordValueTlv *mask;
			QmiMessageHeader *message;
			unsigned int size, length;

			if (request->command != 21 || mode - 1 > 1) {
				result = 0x80330002;
				goto errorLocked;
			}
			client = binding->client;
			mask = (QmiWordValueTlv *)(client->request.data +
				sizeof(client->request.data) - 1 - 2 * sizeof(*mask));
			mask[0].header.type = 16;
			mask[0].header.lengthLow = 4;
			mask[0].header.lengthHigh = 0;
			mask[0].value[2] = 0;
			mask[0].value[3] = 0;
			if (mode == 1) {
				mask[0].value[0] = 4;
				mask[0].value[1] = 0;
				size = sizeof(*mask);
			} else {
				mask[0].value[0] = 255;
				mask[0].value[1] = 31;
				mask[1].header.type = 17;
				mask[1].header.lengthLow = 4;
				mask[1].header.lengthHigh = 0;
				mask[1].value[0] = 255;
				mask[1].value[1] = 3;
				mask[1].value[2] = 0;
				mask[1].value[3] = 0;
				size = 2 * sizeof(*mask);
			}
			client = binding->client;
			if (client->service != 6) {
				result = 0x80330002;
				goto errorLocked;
			}
			length = client->request.length;
			if (length + sizeof(*message) + size > sizeof(client->request.data)) {
				result = 0x80330009;
				goto errorLocked;
			}
			message = (QmiMessageHeader *)(client->request.data + length);
			message->messageIdLow = 52;
			message->messageIdHigh = 0;
			message->lengthLow = size;
			message->lengthHigh = 0;
			client->request.length = length + sizeof(*message);
			memmove(message + 1, mask, size);
			client->request.length += size;
			request->state = 4;
		} else {
			if (request->command - 20 > 1) {
				result = 0x80330002;
				goto errorLocked;
			}
			result = _constructTrackingStartQmi(&binding->client,
				request->input, request->command);
			if (result < 0)
				goto errorLocked;
			request->state = 3;
		}
		goto send;
	}
	case 3: {
		QmiTrackingMessageResult startResults[4], oneShotResults[3];
		QmiTrackingMessageResult *results;
		const QmiMessageHeader *message;
		unsigned int count, totalSize, remaining, messageId, messageSize, index;
		unsigned int responseCommand;

		client = binding->client;
		__builtin_memcpy(startResults, g_trackingStartMessageResults, sizeof(startResults));
		responseCommand = request->command;
		__builtin_memcpy(oneShotResults, g_trackingOneShotMessageResults, sizeof(oneShotResults));
		if (responseCommand - 20 > 1) {
			result = 0x80330101;
			goto errorLocked;
		}
		totalSize = client->response.length;
		if (totalSize <= 10) {
			result = 0x80330002;
			goto errorLocked;
		}
		if (responseCommand == 21) {
			results = startResults;
			count = 4;
		} else {
			results = oneShotResults;
			count = 3;
		}
		remaining = totalSize - sizeof(*message);
		message = (const QmiMessageHeader *)client->response.data;
		messageId = message->messageIdLow | (message->messageIdHigh << 8);
		messageSize = message->lengthLow | (message->lengthHigh << 8);
		if (messageSize == 0)
			goto invalidResponse;
		for (;;) {
			for (index = 0; index < count; ++index) {
				if (results[index].messageId == messageId)
					break;
			}
			if (index < count) {
				const QmiTlvHeader *parameter = (const QmiTlvHeader *)(message + 1);
				unsigned int parameterRemaining, length;

				if (messageSize <= 2)
					goto invalidResponse;
				parameterRemaining = messageSize - sizeof(*parameter);
				length = parameter->lengthLow | (parameter->lengthHigh << 8);
				if (length == 0)
					goto invalidResponse;
				for (;;) {
					if (parameter->type == 2) {
						const QmiResultTlv *status = (const QmiResultTlv *)parameter;
						SceUInt16 value = status->resultLow | (status->resultHigh << 8);

						if ((SceInt16)value != 0)
							value = status->errorLow | (status->errorHigh << 8);
						results[index].result = value;
					}
					parameterRemaining -= length;
					parameter = (const QmiTlvHeader *)
						((const unsigned char *)(parameter + 1) + length);
					if (parameterRemaining > messageSize || parameterRemaining == 0)
						break;
					parameterRemaining -= sizeof(*parameter);
					length = parameter->lengthLow | (parameter->lengthHigh << 8);
					if (length == 0)
						goto invalidResponse;
				}
			}
			remaining -= messageSize;
			message = (const QmiMessageHeader *)((const unsigned char *)(message + 1) + messageSize);
			if (remaining == 0 || remaining > totalSize)
				break;
			if (remaining <= 3)
				goto invalidResponse;
			remaining -= sizeof(*message);
			messageId = message->messageIdLow | (message->messageIdHigh << 8);
			messageSize = message->lengthLow | (message->lengthHigh << 8);
			if (messageSize == 0)
				goto invalidResponse;
		}
		result = results[0].result;
		if (result == 0 || result == 26) {
			for (index = 1; index < count; ++index) {
				result = results[index].result;
				if (result != 0)
					break;
			}
			if (index == count) {
				if (responseCommand == 21) {
					if (g_trackingShortTimer != 0)
						bbmcTimerCancelHandler(g_trackingShortTimer);
					g_trackingShortTimer = 0;
					g_trackingShortPending = 0;
					if (g_trackingLongTimer != 0)
						bbmcTimerCancelHandler(g_trackingLongTimer);
					g_trackingSampleCount = 0;
					g_trackingEnabled = 1;
					g_trackingSatelliteAverage = 0;
					g_trackingSignalAverage = 0;
					g_trackingLongTimer = 0;
					g_trackingLongPending = 0;
					result = bbmcTimerSetHandler(180000000,
						(void (*)(void *))bbcmdStoreStateOneDuplicate, &g_trackingShortPending);
					if (result < 0)
						goto errorLocked;
					g_trackingShortTimer = result;
					result = bbmcTimerSetHandler(1800000000,
						(void (*)(void *))bbcmdStoreStateOne, &g_trackingLongPending);
					if (result < 0)
						goto errorLocked;
					g_trackingLongTimer = result;
					request->state = 5;
					unlockBackend = request->node->control;
				} else {
					unlockBackend = backend;
				}
				goto unlockCompleted;
			}
		}
		result |= 0x80335000;
		if (responseCommand != 21 || result != (int)0x80335003)
			goto errorLocked;
		if (g_trackingStartRetryCount++ > 3) {
			result = 0x80390090;
			goto errorLocked;
		}
		request->state = 2;
		client->callbackContext = request;
		request->retryTimer = bbmcTimerSetHandler(1000000, bbcmdClientRetryTimer, client);
		result = 0;
		unlockBackend = request->node->control;
		goto unlockCompleted;
invalidResponse:
		result = 0x80330201;
		goto errorLocked;
	}
	case 4:
		client = binding->client;
		binding->requestBits |= 1U << request->node->index;
		client->request.length = 0;
		memset(client->request.data, 0, sizeof(client->request.data));
		result = request->result;
		if (result >= 0) {
			request->result = 0;
			control->trackingStartMode = 0;
			result = _constructTrackingStartQmi(&binding->client, request->input, request->command);
			if (result < 0)
				goto errorLocked;
			request->state = 3;
send:
			client = binding->client;
			client->callbackContext = request;
			result = qmiSendClientRequest(client);
			bbcmdUnlockMutex(&request->node->control->mutex);
			if (result < 0)
				goto error;
			goto checkCompleted;
		}
		if (result != (int)0x80335003)
			goto errorLocked;
		if (g_trackingStopRetryCount++ > 16) {
			result = 0x80390090;
			goto errorLocked;
		}
		request->state = 2;
		client = binding->client;
		client->callbackContext = request;
		request->retryTimer = bbmcTimerSetHandler(1000000, bbcmdClientRetryTimer, client);
		request->result = 0;
		result = 0;
		unlockBackend = request->node->control;
		goto unlockCompleted;
	default:
		result = 0x80330102;
		goto errorLocked;
	}
unlockCompleted:
	bbcmdUnlockMutex(&unlockBackend->mutex);
	goto checkCompleted;
errorLocked:
	bbcmdUnlockMutex(&request->node->control->mutex);
error:
	if (g_trackingShortTimer != 0)
		bbmcTimerCancelHandler(g_trackingShortTimer);
	g_trackingShortTimer = 0;
	g_trackingShortPending = 0;
	if (g_trackingLongTimer != 0)
		bbmcTimerCancelHandler(g_trackingLongTimer);
	request->result = result;
	g_trackingSatelliteAverage = 0;
	g_trackingSignalAverage = 0;
	g_trackingLongTimer = 0;
	g_trackingLongPending = 0;
	g_trackingSampleCount = 0;
	g_trackingEnabled = 0;
	request->state = 5;
	clearTracking = 1;
completed:
	g_trackingStopRetryCount = 0;
	g_trackingStartRetryCount = 0;
	bbmcTimerCancelHandler(request->retryTimer);
	request->retryTimer = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	node->state = 2;
	request->state = 0;
	if (clearTracking != 0) {
		binding = &node->control->protocols[request->channel]->bindings[4];
		bits = binding->requestBits & ~(1U << node->index);
		if ((bits >> 16) != 0)
			bits &= ~(1U << (node->index + 16));
		binding->requestBits = bits;
		bbmcEnterCS(&control->mutex);
		if (control->trackingState == 2 || control->trackingState == 4)
			control->trackingState = 0;
		bbmcLeaveCS(&control->mutex);
		node = request->node;
	}
	bbcmdUnlockMutex(&node->control->mutex);
	completion = request->completion;
	if (completion != NULL && (!cancelled || request->node->control->cancelMask == 0))
		completion(request->channel, completionCommand, request->result,
			request->input, request->output, request->completionArg);
	return result;
checkCompleted:
	if (request->state == 5) {
		clearTracking = completionCommand == 0;
		cancelled = completionCommand == 0;
		goto completed;
	}
	return result;
}

int bbcmdReadSimRefreshDetails(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *context;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	QmiUimSessionTlv *session;
	QmiMessageHeader *message;
	BbmcFileRefreshDetail *detail;
	unsigned int bit, bits, cancelMask, zero, length;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 44)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if (cancelMask & bit) {
		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		node = request->node;
		result = 0;
		if (completion != NULL) {
			control = node->control;
			context = control->protocols[request->channel];
			bit = 1U << node->index;
			bits = context->bindings[7].requestBits & ~bit;
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			context->bindings[7].requestBits = bits;
			bits = context->bindings[1].requestBits & ~bit;
			zero = bits >> 16;
			if (zero != 0)
				zero = 1U << (node->index + 16);
			result = control->cancelMask;
			if (bits >> 16)
				bits &= ~zero;
			context->bindings[1].requestBits = bits;
			if (result != 0) {
				result = 0;
			} else {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				node = request->node;
				control = node->control;
			}
		} else {
			control = node->control;
		}
		goto reset_node;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	context = control->protocols[request->channel];
	switch (request->state) {
	case 0:
	case 2:
		binding = &context->bindings[7];
		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto error;
		}
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 2;
			result = 0;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		session = (QmiUimSessionTlv *)(client->request.data +
			sizeof(client->request.data) - sizeof(*session) - 1);
		session->header.type = 1;
		session->header.lengthHigh = zero;
		session->sessionType = zero;
		session->aidLength = zero;
		session->header.lengthLow = 2;
		sendClient = binding->client;
		client->request.length = zero;
		client = sendClient;
		if (client->service != 11) {
			result = 0x80330002;
			goto request_failed;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*session) >
		    sizeof(client->request.data)) {
			result = 0x80330009;
			goto request_failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->lengthLow = sizeof(*session);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 45;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), session, sizeof(*session));
		sendClient = binding->client;
		client->request.length += sizeof(*session);
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		control = request->node->control;
		if (result < 0)
			goto error;
		request->state = 1;
		goto unlocked;
	case 1: {
		const QmiResultResponse *response;
		unsigned int remaining, offset;

		result = request->result;
		if (result < 0)
			goto error;
		client = context->bindings[7].client;
		if (client->response.length <= 10) {
			result = 0x80330002;
			goto error;
		}
		response = (const QmiResultResponse *)client->response.data;
		result = response->result.resultLow | (response->result.resultHigh << 8);
		if (result != 0)
			goto scan_files;
		remaining = response->message.lengthLow | (response->message.lengthHigh << 8);
		if (remaining <= sizeof(response->result))
			goto scan_files;
		remaining -= sizeof(response->result);
		if ((response->message.messageIdLow | (response->message.messageIdHigh << 8)) != 45 ||
		    client->service != 11) {
			result = 0x80330002;
			goto error;
		}
		offset = 0;
		do {
			const QmiUimRefreshEventTlv *event =
				(const QmiUimRefreshEventTlv *)(response->parameters + offset);
			unsigned int size = event->header.lengthLow | (event->header.lengthHigh << 8);

			if (event->header.type == 16) {
				const QmiUimRefreshEventFiles *files;
				const QmiUimRefreshFile *file;
				unsigned int aidLength, fileCount, fileIndex, selection;
				unsigned char *ownerBytes, *fileBytes;
				SceUInt16 firstPath, secondPath, fileId;
				SceUInt32 fileKind;

				detail = request->output[0].pointer;
				detail->stage = event->stage;
				detail->mode = event->mode;
				detail->sessionType = event->sessionType;
				aidLength = event->aidLength;
				detail->aidLength = aidLength > sizeof(detail->aid) ? 17 : aidLength;
				/* The sole caller supplies the complete manager's refresh member. */
				ownerBytes = (unsigned char *)detail - offsetof(BbmcControl, refreshDetail);
				if (aidLength != 0)
					memcpy(ownerBytes + offsetof(BbmcControl, refreshDetail.aid),
						event->aid, aidLength);
				files = (const QmiUimRefreshEventFiles *)(event->aid + aidLength);
				fileCount = files->fileCount[0] | (files->fileCount[1] << 8);
				detail->fileCount = fileCount > 14 ? 15 : fileCount;
				if (detail->fileCount == 0) {
					result = 0x80330002;
					goto next_event;
				}
				file = (const QmiUimRefreshFile *)files->files;
				fileIndex = 0;
				do {
					BbmcRefreshedFile *stored = &detail->files[fileIndex];
					unsigned int pathLength;

					stored->fileId = file->fileId;
					pathLength = file->pathLength;
					stored->pathLength = pathLength < sizeof(stored->path) ?
						pathLength : sizeof(stored->path);
					memcpy(stored->path, file->path, stored->pathLength);
					file = (const QmiUimRefreshFile *)((const unsigned char *)(file + 1) +
						pathLength);
					++fileIndex;
				} while (fileIndex < detail->fileCount);
				fileCount = detail->fileCount;
				if (fileCount == 0) {
					result = 0x80330002;
					goto next_event;
				}
				/* Sony searches and writes kinds at a whole-record stride of 216. */
				fileIndex = 0;
				do {
					fileBytes = ownerBytes + offsetof(BbmcControl, refreshDetail.files) +
						fileIndex * sizeof(*detail);
					__builtin_memcpy(&firstPath,
						fileBytes + offsetof(BbmcRefreshedFile, path[0]), sizeof(firstPath));
					fileKind = 0;
					__builtin_memcpy(fileBytes + offsetof(BbmcRefreshedFile, fileKind),
						&fileKind, sizeof(fileKind));
					for (selection = 0; selection < 21; ++selection) {
						const QmiUimFileSelection *configured = &g_qmiUimFileSelections[selection];

						if (configured->path[0] != firstPath)
							continue;
						__builtin_memcpy(&secondPath,
							fileBytes + offsetof(BbmcRefreshedFile, path[1]), sizeof(secondPath));
						if (configured->path[1] != secondPath)
							continue;
						__builtin_memcpy(&fileId,
							fileBytes + offsetof(BbmcRefreshedFile, fileId), sizeof(fileId));
						if (configured->fileId != fileId)
							continue;
						fileKind = configured->fileKind;
						__builtin_memcpy(fileBytes + offsetof(BbmcRefreshedFile, fileKind),
							&fileKind, sizeof(fileKind));
						break;
					}
					++fileIndex;
				} while (fileIndex != fileCount);
				result = 0;
			}
next_event:
			offset += size + sizeof(QmiTlvHeader);
		} while (offset < remaining);
		if (result != 0) {
			control = request->node->control;
			goto error;
		}
		node = request->node;
		control = node->control;
scan_files:
		detail = request->output[0].pointer;
		request->state = 5;
		if (detail == NULL || detail->fileCount == 0) {
			result = 0;
			goto unlocked;
		}
		{
			unsigned char *ownerBytes = (unsigned char *)detail -
				offsetof(BbmcControl, refreshDetail);
			unsigned int fileIndex = 0, fileCount = detail->fileCount;

			do {
				SceUInt32 fileKind;
				const unsigned char *fileBytes = ownerBytes +
					offsetof(BbmcControl, refreshDetail.files) + fileIndex * sizeof(*detail);

				__builtin_memcpy(&fileKind, fileBytes + offsetof(BbmcRefreshedFile, fileKind),
					sizeof(fileKind));
				if (fileKind == 17) {
					request->state = 3;
					result = sceKernelSetEventFlag(control->event, 1U << node->index);
					control = request->node->control;
					goto response_unlocked;
				}
				++fileIndex;
			} while (fileIndex < fileCount);
		}
		result = 0;
		goto unlocked;
	}
	case 3:
		binding = &context->bindings[1];
		bits = binding->requestBits;
		zero = (unsigned short)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			result = 0;
			goto unlocked;
		}
		binding->requestBits = bits | (1U << node->index);
		client = binding->client;
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = zero;
		if (client->service != 3) {
			result = 0x80330002;
			client->callbackContext = request;
			control = request->node->control;
			goto error;
		}
		message = (QmiMessageHeader *)client->request.data;
		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 59;
		sendClient = binding->client;
		client->request.length = sizeof(*message);
		sendClient->callbackContext = request;
		result = qmiSendClientRequest(sendClient);
		control = request->node->control;
		if (result < 0)
			goto error;
		request->state = 4;
		goto unlocked;
	case 4:
		result = request->result;
		if (result >= 0) {
			unsigned int remaining;
			const QmiMessageHeader *reply;

			client = context->bindings[1].client;
			if (client == NULL || (unsigned int)client->response.length <= 6) {
				result = 0x80330002;
				goto response_done;
			}
			remaining = client->response.length;
			reply = (const QmiMessageHeader *)client->response.data;
			do {
				unsigned int messageId = reply->messageIdLow | (reply->messageIdHigh << 8);
				unsigned int messageLength = reply->lengthLow | (reply->lengthHigh << 8);
				const QmiTlvHeader *tlv = (const QmiTlvHeader *)(reply + 1);

				if (messageLength > 7 && messageId == 59 && client->service == 3) {
					unsigned int bytesLeft = messageLength;

					do {
						unsigned int size = tlv->lengthLow | (tlv->lengthHigh << 8);

						if (tlv->type == 16)
							*(SceUInt32 *)request->output[1].pointer =
								((const QmiByteValueTlv *)tlv)->value;
						tlv = (const QmiTlvHeader *)((const unsigned char *)(tlv + 1) + size);
						bytesLeft -= size + sizeof(*tlv);
					} while (bytesLeft != 0);
				}
				reply = (const QmiMessageHeader *)((const unsigned char *)(reply + 1) + messageLength);
				remaining -= messageLength + sizeof(*reply);
			} while (remaining != 0);
			result = remaining;
		}
response_done:
		request->state = 5;
response_unlocked:
		bbcmdUnlockMutex(&control->mutex);
		if (result < 0)
			goto failed;
		goto check_completed;
	default:
		result = 0;
		request->result = 0x80330102;
		goto unlocked;
	}
request_failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed:
	request->result = result;
	goto completed;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 5)
		return result;
completed:
	request->state = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 44, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
reset_node:
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

int bbcmdReadProactiveCommand(BbcmdRequest *request)
{
	BbcmdControl *control;
	BbcmdRequestNode *node;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	BbmcProactiveCmd *output;
	const QmiResultResponse *response;
	const unsigned char *parameters;
	unsigned int mask, bits, length, offset, responseLength, messageId;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 42)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		node = request->node;
		control = node->control;
		result = 0;
		if (completion != NULL) {
			binding = &control->protocols[request->channel]->bindings[8];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			binding->requestBits = bits;
			if (control->cancelMask == 0) {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				node = request->node;
				control = node->control;
			}
		}
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		return result;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[8];
	if (request->state == 1) {
		result = request->result;
		if (result < 0) {
			request->state = 3;
			goto error;
		}
		client = binding->client;
		if ((unsigned int)client->response.length <= 10) {
			request->state = 3;
			result = 0x80330002;
			goto error;
		}
		response = (const QmiResultResponse *)client->response.data;
		length = response->message.lengthLow | (response->message.lengthHigh << 8);
		if ((response->result.resultLow | (response->result.resultHigh << 8)) != 0 ||
		    length <= 7) {
			result = 0;
			goto parsed;
		}
		messageId = response->message.messageIdLow | (response->message.messageIdHigh << 8);
		responseLength = length - 7;
		parameters = response->parameters;
		if (request->input[1].value == 1) {
			const QmiCat2ProactiveCommandTlv *command;

			if (messageId != 35) {
				result = 0x80330002;
				goto parsed;
			}
			offset = 0;
			for (;;) {
				const QmiTlvHeader *header = (const QmiTlvHeader *)(parameters + offset);

				if (header->type == 27) {
					command = (const QmiCat2ProactiveCommandTlv *)header;
					break;
				}
				offset += sizeof(*header) + header->lengthLow + (header->lengthHigh << 8);
				if (offset >= responseLength) {
					result = 0x80330002;
					goto parsed;
				}
			}
			output = request->output[0].pointer;
			output->command = command->command;
			output = request->output[0].pointer;
			output->reference = command->reference[0] | (command->reference[1] << 8) |
				(command->reference[2] << 16) | (command->reference[3] << 24);
			output = request->output[0].pointer;
			output->qualifier = command->qualifier;
			if (client->service != 10) {
				result = 0x80330002;
				goto parsed;
			}
		} else {
			if (client->service != 10) {
				result = 0x80330002;
				goto parsed;
			}
			if (messageId != 35) {
				result = 0;
				goto parsed;
			}
		}
		offset = 0;
		do {
			const QmiTlvHeader *header = (const QmiTlvHeader *)(parameters + offset);
			const unsigned char *value = (const unsigned char *)(header + 1);
			BbmcProactiveText *text = NULL;
			BbmcProactiveIcon *icon = NULL;
			unsigned int size = header->lengthLow | (header->lengthHigh << 8);
			unsigned int count, index, inputOffset;

			output = request->output[0].pointer;
			switch (header->type) {
			case 28:
				switch (output->command) {
				case 1: case 2: case 3: case 13:
					text = &output->body.text.text;
					break;
				case 6:
					text = &output->body.selectItem.text;
					break;
				}
				if (text != NULL) {
					const QmiCat2TextPrefix *wire = (const QmiCat2TextPrefix *)header;

					text->encoding = wire->encoding;
					length = wire->length;
					if (length >= 241)
						length = 241;
					text->length = length;
					memcpy(text->text, wire->text, length);
					text->text[240] = 0;
					output = request->output[0].pointer;
					if (output->command == 6)
						output->validFields |= 0x400000;
				}
				break;
			case 29:
				output->body.text.textOption = value[0];
				break;
			case 30: case 35:
				output->body.text.responseOption = value[0];
				break;
			case 31:
				switch (output->command) {
				case 1: case 2: icon = &output->body.text.icon; break;
				case 3: icon = &output->body.input.icon; break;
				case 6: icon = &output->body.selectItem.icon; break;
				case 7: icon = &output->body.sendSms.icon; break;
				case 12: icon = &output->body.setupMenu.icon; break;
				case 13: icon = &output->body.idleText.icon; break;
				}
				if (icon != NULL) {
					const QmiCat2IconPrefix *wire = (const QmiCat2IconPrefix *)header;

					icon->qualifier = wire->qualifier;
					icon->height = wire->height;
					icon->width = wire->width;
					icon->codingScheme = wire->codingScheme;
					icon->recordNumber = wire->recordNumber;
					length = wire->length[0] | (wire->length[1] << 8);
					if (length >= 255)
						length = 255;
					icon->length = length;
					memcpy(icon->data, wire->data, length);
					output = request->output[0].pointer;
					output->validFields |= 1;
				}
				break;
			case 32:
				if (output->command == 1 || output->command == 2) {
					output->body.text.duration.unit = value[0];
					output->body.text.duration.interval = value[1];
					output = request->output[0].pointer;
					output->validFields |= 4;
				}
				break;
			case 33:
				if (output->command == 2 || output->command == 3)
					output->body.input.responseFormat = value[0];
				break;
			case 34:
				switch (output->command) {
				case 2: output->body.text.responseOption = value[0]; break;
				case 3: output->body.input.inputOption = value[0]; break;
				case 6: case 12: output->body.selectItem.selectionOption = value[0]; break;
				}
				break;
			case 36:
				if (output->command == 3) {
					output->body.input.minimumLength = value[0];
					output->body.input.maximumLength = value[1];
				}
				break;
			case 37:
				output->body.input.inputPresentation = value[0];
				break;
			case 39:
				if (output->command == 6 || output->command == 12)
					output->body.selectItem.softKeyOption = value[0];
				break;
			case 40:
				if (output->command == 6 || output->command == 12) {
					BbmcProactiveItems *items = &output->body.selectItem.items;
					const QmiCat2CountedPrefix *wire = (const QmiCat2CountedPrefix *)header;

					count = wire->count;
					if (count >= 30)
						count = 30;
					items->count = count;
					inputOffset = 0;
					for (index = 0; index < items->count; index++) {
						const QmiCat2ItemPrefix *item = (const QmiCat2ItemPrefix *)(wire->entries + inputOffset);

						items->items[index].identifier = item->identifier;
						length = item->length;
						if (length >= 254)
							length = 254;
						items->items[index].length = length;
						memcpy(items->items[index].text, item->text, length);
						inputOffset += items->items[index].length + sizeof(*item);
					}
				}
				break;
			case 41:
				if (output->command == 6)
					output->body.selectItem.defaultItem = value[0];
				else if (output->command == 12)
					output->body.setupMenu.defaultItem = value[0];
				output = request->output[0].pointer;
				output->validFields |= 0x40;
				break;
			case 42:
				if (output->command == 6 || output->command == 12) {
					BbmcProactiveNextActions *actions = output->command == 6 ?
						&output->body.selectItem.nextActions : &output->body.setupMenu.nextActions;
					const QmiCat2CountedPrefix *wire = (const QmiCat2CountedPrefix *)header;

					count = wire->count;
					if (count >= 8)
						count = 8;
					actions->count = count;
					for (index = 0; index < actions->count; index++)
						actions->actions[index] = wire->entries[index];
				}
				output = request->output[0].pointer;
				output->validFields |= 0x80;
				break;
			case 43:
				if (output->command == 6 || output->command == 12) {
					BbmcProactiveIcons *icons = output->command == 6 ?
						&output->body.selectItem.itemIcons : &output->body.setupMenu.itemIcons;
					const QmiCat2IconListPrefix *wire = (const QmiCat2IconListPrefix *)header;
					unsigned char *iconBytes = (unsigned char *)output +
						(output->command == 6 ? offsetof(BbmcProactiveCmd, body.selectItem.itemIcons.icons) :
						 offsetof(BbmcProactiveCmd, body.setupMenu.itemIcons.icons));

					icons->qualifier = wire->qualifier;
					count = wire->count;
					if (count >= 254)
						count = 254;
					icons->count = count;
					inputOffset = 0;
					/* Original count is unchecked against the nominal thirty slots. */
					for (index = 0; index < icons->count; index++) {
						const QmiCat2IconEntryPrefix *entry = (const QmiCat2IconEntryPrefix *)(wire->entries + inputOffset);
						SceUInt32 qualifier = entry->qualifier;
						SceUInt32 codingScheme;
						SceUInt16 copiedLength;

#define STORE_PROACTIVE_ICON(member, source) \
	__builtin_memcpy(iconBytes + offsetof(BbmcProactiveIcon, member), &(source), sizeof(source))
						STORE_PROACTIVE_ICON(qualifier, qualifier);
						STORE_PROACTIVE_ICON(height, entry->height);
						STORE_PROACTIVE_ICON(width, entry->width);
						codingScheme = entry->codingScheme;
						STORE_PROACTIVE_ICON(codingScheme, codingScheme);
						STORE_PROACTIVE_ICON(recordNumber, entry->recordNumber);
						length = entry->length[0] | (entry->length[1] << 8);
						copiedLength = length >= 255 ? 255 : length;
						STORE_PROACTIVE_ICON(length, copiedLength);
#undef STORE_PROACTIVE_ICON
						inputOffset += length + sizeof(*entry);
						iconBytes = (unsigned char *)memcpy(iconBytes + offsetof(BbmcProactiveIcon, data),
							entry->data, copiedLength) - offsetof(BbmcProactiveIcon, data) + sizeof(BbmcProactiveIcon);
					}
					output = request->output[0].pointer;
					output->validFields |= 0x100;
				}
				break;
			case 44:
				output->body.selectItem.presentationOption = value[0];
				break;
			case 45: case 54:
				output->body.scalar.option = value[0];
				break;
			case 46:
				if (output->command == 7) {
					output->body.sendSms.address.length = value[0];
					memcpy(output->body.sendSms.address.data, value + 1, value[0]);
				}
				break;
			case 47:
				output->body.selectItem.softKeyOption = value[0];
				break;
			case 48:
				if (output->command == 7) {
					const QmiCat2SmsPayloadPrefix *wire = (const QmiCat2SmsPayloadPrefix *)header;

					output->body.sendSms.payload.format = wire->format;
					output->body.sendSms.payload.packing = wire->packing;
					length = wire->length;
					if (length >= 200)
						length = 200;
					output->body.sendSms.payload.length = length;
					memcpy(output->body.sendSms.payload.data, wire->data, length);
					output = request->output[0].pointer;
					output->validFields |= 0x200;
				}
				break;
			case 55:
				output->body.scalar.value = value[0] | (value[1] << 8);
				break;
			case 62:
				output->body.text.defaultText.encoding = value[0];
				output = request->output[0].pointer;
				output->body.text.defaultText.length = value[1];
				output = request->output[0].pointer;
				if (output->body.text.defaultText.length >= 241)
					output->body.text.defaultText.length = 241;
				output = request->output[0].pointer;
				memcpy(output->body.text.defaultText.text, value + 2,
					output->body.text.defaultText.length);
				output = request->output[0].pointer;
				output->body.text.defaultText.text[240] = 0;
				output = request->output[0].pointer;
				output->validFields |= 8;
				break;
			case 63:
				output->body.text.textResponseOption = value[0];
				output = request->output[0].pointer;
				output->validFields |= 2;
				break;
			case 69:
				if (output->command == 7)
					text = &output->body.sendSms.text;
				else if (output->command == 12)
					text = &output->body.setupMenu.title;
				else if (output->command == 6)
					text = &output->body.selectItem.title;
				if (text != NULL) {
					const QmiCat2TextPrefix *wire = (const QmiCat2TextPrefix *)header;

					text->encoding = wire->encoding;
					length = wire->length;
					if (length >= 241)
						length = 241;
					text->length = length;
					memcpy(text->text, wire->text, length);
					text->text[240] = 0;
					output = request->output[0].pointer;
					output->validFields |= 0x10;
				}
				break;
			}
			offset += size + sizeof(*header);
		} while (responseLength > offset);
		result = 0;
parsed:
		control = request->node->control;
		request->state = 3;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			request->result = result;
		goto complete;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto error;
	}
	bits = binding->requestBits;
	if ((unsigned short)bits != 0) {
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		result = 0;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = 0;
	result = request->input[1].value;
	if (result != 0) {
		QmiCat2ProactiveRequest *wire = (QmiCat2ProactiveRequest *)(client->request.data +
			sizeof(client->request.data) - 1 - sizeof(*wire));
		QmiMessageHeader *message;

		wire->header.type = 1;
		wire->header.lengthHigh = 0;
		wire->header.lengthLow = 5;
		wire->reference[0] = request->input[0].value;
		wire->reference[1] = request->input[0].value >> 8;
		wire->reference[2] = request->input[0].halfwords[1];
		wire->command = 2;
		wire->reference[3] = request->input[0].bytes[3];
		client = binding->client;
		if (client->service != 10) {
			result = 0x80330002;
			goto error;
		}
		length = client->request.length;
		if (length + sizeof(*message) + sizeof(*wire) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto error;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->lengthLow = sizeof(*wire);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 35;
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), wire, sizeof(*wire));
		client->request.length += sizeof(*wire);
		client = binding->client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		control = request->node->control;
		if (result < 0)
			goto error;
		request->state = 1;
	}
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 3)
		return result;
	goto complete;
error:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
complete:
	request->state = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 42, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

int bbcmdReleaseClients(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *context, *current;
	BbcmdClientBinding *binding;
	BbcmdClientBinding snapshot;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int service, bits;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 10)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	context = control->protocols[request->channel];

#define RELEASE_CLIENT_CASE(stateValue, serviceLimit, bindingIndex, nextState) \
	case stateValue: \
		if (qmiGetConfiguredService() <= serviceLimit) \
			goto invalid; \
		snapshot = context->bindings[bindingIndex]; \
		if ((snapshot.requestBits << 16) != 0) { \
			node = request->node; \
			result = 1; \
			control = node->control; \
			context->bindings[bindingIndex].requestBits = snapshot.requestBits | \
				(1U << (node->index + 16)); \
			goto unlocked; \
		} \
		context->bindings[bindingIndex].requestBits = snapshot.requestBits | \
			(1U << request->node->index); \
		snapshot.client->callbackContext = request; \
		result = qmiReleaseClientInstance(snapshot.client); \
		if (result < 0) \
			goto failed; \
		node = request->node; \
		if (result != 1) \
			request->state = nextState; \
		control = node->control; \
		goto unlocked

	switch (request->state) {
	case 0:
		client = context->bindings[0].client;
		if (client != NULL && client->clientId != 255) {
			request->state = 1;
		} else {
			client = context->bindings[9].client;
			if (client != NULL && client->clientId != 255) {
				request->state = 3;
			} else {
				client = context->bindings[1].client;
				if (client != NULL && client->clientId != 255) {
					request->state = 5;
				} else {
					client = context->bindings[8].client;
					if (client != NULL && client->clientId != 255) {
						request->state = 7;
					} else {
						client = context->bindings[4].client;
						if (client != NULL && client->clientId != 255) {
							request->state = 15;
						} else {
							client = context->bindings[7].client;
							if (client != NULL && client->clientId != 255) {
								request->state = 9;
							} else {
								client = context->bindings[2].client;
								if (client != NULL && client->clientId != 255) {
									request->state = 11;
								} else {
									client = context->bindings[3].client;
									request->state = client == NULL || client->clientId == 255 ? 0 : 13;
								}
							}
						}
					}
				}
			}
		}
		result = sceKernelSetEventFlag(control->event, 1U << node->index);
		control = request->node->control;
		goto check_result;
	RELEASE_CLIENT_CASE(1, 1, 0, 2);
	case 2:
		result = request->result;
		if (result < 0)
			goto error;
		goto select_first;
	case 3:
		service = qmiGetConfiguredService();
		current = request->node->control->protocols[request->channel];
		if (service > qmiGetConfiguredService())
			goto invalid;
		switch (service) {
		case 1: binding = &current->bindings[2]; break;
		case 2: binding = &current->bindings[0]; break;
		case 3: binding = &current->bindings[1]; break;
		case 4: binding = &current->bindings[5]; break;
		case 5: binding = &current->bindings[3]; break;
		case 6: binding = &current->bindings[4]; break;
		case 8: binding = &current->bindings[6]; break;
		case 10: binding = &current->bindings[8]; break;
		case 11: binding = &current->bindings[7]; break;
		default:
			if (service != qmiGetConfiguredService())
				goto invalid;
			binding = &current->bindings[9];
			break;
		}
		client = binding->client;
		bits = binding->requestBits;
		if ((bits << 16) != 0) {
			node = request->node;
			result = 1;
			control = node->control;
			binding->requestBits = bits | (1U << (node->index + 16));
			goto unlocked;
		}
		binding->requestBits = bits | (1U << request->node->index);
		client->callbackContext = request;
		result = qmiReleaseClientInstance(client);
		if (result < 0)
			goto failed;
		node = request->node;
		if (result != 1)
			request->state = 4;
		control = node->control;
		goto unlocked;
	case 4:
		result = request->result;
		if (result < 0)
			goto error;
		result = bbcmdSelectClientDeactivation(request, 1);
		control = request->node->control;
		goto check_result;
	RELEASE_CLIENT_CASE(5, 2, 1, 6);
	case 6:
		result = request->result;
		if (result < 0)
			goto error;
		result = bbcmdSelectClientDeactivation(request, 2);
		control = request->node->control;
		goto check_result;
	RELEASE_CLIENT_CASE(7, 9, 8, 8);
	case 8:
		result = request->result;
		if (result == 0) {
select_first:
			result = bbcmdSelectClientDeactivation(request, 0);
			control = request->node->control;
		}
		goto check_result;
	RELEASE_CLIENT_CASE(9, 10, 7, 10);
	case 10:
		result = request->result;
		if (result != 0)
			goto check_result;
		result = bbcmdSelectClientDeactivation(request, 4);
		if (result >= 0) {
			control = request->node->control;
			goto unlocked;
		}
		node = request->node;
		request->result = 0x80330002;
		control = node->control;
		request->state = 0;
		goto error;
	RELEASE_CLIENT_CASE(11, 0, 2, 12);
	case 12:
		result = request->result;
		if (result == 0) {
			result = bbcmdSelectClientDeactivation(request, 8);
			control = request->node->control;
		}
		goto check_result;
	RELEASE_CLIENT_CASE(13, 4, 3, 14);
	case 14:
		result = request->result;
		if (result == 0) {
			result = bbcmdSelectClientDeactivation(request, 16);
			control = request->node->control;
		}
		goto check_result;
	RELEASE_CLIENT_CASE(15, 5, 4, 16);
	case 16:
		result = request->result;
		if (result == 0) {
			result = bbcmdSelectClientDeactivation(request, 32);
			control = request->node->control;
		}
		goto check_result;
	case 17:
		result = 0;
		goto unlocked;
	case 18:
		result = request->result;
		if (result < 0)
			goto error;
		result = bbcmdSelectClientDeactivation(request, 64);
		control = request->node->control;
		goto check_result;
	default:
		result = 0x80330102;
		goto error;
	}
#undef RELEASE_CLIENT_CASE

invalid:
	result = 0x80330002;
failed:
	control = request->node->control;
error:
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
	request->state = 0;
	goto completed;
check_result:
	bbcmdUnlockMutex(&control->mutex);
	if (result < 0)
		goto failed_unlocked;
	goto check_completed;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 0)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	if (request->result == 0) {
		if (context->bindings != NULL) {
			client = context->clientStorage;
			if (client != NULL)
				bbcmdFree(client);
		}
		bbcmdSetChannelState(request->channel, 2);
		bbcmdSetRequestedMask(request->channel, 0);
		memset(context->bindings, 0,
			sizeof(*context) - offsetof(BbcmdProtocolContext, bindings));
	}
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 10, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdReadFirmwareSelection(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int mask, bits, offset, responseLength, length;
	unsigned short zero;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 51)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[0];
		bits = binding->requestBits & ~(1U << node->index);
		if (bits >> 16)
			bits &= ~(1U << (node->index + 16));
		result = control->cancelMask;
		binding->requestBits = bits;
		if (result == 0)
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[0];
	if (request->state == 1) {
		result = request->result;
		if (result < 0)
			goto error;
		client = binding->client;
		responseLength = client->response.length;
		/* The original returns here with the command mutex still held. */
		if (responseLength <= 10)
			return 0x80330302;
		offset = 0;
		result = 0;
		do {
			const QmiResultResponse *response =
				(const QmiResultResponse *)(client->response.data + offset);
			unsigned int messageLength = response->message.lengthLow |
				(response->message.lengthHigh << 8);

			if (messageLength > 7) {
				unsigned int messageId = response->message.messageIdLow |
					(response->message.messageIdHigh << 8);
				unsigned int cursor = 0;

				switch (messageId) {
				case 71:
					result = 0;
					do {
						const QmiTlvHeader *header =
							(const QmiTlvHeader *)(response->parameters + cursor);
						unsigned int size = header->lengthLow | (header->lengthHigh << 8);

						if (header->type == 1) {
							const QmiFirmwareListWire *list =
								(const QmiFirmwareListWire *)(header + 1);
							const QmiFirmwareImageWire *image =
								(const QmiFirmwareImageWire *)list->images;
							unsigned int index = 0;

							while (index < list->count) {
								if (image->imageType == 1) {
									const unsigned char *build = image->buildId;
									unsigned int ruleIndex;

									if (build[0] != 'D' || build[1] != '3' || build[2] != '2' ||
									    build[3] != '0' || build[4] != '0')
										goto unknown_build;
									for (ruleIndex = 0; ruleIndex < 11; ++ruleIndex) {
										const QmiBuildSuffixRule *rule = &g_qmiBuildSuffixRules[ruleIndex];

										if (rule->suffixSize == 1 ||
										    (build[6] == rule->suffix[0] && (rule->suffixSize == 2 ||
										    (build[7] == rule->suffix[1] && (rule->suffixSize == 3 ||
										    (build[8] == rule->suffix[2] && (rule->suffixSize == 4 ||
										    (build[9] == rule->suffix[3] && (rule->suffixSize == 5 ||
										    (build[10] == rule->suffix[4] && (rule->suffixSize == 6 ||
										    (build[11] == rule->suffix[5] && (rule->suffixSize == 7 ||
										    (build[12] == rule->suffix[6] && (rule->suffixSize == 8 ||
										     build[13] == rule->suffix[7]))))))))))))))) {
											*(SceUInt32 *)request->output[0].pointer = rule->matchValue;
											result = 0;
											goto preference_done;
										}
									}
unknown_build:
									result = 0x80330002;
									*(SceUInt32 *)request->output[0].pointer = 4;
									break;
								}
								++index;
								image = (const QmiFirmwareImageWire *)(image->buildId + image->buildIdLength);
							}
						}
preference_done:
						cursor += size + sizeof(*header);
					} while (messageLength - 7 > cursor);
					break;
				case 73:
					do {
						/* Sony repeatedly examines the first TLV. */
						const QmiTlvHeader *header = (const QmiTlvHeader *)response->parameters;
						const QmiFirmwareListWire *list = (const QmiFirmwareListWire *)(header + 1);
						unsigned int size = header->lengthLow | (header->lengthHigh << 8);

						if (header->type == 1 && list->count != 0) {
							const unsigned char *groupCursor = &list->count;
							unsigned int groupIndex = 0;

							do {
								const QmiStoredImageGroupWire *group =
									(const QmiStoredImageGroupWire *)(groupCursor + 1);

								if (group->imageType != 0) {
									unsigned int count = group->count;
									unsigned char index = 0;

									groupCursor = &group->count;
									if (count != 0) do {
										const QmiStoredImageWire *image =
											(const QmiStoredImageWire *)(groupCursor + 1);

										++index;
										/* Preserve the original doubled fixed stride. */
										groupCursor += image->buildIdLength + 2 * sizeof(*image);
									} while (index != count);
								} else if (group->runningIndex != 255) {
									*(SceUInt32 *)request->output[2].pointer = group->runningIndex;
									break;
								}
								++groupIndex;
							} while (list->count > groupIndex);
						}
						cursor += size + sizeof(*header);
					} while (messageLength - 7 > cursor);
					result = 0;
					break;
				case 44:
					result = 0;
					do {
						const QmiTlvHeader *header = (const QmiTlvHeader *)response->parameters;
						unsigned int size = header->lengthLow | (header->lengthHigh << 8);

						if (header->type == 1) {
							const unsigned char *revision = (const unsigned char *)(header + 1) + 1;
							unsigned int value = 0, index;

							for (index = 0; index < 4; ++index) {
								unsigned char digit = revision[index] - '0';

								if (digit > 9) {
									result = 0x80330002;
									goto revision_done;
								}
								if (revision[index] != '0')
									value += digit << (12 - 4 * index);
							}
							if (value == 0x490) {
								result = 0;
								*(SceInt32 *)request->output[1].pointer = 0;
							} else if (value == 0x500) {
								result = 0;
								*(SceInt32 *)request->output[1].pointer = 1;
							} else {
								result = 0x80330002;
								*(SceInt32 *)request->output[1].pointer = -1;
							}
						}
revision_done:
						cursor += size + sizeof(*header);
					} while (messageLength - 7 > cursor);
					break;
				default:
					result = 0x80330002;
					break;
				}
			}
			offset += messageLength + sizeof(response->message);
		} while (responseLength > offset);
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed_unlocked;
		goto completed;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto error;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330101;
		request->result = result;
		goto error;
	}
	bits = binding->requestBits;
	zero = bits;
	if (zero != 0) {
		result = 0;
		control = request->node->control;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	if (client->service != 2) {
		result = 0x80330002;
		request->result = result;
		goto failed;
	}
	{
		QmiMessageHeader *message = (QmiMessageHeader *)client->request.data;
		BbcmdProtocolClient *next;

		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 71;
		next = binding->client;
		client->request.length = sizeof(*message);
		client = next;
	}
#define APPEND_SELECTION_MESSAGE(id) do { \
	QmiMessageHeader *message; \
	BbcmdProtocolClient *next; \
	if (client->service != 2) { \
		result = 0x80330002; \
		goto failed; \
	} \
	length = client->request.length; \
	if (length + sizeof(*message) > sizeof(client->request.data)) { \
		result = 0x80330009; \
		goto failed; \
	} \
	message = (QmiMessageHeader *)(client->request.data + length); \
	message->messageIdHigh = zero; \
	message->lengthLow = zero; \
	message->lengthHigh = zero; \
	message->messageIdLow = id; \
	next = binding->client; \
	client->request.length = length + sizeof(*message); \
	client = next; \
} while (0)
	APPEND_SELECTION_MESSAGE(44);
	APPEND_SELECTION_MESSAGE(73);
#undef APPEND_SELECTION_MESSAGE
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	control = request->node->control;
	if (result < 0) {
		request->result = result;
		goto error;
	}
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	control = request->node->control;
	request->result = result;
error:
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 51, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdReadSimContent(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client, *sendClient;
	BbcmdCompletion completion;
	const BbmcSimFile *file;
	BbmcSimContentInfo *output;
	QmiUimReadTransparentPrefix *parameters;
	QmiUimReadInformation *readInfo;
	QmiMessageHeader *message;
	unsigned int mask, bits, offset, responseLength, length, pathLength, payloadLength;
	unsigned short zero;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 47)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	result = control->cancelMask & mask;
	if (result != 0) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			node = request->node;
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[7];
			bits = binding->requestBits & ~(1U << node->index);
			if ((bits >> 16) != 0)
				bits &= ~(1U << (node->index + 16));
			result = control->cancelMask;
			binding->requestBits = bits;
			if (result == 0) {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				return 0;
			}
		}
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[7];
	if (request->state == 1) {
		if (request->result >= 0) {
			const QmiResultResponse *response;

			client = binding->client;
			response = (const QmiResultResponse *)client->response.data;
			responseLength = response->message.lengthLow - 7 +
				(response->message.lengthHigh << 8);
			if (responseLength != 0) {
				offset = 0;
				do {
					const QmiTlvHeader *header;
					unsigned int size;

					client = binding->client;
					response = (const QmiResultResponse *)client->response.data;
					header = (const QmiTlvHeader *)(response->parameters + offset);
					size = header->lengthLow | (header->lengthHigh << 8);
					if (header->type == 16) {
						const unsigned char *status = (const unsigned char *)(header + 1);

						output = request->output[0].pointer;
						output->sw1 = status[0];
						client = binding->client;
						response = (const QmiResultResponse *)client->response.data;
						status = response->parameters + offset + sizeof(*header);
						output = request->output[0].pointer;
						output->sw2 = status[1];
						output = request->output[0].pointer;
						output->validFields |= 1;
					} else if (header->type == 17) {
						const QmiUimReadDataPrefix *data = (const QmiUimReadDataPrefix *)header;
						unsigned int dataLength = data->dataLengthLow |
							(data->dataLengthHigh << 8);
						unsigned short copyLength = 32;

						if (dataLength <= 31)
							copyLength = dataLength;
						output = request->output[0].pointer;
						output->dataLength = copyLength;
						output = request->output[0].pointer;
						memcpy(output->data, data->data, output->dataLength);
						output = request->output[0].pointer;
						output->validFields |= 2;
					}
					offset += size + sizeof(*header);
				} while (responseLength > offset);
				file = request->input[0].pointer;
				output = request->output[0].pointer;
				output->file.fileId = file->fileId;
				file = request->input[0].pointer;
				output = request->output[0].pointer;
				output->file.pathLength = file->pathLength;
				file = request->input[0].pointer;
				output = request->output[0].pointer;
				memcpy(output->file.path, file->path, file->pathLength);
				control = request->node->control;
			}
			result = 0;
		}
		request->state = 0;
		goto unlocked;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto failed;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto failed;
	}
	bits = binding->requestBits;
	zero = bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	file = request->input[0].pointer;
	pathLength = file->pathLength;
	payloadLength = pathLength + sizeof(*parameters) + sizeof(*readInfo);
	parameters = (QmiUimReadTransparentPrefix *)(client->request.data +
		sizeof(client->request.data) - 1 - payloadLength);
	parameters->session.header.type = 1;
	parameters->session.header.lengthHigh = zero;
	parameters->session.sessionType = zero;
	parameters->session.aidLength = zero;
	parameters->file.header.lengthLow = pathLength + 3;
	parameters->file.header.lengthHigh = (pathLength + 3) >> 8;
	parameters->session.header.lengthLow = 2;
	parameters->file.header.type = 2;
	file = request->input[0].pointer;
	parameters->file.fileIdLow = file->fileId;
	file = request->input[0].pointer;
	parameters->file.pathLength = pathLength;
	parameters->file.fileIdHigh = file->fileId >> 8;
	file = request->input[0].pointer;
	memcpy(parameters->path, file->path, pathLength);
	readInfo = (QmiUimReadInformation *)(parameters->path + pathLength);
	readInfo->header.lengthHigh = zero;
	readInfo->offsetLow = zero;
	readInfo->offsetHigh = zero;
	readInfo->lengthLow = zero;
	readInfo->lengthHigh = zero;
	readInfo->header.type = 3;
	readInfo->header.lengthLow = 4;
	client = binding->client;
	if (client->service != 11) {
		result = 0x80330002;
		goto failed;
	}
	length = client->request.length;
	if (payloadLength + length + sizeof(*message) > sizeof(client->request.data)) {
		result = 0x80330009;
		goto failed;
	}
	message = (QmiMessageHeader *)(client->request.data + length);
	message->messageIdHigh = zero;
	message->lengthHigh = payloadLength >> 8;
	message->messageIdLow = 32;
	message->lengthLow = payloadLength;
	client->request.length = length + sizeof(*message);
	memmove(client->request.data + length + sizeof(*message), parameters, payloadLength);
	sendClient = binding->client;
	client->request.length += payloadLength;
	sendClient->callbackContext = request;
	result = qmiSendClientRequest(sendClient);
	control = request->node->control;
	if (result < 0)
		goto failed;
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
	goto completed;
failed:
	control = request->node->control;
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 47, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

int bbcmdReplyUimRefresh(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	QmiUimRefreshReply *reply;
	QmiMessageHeader *message;
	unsigned int mask, bits, length;
	unsigned short zero;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 45)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	mask = 1U << node->index;
	if (control->cancelMask & mask) {
		control->cancelMask &= ~mask;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		completion = request->completion;
		node = request->node;
		if (completion != NULL) {
			control = node->control;
			binding = &control->protocols[request->channel]->bindings[7];
			bits = binding->requestBits & ~(1U << node->index);
			if (bits >> 16)
				bits &= ~(1U << (node->index + 16));
			result = control->cancelMask;
			binding->requestBits = bits;
			if (result == 0) {
				completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
				node = request->node;
				control = node->control;
			} else {
				result = 0;
			}
		} else {
			result = 0;
			control = node->control;
		}
		goto finish_locked;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[7];
	if (request->state == 1) {
		result = request->result;
		request->state = 3;
		result &= result >> 31;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto failed_unlocked;
		goto check_completed;
	}
	if (request->state != 0 && request->state != 2) {
		result = 0x80330102;
		goto failed;
	}
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto failed;
	}
	bits = binding->requestBits;
	zero = bits;
	if (zero != 0) {
		result = 0;
		binding->requestBits = bits | (1U << (node->index + 16));
		request->state = 2;
		goto unlocked;
	}
	binding->requestBits = bits | (1U << node->index);
	memset(client->request.data, 0, sizeof(client->request.data));
	client = binding->client;
	client->request.length = zero;
	reply = (QmiUimRefreshReply *)(client->request.data +
		sizeof(client->request.data) - 1 - sizeof(*reply));
	if (request->input[0].value <= 1) {
		reply->session.header.type = 1;
		reply->session.header.lengthHigh = zero;
		reply->session.sessionType = zero;
		reply->session.aidLength = zero;
		reply->info.header.lengthLow = 1;
		reply->info.header.lengthHigh = zero;
		reply->session.header.lengthLow = 2;
		reply->info.header.type = 2;
		reply->info.value = request->input[0].value;
		client = binding->client;
		if (client->service != 11) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + sizeof(*reply) + sizeof(*message) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 43;
		message->lengthLow = sizeof(*reply);
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), reply, sizeof(*reply));
		client->request.length += sizeof(*reply);
	} else {
		if (request->input[0].value - 2 > 1) {
			result = 0x80330002;
			goto failed;
		}
		reply->session.header.type = 1;
		reply->session.header.lengthHigh = zero;
		reply->session.sessionType = zero;
		reply->session.aidLength = zero;
		reply->info.header.lengthLow = 1;
		reply->info.header.lengthHigh = zero;
		reply->session.header.lengthLow = 2;
		reply->info.header.type = 2;
		reply->info.value = request->input[0].value == 2;
		client = binding->client;
		if (client->service != 11) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + sizeof(*reply) + sizeof(*message) > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 44;
		message->lengthLow = sizeof(*reply);
		client->request.length = length + sizeof(*message);
		memmove(client->request.data + length + sizeof(*message), reply, sizeof(*reply));
		client->request.length += sizeof(*reply);
	}
	client = binding->client;
	client->callbackContext = request;
	result = qmiSendClientRequest(client);
	node = request->node;
	control = node->control;
	if (result < 0)
		goto failed;
	request->state = 1;
unlocked:
	bbcmdUnlockMutex(&control->mutex);
check_completed:
	if (request->state != 3)
		return result;
	goto completed;
failed:
	control = request->node->control;
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
completed:
	request->state = 0;
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 45, request->result,
			request->input, request->output, request->completionArg);
	node = request->node;
	control = node->control;
finish_locked:
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}

#undef READ_CHANNEL_RATE

/* name unknown: reset and publish one complete protocol context. */
BbcmdProtocolContext *bbcmdCreateProtocolContext(unsigned int channel)
{
	BbcmdProtocolContext *context = &g_qmiProtocolContexts[channel];

	memset(context, 0, sizeof(*context));
	g_qmiPublishedContexts[channel] = context;
	context->initializeHandlers = bbcmdRegisterQmiHandlers;
	return context;
}

int bbcmdInitializeProtocol(unsigned int command, BbcmdControl *control)
{
	int ret;

	if (command != 1)
		return (int)0x80330002;
	ret = qmiInitializeBackend();
	if (ret >= 0)
		control->state = 2;
	return ret;
}

int bbcmdFinalizeProtocol(unsigned int command, BbcmdControl *control)
{
	int ret;

	if (command != 2)
		return (int)0x80330002;
	ret = qmiFinalizeBackend();
	control->state = 0;
	return ret;
}

int bbcmdOpenChannel(unsigned int command, unsigned int channel)
{
	unsigned int channels[2] = { 0, 1 };
	int ret = (int)0x80330002;

	if (command != 3)
		goto out;
	if (channel > 1)
		goto out;
	ret = qmiOpenTransport(channels[channel]);
	if (ret >= 0)
		ret = bbcmdSetChannelState(channel, 1);
out:
	return ret;
}

int bbcmdCloseChannel(unsigned int command, unsigned int channel)
{
	unsigned int channels[2] = { 0, 1 };
	int ret = (int)0x80330002;

	if (command != 4)
		goto out;
	if (channel > 1)
		goto out;
	ret = qmiCloseTransport(channels[channel]);
	if (ret >= 0)
		ret = bbcmdSetChannelState(channel, 0);
out:
	return ret;
}

/* name unknown: invalidate protocol client IDs and release their storage. */
int bbcmdResetChannel(unsigned int command, unsigned int channel,
	BbcmdControl *control)
{
	BbcmdProtocolContext *context;
	int ret;

	if (command != 48)
		return (int)0x80330002;
	ret = qmiResetClientInstances();
	if (ret < 0)
		return (int)0x80330002;
	bbcmdLockMutex(&control->mutex);
	context = control->protocols[channel];
	if (context->bindings[2].client != NULL)
		context->bindings[2].client->clientId = 255;
	if (context->bindings[0].client != NULL)
		context->bindings[0].client->clientId = 255;
	if (context->bindings[1].client != NULL)
		context->bindings[1].client->clientId = 255;
	if (context->bindings[5].client != NULL)
		context->bindings[5].client->clientId = 255;
	if (context->bindings[3].client != NULL)
		context->bindings[3].client->clientId = 255;
	if (context->bindings[4].client != NULL)
		context->bindings[4].client->clientId = 255;
	if (context->bindings[6].client != NULL)
		context->bindings[6].client->clientId = 255;
	if (context->bindings[7].client != NULL)
		context->bindings[7].client->clientId = 255;
	if (context->bindings[8].client != NULL)
		context->bindings[8].client->clientId = 255;
	if (context->bindings[9].client != NULL)
		context->bindings[9].client->clientId = 255;
	if (context->clientStorage != NULL)
		bbcmdFree(context->clientStorage);
	__builtin_memset(control->completedMask, 0, sizeof(control->completedMask));
	bbcmdSetChannelState(channel, 2);
	bbcmdUnlockMutex(&control->mutex);
	return ret;
}
