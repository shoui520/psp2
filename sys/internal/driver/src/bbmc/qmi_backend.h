#ifndef BBMC_QMI_BACKEND_H
#define BBMC_QMI_BACKEND_H

#include "bbcmd.h"
#include "bbmc_network_info.h"
#include "bbmc_sim_content.h"

typedef struct QmiBuildSuffixRule {
	SceUInt32 suffixSize;
	char suffix[8];
	SceUInt32 matchValue;
} QmiBuildSuffixRule;

_Static_assert(sizeof(QmiBuildSuffixRule) == 16, "build suffix rule size");
_Static_assert(offsetof(QmiBuildSuffixRule, matchValue) == 12,
	"build suffix result");

typedef struct QmiUimFileSelection {
	SceUInt16 pathCount;
	SceUInt16 path[2];
	SceUInt16 fileId;
	SceUInt32 fileKind;
} QmiUimFileSelection;

_Static_assert(sizeof(QmiUimFileSelection) == 12, "UIM file selection size");
_Static_assert(offsetof(QmiUimFileSelection, fileKind) == 8,
	"UIM file selection kind");

typedef struct QmiPacketStorage {
	SceInt32 length;
	SceUInt8 data[2075];
} QmiPacketStorage;

struct BbcmdProtocolClient {
	SceUInt32 channel;
	SceUInt32 service;
	SceUInt32 clientId;
	QmiPacketStorage request;
	QmiPacketStorage response;
	QmiPacketStorage indication;
	int (*callback)(SceUInt32 event, BbcmdProtocolClient *client);
	void *callbackContext;
	SceUInt16 transaction;
	SceInt32 result;
};

_Static_assert(sizeof(QmiPacketStorage) == 2080, "QMI packet storage size");
_Static_assert(sizeof(BbcmdProtocolClient) == 6268, "QMI client size");
_Static_assert(offsetof(BbcmdProtocolClient, response) == 2092,
	"QMI client response");
_Static_assert(offsetof(BbcmdProtocolClient, callback) == 6252,
	"QMI client callback");
_Static_assert(offsetof(BbcmdProtocolClient, result) == 6264,
	"QMI client result");

typedef struct QmiBackendControl QmiBackendControl;

typedef struct QmiTransmitHeader {
	SceUInt8 marker;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
	SceUInt8 senderFlags;
	SceUInt8 service;
	SceUInt8 clientId;
	SceUInt8 messageFlags;
	SceUInt8 transactionLow;
	SceUInt8 transactionHigh;
} QmiTransmitHeader;

typedef struct QmiControlWireHeader {
	SceUInt8 marker;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
	SceUInt8 senderFlags;
	SceUInt8 service;
	SceUInt8 clientId;
	SceUInt8 messageFlags;
	SceUInt8 transaction;
} QmiControlWireHeader;

typedef struct QmiControlWirePacket {
	QmiControlWireHeader header;
	SceUInt8 payload[];
} QmiControlWirePacket;

typedef struct QmiServiceWirePacket {
	QmiTransmitHeader header;
	SceUInt8 payload[];
} QmiServiceWirePacket;

typedef struct QmiMessageHeader {
	SceUInt8 messageIdLow;
	SceUInt8 messageIdHigh;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
} QmiMessageHeader;

typedef struct QmiEmptyMessageList {
	SceUInt32 count;
	SceUInt16 messageIds[5];
} QmiEmptyMessageList;

typedef struct QmiServiceParameterSet {
	SceUInt32 parameterKind;
	const SceUInt16 *messages;
	SceUInt32 messageCount;
	SceUInt32 service;
} QmiServiceParameterSet;

typedef struct QmiServiceParameterContext {
	const QmiServiceParameterSet *parameters;
	SceUInt8 messageIndex;
	SceUInt8 reserved[3];
} QmiServiceParameterContext;

_Static_assert(sizeof(QmiServiceParameterSet) == 16, "service parameter descriptor");
_Static_assert(sizeof(QmiServiceParameterContext) == 8, "service parameter context");

_Static_assert(sizeof(QmiEmptyMessageList) == 16, "empty QMI message list");

typedef struct QmiFirmwareImage {
	SceUInt32 imageType;
	SceUInt8 uniqueId[16];
	SceUInt32 buildIdLength;
	SceUInt8 buildId[36];
} QmiFirmwareImage;

typedef struct QmiFirmwarePreference {
	SceUInt32 count;
	QmiFirmwareImage images[2];
} QmiFirmwarePreference;

typedef struct QmiSetFirmwarePreference {
	QmiFirmwarePreference preference;
	SceUInt32 downloadOverride;
	SceUInt32 hasStorageIndex;
	SceUInt32 storageIndex;
} QmiSetFirmwarePreference;

typedef struct QmiFirmwareImageWire {
	SceUInt8 imageType;
	SceUInt8 uniqueId[16];
	SceUInt8 buildIdLength;
	SceUInt8 buildId[];
} QmiFirmwareImageWire;

typedef struct QmiFirmwareListWire {
	SceUInt8 count;
	SceUInt8 images[];
} QmiFirmwareListWire;

