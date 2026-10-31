#pragma once

#include <bbmc/bbmc.h>

#include "bbmc_memory.h"
#include "bbmc_queue.h"
#include "bbmc_syscon_reply.h"
#include "qmi_uim_card_status.h"

/* Names unknown: buffers are copied with their terminating byte by the owner. */
typedef struct BbmcStringPair {
	char *first;
	char *second;
	SceSize firstLength;
	SceSize secondLength;
} BbmcStringPair;

typedef struct BbmcStringPairEntry {
	BbmcStringPair strings;
	SceUInt32 forwardedValue;
} BbmcStringPairEntry;

typedef struct BbmcStringPairList {
	SceUInt32 count;
	SceUInt32 selectedIndex;
	BbmcStringPairEntry entries[5];
} BbmcStringPairList;

typedef struct BbmcStringSet {
	SceUInt32 forwardedValues[3];
	char *strings[5];
	SceSize lengths[5];
} BbmcStringSet;

typedef struct BbmcSmsListOutput {
	SceUInt32 *count;
	SceUInt32 *indices;
} BbmcSmsListOutput;

_Static_assert(sizeof(BbmcSmsListOutput) == 8, "SMS list output size");
_Static_assert(offsetof(BbmcSmsListOutput, count) == 0, "SMS list count");
_Static_assert(offsetof(BbmcSmsListOutput, indices) == 4, "SMS list indices");

typedef struct BbmcSmsReadRequest {
	SceUInt32 memoryIndex;
	SceUInt8 *data;
	SceUInt32 *length;
	SceUInt32 *format;
} BbmcSmsReadRequest;

_Static_assert(sizeof(BbmcSmsReadRequest) == 16, "SMS read request size");
_Static_assert(offsetof(BbmcSmsReadRequest, memoryIndex) == 0, "SMS memory index");
_Static_assert(offsetof(BbmcSmsReadRequest, data) == 4, "SMS read data");
_Static_assert(offsetof(BbmcSmsReadRequest, length) == 8, "SMS read length");
_Static_assert(offsetof(BbmcSmsReadRequest, format) == 12, "SMS read format");

typedef struct BbmcSmsReadAcknowledgement {
	SceUInt32 memoryIndex;
	SceUInt32 deleteMessage;
} BbmcSmsReadAcknowledgement;

typedef struct BbmcSmsSendRequest {
	SceUInt8 *data;
	SceSize length;
	SceUInt32 format;
} BbmcSmsSendRequest;

/* Fields are shared by setup requests 145/147 and completions 146/148. */
typedef struct BbmcSetupMessage {
	SceSize valueSize;
	SceUInt32 type;
	SceInt32 result;
	SceUInt32 completionFlag;
	void *outputBuffer;
	const void *inputBuffer;
} BbmcSetupMessage;

/* Each tag writes only its own fields; allocation does not clear the body. */
typedef struct BbmcPinMessage {
	union {
		BbmcPinFlags flags;
		SceInt32 result;
	} status;
	union {
		SceUInt32 configuredServiceValue;
		SceUInt8 retries;
	} detail;
	SceUInt32 pinStatusUnavailable;
	SceInt32 cardStatusResult;
	SceUInt32 cardUnavailable;
	SceUInt32 applicationNotReady;
} BbmcPinMessage;

/* Request and completion tags 155 through 162 use distinct live slots. */
typedef struct BbmcFirmwareMessage {
	SceInt32 storedImagesResult;
	SceInt32 firmwarePreferenceResult;
	SceInt32 firmwareImageResult;
	SceInt32 setFirmwarePreferenceResult;
	QmiStoredImages *storedImages;
	QmiFirmwarePreference *firmwarePreference;
	const QmiFirmwareImage *firmwareImage;
	const QmiSetFirmwarePreference *setFirmwarePreference;
	SceUInt32 imageCount;
	SceUInt32 firstImageType;
	SceUInt32 secondImageType;
} BbmcFirmwareMessage;

/* The request and completion tags share slots, not initialized contents. */
typedef struct BbmcSmsMessage {
	union {
		BbmcSmsListOutput listOutput;
		BbmcSmsReadRequest readRequest;
		BbmcSmsReadAcknowledgement acknowledgement;
		BbmcSmsSendRequest sendRequest;
		struct {
			BbmcSmsListOutput output;
			SceInt32 acknowledgementResult;
			SceInt32 listResult;
		} completion;
	};
	SceInt32 readResult;
} BbmcSmsMessage;

typedef struct BbmcControlKeyReply {
	SceUInt32 featureId;
	SceInt32 result;
	SceUInt8 remainingRetries;
} BbmcControlKeyReply;

typedef struct BbmcChannelRatesMessage {
	SceUInt32 *txRate;
	SceUInt32 *rxRate;
	SceUInt32 *maxTxRate;
	SceUInt32 *maxRxRate;
	SceInt32 result;
} BbmcChannelRatesMessage;

