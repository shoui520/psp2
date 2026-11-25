/*
 * tel_sms_pdu_transcoder.h: Qualcomm AMSS WMS 3GPP TS 23.040 / 3GPP2 C.S0015
 * SMS PDU encoder/decoder and TelSmsPduGenerator / TelSmsPduParser classes
 * (3.65-CEX: 0x813ED8B8..0x813F3DBB).
 */
#ifndef TELEPHONY_SMS_PDU_TRANSCODER_H
#define TELEPHONY_SMS_PDU_TRANSCODER_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include "../common/tel_common.h"

namespace telephony {

/* Format identifiers used by LibWMS (WMS_FORMAT_*) */
enum WmsMessageFormat {
	WMS_FORMAT_CDMA = 0,
	WMS_FORMAT_ANALOG_CLI = 1,
	WMS_FORMAT_ANALOG_VOICE_MAIL = 2,
	WMS_FORMAT_ANALOG_SMS = 3,
	WMS_FORMAT_ANALOG_AWISMS = 4,
	WMS_FORMAT_MWI = 5,
	WMS_FORMAT_GW_PP = 6
};

/* 3GPP TS 23.040 TPDU types (WMS_TPDU_*) */
enum WmsTpduType {
	WMS_TPDU_DELIVER = 0,
	WMS_TPDU_DELIVER_REPORT_ACK = 1,
	WMS_TPDU_DELIVER_REPORT_ERROR = 2,
	WMS_TPDU_SUBMIT = 3,
	WMS_TPDU_SUBMIT_REPORT_ACK = 4,
	WMS_TPDU_SUBMIT_REPORT_ERROR = 5,
	WMS_TPDU_STATUS_REPORT = 6,
	WMS_TPDU_COMMAND = 7
};

/* WMS status / error codes returned by wmsTs* */
enum WmsStatusCode {
	WMS_OK = 0,
	WMS_ERR_UNSUPPORTED = 5,
	WMS_ERR_INVALID_FORMAT = 6,
	WMS_ERR_NULL_PTR = 8,
	WMS_ERR_INVALID_PARM_SIZE = 100,
	WMS_ERR_INVALID_USER_DATA_SIZE = 101,
	WMS_ERR_INVALID_PARM_VALUE = 102,
	WMS_ERR_INVALID_TPDU_TYPE = 200,
	WMS_ERR_INVALID_VALIDITY_FORMAT = 201
};

/* 3GPP address number mode / type / plan */
enum WmsDigitMode {
	WMS_DIGIT_MODE_4_BIT = 0,
	WMS_DIGIT_MODE_8_BIT = 1
};

enum WmsNumberType {
	WMS_NUMBER_UNKNOWN = 0,
	WMS_NUMBER_INTERNATIONAL = 1,
	WMS_NUMBER_NATIONAL = 2,
	WMS_NUMBER_NETWORK_SPECIFIC = 3,
	WMS_NUMBER_SUBSCRIBER = 4,
	WMS_NUMBER_ALPHANUMERIC = 5,
	WMS_NUMBER_ABBREVIATED = 6
};

/* 3GPP TS 23.038 Data Coding Scheme fields */
enum WmsAlphabet {
	WMS_ALPHABET_DEFAULT_7BIT = 0,
	WMS_ALPHABET_8BIT = 1,
	WMS_ALPHABET_UCS2 = 2
};

enum WmsMsgWaitingKind {
	WMS_MW_NONE = 0,
	WMS_MW_DISCARD = 1,
	WMS_MW_STORE = 2,
	WMS_MW_DATA_CODING = 3
};

enum WmsValidityFormat {
	WMS_VALIDITY_NONE = 0,
	WMS_VALIDITY_ENHANCED = 1,
	WMS_VALIDITY_RELATIVE = 2,
	WMS_VALIDITY_ABSOLUTE = 3
};

/* 3GPP2 C.S0015 CDMA bearer data encoding */
enum WmsCdmaEncoding {
	WMS_CDMA_ENC_OCTET = 0,
	WMS_CDMA_ENC_IS91EP = 1,
	WMS_CDMA_ENC_ASCII = 2,
	WMS_CDMA_ENC_IA5 = 3,
	WMS_CDMA_ENC_UNICODE = 4,
	WMS_CDMA_ENC_GSM7 = 9
};

enum WmsIs91EpType {
	WMS_IS91EP_VOICE_MAIL = 130,
	WMS_IS91EP_SHORT_MESSAGE_FULL = 131,
	WMS_IS91EP_CLI_ORDER = 132,
	WMS_IS91EP_SHORT_MESSAGE = 133
};

/* User Data Header (UDH) Information Element IDs (3GPP TS 23.040 section 9.2.3.24) */
enum WmsUdhId {
	WMS_UDH_CONCAT_8 = 0x00,
	WMS_UDH_SPECIAL_SM = 0x01,
	WMS_UDH_PORT_8 = 0x04,
	WMS_UDH_PORT_16 = 0x05,
	WMS_UDH_CONCAT_16 = 0x08,
	WMS_UDH_TEXT_FORMATTING = 0x0A,
	WMS_UDH_PRE_DEF_SOUND = 0x0B,
	WMS_UDH_USER_DEF_SOUND = 0x0C,
	WMS_UDH_PRE_DEF_ANIM = 0x0D,
	WMS_UDH_LARGE_ANIM = 0x0E,
	WMS_UDH_SMALL_ANIM = 0x0F,
	WMS_UDH_LARGE_PICTURE = 0x10,
	WMS_UDH_SMALL_PICTURE = 0x11,
	WMS_UDH_VAR_PICTURE = 0x12,
	WMS_UDH_USER_PROMPT = 0x13,
	WMS_UDH_EXT_OBJECT = 0x14,
	WMS_UDH_RFC822 = 0x20,
	WMS_UDH_NAT_LANG_SINGLE_SHIFT = 0x24,
	WMS_UDH_NAT_LANG_LOCKING_SHIFT = 0x25
};

enum {
	WMS_MAX_ADDRESS_DIGITS = 48,
	WMS_MAX_UDH_HEADERS = 7,
	WMS_MAX_GW_USER_DATA = 255,
	WMS_MAX_CDMA_USER_DATA = 229,
	WMS_MAX_PDU_BYTES = 255,
	WMS_MAX_SUBADDR_BYTES = 48,
	WMS_MAX_BC_CATEGORIES = 5
};

struct WmsAddress {
	int digitMode;
	int numberMode;
	int numberType;
	int numberPlan;
	SceUInt8 digitCount;
	SceUInt8 digits[WMS_MAX_ADDRESS_DIGITS];
};

struct WmsDcs {
	int messageClass;	/* 0..3 or 4 = none */
	bool isCompressed;
	int alphabet;		/* WmsAlphabet */
	int msgWaitingKind;	/* WmsMsgWaitingKind */
	bool msgWaitingActive;
	int msgWaitingType;
	SceUInt8 rawDcs;
};

struct WmsTimestamp {
	SceUInt8 year;
	SceUInt8 month;
	SceUInt8 day;
	SceUInt8 hour;
	SceUInt8 minute;
	SceUInt8 second;
	SceInt8 timezone;	/* quarter-hours, -48..+48 */
};

struct WmsValidity {
	int format;		/* WmsValidityFormat */
	WmsTimestamp time;
};

struct WmsUdhConcat8 {
	SceUInt8 referenceNumber;
	SceUInt8 totalSegments;
	SceUInt8 sequenceNumber;
};

struct WmsUdhConcat16 {
	SceUInt16 referenceNumber;
	SceUInt8 totalSegments;
	SceUInt8 sequenceNumber;
};

struct WmsUdhSpecialSm {
	int msgWaitingStore;	/* 1 = discard, 2 = store */
	int msgWaitingType;
	SceUInt8 messageCount;
};

struct WmsUdhPort8 {
	SceUInt8 destPort;
	SceUInt8 origPort;
};

struct WmsUdhPort16 {
	SceUInt16 destPort;
	SceUInt16 origPort;
};

struct WmsUdhTextFormatting {
	SceUInt8 startPosition;
	SceUInt8 length;
	int alignment;
	int fontSize;
	bool bold;
	bool italic;
	bool underlined;
	bool strikethrough;
	bool hasColor;
	int foregroundColor;
	int backgroundColor;
};

struct WmsUdhUserDefSound {
	SceUInt8 length;
	SceUInt8 position;
	SceUInt8 melody[128];
};

struct WmsUdhPreDefMedia {
	SceUInt8 position;
	SceUInt8 mediaIndex;
};

struct WmsUdhLargeAnim {
	SceUInt8 position;
	SceUInt8 frames[4][32];
};

struct WmsUdhSmallAnim {
	SceUInt8 position;
	SceUInt8 frames[4][8];
};

struct WmsUdhLargePicture {
	SceUInt8 position;
	SceUInt8 bitmap[128];
};

struct WmsUdhSmallPicture {
	SceUInt8 position;
	SceUInt8 bitmap[32];
};

struct WmsUdhVarPicture {
	SceUInt8 position;
	SceUInt8 width;
	SceUInt8 height;
	SceUInt8 bitmap[134];
};

struct WmsUdhExtObject {
	SceUInt8 dataLength;
	SceUInt8 payload[131];
	bool hasHeader;
	SceUInt8 referenceNumber;
	SceUInt16 totalLength;
	SceUInt8 controlByte;
	int objectType;
	SceUInt16 position;
};

struct WmsUdhOther {
	int iei;
	SceUInt8 length;
	SceUInt8 payload[226];
};

struct WmsUdhHeader {
	int headerId;		/* WmsUdhId */
	union {
		WmsUdhConcat8 concat8;
		WmsUdhConcat16 concat16;
		WmsUdhSpecialSm specialSm;
		WmsUdhPort8 port8;
		WmsUdhPort16 port16;
		WmsUdhTextFormatting textFormat;
		WmsUdhPreDefMedia preDefSound;
		WmsUdhUserDefSound userDefSound;
		WmsUdhPreDefMedia preDefAnim;
		WmsUdhLargeAnim largeAnim;
		WmsUdhSmallAnim smallAnim;
		WmsUdhLargePicture largePic;
		WmsUdhSmallPicture smallPic;
		WmsUdhVarPicture varPic;
		SceUInt8 userPromptCount;
		WmsUdhExtObject extObject;
		SceUInt8 rfc822HeaderLen;
		int natLangShiftTable;
		WmsUdhOther other;
	} u;
};

struct WmsGwUserData {
	SceUInt8 headerCount;
	WmsUdhHeader headers[WMS_MAX_UDH_HEADERS];
	SceUInt16 dataLength;
	SceUInt8 bytes[WMS_MAX_GW_USER_DATA];
};

struct WmsGwDeliver {
	bool moreMessages;
	bool replyPath;
	bool userDataHeaderPresent;
	bool statusReportIndication;
	WmsAddress address;
	int protocolId;
	WmsDcs dcs;
	WmsTimestamp timestamp;
	WmsGwUserData userData;
};

struct WmsGwDeliverReportAck {
	bool userDataHeaderPresent;
	int mask;
	int protocolId;
	WmsDcs dcs;
	WmsGwUserData userData;
};

struct WmsGwDeliverReportError {
	bool userDataHeaderPresent;
	int tpFailureCause;
	int mask;
	int protocolId;
	WmsDcs dcs;
	WmsGwUserData userData;
};

struct WmsGwSubmit {
	bool rejectDuplicates;
	bool replyPath;
	bool userDataHeaderPresent;
	bool statusReportRequest;
	int messageReference;
	WmsAddress address;
	int protocolId;
	WmsDcs dcs;
	WmsValidity validity;
	WmsGwUserData userData;
};

struct WmsGwSubmitReportAck {
	bool userDataHeaderPresent;
	WmsTimestamp timestamp;
	int mask;
	int protocolId;
	WmsDcs dcs;
	WmsGwUserData userData;
};

struct WmsGwSubmitReportError {
	bool userDataHeaderPresent;
	int tpFailureCause;
	WmsTimestamp timestamp;
	int mask;
	int protocolId;
	WmsDcs dcs;
	WmsGwUserData userData;
};

struct WmsGwStatusReport {
	bool userDataHeaderPresent;
	bool moreMessages;
	bool statusReportQualifier;
	int messageReference;
	WmsAddress address;
	WmsTimestamp timestamp;
	WmsTimestamp dischargeTime;
	int tpStatus;
	int mask;
	int protocolId;
	WmsDcs dcs;
	WmsGwUserData userData;
};

struct WmsGwCommand {
	bool userDataHeaderPresent;
	bool statusReportRequest;
	int messageReference;
	int protocolId;
	int commandType;
	int messageNumber;
	WmsAddress address;
	SceUInt8 commandDataLen;
	SceUInt8 commandData[157];
};

struct WmsGwTpdu {
	int tpduType;		/* WmsTpduType */
	union {
		WmsGwDeliver deliver;
		WmsGwDeliverReportAck deliverReportAck;
		WmsGwDeliverReportError deliverReportError;
		WmsGwSubmit submit;
		WmsGwSubmitReportAck submitReportAck;
		WmsGwSubmitReportError submitReportError;
		WmsGwStatusReport statusReport;
		WmsGwCommand command;
	} u;
};

struct WmsCdmaMessageId {
	int type;		/* 1 = deliver, 2 = submit, ... */
	int idNumber;
	bool udhPresent;
};

struct WmsCdmaUserData {
	SceUInt8 headerCount;
	WmsUdhHeader headers[WMS_MAX_UDH_HEADERS];
	int encoding;		/* WmsCdmaEncoding */
	int is91epType;		/* WmsIs91EpType */
	SceUInt8 dataLength;
	SceUInt8 paddingBits;
	SceUInt8 bytes[WMS_MAX_CDMA_USER_DATA];
	SceUInt8 numCharacters;
};

struct WmsCdmaReplyOption {
	bool userAckRequested;
	bool deliveryAckRequested;
	bool readAckRequested;
};

struct WmsCdmaDeliveryStatus {
	int errorClass;
	int statusCode;
};

struct WmsCdmaScptCategory {
	int operationCode;
	int category;
	int language;
	SceUInt8 maxMessages;
	int alertOption;
	SceUInt8 labelLength;
	char label[30];
};

struct WmsCdmaScptData {
	int encoding;
	SceUInt8 categoryCount;
	WmsCdmaScptCategory categories[WMS_MAX_BC_CATEGORIES];
};

struct WmsCdmaScptResultEntry {
	int category;
	int language;
	int status;
};

struct WmsCdmaScptResult {
	SceUInt8 categoryCount;
	WmsCdmaScptResultEntry entries[WMS_MAX_BC_CATEGORIES];
};

struct WmsCdmaBearerData {
	SceUInt32 mask;
	WmsCdmaMessageId messageId;
	WmsCdmaUserData userData;
	SceUInt8 userResponseCode;
	WmsTimestamp mcTime;
	WmsTimestamp validityAbsolute;
	WmsTimestamp validityRelative;
	WmsTimestamp deferredAbsolute;
	WmsTimestamp deferredRelative;
	int priority;
	int privacy;
	WmsCdmaReplyOption replyOption;
	SceUInt8 numMessages;
	int alertMode;
	int language;
	WmsAddress callback;
	int displayMode;
	WmsCdmaDeliveryStatus deliveryStatus;
	int depositIndex;
	WmsCdmaScptData *scptData;
	WmsCdmaScptResult *scptResult;
	SceUInt8 otherDataMaxLen;
	SceUInt8 otherDataLen;
	SceUInt8 *otherData;
};

struct WmsClientTsData {
	int format;		/* WmsMessageFormat */
	union {
		WmsGwTpdu gwPp;
		WmsCdmaBearerData cdma;
	} u;
};

struct WmsRawTsData {
	int format;		/* WmsMessageFormat */
	int tpduType;		/* WmsTpduType */
	SceSize length;
	SceUInt8 bytes[WMS_MAX_PDU_BYTES + 1];
};

/* LibWMS low-level C API (0x813EDED2..0x813F2A9F, 0x813F3AC4..0x813F3DBB) */
int wmsTsBcdToAscii(const SceUInt8 *bcdDigits, int count, char *outAscii);
int wmsTsIsValidAsciiDigits(const char *digits, int count);
int wmsTsPackGsm7(const SceUInt8 *septets, SceUInt16 septetCount, SceUInt16 shiftBits,
		  SceUInt16 maxOctets, SceUInt8 *outOctets);
int wmsTsUnpackGsm7(const SceUInt8 *octets, SceUInt8 septetCount, SceUInt8 maxSeptets,
		    SceInt16 shiftBits, SceUInt8 *outSeptets);
int wmsTsEncodeAddress(const WmsAddress *address, SceUInt8 *outBytes);
int wmsTsEncodeScAddress(const WmsAddress *address, SceUInt8 *outBytes);
int wmsTsDecodeAddress(const SceUInt8 *bytes, WmsAddress *outAddress);
int wmsTsDecodeScAddress(const SceUInt8 *bytes, WmsAddress *outAddress);
int wmsTsEncode(const WmsClientTsData *clientData, WmsRawTsData *outRawData);
int wmsTsDecode(const WmsRawTsData *rawData, WmsClientTsData *outClientData);

void wmsBitPackByte(SceUInt8 value, SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength);
void wmsBitPackWord(SceUInt16 value, SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength);
SceUInt8 wmsBitUnpackByte(const SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength);
SceUInt16 wmsBitUnpackWord(const SceUInt8 *buffer, SceUInt16 bitOffset, SceUInt16 bitLength);

/* Transcoder mode selector */
enum TelSmsTranscoderMode {
	TEL_SMS_TRANSCODER_GSM = 0,
	TEL_SMS_TRANSCODER_CDMA = 1
};

enum TelSmsTranscoderFactoryType {
	TEL_SMS_FACTORY_NONE = 0,
	TEL_SMS_FACTORY_GSM = 1,
	TEL_SMS_FACTORY_CDMA = 2
};

/* Generates GSM/WCDMA or CDMA SMS PDUs from TelSmsMessage (0x813F2AA0..0x813F3057) */
class TelSmsPduGenerator {
public:
	TelSmsPduGenerator();
	virtual ~TelSmsPduGenerator() {}