_Static_assert(sizeof(QmiFirmwareImage) == 60, "firmware image record");
_Static_assert(sizeof(QmiFirmwarePreference) == 124, "firmware preference");
_Static_assert(sizeof(QmiFirmwareImageWire) == 18, "firmware image wire prefix");
_Static_assert(sizeof(QmiFirmwareListWire) == 1, "firmware list wire prefix");

typedef struct QmiStoredImage {
	SceUInt32 isRunning;
	SceUInt32 storageIndex;
	SceUInt32 failureCount;
	SceUInt8 uniqueId[16];
	SceUInt32 buildIdLength;
	SceUInt8 buildId[35];
} QmiStoredImage;

typedef struct QmiStoredImageGroup {
	SceUInt32 maximumImages;
	SceUInt32 count;
	QmiStoredImage images[5];
} QmiStoredImageGroup;

typedef struct QmiStoredImages {
	QmiStoredImageGroup groups[2];
} QmiStoredImages;

typedef struct QmiStoredImageWire {
	SceUInt8 storageIndex;
	SceUInt8 failureCount;
	SceUInt8 uniqueId[16];
	SceUInt8 buildIdLength;
	SceUInt8 buildId[];
} QmiStoredImageWire;

typedef struct QmiStoredImageGroupWire {
	SceUInt8 imageType;
	SceUInt8 maximumImages;
	SceUInt8 runningIndex;
	SceUInt8 count;
	SceUInt8 images[];
} QmiStoredImageGroupWire;

_Static_assert(sizeof(QmiStoredImage) == 68, "stored image record");
_Static_assert(sizeof(QmiStoredImageGroup) == 348, "stored image group");
_Static_assert(sizeof(QmiStoredImages) == 696, "stored image groups");
_Static_assert(sizeof(QmiStoredImageWire) == 19, "stored image wire prefix");
_Static_assert(sizeof(QmiStoredImageGroupWire) == 4, "stored image group wire prefix");

typedef struct QmiTlvHeader {
	SceUInt8 type;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
} QmiTlvHeader;

typedef struct QmiByteArrayTlv {
	QmiTlvHeader header;
	SceUInt8 data[];
} QmiByteArrayTlv;

typedef struct QmiPinProtectionTlv {
	QmiTlvHeader header;
	SceUInt8 pinId;
	SceUInt8 protectionEnabled;
	SceUInt8 pinLength;
	SceUInt8 pin[];
} QmiPinProtectionTlv;

typedef struct QmiPinChangeTlv {
	QmiTlvHeader header;
	SceUInt8 pinId;
	SceUInt8 oldPinLength;
	SceUInt8 pins[];
} QmiPinChangeTlv;

_Static_assert(sizeof(QmiByteArrayTlv) == 3, "variable byte TLV header");
_Static_assert(sizeof(QmiPinProtectionTlv) == 6, "PIN protection TLV prefix");
_Static_assert(sizeof(QmiPinChangeTlv) == 5, "PIN change TLV prefix");

typedef struct QmiFirmwarePreferenceTlv {
	QmiTlvHeader header;
	SceUInt8 count;
	SceUInt8 images[];
} QmiFirmwarePreferenceTlv;

_Static_assert(sizeof(QmiSetFirmwarePreference) == 136, "firmware preference input");
_Static_assert(sizeof(QmiFirmwarePreferenceTlv) == 4, "firmware preference TLV prefix");

typedef struct QmiResultTlv {
	QmiTlvHeader header;
	SceUInt8 resultLow;
	SceUInt8 resultHigh;
	SceUInt8 errorLow;
	SceUInt8 errorHigh;
} QmiResultTlv;

typedef struct QmiResultResponse {
	QmiMessageHeader message;
	QmiResultTlv result;
	SceUInt8 parameters[];
} QmiResultResponse;

typedef struct QmiCountedBytes {
	SceUInt8 length;
	SceUInt8 values[10];
	SceUInt8 reserved;
} QmiCountedBytes;

typedef struct QmiCountedByteParameter {
	SceUInt8 length;
	SceUInt8 values[];
} QmiCountedByteParameter;

typedef struct QmiIndexedResponse {
	SceUInt32 index;
	SceUInt32 primaryValue;
	SceUInt32 secondaryValue;
	SceUInt8 detailBytes[2];
} QmiIndexedResponse;

typedef struct QmiByteValueTlv {
	QmiTlvHeader header;
	SceUInt8 value;
} QmiByteValueTlv;

typedef struct QmiWdsPacketServiceStatusTlv {
	QmiTlvHeader header;
	SceUInt8 connectionStatus, reconfigurationRequired;
} QmiWdsPacketServiceStatusTlv;

typedef struct QmiWdsVerboseCallEndReasonTlv {
	QmiTlvHeader header;
	SceUInt8 reasonType[2], reason[2];
} QmiWdsVerboseCallEndReasonTlv;

typedef struct QmiPdsServiceStateTlv {
	QmiTlvHeader header;
	SceUInt8 serviceState, trackingSessionState;
} QmiPdsServiceStateTlv;