_Static_assert(sizeof(BbmcChannelRatesMessage) == 20, "channel rates message size");
_Static_assert(offsetof(BbmcChannelRatesMessage, txRate) == 0, "transmit rate output");
_Static_assert(offsetof(BbmcChannelRatesMessage, rxRate) == 4, "receive rate output");
_Static_assert(offsetof(BbmcChannelRatesMessage, maxTxRate) == 8,
	"maximum transmit rate output");
_Static_assert(offsetof(BbmcChannelRatesMessage, maxRxRate) == 12,
	"maximum receive rate output");
_Static_assert(offsetof(BbmcChannelRatesMessage, result) == 16, "channel rates result");

_Static_assert(sizeof(BbmcSmsMessage) == 20, "SMS message family size");
_Static_assert(offsetof(BbmcSmsMessage, completion.acknowledgementResult) == 8,
	"SMS acknowledgement result");
_Static_assert(offsetof(BbmcSmsMessage, completion.listResult) == 12,
	"SMS list result");
_Static_assert(offsetof(BbmcSmsMessage, readResult) == 16, "SMS read result");
_Static_assert(sizeof(BbmcControlKeyReply) == 12, "control key reply size");
_Static_assert(offsetof(BbmcControlKeyReply, featureId) == 0, "control key feature");
_Static_assert(offsetof(BbmcControlKeyReply, result) == 4, "control key result");
_Static_assert(offsetof(BbmcControlKeyReply, remainingRetries) == 8,
	"control key remaining retries");

_Static_assert(sizeof(BbmcPinMessage) == 24, "PIN message family size");
_Static_assert(offsetof(BbmcPinMessage, status) == 0, "PIN message status");
_Static_assert(offsetof(BbmcPinMessage, detail) == 4, "PIN message detail");
_Static_assert(offsetof(BbmcPinMessage, pinStatusUnavailable) == 8, "PIN status flag");
_Static_assert(offsetof(BbmcPinMessage, cardStatusResult) == 12, "card status result");
_Static_assert(offsetof(BbmcPinMessage, cardUnavailable) == 16, "card availability flag");
_Static_assert(offsetof(BbmcPinMessage, applicationNotReady) == 20,
	"application readiness flag");
_Static_assert(sizeof(BbmcFirmwareMessage) == 44, "firmware message family size");
_Static_assert(offsetof(BbmcFirmwareMessage, storedImages) == 16, "stored images output");
_Static_assert(offsetof(BbmcFirmwareMessage, firmwarePreference) == 20,
	"firmware preference output");
_Static_assert(offsetof(BbmcFirmwareMessage, firmwareImage) == 24,
	"firmware image input");
_Static_assert(offsetof(BbmcFirmwareMessage, setFirmwarePreference) == 28, "firmware preference input");
_Static_assert(offsetof(BbmcFirmwareMessage, imageCount) == 32, "image completion count");
_Static_assert(offsetof(BbmcFirmwareMessage, firstImageType) == 36, "first image type");
_Static_assert(offsetof(BbmcFirmwareMessage, secondImageType) == 40, "second image type");

_Static_assert(sizeof(BbmcSetupMessage) == 24, "setup message size");
_Static_assert(offsetof(BbmcSetupMessage, valueSize) == 0, "setup value size");
_Static_assert(offsetof(BbmcSetupMessage, type) == 4, "setup value type");
_Static_assert(offsetof(BbmcSetupMessage, result) == 8, "setup completion result");
_Static_assert(offsetof(BbmcSetupMessage, completionFlag) == 12, "setup completion flag");
_Static_assert(offsetof(BbmcSetupMessage, outputBuffer) == 16, "setup output buffer");
_Static_assert(offsetof(BbmcSetupMessage, inputBuffer) == 20, "setup input buffer");

_Static_assert(sizeof(BbmcSmsSendRequest) == 12, "SMS send request size");
_Static_assert(offsetof(BbmcSmsSendRequest, data) == 0, "SMS send data");
_Static_assert(offsetof(BbmcSmsSendRequest, length) == 4, "SMS send length");
_Static_assert(offsetof(BbmcSmsSendRequest, format) == 8, "SMS send format");

_Static_assert(sizeof(BbmcSmsReadAcknowledgement) == 8, "SMS read acknowledgement size");
_Static_assert(offsetof(BbmcSmsReadAcknowledgement, memoryIndex) == 0,
	"SMS acknowledged index");
_Static_assert(offsetof(BbmcSmsReadAcknowledgement, deleteMessage) == 4,
	"SMS acknowledged deletion flag");

/* Command 103 leaves the first word unwritten; no consumer reads it. */
typedef struct BbmcNetworkStartMessage {
	SceUInt32 reserved;
	SceUInt32 *networkHandle;
	SceUInt32 *ipv4Address;
	SceUInt8 *hasIpv4Address;
	SceUInt32 *runtimeValue;
} BbmcNetworkStartMessage;