	virtual TelephonyResult Initialize(int mode);
	virtual TelephonyResult Generate(const TelSmsMessage &message,
					 TelSmsProtocolDataUnit *outPdu);

private:
	TelephonyResult BuildCdmaSubmit(const TelSmsMessage &message, WmsClientTsData *outTsData);
	TelephonyResult BuildGsmAddress(const TelSmsMessage &message, WmsClientTsData *outTsData);
	TelephonyResult BuildGsmUserData(const TelSmsMessage &message, WmsClientTsData *outTsData);

	int m_wmsFormat;
	SceUInt16 m_cdmaMessageId;
};

/* Parses GSM/WCDMA or CDMA SMS PDUs into TelSmsMessage (0x813F3058..0x813F3AC3) */
class TelSmsPduParser {
public:
	TelSmsPduParser();
	virtual ~TelSmsPduParser() {}

	virtual TelephonyResult Initialize(int mode);
	virtual TelephonyResult Parse(const TelSmsProtocolDataUnit &pdu,
				      TelSmsMessage *outMessage);

private:
	TelephonyResult ExtractGsmTpdu(const TelSmsProtocolDataUnit &pdu,
				       WmsRawTsData *outRawData) const;
	TelephonyResult ValidateGsmDeliver(const WmsClientTsData &tsData) const;
	TelephonyResult ParseGsmAddress(const WmsClientTsData &tsData,
					TelSmsMessage *outMessage) const;
	TelephonyResult ParseGsmDeliver(const WmsClientTsData &tsData,
					TelSmsMessage *outMessage) const;
	TelephonyResult ParseCdmaDeliver(const WmsClientTsData &tsData,
					 TelSmsMessage *outMessage) const;

	int m_wmsFormat;
};

/* Factories and loopback Submit->Deliver converter (0x813ED8B8..0x813EDD60) */
class TelSmsPduGeneratorFactory {
public:
	static TelephonyResult Create(int factoryType, TelSmsPduGenerator **outGenerator);
};

class TelSmsPduParserFactory {
public:
	static TelephonyResult Create(int factoryType, TelSmsPduParser **outParser);
};

TelephonyResult telConvertSubmitPduToLoopbackDeliver(const TelPhoneNumber &senderNumber,
						     TelSmsProtocolDataUnit *inOutPdu,
						     TelPhoneNumber *outDestinationNumber);

} /* namespace telephony */

#endif /* TELEPHONY_SMS_PDU_TRANSCODER_H */