typedef struct QmiNasCauseTlv {
	QmiTlvHeader header;
	SceUInt8 cause;
	SceUInt8 reason[2];
} QmiNasCauseTlv;

_Static_assert(sizeof(QmiWdsPacketServiceStatusTlv) == 5, "WDS packet-service status");
_Static_assert(sizeof(QmiWdsVerboseCallEndReasonTlv) == 7, "WDS verbose call-end reason");
_Static_assert(sizeof(QmiPdsServiceStateTlv) == 5, "PDS service state");
_Static_assert(sizeof(QmiNasCauseTlv) == 6, "NAS cause indication");

typedef struct QmiHalfwordValueTlv {
	QmiTlvHeader header;
	SceUInt8 value[2];
} QmiHalfwordValueTlv;

typedef struct QmiCat2CommandOptions {
	SceUInt32 operation;
	SceUInt32 optionalValue;
	SceUInt32 hasOptionalValue;
	SceUInt16 forwardedHalfword;
	SceUInt8 forwardedByte;
} QmiCat2CommandOptions;

typedef struct QmiCat2CommandPrefix {
	QmiByteValueTlv operation;
	SceUInt8 parameters[];
} QmiCat2CommandPrefix;

_Static_assert(sizeof(QmiCat2CommandOptions) == 16, "CAT2 native command options");
_Static_assert(sizeof(QmiCat2CommandPrefix) == 4, "CAT2 command prefix");

typedef struct QmiCat2ProactiveCommandTlv {
	QmiTlvHeader header;
	SceUInt8 command;
	SceUInt8 reference[4];
	SceUInt8 qualifier;
} QmiCat2ProactiveCommandTlv;

typedef struct QmiCat2ProactiveRequest {
	QmiTlvHeader header;
	SceUInt8 reference[4];
	SceUInt8 command;
} QmiCat2ProactiveRequest;

typedef struct QmiCat2TextPrefix {
	QmiTlvHeader header;
	SceUInt8 encoding;
	SceUInt8 length;
	SceUInt8 text[];
} QmiCat2TextPrefix;

typedef struct QmiCat2IconPrefix {
	QmiTlvHeader header;
	SceUInt8 qualifier;
	SceUInt8 height;
	SceUInt8 width;
	SceUInt8 codingScheme;
	SceUInt8 recordNumber;
	SceUInt8 length[2];
	SceUInt8 data[];
} QmiCat2IconPrefix;

typedef struct QmiCat2ItemPrefix {
	SceUInt8 identifier;
	SceUInt8 length;
	SceUInt8 text[];
} QmiCat2ItemPrefix;

typedef struct QmiCat2CountedPrefix {
	QmiTlvHeader header;
	SceUInt8 count;
	SceUInt8 entries[];
} QmiCat2CountedPrefix;

typedef struct QmiCat2IconListPrefix {
	QmiTlvHeader header;
	SceUInt8 qualifier;
	SceUInt8 count;
	SceUInt8 entries[];
} QmiCat2IconListPrefix;

typedef struct QmiCat2IconEntryPrefix {
	SceUInt8 qualifier;
	SceUInt8 height;
	SceUInt8 width;
	SceUInt8 codingScheme;
	SceUInt8 recordNumber;
	SceUInt8 length[2];
	SceUInt8 data[];
} QmiCat2IconEntryPrefix;

typedef struct QmiCat2SmsPayloadPrefix {
	QmiTlvHeader header;
	SceUInt8 format;
	SceUInt8 packing;
	SceUInt8 length;
	SceUInt8 data[];
} QmiCat2SmsPayloadPrefix;

typedef struct QmiCat2TerminalResultPrefix {
	QmiTlvHeader header;
	SceUInt8 reference[4];
	SceUInt8 commandNumber;
	SceUInt8 commandType;
	SceUInt8 generalResult;
	SceUInt8 additionalInfoLength;
	SceUInt8 additionalInfo[];
} QmiCat2TerminalResultPrefix;

typedef struct QmiCat2GetInkeyPrefix {
	QmiTlvHeader header;
	SceUInt8 units;
	SceUInt8 interval;
	SceUInt8 dataCodingScheme;
	SceUInt8 length;
	SceUInt8 text[];
} QmiCat2GetInkeyPrefix;

_Static_assert(sizeof(QmiCat2ProactiveCommandTlv) == 9, "CAT2 proactive header");
_Static_assert(sizeof(QmiCat2ProactiveRequest) == 8, "CAT2 proactive request");
_Static_assert(sizeof(QmiCat2TextPrefix) == 5, "CAT2 text prefix");
_Static_assert(sizeof(QmiCat2IconPrefix) == 10, "CAT2 icon prefix");
_Static_assert(sizeof(QmiCat2ItemPrefix) == 2, "CAT2 item prefix");
_Static_assert(sizeof(QmiCat2CountedPrefix) == 4, "CAT2 counted prefix");
_Static_assert(sizeof(QmiCat2IconListPrefix) == 5, "CAT2 icon list prefix");
_Static_assert(sizeof(QmiCat2IconEntryPrefix) == 7, "CAT2 icon entry prefix");
_Static_assert(sizeof(QmiCat2SmsPayloadPrefix) == 6, "CAT2 SMS prefix");
_Static_assert(sizeof(QmiCat2TerminalResultPrefix) == 11, "CAT2 terminal result prefix");
_Static_assert(sizeof(QmiCat2GetInkeyPrefix) == 7, "CAT2 get inkey prefix");
_Static_assert(sizeof(QmiHalfwordValueTlv) == 5, "QMI halfword TLV");