_Static_assert(sizeof(BbmcNetworkStartMessage) == 20, "network start message size");
_Static_assert(offsetof(BbmcNetworkStartMessage, networkHandle) == 4,
	"network start handle");
_Static_assert(offsetof(BbmcNetworkStartMessage, ipv4Address) == 8,
	"network start IPv4 address");
_Static_assert(offsetof(BbmcNetworkStartMessage, hasIpv4Address) == 12,
	"network start IPv4 flag");
_Static_assert(offsetof(BbmcNetworkStartMessage, runtimeValue) == 16,
	"network start runtime value");

typedef struct BbmcNetworkStopMessage {
	SceInt32 commandResult;
	SceUInt32 *networkHandle;
} BbmcNetworkStopMessage;

_Static_assert(sizeof(BbmcNetworkStopMessage) == 8, "network stop message size");
_Static_assert(offsetof(BbmcNetworkStopMessage, networkHandle) == 4,
	"network stop handle");

/* The updater's chunk (commands 175/176): the package fields, under the message's own type. */
typedef struct BbmcUpdatePackageMessage {
	SceUInt32 chunkCount;
	SceUInt32 chunkIndex;
	void *buffer;
	SceSize size;
} BbmcUpdatePackageMessage;

_Static_assert(sizeof(BbmcUpdatePackageMessage) == 16, "update package message size");

/* name unknown: command 113 sets (set 1) or clears the packet interface control flags */
typedef struct BbmcControlFlagMessage {
	SceUInt32 set;
	SceUInt32 mask;
} BbmcControlFlagMessage;

/* name unknown: command 101 asks the manager to run a SceGps ioctl through bbcmd */
typedef struct BbmcGpsRequestMessage {
	SceUInt32 code;
	SceUInt32 size;
	void *data;
} BbmcGpsRequestMessage;

/* name unknown: message 165 carries the syscon's proactive word, and 1 when message 15 asked for it */
typedef struct BbmcProactiveReplyMessage {
	SceUInt32 sysconValue;
	SceUInt32 fetchRequested;
} BbmcProactiveReplyMessage;

typedef union BbmcMessageBody {
	SceInt32 commandResult;
	BbmcSmsListOutput smsListOutput;
	BbmcSmsReadRequest smsReadRequest;
	BbmcSmsReadAcknowledgement smsReadAcknowledgement;
	BbmcSmsSendRequest smsSendRequest;
	BbmcSetupMessage setupMessage;
	BbmcPinMessage pinMessage;
	BbmcFirmwareMessage firmwareMessage;
	BbmcSmsMessage smsMessage;
	BbmcControlKeyReply controlKeyReply;
	BbmcChannelRatesMessage channelRatesMessage;
	BbmcControlFlagMessage controlFlag;
	BbmcGpsRequestMessage gpsRequest;
	BbmcProactiveReplyMessage proactiveReply;
	const struct BbmcTerminalResponse *terminalResponse;
	const struct QmiCat2CommandOptions *envelopeOptions;
	SceUInt32 refreshResponse;
	const struct BbmcSimFile *simFile;
	BbmcNetworkStartMessage networkStart;
	BbmcNetworkStopMessage networkStop;
	BbmcUpdatePackageMessage updatePackage;
	SceUInt32 timedOutCommand;
	SceUInt32 operationMode;
	SceUInt32 sysconReplyFlags;
	SceUInt8 sysconReplyByte;
	SceUInt16 sysconReplyHalfword;
	SceUInt8 sysconReplyBytes[2];
	BbmcSysconReplyWords8 sysconReplyWords;
	BbmcSysconDecodedReply8 sysconDecodedReply;
	BbmcStringPair stringPair;
	BbmcStringPairList stringPairs;
	BbmcStringSet stringSet;
} BbmcMessageBody;

/* The list constructor establishes the complete 108-byte body, not padding. */
typedef struct BbmcMessage {
	BbmcQueueEntry queue;
	SceUInt32 command;
	BbmcMessageBody body;
	SceUInt32 notificationValue1;
	SceUInt32 notificationValue2;
} BbmcMessage;

BbmcMessage *msgAllocMsgPacket(BbmcControl *control, SceUInt32 command);
void msgFreeMsgPacket(BbmcControl *control, BbmcMessage *message);
int msgSendMsgPacket(BbmcControl *control, BbmcMessage *message);
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int msgReceiveMsgPacket(BbmcControl *control, BbmcMessage **message);

/* No surviving caller: these are the three words read by the forwarding helper. */
typedef struct BbmcNotificationArgs {
	SceUInt32 forwardedValue1;
	SceUInt32 forwardedValue2;
	BbmcControl *control;
} BbmcNotificationArgs;