typedef struct QmiUimSessionTlv {
	QmiTlvHeader header;
	SceUInt8 sessionType;
	SceUInt8 aidLength;
} QmiUimSessionTlv;

typedef struct QmiUimRefreshReply {
	QmiUimSessionTlv session;
	QmiByteValueTlv info;
} QmiUimRefreshReply;

typedef struct QmiUimFilePrefix {
	QmiTlvHeader header;
	SceUInt8 fileIdLow;
	SceUInt8 fileIdHigh;
	SceUInt8 pathLength;
} QmiUimFilePrefix;

typedef struct QmiUimReadTransparentPrefix {
	QmiUimSessionTlv session;
	QmiUimFilePrefix file;
	SceUInt8 path[];
} QmiUimReadTransparentPrefix;

typedef struct QmiUimReadInformation {
	QmiTlvHeader header;
	SceUInt8 offsetLow;
	SceUInt8 offsetHigh;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
} QmiUimReadInformation;

typedef struct QmiUimReadDataPrefix {
	QmiTlvHeader header;
	SceUInt8 dataLengthLow;
	SceUInt8 dataLengthHigh;
	SceUInt8 data[];
} QmiUimReadDataPrefix;

_Static_assert(sizeof(QmiUimFilePrefix) == 6, "UIM file prefix size");
_Static_assert(sizeof(QmiUimReadTransparentPrefix) == 11, "UIM read prefix size");
_Static_assert(sizeof(QmiUimReadInformation) == 7, "UIM read information size");
_Static_assert(sizeof(QmiUimReadDataPrefix) == 5, "UIM read data prefix size");

_Static_assert(sizeof(QmiUimRefreshReply) == 9, "UIM refresh reply size");

typedef struct QmiUimPinParameterPrefix {
	QmiTlvHeader header;
	SceUInt8 pinId;
	SceUInt8 pinLength;
} QmiUimPinParameterPrefix;

typedef struct QmiUimVerifyPinParameters {
	QmiUimSessionTlv session;
	QmiUimPinParameterPrefix pin;
	SceUInt8 value[];
} QmiUimVerifyPinParameters;

typedef struct QmiUimUnblockPinPrefix {
	QmiTlvHeader header;
	SceUInt8 pinId;
	SceUInt8 pukLength;
} QmiUimUnblockPinPrefix;

typedef struct QmiUimUnblockPinParameters {
	QmiUimSessionTlv session;
	QmiUimUnblockPinPrefix pin;
	SceUInt8 value[];
} QmiUimUnblockPinParameters;

typedef struct QmiLengthPrefixedBytes {
	SceUInt8 length;
	SceUInt8 value[];
} QmiLengthPrefixedBytes;

_Static_assert(sizeof(QmiUimUnblockPinParameters) == 10, "UIM unblock PIN prefix");
_Static_assert(sizeof(QmiLengthPrefixedBytes) == 1, "QMI byte string prefix");

typedef struct QmiUimPinRetryTlv {
	QmiTlvHeader header;
	SceUInt8 verifyRetries;
	SceUInt8 unblockRetries;
} QmiUimPinRetryTlv;

typedef struct QmiUimDepersonalizationTlv {
	QmiTlvHeader header;
	SceUInt8 feature;
	SceUInt8 operation;
	SceUInt8 controlKeyLength;
	char controlKey[];
} QmiUimDepersonalizationTlv;

_Static_assert(sizeof(QmiUimDepersonalizationTlv) == 6,
	"UIM depersonalization prefix");

typedef struct QmiUimRefreshEventTlv {
	QmiTlvHeader header;
	SceUInt8 stage;
	SceUInt8 mode;
	SceUInt8 sessionType;
	SceUInt8 aidLength;
	SceUInt8 aid[];
} QmiUimRefreshEventTlv;

typedef struct QmiUimRefreshEventFiles {
	SceUInt8 fileCount[2];
	SceUInt8 files[];
} QmiUimRefreshEventFiles;

_Static_assert(sizeof(QmiUimRefreshEventTlv) == 7, "UIM refresh event prefix");
_Static_assert(sizeof(QmiUimRefreshEventFiles) == 2, "UIM refresh event file count");

_Static_assert(sizeof(QmiUimVerifyPinParameters) == 10, "UIM verify PIN prefix");
_Static_assert(sizeof(QmiUimPinRetryTlv) == 5, "UIM PIN retry response");

typedef struct QmiPdsTrackingSessionTlv {
	QmiTlvHeader header;
	SceUInt8 sessionOperation;
	SceUInt8 positionDataTimeout;
	SceUInt8 interval[4];
	SceUInt8 accuracyThreshold[4];
} QmiPdsTrackingSessionTlv;

typedef struct QmiPdsReportOption {
	SceUInt16 type;
	SceUInt16 reportMask;
} QmiPdsReportOption;

typedef struct QmiTrackingMessageResult {
	SceUInt16 messageId;
	SceUInt16 result;
} QmiTrackingMessageResult;

_Static_assert(sizeof(QmiPdsTrackingSessionTlv) == 13, "PDS tracking session TLV");
_Static_assert(sizeof(QmiPdsReportOption) == 4, "PDS report option");
_Static_assert(sizeof(QmiTrackingMessageResult) == 4, "tracking message result");

typedef struct QmiWordValueTlv {
	QmiTlvHeader header;
	SceUInt8 value[4];
} QmiWordValueTlv;

typedef struct QmiWmsDeleteParameters {
	QmiByteValueTlv storage;
	QmiWordValueTlv messageIndex;
} QmiWmsDeleteParameters;

_Static_assert(sizeof(QmiWmsDeleteParameters) == 11, "WMS delete parameters");

typedef struct QmiWmsMessageListEntry {
	SceUInt8 memoryIndex[4];
	SceUInt8 messageTag;
} QmiWmsMessageListEntry;

typedef struct QmiWmsMessageListTlv {
	QmiTlvHeader header;
	SceUInt8 count[4];
	QmiWmsMessageListEntry entries[];
} QmiWmsMessageListTlv;

_Static_assert(sizeof(QmiWmsMessageListEntry) == 5, "WMS message list entry");
_Static_assert(sizeof(QmiWmsMessageListTlv) == 7, "WMS message list prefix");

typedef struct QmiWmsRawMessageTlv {
	QmiTlvHeader header;
	SceUInt8 format;
	SceUInt8 lengthLow;
	SceUInt8 lengthHigh;
	SceUInt8 data[];
} QmiWmsRawMessageTlv;

_Static_assert(sizeof(QmiWmsRawMessageTlv) == 6, "WMS raw message prefix");

typedef struct QmiWmsMemoryIndexTlv {
	QmiTlvHeader header;
	SceUInt8 storageType;
	SceUInt8 memoryIndex[4];
} QmiWmsMemoryIndexTlv;

typedef struct QmiWmsModifyTagTlv {
	QmiWmsMemoryIndexTlv memory;
	SceUInt8 messageTag;
} QmiWmsModifyTagTlv;

typedef struct QmiWmsReadMessageTlv {
	QmiTlvHeader header;
	SceUInt8 messageTag;
	SceUInt8 format;
	SceUInt8 dataLength[2];
	SceUInt8 data[];
} QmiWmsReadMessageTlv;

_Static_assert(sizeof(QmiWmsMemoryIndexTlv) == 8, "WMS memory index parameter");
_Static_assert(sizeof(QmiWmsModifyTagTlv) == 9, "WMS modify tag parameter");
_Static_assert(sizeof(QmiWmsReadMessageTlv) == 7, "WMS read message prefix");

typedef struct QmiWdsBearerTlv {
	QmiTlvHeader header;
	SceUInt8 currentBearer;
	SceUInt8 bearerDetails[4];
} QmiWdsBearerTlv;

_Static_assert(sizeof(QmiWdsBearerTlv) == 8, "WDS bearer response");

_Static_assert(sizeof(QmiWordValueTlv) == 7, "QMI word parameter");

typedef struct QmiPdsXtraDataTlv {
	QmiTlvHeader header;
	SceUInt8 sequenceNumber;
	SceUInt8 totalLength[2];
	SceUInt8 sequenceLength[2];
	SceUInt8 data[];
} QmiPdsXtraDataTlv;

typedef struct QmiPdsAbsoluteTimeTlv {
	QmiTlvHeader header;
	SceUInt8 timestamp[8];
	SceUInt8 uncertainty[4];
	SceUInt8 timeBase;
	SceUInt8 forceAcceptance;
} QmiPdsAbsoluteTimeTlv;

typedef struct QmiPdsWifiPositionTlv {
	QmiTlvHeader header;
	SceUInt8 latitude[4];
	SceUInt8 longitude[4];
	SceUInt8 hepe[2];
	SceUInt8 apCount;
	SceUInt8 errorCode;
} QmiPdsWifiPositionTlv;

typedef struct QmiPdsWifiAccessPoint {
	SceUInt8 mac[6];
	SceUInt8 signalStrength[4];
	SceUInt8 beaconChannel[2];
	SceUInt8 flags;
} QmiPdsWifiAccessPoint;

typedef struct QmiPdsWifiAssistance {
	QmiPdsWifiPositionTlv position;
	QmiWordValueTlv time;
	QmiTlvHeader accessPointHeader;
	SceUInt8 accessPointCount;
	QmiPdsWifiAccessPoint accessPoints[];
} QmiPdsWifiAssistance;

_Static_assert(sizeof(QmiPdsXtraDataTlv) == 8, "PDS XTRA prefix");
_Static_assert(sizeof(QmiPdsAbsoluteTimeTlv) == 17, "PDS absolute time");
_Static_assert(sizeof(QmiPdsWifiPositionTlv) == 15, "PDS WiFi position");
_Static_assert(sizeof(QmiPdsWifiAccessPoint) == 13, "PDS WiFi access point");
_Static_assert(sizeof(QmiPdsWifiAssistance) == 26, "PDS WiFi assistance prefix");

typedef struct QmiIndexedValueTlv {
	QmiTlvHeader header;
	SceUInt8 value;
	SceUInt8 detailBytes[2];
} QmiIndexedValueTlv;

_Static_assert(sizeof(QmiIndexedResponse) == 16, "QMI indexed output stride");
_Static_assert(sizeof(QmiByteValueTlv) == 4, "QMI scalar parameter");
_Static_assert(sizeof(QmiIndexedValueTlv) == 6, "QMI indexed value parameter");

typedef struct QmiUmtsQos {
	SceUInt32 trafficClass;
	SceUInt32 maxUplinkBitrate;
	SceUInt32 maxDownlinkBitrate;
	SceUInt32 guaranteedUplinkBitrate;
	SceUInt32 guaranteedDownlinkBitrate;
	SceUInt32 deliveryOrder;
	SceUInt32 maximumSduSize;
	SceUInt32 sduErrorRatio;
	SceUInt32 residualBitErrorRatio;
	SceUInt32 deliveryErroneousSdu;
	SceUInt32 transferDelay;
	SceUInt32 trafficHandlingPriority;
} QmiUmtsQos;

typedef struct QmiLittleEndianWord {
	SceUInt16 low;
	SceUInt8 middle;
	SceUInt8 high;
} __attribute__((packed)) QmiLittleEndianWord;

typedef struct QmiUmtsQosTlv {
	QmiTlvHeader header;
	SceUInt8 trafficClass;
	QmiLittleEndianWord maxUplinkBitrate;
	QmiLittleEndianWord maxDownlinkBitrate;
	QmiLittleEndianWord guaranteedUplinkBitrate;
	QmiLittleEndianWord guaranteedDownlinkBitrate;
	SceUInt8 deliveryOrder;
	QmiLittleEndianWord maximumSduSize;
	SceUInt8 sduErrorRatio;
	SceUInt8 residualBitErrorRatio;
	SceUInt8 deliveryErroneousSdu;
	QmiLittleEndianWord transferDelay;
	QmiLittleEndianWord trafficHandlingPriority;
} QmiUmtsQosTlv;

_Static_assert(sizeof(QmiUmtsQos) == 48, "UMTS QoS public argument");
_Static_assert(sizeof(QmiUmtsQosTlv) == 36, "UMTS QoS wire parameter");

typedef struct QmiChannelRatesTlv {
	QmiTlvHeader header;
	SceUInt8 txRate[4];
	SceUInt8 rxRate[4];
	SceUInt8 maxTxRate[4];
	SceUInt8 maxRxRate[4];
} QmiChannelRatesTlv;

typedef struct QmiChannelRatesResponse {
	QmiMessageHeader message;
	QmiResultTlv result;
	QmiChannelRatesTlv rates;
} QmiChannelRatesResponse;

_Static_assert(sizeof(QmiChannelRatesTlv) == 19, "QMI channel rates parameter");
_Static_assert(sizeof(QmiChannelRatesResponse) == 30, "QMI channel rates response");

_Static_assert(sizeof(QmiResultResponse) == 11, "QMI result response prefix");
_Static_assert(sizeof(QmiCountedBytes) == 12, "QMI counted response bytes");

typedef struct QmiControlScalarRequest {
	QmiMessageHeader message;
	QmiTlvHeader parameter;
	SceUInt8 value;
} QmiControlScalarRequest;

_Static_assert(sizeof(QmiControlScalarRequest) == 8,
	"QMI scalar control request");

typedef struct QmiClientIdTlv {
	QmiTlvHeader header;
	SceUInt8 service;
	SceUInt8 clientId;
} QmiClientIdTlv;

typedef struct QmiControlClientRequest {
	QmiMessageHeader message;
	QmiClientIdTlv client;
} QmiControlClientRequest;

typedef struct QmiControlExtendedRequest {
	QmiControlScalarRequest primary;
	QmiTlvHeader forwarded;
	SceUInt8 forwardedLow;
	SceUInt8 forwardedHigh;
} QmiControlExtendedRequest;

typedef struct QmiControlArgumentValues {
	SceUInt8 forwardedByte;
	SceUInt8 service;
	SceUInt8 clientId;
	SceUInt8 selector;
	SceUInt16 forwardedValue;
} QmiControlArgumentValues;

_Static_assert(sizeof(QmiControlClientRequest) == 9, "QMI client control request");
_Static_assert(sizeof(QmiControlExtendedRequest) == 13, "QMI extended control request");
_Static_assert(sizeof(QmiControlArgumentValues) == 6, "QMI control argument values");

typedef struct QmiControlClientResponse {
	QmiControlWireHeader wire;
	QmiMessageHeader message;
	QmiResultTlv result;
	QmiClientIdTlv client;
} QmiControlClientResponse;

_Static_assert(sizeof(QmiMessageHeader) == 4, "QMI message header");
_Static_assert(sizeof(QmiTlvHeader) == 3, "QMI TLV header");
_Static_assert(sizeof(QmiResultTlv) == 7, "QMI result TLV");
_Static_assert(sizeof(QmiClientIdTlv) == 5, "QMI client ID TLV");
_Static_assert(sizeof(QmiControlClientResponse) == 24,
	"QMI control client response");

_Static_assert(sizeof(QmiControlWireHeader) == 8, "QMI control wire header");
_Static_assert(offsetof(QmiControlWirePacket, payload) == 8,
	"QMI control packet payload");
_Static_assert(offsetof(QmiServiceWirePacket, payload) == 9,
	"QMI service packet payload");

/* The reset path clears this complete length/header object in one operation. */
typedef struct QmiTransmitPacketHeader {
	SceUInt32 length;
	QmiTransmitHeader wire;
} QmiTransmitPacketHeader;

_Static_assert(sizeof(QmiTransmitPacketHeader) == 16,
	"QMI transmit packet header size");

typedef struct QmiReceivedHeader {
	SceUInt16 length;
	SceUInt8 senderFlags;
	SceUInt8 service;
	SceUInt8 clientId;
	SceUInt8 messageFlags;
	SceUInt16 transaction;
	SceUInt16 messageId;
	SceUInt8 controlService;
	SceUInt8 controlClientId;
} QmiReceivedHeader;

_Static_assert(sizeof(QmiReceivedHeader) == 12, "QMI received header size");

typedef struct QmiControlCommandOptions {
	SceUInt8 selector;
	SceUInt16 forwardedValue;
} QmiControlCommandOptions;

_Static_assert(sizeof(QmiControlCommandOptions) == 4,
	"QMI control command options size");

typedef struct QmiClientInstance {
	BbcmdProtocolClient *client;
	SceUInt8 reserved;
	SceUInt16 transaction;
	SceUInt32 state;
	SceUInt32 waitMode;
	QmiTransmitPacketHeader transmit;
	SceUInt32 retryTimer;
	SceUInt32 event;
	int (*callback)(SceUInt32 event, BbcmdProtocolClient *client);
	int (**activationCallback)(BbcmdRequest *request, int result);
	BbcmdRequest **activationRequest;
} QmiClientInstance;

struct QmiBackendControl {
	SceUInt32 state;
	SceUID thread;
	SceUID interfaceEvent;
	SceUID controlEvent;
	SceInt32 channelState[2];
	SceUID instanceMutex;
	SceUID fifoMutex;
	SceUInt16 transaction;
	SceUInt16 fifoInitialized;
	SceUInt8 reportedVersionByte1;
	SceUInt8 reportedVersionByte2;
	SceUInt16 reportedVersionWord1;
	SceUInt16 reportedVersionWord2;
	QmiClientInstance instances[12];
};

_Static_assert(sizeof(QmiTransmitHeader) == 9, "QMI transmit header size");
_Static_assert(sizeof(QmiClientInstance) == 52, "QMI client instance size");
_Static_assert(offsetof(QmiClientInstance, transmit) +
	offsetof(QmiTransmitPacketHeader, wire) == 20,
	"QMI instance transmit header");
_Static_assert(offsetof(QmiBackendControl, instances) == 44,
	"QMI driver instances");
_Static_assert(sizeof(QmiBackendControl) == 668, "QMI driver control size");

extern QmiBackendControl *g_qmiBackendControl;
extern QmiBackendControl g_qmiBackendStorage;
extern BbcmdProtocolClient g_qmiSecondaryControlClient;
extern BbcmdProtocolClient g_qmiPrimaryControlClient;
QmiBackendControl *qmiGetBackendControl(void);
int bbcmdDmsClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdConfiguredServiceClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdNasClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdWdsClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdCat2ClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdUimClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdWmsClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdPdsClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdQosClientCallback(unsigned int event, BbcmdProtocolClient *client);
int bbcmdAtClientCallback(unsigned int event, BbcmdProtocolClient *client);
unsigned int qmiGetConfiguredService(void);
int qmiSendClientRequest(BbcmdProtocolClient *client);
void qmiSetConfiguredService(unsigned int service);
int qmiCreateBackend(void);
int qmiDestroyBackend(void);
int qmiConnectTransport(unsigned int channel);
int qmiDisconnectTransport(unsigned int channel);
int qmiResetClientInstances(void);
QmiClientInstance *qmiGetClientInstance(unsigned int index);
int qmiCompleteClientInstance(QmiClientInstance *instance,
	unsigned int forwardedService, unsigned int packetType, int result);
int qmiControlClientCompleted(unsigned int event, BbcmdProtocolClient *client);
int qmiSendControlCommand(QmiClientInstance *instance, unsigned int command,
	const void *arguments);

/* qmi_backend.c */
int qmiInitializeBackend(void);
int qmiFinalizeBackend(void);
int qmiOpenTransport(unsigned int channel);
int qmiCloseTransport(unsigned int channel);
int qmiActivateChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request);
int qmiDeactivateChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request);
int qmiSuspendChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);
int qmiResumeChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);
int qmiAcquireClientInstance(BbcmdProtocolClient *client);
int qmiReleaseClientInstance(BbcmdProtocolClient *client);

/* qmi_channel.c */
int qmiStartProtocolChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request);
int qmiStopProtocolChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request);
int qmiPauseProtocolChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);
int qmiResumeProtocolChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);

/* qmi_control.c */
int qmiControlThread(SceSize args, void *argp);
int qmiWaitControlTransaction(QmiClientInstance *instance);

/* qmi_instance/qmi_instance.c */
int qmiLockBackend(unsigned int kind);
int qmiUnlockBackend(unsigned int kind, int locked);
int qmiGetClientInstance_internal(BbcmdProtocolClient *client);
int qmiReleaseClientInstanceInternal(BbcmdProtocolClient *client);
int qmiResetClientInstancesInternal(void);
int qmiLookupClientInstance(unsigned int channel, unsigned int service, unsigned int clientId);
int qmiFindActiveClientInstance(unsigned int clientId, unsigned int service, unsigned int channel);
int qmiInitializeControlClient(unsigned int channel);
void qmiClientRequestTimedOut(void *arg);

/* qmi_notify.c */
int qmiNotifyClientPacket(QmiClientInstance *instance, unsigned int packetType,
	const QmiReceivedHeader *header);

/* qmi_packet.c */
int qmiStoreReceivedClientPacket(unsigned int index,
	const QmiReceivedHeader *header, unsigned int packetType,
	const void *packet, unsigned int length);
int qmiMatchReceivedClient(unsigned int channel, const QmiReceivedHeader *header, int *packetType);
int qmiInitializeChannelClient(unsigned int channel);
int qmiResetChannelFifo(unsigned int channel);

/* qmi_prot_main.c */
int qmiInitializeDeviceDriver(void);
int qmiFinalizeDeviceDriver(void);
int qmiOpenDeviceChannel(unsigned int channel);
int qmiCloseDeviceChannel(unsigned int channel);
int qmiActivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request);
int qmiDeactivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request);
int qmiSuspendDeviceChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);
int qmiResumeDeviceChannel(unsigned int channel, void *forwardedValue1, void *forwardedValue2);
int qmiWakePacketTransmitter(void);

/* qmi_receive.c */
int qmiReceiveClientPacket(unsigned int channel, const void *packet, unsigned int length);

/* qmi_service.c */
int qmiClassifyService(unsigned int service);

/* qmi_service_parameters.c */
int bbcmdInitializeClients(BbcmdRequest *request);

/* qmi_signal.c */
int qmiWaitInterface(unsigned int kind, unsigned int channel, SceUInt32 timeout);
int qmiSignalBackend(unsigned int kind, unsigned int channel, unsigned int eventOffset);

/* qmi_solution.c */
int qmiReadCountedResponseBytes(const QmiResultResponse *response, QmiCountedBytes *output);
int bbcmdReadNetworkStartResponse(BbcmdRequest *request, BbcmdProtocolClient *client);
int bbcmdUpdateCompletedMaskForState(BbcmdRequest *request, int state);
int bbcmdSendEmptyDmsMessages(BbcmdRequest *request, QmiEmptyMessageList messageList);
int bbcmdInitializeProtocol(unsigned int command, BbcmdControl *control);
int bbcmdFinalizeProtocol(unsigned int command, BbcmdControl *control);
int bbcmdOpenChannel(unsigned int command, unsigned int channel, void *completionArg);
int bbcmdCloseChannel(unsigned int command, unsigned int channel, void *completionArg);
int bbcmdResetChannel(unsigned int command, unsigned int channel, BbcmdControl *control);

/* qmi_transmit.c */
int qmiSendClientRequestInternal(BbcmdProtocolClient *client);
int qmiBuildTransmitHeader(unsigned int service, QmiClientInstance *instance,
	QmiTransmitHeader *header, unsigned int *length, unsigned int payloadLength);
int qmiNotifyTransmittedPacket(unsigned int channel, const void *packet);
int qmiStartClientRequestTimer(unsigned int service, QmiClientInstance *instance);

/* uart_framing/lib_qmi_uart.c */
int qmiInitializeUartFraming(unsigned int format, int (*lock)(void *),
	int (*unlock)(void *), void *context,
	int (*send)(const void *, unsigned int), unsigned int (*transmitPending)(void));
int qmiFinalizeUartFraming(void);
int qmiSuspendUartFraming(void);
int qmiResumeUartFraming(void);
int qmiQueueUartFrame(const void *data, unsigned int length);
void qmiPumpUartFrames(void);
int qmiDecodeUartFrame(const unsigned char *data, unsigned int length,
	void **packet, unsigned int *packetLength);
int qmiIsUartFrameQueueEmpty(void);

#endif
