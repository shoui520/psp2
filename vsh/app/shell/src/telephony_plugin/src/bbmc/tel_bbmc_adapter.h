/*
 * tel_bbmc_adapter.h: BBMC modem adapter, event handlers, subscriber thread,
 * shared state, diagnostic binary logger, and loopback simulator for
 * telephony_plugin (3.65-CEX: 0x813CB470..0x813DBD13).
 */
#ifndef TELEPHONY_BBMC_ADAPTER_H
#define TELEPHONY_BBMC_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <thread/thread.h>
#include "../common/tel_common.h"

/* User-mode BBMC driver structures (matching sys/internal/driver/src/bbmc/) */
struct SceBbmcDriverInfo {
	SceInt32 currentState;
	SceInt32 targetState;
	SceUInt32 firstBootReply;
	SceUInt32 secondBootReply;
};

struct SceBbmcRejectCause {
	SceUInt32 domain;
	SceUInt32 cause;
};

struct SceBbmcFailureInfo {
	SceUInt32 category;
	SceUInt32 stage;
	SceUInt32 detailCode;
	SceUInt32 errorCode;
};

struct SceBbmcRoamingIndicator {
	SceUInt32 radioInterface;
	SceUInt32 roamingIndicator;
};

struct SceBbmcServingSystemInfo {
	SceUInt8 validFlags;
	SceUInt32 registrationState;
	SceUInt32 csAttachState;
	SceUInt32 psAttachState;
	SceUInt32 selectedNetwork;
	SceUInt32 radioInterfaces[8];
	SceUInt32 roamingIndicator;
	SceUInt32 dataCapabilities[8];
	SceUInt16 mcc;
	SceUInt16 mnc;
	char description[68];
	SceUInt8 descriptionLength;
	SceUInt8 radioInterfaceCount;
	SceUInt8 dataCapabilityCount;
	SceUInt16 cdmaSystemId;
	SceUInt16 cdmaNetworkId;
	SceUInt16 baseStationId;
	SceInt32 baseStationLatitude;
	SceInt32 baseStationLongitude;
	SceUInt8 roamingIndicatorCount;
	SceUInt8 leapSeconds;
	SceInt8 localTimeOffset;
	SceUInt8 cdmaProtocolRevision;
	SceBbmcRoamingIndicator roamingIndicators[8];
	SceUInt32 defaultRoamingIndicator;
	SceUInt32 daylightSavingTime;
};

struct SceBbmcSignalStrength {
	SceInt32 level;
	SceUInt32 radioInterface;
	SceInt8 strength;
};

struct SceBbmcRadioSignalValue {
	SceUInt32 radioInterface;
	SceUInt8 value;
};

struct SceBbmcSignalStrengthInfo {
	SceUInt32 validMask;
	SceUInt32 radioInterface;
	SceInt8 strength;
	SceUInt8 reserved[3];
	SceUInt16 strengthCount;
	SceBbmcSignalStrength strengths[4];
	SceUInt16 rssiCount;
	SceBbmcSignalStrength rssi[4];
	SceUInt16 ecioCount;
	SceBbmcRadioSignalValue ecio[4];
	SceUInt16 errorRateCount;
	SceBbmcRadioSignalValue errorRates[4];
	SceInt32 io;
	SceUInt8 sinr;
	SceUInt8 rsrq;
};

struct SceBbmcNetworkInfo {
	SceInt32 signalLevel;
	SceBbmcServingSystemInfo serving;
	SceBbmcSignalStrengthInfo signal;
};

struct SceBbmcHomeNetworkInfo {
	SceUInt16 mcc;
	SceUInt16 mnc;
	char description[68];
};

struct SceBbmcDeviceInformation {
	char manufacturer[33];
	SceUInt8 reservedManufacturer[3];
	char model[33];
	SceUInt8 reservedModel[3];
	char revision[101];
	SceUInt8 reservedRevision[3];
	char msisdn[33];
	SceUInt8 reservedMsisdn[3];
	char esn[33];
	SceUInt8 reservedEsn[3];
	char imei[32];
	char meid[33];
	SceUInt8 reservedMeid[3];
	char iccid[21];
	SceUInt8 reservedIccid[3];
	SceUInt8 messageProtocol;
	SceUInt8 reservedProtocol;
	SceUInt16 prlVersion;
	char imsi[16];
	SceUInt32 imeiSoftwareVersion;
	SceBbmcHomeNetworkInfo homeNetwork;
};

struct SceBbmcUmtsQos {
	SceUInt32 trafficClass;
	SceUInt32 maxUplinkBitrate;
	SceUInt32 maxDownlinkBitrate;
	SceUInt32 guaranteedUplinkBitrate;
	SceUInt32 guaranteedDownlinkBitrate;
	SceUInt32 deliveryOrder;
	SceUInt32 maxSduSize;
	SceUInt32 sduErrorRatio;
	SceUInt32 residualBitErrorRatio;
	SceUInt32 erroneousSduDelivery;
	SceUInt32 transferDelay;
	SceUInt32 handlingPriority;
};

struct SceBbmcGprsQos {
	SceUInt32 precedenceClass;
	SceUInt32 delayClass;
	SceUInt32 reliabilityClass;
	SceUInt32 peakThroughputClass;
	SceUInt32 meanThroughputClass;
};

struct SceBbmcProfileSettings {
	SceUInt32 validFields;
	char profileName[15];
	SceUInt8 profileNameLength;
	SceUInt32 pdpType;
	char apn[100];
	SceUInt32 apnLength;
	SceUInt32 preferredDnsAddress;
	SceUInt32 alternateDnsAddress;
	SceUInt32 papAuthentication;
	SceUInt32 chapAuthentication;
	SceUInt32 pcscfAddressUsingPco;
	SceUInt32 ipv4Address;
	SceBbmcUmtsQos requestedUmtsQos;
	SceBbmcUmtsQos minimumUmtsQos;
	SceBbmcGprsQos requestedGprsQos;
	SceBbmcGprsQos minimumGprsQos;
	char username[128];
	SceUInt32 usernameLength;
	char password[128];
	SceUInt32 passwordLength;
};

struct SceBbmcScannedNetwork {
	SceUInt16 mcc;
	SceUInt16 mnc;
	SceUInt8 descriptionLength;
	SceUInt8 reserved[3];
	char description[68];
	SceUInt32 forbiddenStatus;
	SceUInt32 preferredStatus;
	SceUInt32 availabilityStatus;
	SceUInt32 currentStatus;
	SceUInt32 radioInterface;
};

struct SceBbmcNetworkScanInfo {
	SceUInt16 count;
	SceUInt16 reserved;
	SceBbmcScannedNetwork networks[10];
};

struct SceBbmcNetworkRegistration {
	SceUInt32 action;
	SceUInt32 radioInterface;
	SceUInt16 mcc;
	SceUInt16 mnc;
};

struct SceBbmcRssiThresholdList {
	SceUInt8 count;
	SceInt8 thresholds[11];
};

struct SceBbmcPinProtection {
	SceUInt8 pinId;
	SceUInt8 protectionEnabled;
	SceUInt8 pinLength;
	SceUInt8 reserved;
	char pin[12];
};

struct SceBbmcPinChange {
	SceUInt8 pinId;
	SceUInt8 oldPinLength;
	SceUInt8 newPinLength;
	SceUInt8 reserved;
	char oldPin[12];
	char newPin[12];
};

struct SceBbmcUnblockPinParam {
	char puk[36];
	char pin[12];
	SceUInt8 reserved;
	SceUInt8 pukLength;
	SceUInt8 pinLength;
	SceUInt8 reservedTail;
};

struct SceBbmcControlKeyParam {
	SceUInt32 featureId;
	SceUInt8 controlKeyLength;
	SceUInt8 reserved[3];
	char controlKey[20];
};

struct SceBbmcControlKeyStatus {
	SceUInt32 featureId;
	SceUInt32 state;
	SceUInt32 blockType;
	SceUInt8 verifyRetries;
	SceUInt8 unblockRetries;
	SceUInt16 reserved;
};

struct SceBbmcRefreshedFile {
	SceUInt16 fileId;
	SceUInt8 pathLength;
	SceUInt8 reserved;
	SceUInt16 path[2];
	SceUInt32 fileKind;
};

struct SceBbmcFileRefreshDetail {
	SceUInt32 stage;
	SceUInt32 mode;
	SceUInt32 sessionType;
	SceUInt8 aidLength;
	SceUInt8 reserved;
	SceUInt16 fileCount;
	SceUInt8 aid[16];
	SceUInt8 reservedAid[4];
	SceBbmcRefreshedFile files[15];
};

struct SceBbmcCallEndReason {
	SceUInt16 type;
	SceUInt16 reason;
};

struct SceBbmcBearerTech {
	SceUInt32 networkType;
	SceUInt32 techMask;
};

/* User-mode SceBbmc syscalls (matching sys/internal/driver/src/bbmc/bbmc.c) */
extern "C" {
int sceBbmcGetNetworkInfo(SceBbmcNetworkInfo *info);
int sceBbmcSubscribeFeatureId(SceUInt32 featureId, SceUInt32 eventMask, SceUID callback);
int sceBbmcUnsubscribeFeature(int subscriberId);
int sceBbmcClearEvent(int subscriberId, SceUInt32 eventMask);
int sceBbmcResumeSubscriberCallback(int subscriberId);
int sceBbmcTurnOn(int subscriberId);
int sceBbmcTurnOff(int subscriberId);
int sceBbmcGetPinStatus(int subscriberId, SceUInt32 *status, SceUInt16 *retries);
int sceBbmcUnblockPin(int subscriberId, const SceBbmcUnblockPinParam *parameters);
int sceBbmcVerifyPin(int subscriberId, const void *pin, SceSize length);
int sceBbmcGetDriverInfo(int subscriberId, SceBbmcDriverInfo *info);
int sceBbmcReserveOperationMode(int subscriberId, SceUInt32 operationMode);
int sceBbmcSetLPMMode(int subscriberId, int mode);
int sceBbmcSMSGetListStart(int subscriberId);
int sceBbmcSMSGetListDone(int subscriberId, SceUInt32 *count, SceUInt32 *indices,
			  SceUInt32 capacity);
int sceBbmcSMSReadStart(int subscriberId, SceUInt32 memoryIndex);
int sceBbmcSMSReadDone(int subscriberId, void *buffer, SceUInt32 *format);
int sceBbmcSMSReadAck(int subscriberId, SceUInt32 memoryIndex, SceUInt32 deleteMessage);
int sceBbmcSMSSend(int subscriberId, const void *buffer, SceSize length, SceUInt32 format);
int sceBbmcSetupSetValue(int subscriberId, SceUInt32 kind, const void *input, SceSize size);
int sceBbmcSetupGetValueStart(int subscriberId, SceUInt32 kind);
int sceBbmcSetupGetValueDone(int subscriberId, SceUInt32 kind, void *output, SceSize size);
int sceBbmcNetworkScanStart(int subscriberId);
int sceBbmcNetworkScanDone(int subscriberId, SceBbmcNetworkScanInfo *scan);
int sceBbmcNetworkScanAbort(int subscriberId);
int sceBbmcNetGetRejectCause(int subscriberId, SceBbmcRejectCause *cause);
int sceBbmcPacketReleasePDPContext(int subscriberId);
int sceBbmcPacketGetLastCallEndReason(int subscriberId, SceBbmcCallEndReason *reason);
int sceBbmcPacketIfSetControlFlag(int subscriberId, SceUInt32 flags);
int sceBbmcPacketIfClearControlFlag(int subscriberId, SceUInt32 flags);
int sceBbmcPacketIfGetControlFlag(int subscriberId, SceUInt32 *flags);
int sceBbmcPacketGetCurrentBearerTech(int subscriberId, SceBbmcBearerTech *bearer);
int sceBbmcGetFailureInfo(SceBbmcFailureInfo *failure);
int sceBbmcDLUpdaterGetArchiveVersion(int subscriberId, SceUInt32 *version);
int sceBbmcDLUpdaterGetLatestArchiveVersion(int subscriberId, SceUInt32 *version);
int sceBbmcDLUpdaterCheckComVersion(int subscriberId, SceUInt32 *matches);
int sceBbmcGetFileRefreshDetail(int subscriberId, SceBbmcFileRefreshDetail *detail);
int sceBbmcFileRefreshResponse(int subscriberId, SceUInt32 response);
int sceBbmcDepersonalizeCK(int subscriberId, const SceBbmcControlKeyParam *param);
int sceBbmcUnblockCK(int subscriberId, const SceBbmcControlKeyParam *param);
int sceBbmcGetCKStatus(int subscriberId, SceUInt32 featureId, SceBbmcControlKeyStatus *status);
int sceBbmcSimGetPLMNModeBit(int subscriberId, SceUInt32 *value);
int sceBbmcIsAbleToRunUnderCurrentTemp(int subscriberId);
int sceBbmcGetComFirmVersion(int subscriberId, char *revision, SceSize size);
int sceBbmcUsbExtIfStart(int mode);
int sceBbmcUsbExtIfStop(void);
int sceBbmcDebugSelectOutMedia(int channel, int media);
int sceBbmcDebugOutString(int channel, const char *text, SceSize length);
} /* extern "C" */

namespace telephony {

class TelSmsPduGenerator;

/* BBMC attribute IDs stored in TelBbmcSharedData / TelAttributeStore (524289..524331) */
enum TelBbmcAttributeId {
	TEL_ATTR_BBMC_MODEM_READY = 524289,
	TEL_ATTR_BBMC_PIN_VALID = 524290,
	TEL_ATTR_BBMC_CK_PERMANENT_BLOCKED = 524291,
	TEL_ATTR_BBMC_SIM_BOOT_MISSING = 524292,
	TEL_ATTR_BBMC_MODEM_BOOTED = 524293,
	TEL_ATTR_BBMC_SIM_INVALID = 524294,
	TEL_ATTR_BBMC_DEVINFO_VALID = 524295,
	TEL_ATTR_BBMC_DRIVER_STATE = 524296,
	TEL_ATTR_BBMC_TARGET_STATE = 524297,
	TEL_ATTR_BBMC_PIN_STATUS = 524298,
	TEL_ATTR_BBMC_PIN_ENABLED_STATE = 524299,
	TEL_ATTR_BBMC_SIM_STATE = 524300,
	TEL_ATTR_BBMC_CK_STATE = 524301,
	TEL_ATTR_BBMC_CK_BLOCK_TYPE = 524302,
	TEL_ATTR_BBMC_LAST_SIM_EVENT = 524303,
	TEL_ATTR_BBMC_TEMP_STATE = 524304,
	TEL_ATTR_BBMC_VOLTAGE_STATE = 524305,
	TEL_ATTR_BBMC_LPM_OVERRIDE = 524306,
	TEL_ATTR_BBMC_CK_EVENT_COUNT = 524307,
	TEL_ATTR_BBMC_PIN_RETRIES = 524308,
	TEL_ATTR_BBMC_SIM_ENABLE_COUNT = 524309,
	TEL_ATTR_BBMC_SIM_DISABLE_COUNT = 524310,
	TEL_ATTR_BBMC_BOOT_COUNT = 524311,
	TEL_ATTR_BBMC_FAILURE_COUNT = 524312,
	TEL_ATTR_BBMC_REVISION = 524313,
	TEL_ATTR_BBMC_HOME_MCC = 524314,
	TEL_ATTR_BBMC_HOME_MNC = 524315,
	TEL_ATTR_BBMC_HOME_DESC = 524316,
	TEL_ATTR_BBMC_IMSI = 524317,
	TEL_ATTR_BBMC_MSISDN = 524318,
	TEL_ATTR_BBMC_IMEI = 524319,
	TEL_ATTR_BBMC_ICCID = 524320,
	TEL_ATTR_BBMC_MANUFACTURER = 524321,
	TEL_ATTR_BBMC_MODEL = 524322,
	TEL_ATTR_BBMC_ESN = 524323,
	TEL_ATTR_BBMC_MEID = 524324,
	TEL_ATTR_BBMC_PROTOCOL = 524325,
	TEL_ATTR_BBMC_PRL_VERSION = 524326,
	TEL_ATTR_BBMC_SW_VERSION = 524327,
	TEL_ATTR_BBMC_SYSINFO_EXTRA = 524328,
	TEL_ATTR_BBMC_SYSINFO_GROUP_A = 524329,
	TEL_ATTR_BBMC_SYSINFO_GROUP_B = 524330,
	TEL_ATTR_BBMC_SYSINFO_GROUP_C = 524331
};

/* BBMC event IDs sent to TelBbmcAdapter observers */
enum TelBbmcEventId {
	TEL_BBMC_EVENT_SMS_RECEIVED = 261,
	TEL_BBMC_EVENT_MODEM_ONLINE_READY = 517,
	TEL_BBMC_EVENT_DEVINFO_NEEDED = 519,
	TEL_BBMC_EVENT_MODEM_RESET = 520,
	TEL_BBMC_EVENT_LEAVE_ONLINE = 522,
	TEL_BBMC_EVENT_ENTER_ONLINE = 769,
	TEL_BBMC_EVENT_SIM_LOCKED = 772,
	TEL_BBMC_EVENT_SIM_PERMANENT_LOCKED = 773,
	TEL_BBMC_EVENT_TEMP_HIGH = 774,
	TEL_BBMC_EVENT_TEMP_NORMAL = 775,
	TEL_BBMC_EVENT_PIN_STATUS_INIT = 776,
	TEL_BBMC_EVENT_DRIVER_ERROR = 777,
	TEL_BBMC_EVENT_VOLTAGE_LOW = 779,
	TEL_BBMC_EVENT_VOLTAGE_NORMAL = 780,
	TEL_BBMC_EVENT_NET_REJECT = 781,
	TEL_BBMC_EVENT_MODEM_FAILURE = 1025,
	TEL_BBMC_EVENT_SIM_INSERTED = 1028,
	TEL_BBMC_EVENT_SIM_REMOVED = 1029,
	TEL_BBMC_EVENT_MODEM_POWER_ON = 1030,
	TEL_BBMC_EVENT_MODEM_POWER_ON_FAIL = 1031,
	TEL_BBMC_EVENT_MODEM_POWER_OFF = 1032,
	TEL_BBMC_EVENT_PIN_REQUIRED = 1033,
	TEL_BBMC_EVENT_PUK_REQUIRED = 1034,
	TEL_BBMC_EVENT_PIN_VERIFIED = 1035,
	TEL_BBMC_EVENT_PIN_UNBLOCKED = 1036,
	TEL_BBMC_EVENT_SIM_READY = 1037,
	TEL_BBMC_EVENT_SIM_BLOCKED = 1038,
	TEL_BBMC_EVENT_PIN_VERIFY_OK = 1039,
	TEL_BBMC_EVENT_PUK_UNBLOCK_FAIL = 1040,
	TEL_BBMC_EVENT_PIN_VERIFY_FAIL = 1041,
	TEL_BBMC_EVENT_NET_SCAN_DONE = 1063,
	TEL_BBMC_EVENT_NET_SCAN_FAILED = 1064,
	TEL_BBMC_EVENT_CALL_END_REASON = 1076,
	TEL_BBMC_EVENT_BEARER_TECH = 1077,
	TEL_BBMC_EVENT_CK_STATUS_UPDATED = 1083,
	TEL_BBMC_EVENT_PLMN_MODE_BIT = 1086,
	TEL_BBMC_EVENT_PACKET_RESTRICTED = 1087,
	TEL_BBMC_EVENT_PACKET_UNRESTRICTED = 1088,
	TEL_BBMC_EVENT_NETWORK_INFO = 1089,
	TEL_BBMC_EVENT_LOOPBACK_PERIODIC = 2049,
	TEL_BBMC_EVENT_LOOPBACK_AUTO_SMS = 2050
};

/* BBMC feature IDs for sceBbmcSubscribeFeatureId */
enum TelBbmcFeatureId {
	TEL_BBMC_FEATURE_GENERAL = 0,
	TEL_BBMC_FEATURE_SMS = 2,
	TEL_BBMC_FEATURE_SIM = 4,
	TEL_BBMC_FEATURE_PACKET = 5
};

/* BBMC log channel IDs */
enum TelBbmcLogChannel {
	TEL_BBMC_LOG_CH_NONE = 0,
	TEL_BBMC_LOG_CH_GENERAL = 1,
	TEL_BBMC_LOG_CH_SIM = 2,
	TEL_BBMC_LOG_CH_PACKET = 3,
	TEL_BBMC_LOG_CH_SMS = 4
};

/* BBMC driver error codes checked explicitly by shell */
enum TelBbmcDriverError {
	SCE_BBMC_ERROR_BUSY = -2144141306,		/* 0x80330006 */
	SCE_BBMC_ERROR_PDP_BUSY = -2144140025,		/* 0x80330507 */
	SCE_BBMC_ERROR_PDP_STATE = -2144140024,		/* 0x80330508 */
	SCE_BBMC_ERROR_NOT_SUBSCRIBED = -2144139505	/* 0x8033070F */
};

/* Notification item passed from handlers / loopback core to dispatcher */
struct TelBbmcNotifyItem {
	int eventId;
	int eventArg;
};

/* Bounded single-priority FIFO queue for TelBbmcNotifyItem */
class TelBbmcNotifyQueue {
public:
	TelBbmcNotifyQueue(const char *name, SceSize maxCount);
	~TelBbmcNotifyQueue();

	void Activate();
	void Deactivate();
	TelephonyResult Push(const TelBbmcNotifyItem &item);
	bool Pop(TelBbmcNotifyItem *outItem);

private:
	SceSize m_maxCount;
	bool m_active;
	paf::vector<TelBbmcNotifyItem> m_items;
	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_notFullCond;
	paf::thread::Cond m_notEmptyCond;
};

/* Typed event subclasses notified by TelBbmcAdapter (0x81540C44 / C68 / C8C) */
class TelBbmcNetworkEvent : public TelEventInfo {
public:
	TelBbmcNetworkEvent();
	TelBbmcNetworkEvent(const TelBbmcNetworkEvent &src);
	virtual ~TelBbmcNetworkEvent();
	virtual paf::common::SharedPtr<TelObject> Clone() const;

	void SetNetworkInfo(const TelNetworkInfo &networkInfo) { m_networkInfo = networkInfo; }
	const TelNetworkInfo &GetNetworkInfo() const { return m_networkInfo; }

private:
	TelNetworkInfo m_networkInfo;
};

class TelBbmcCallEndEvent : public TelEventInfo {
public:
	TelBbmcCallEndEvent();
	TelBbmcCallEndEvent(const TelBbmcCallEndEvent &src);
	virtual ~TelBbmcCallEndEvent();
	virtual paf::common::SharedPtr<TelObject> Clone() const;

	void SetCallEndReason(const TelCallEndReasonInfo &reason) { m_reason = reason; }
	const TelCallEndReasonInfo &GetCallEndReason() const { return m_reason; }

private:
	TelCallEndReasonInfo m_reason;
};

class TelBbmcBearerEvent : public TelEventInfo {
public:
	TelBbmcBearerEvent();
	TelBbmcBearerEvent(const TelBbmcBearerEvent &src);
	virtual ~TelBbmcBearerEvent();
	virtual paf::common::SharedPtr<TelObject> Clone() const;

	void SetBearerInfo(const TelPacketBearerInfo &bearer) { m_bearer = bearer; }
	const TelPacketBearerInfo &GetBearerInfo() const { return m_bearer; }

private:
	TelPacketBearerInfo m_bearer;
};

/* Request/response synchronizer (0x813D6A74..0x813D6B1F) */
class TelBbmcSync {
public:
	TelBbmcSync();
	~TelBbmcSync();

	void Lock() { m_mutex.Lock(); }
	void Unlock() { m_mutex.Unlock(); }
	void BeginWait(SceUInt32 waitMask);
	bool Wait(int timeoutUsec);
	void Signal(SceUInt32 eventMask, int status);

	SceUInt32 GetMatchedMask() const { return m_matchedMask; }
	SceUInt32 GetLastEventMask() const { return m_lastEventMask; }
	int GetLastStatus() const { return m_lastStatus; }
	void IncrementActiveCount() { ++m_activeCount; }
	void DecrementActiveCount() { --m_activeCount; }
	SceUInt32 GetWaitMask() const { return m_waitMask; }

private:
	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_cond;
	int m_activeCount;
	SceUInt32 m_waitMask;
	SceUInt32 m_matchedMask;
	SceUInt32 m_lastEventMask;
	int m_lastStatus;
};

/* Shared power/suspend status singleton (0x813D3408..0x813D355B) */
enum TelBbmcPowerStatus {
	TEL_BBMC_POWER_READY = 0,
	TEL_BBMC_POWER_OFF = 1,
	TEL_BBMC_POWER_STARTING = 2,
	TEL_BBMC_POWER_FAILURE = 3
};

enum TelBbmcSharedFlagIndex {
	TEL_BBMC_FLAG_SUSPENDING = 0,
	TEL_BBMC_FLAG_SMS_PENDING_SUSPEND = 1,
	TEL_BBMC_FLAG_PACKET_ACTIVE = 2
};

class TelBbmcSharedStatus {
public:
	TelBbmcSharedStatus();
	virtual ~TelBbmcSharedStatus();

	static TelBbmcSharedStatus *Acquire();
	static void Release();
	static TelBbmcSharedStatus *GetInstance();

	TelephonyResult CheckReady() const;
	void SetPowerStatus(int status);
	bool LatchSmsDuringSuspend();
	void SetFlag(int flagIndex, bool value);
	bool GetFlag(int flagIndex);

private:
	paf::thread::Mutex m_mutex;
	bool m_flags[3];
	int m_powerStatus;
};

/* Binary diagnostic ring buffer & log manager (0x813DAC1E..0x813DBD13) */
class TelBbmcLogBuffer {
public:
	TelBbmcLogBuffer();
	virtual ~TelBbmcLogBuffer();

	TelephonyResult Allocate(SceSize capacity);
	void Reset();
	void RewindRead();
	TelephonyResult WriteBytes(const void *src, SceSize size);
	void SaveWritePos() { m_savedWritePos = m_writePos; }
	void CommitWrite() { m_committedSize = m_writePos; }
	void RollbackAndFreeze()
	{
		m_writePos = m_savedWritePos;
		m_frozen = true;
	}
	bool IsFrozen() const { return m_frozen; }
	bool HasUnread() const { return m_readPos < m_writePos; }
	const SceUInt8 *GetReadPointer() const { return m_buffer + m_readPos; }
	void SaveReadPos() { m_savedReadPos = m_readPos; }
	void RollbackReadPos() { m_readPos = m_savedReadPos; }
	TelephonyResult AdvanceRead(SceSize amount);

private:
	bool m_frozen;
	SceSize m_committedSize;
	SceSize m_writePos;
	SceSize m_savedWritePos;
	SceSize m_readPos;
	SceSize m_savedReadPos;
	SceUInt8 *m_buffer;
	SceSize m_capacity;
};

class TelBbmcLogManager;

class TelBbmcLogClient {
public:
	TelBbmcLogClient();
	virtual ~TelBbmcLogClient();
	virtual const char *GetLogTagName(int tagId) const = 0;

	TelephonyResult Register(int channelId);
	TelephonyResult Unregister();
	TelephonyResult LogResult(SceUInt8 tagId, int resultCode);
	TelephonyResult LogU16(SceUInt8 tagId, SceUInt16 value);
	TelephonyResult LogU32(SceUInt8 tagId, SceUInt32 value);
	TelephonyResult LogEventMask(SceUInt32 eventMask);
	TelephonyResult LogTimestamp();

private:
	TelBbmcLogManager *m_manager;
	int m_channelId;
};

class TelBbmcLogManager {
public:
	TelBbmcLogManager();
	virtual ~TelBbmcLogManager();

	static TelephonyResult Initialize();
	static TelephonyResult Terminate();
	static TelBbmcLogManager *GetInstance();

	TelephonyResult InitBuffer();
	void RegisterClient(int channelId, TelBbmcLogClient *client);
	void UnregisterClient(int channelId);
	void RewindRead();
	void ClearLog();
	int ReadFormatted(char *outBuffer, SceSize maxLen);

	TelephonyResult LogResult(int channelId, SceUInt8 tagId, int resultCode);
	TelephonyResult LogU16(int channelId, SceUInt8 tagId, SceUInt16 value);
	TelephonyResult LogU32(int channelId, SceUInt8 tagId, SceUInt32 value);
	TelephonyResult LogEventMask(int channelId, SceUInt32 eventMask);
	TelephonyResult LogTimestamp(int channelId);

private:
	TelephonyResult WriteTaggedRecord(SceUInt8 headerByte, SceUInt8 tagId,
					  const void *payload, SceSize payloadSize);
	TelephonyResult WriteRawRecord(SceUInt8 headerByte, const void *payload,
				       SceSize payloadSize, bool alignEven);
	TelephonyResult FormatRecord(const SceUInt8 *record, SceSize *outConsumed,
				     SceSize *outTextLen);

	paf::thread::Mutex m_mutex;
	TelBbmcLogClient *m_clients[5];
	char m_lineBuf[128];
	TelBbmcLogBuffer *m_logBuffer;
};

/* Driver state history ring (16 entries) */
class TelBbmcStateHistory {
public:
	TelBbmcStateHistory();
	virtual ~TelBbmcStateHistory();

	void PushState(SceUInt8 state);
	paf::string FormatHex() const;

private:
	SceSize m_maxEntries;
	paf::list<SceUInt8> m_states;
};

/* Shared BBMC data and SceTelBbmcSysInfoEvf mirror (0x813D982C..0x813DAC1D) */
class TelBbmcSharedData {
public:
	TelBbmcSharedData();
	virtual ~TelBbmcSharedData();

	static TelephonyResult Initialize();
	static TelephonyResult Terminate();
	static TelBbmcSharedData *GetInstance();
	static void SetStaticInt(int attrId, SceUInt32 value);

	void SetInt(int attrId, SceUInt32 value);
	int GetInt(int attrId);
	void AddInt(int attrId, int delta);
	void SetBool(int attrId, bool value);
	bool GetBool(int attrId);
	void SetString(int attrId, const paf::string &value);
	void SetStringIfEmpty(int attrId, const paf::string &value);
	paf::string GetString(int attrId);
	paf::string GetHomeDescription();
	void GetHomePlmn(SceUInt16 *outMcc, SceUInt16 *outMnc);

	TelephonyResult CheckSimCondition();
	bool IsRadioReady();
	void RecordDriverState(SceUInt8 state);
	paf::string FormatStateHistory();
	void ResetPinAttributes();
	void SetControlKeyAttributes(SceUInt32 ckState, SceUInt32 blockType);
	bool HasControlKeyStatus();
	void SetSimStateAttributes(SceUInt32 simState, bool simInvalid);
	void ResetDeviceInfoAttributes();

private:
	void UpdateSysInfoEventFlag(int attrId, SceUInt32 value);

	paf::thread::Mutex m_mutex;
	SceUInt32 m_evfBits;
	SceUID m_sysInfoEvf;
	TelBbmcStateHistory m_stateHistory;
};

/* Abstract base class for BBMC feature handlers (0x813D2E10..0x813D3407) */
class TelBbmcHandler {
public:
	explicit TelBbmcHandler(TelBbmcSharedData *sharedData);
	virtual ~TelBbmcHandler();

	virtual bool CanProcessTimerRetry() = 0;
	virtual SceUInt32 GetFeatureId() = 0;
	virtual SceUInt32 GetEventMask() = 0;
	virtual SceUInt32 GetSubscriberEvfBit() = 0;
	virtual void ProcessTasks(SceUInt32 taskBits) = 0;
	virtual void OnBroadcastFlags(SceUInt32 flags) = 0;
	virtual int HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					SceUInt32 *outTaskBits) = 0;
	virtual bool ShouldRetryResult(const TelephonyResult &res);
	virtual void ResetClearedEvents();
	virtual SceUInt32 GetClearedEvents();
	virtual bool ConsumeRetryAndDelay(const TelephonyResult &res);

	TelephonyResult Subscribe(SceUID subscriberEvf);
	TelephonyResult Unsubscribe();
	TelephonyResult PopNotify(TelBbmcNotifyItem *outItem);
	void HandleEvfWake(SceUInt32 evfBits);
	void DispatchBroadcast(SceUInt32 flags);

	SceUInt32 GetPendingRetryTasks() const { return m_retryTasks; }
	SceUInt32 TakeBroadcastFlags()
	{
		SceUInt32 flags = m_broadcastFlags;
		m_broadcastFlags = 0;
		return flags;
	}

protected:
	TelephonyResult PushNotify(int eventId, int eventArg);
	int PostTask(SceUInt32 taskBit);
	void SignalSyncError(SceUInt32 eventMask, int status);
	void ClearAndSignalEvent(SceUInt32 eventMask);
	void BeginSyncWait(SceUInt32 waitMask);
	SceUInt32 WaitSync(SceUInt32 *outLastEvent, int *outStatus, int timeoutUsec);

	void SetMaxRetries(int count) { m_maxRetries = count; }
	void ResetRetryCount(int count);
	void SetRetryDelayMs(int delayMs) { m_retryDelayMs = delayMs; }
	void SetTaskRetryBudget(SceUInt32 taskBit, int retries);
	bool ScheduleTaskRetry(SceUInt32 taskBit);

	static int CallbackEntry(int notifyId, int notifyCount, int notifyArg, void *common);

	paf::thread::Mutex m_taskMutex;
	SceUID m_subscriberEvf;
	SceUInt32 m_pendingTasks;
	SceUInt32 m_broadcastFlags;
	SceUInt32 m_retryTasks;
	SceInt8 m_taskRetryCounters[32][2];
	bool m_syncTimedOut;
	int m_subscriberId;
	SceUID m_callbackId;
	TelBbmcNotifyQueue m_notifyQueue;
	TelBbmcSync m_sync;
	int m_maxRetries;
	int m_remainingRetries;
	int m_retryDelayMs;
	SceUInt32 m_clearedEvents;
	TelBbmcSharedData *m_sharedData;
};

/* General/radio/scan/device-info handler (0x813CFC1A..0x813D2E0F) */
class TelBbmcGeneralHandler : public TelBbmcHandler, public TelBbmcLogClient {
public:
	explicit TelBbmcGeneralHandler(TelBbmcSharedData *sharedData);
	virtual ~TelBbmcGeneralHandler();

	virtual bool CanProcessTimerRetry();
	virtual SceUInt32 GetFeatureId();
	virtual SceUInt32 GetEventMask();
	virtual SceUInt32 GetSubscriberEvfBit();
	virtual void ProcessTasks(SceUInt32 taskBits);
	virtual void OnBroadcastFlags(SceUInt32 flags);
	virtual int HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					SceUInt32 *outTaskBits);
	virtual bool ShouldRetryResult(const TelephonyResult &res);
	virtual const char *GetLogTagName(int tagId) const;

	void ResetState();
	TelephonyResult TurnOn();
	TelephonyResult AbortNetworkScan();
	TelephonyResult TurnOff(bool forceStop);
	TelephonyResult SetLowPowerMode(bool enableLpm, bool waitDone);
	TelephonyResult GetSignalLevel(TelAntennaLevelThreshold *outLevel);
	TelephonyResult GetNetworkInfo(TelNetworkInfo *outInfo);
	TelephonyResult GetRoamingInfo(TelRoamingInfo *outRoaming);
	TelephonyResult GetServingPlmn(TelephonyPlmn *outPlmn);
	TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber);
	TelephonyResult ReserveOperationMode(SceUInt32 opMode);
	TelephonyResult SetupGetValue(SceUInt32 kind, void *outBuf, SceSize size);
	TelephonyResult FetchDeviceInformation();
	TelephonyResult SetupSetValue(SceUInt32 kind, const void *inBuf, SceSize size);
	TelephonyResult SetProfileSettings(const SceBbmcProfileSettings *profile);
	TelephonyResult GetProfileSettings(SceBbmcProfileSettings *outProfile);
	TelephonyResult GetImsi(TelImeiInfo *outImsi);
	TelephonyResult GetImei(paf::string *outImei);
	TelephonyResult GetIccid(paf::string *outIccid);
	TelephonyResult GetHomePlmn(TelephonyPlmn *outPlmn);
	TelephonyResult StartNetworkScan();
	TelephonyResult GetNetworkScanInfo(SceBbmcNetworkScanInfo *outScan);
	TelephonyResult SetNetworkRegistration(const SceBbmcNetworkRegistration *reg);
	TelephonyResult RefreshNetworkInfo();
	TelephonyResult SetRssiThresholds(const SceBbmcRssiThresholdList *thresholds);
	TelephonyResult GetRawNetworkInfo(SceBbmcNetworkInfo *outRaw);
	TelephonyResult GetLastRejectCause(SceBbmcRejectCause *outCause);
	TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion);
	TelephonyResult GetArchiveVersion(SceUInt32 *outVersion);
	TelephonyResult CheckComVersion(bool *outMatches);
	TelephonyResult FetchComFirmVersion();
	void SetDebugFlags(int flags);
	int CheckTemperatureStatus();
	TelephonyResult SyncBarrier();

private:
	void PollNetworkInfoLocked();

	paf::thread::Mutex m_stateMutex;
	SceBbmcDriverInfo m_driverInfo;
	SceBbmcNetworkInfo m_prevNetworkInfo;
	SceBbmcNetworkInfo m_curNetworkInfo;
	SceUInt32 m_operationMode;
	SceBbmcFailureInfo m_failureInfo;
	bool m_lpmRequested;
	int m_activeSetupCount;
	int m_powerCommand;
	int m_powerTransition;
	int m_powerError;
	bool m_fatalNotified;
	bool m_forceStop;
	bool m_turnOffPending;
	SceUInt32 m_getSetupKind;
	void *m_getSetupBuffer;
	SceSize m_getSetupSize;
	SceUInt32 m_setSetupKind;
	const void *m_setSetupBuffer;
	SceSize m_setSetupSize;
	SceUInt32 m_validCacheMask;
	SceBbmcNetworkRegistration m_regParam;
	SceBbmcRejectCause m_rejectCause;
	bool m_awaitingOnlineReady;
	bool m_devInfoRequested;
	SceBbmcNetworkScanInfo m_scanInfo;
	int m_scanState;
	SceUInt32 m_scanResultEvent;
	int m_scanDoneRetries;
	bool m_scanActive;
	bool m_scanResultValid;
	bool m_scanInProgress;
	int m_scanAbortCount;
	int m_debugFlags;
};

/* Packet data / bearer / call-end handler (0x813D355C..0x813D3E3F) */
class TelBbmcPacketHandler : public TelBbmcHandler, public TelBbmcLogClient {
public:
	explicit TelBbmcPacketHandler(TelBbmcSharedData *sharedData);
	virtual ~TelBbmcPacketHandler();

	virtual bool CanProcessTimerRetry();
	virtual SceUInt32 GetFeatureId();
	virtual SceUInt32 GetEventMask();
	virtual SceUInt32 GetSubscriberEvfBit();
	virtual void ProcessTasks(SceUInt32 taskBits);
	virtual void OnBroadcastFlags(SceUInt32 flags);
	virtual int HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					SceUInt32 *outTaskBits);
	virtual const char *GetLogTagName(int tagId) const;

	TelephonyResult SetPacketControlBit(bool enable);
	TelephonyResult SetRoamingControlBit(bool enable);
	TelephonyResult SetSuspendControlBit(bool enable);
	TelephonyResult GetLastCallEndReason(SceBbmcCallEndReason *outReason);
	TelephonyResult GetCurrentBearerTech(SceBbmcBearerTech *outBearer);
	TelephonyResult ReleasePdpContext();
	bool IsPacketCallActive() const;
	SceUInt32 GetControlFlagValue();
	int GetControlSetCount();
	int GetControlEventCount();

private:
	paf::thread::Mutex m_stateMutex;
	SceBbmcCallEndReason m_callEndReason;
	SceBbmcBearerTech m_bearerTech;
	SceUInt32 m_controlFlags;
	int m_controlSetCount;
	int m_controlEventCount;
};

/* SIM / PIN / PUK / CK / refresh handler (0x813D3E40..0x813D5ADB) */
class TelBbmcSimHandler : public TelBbmcHandler, public TelBbmcLogClient {
public:
	explicit TelBbmcSimHandler(TelBbmcSharedData *sharedData);
	virtual ~TelBbmcSimHandler();

	virtual bool CanProcessTimerRetry();
	virtual SceUInt32 GetFeatureId();
	virtual SceUInt32 GetEventMask();
	virtual SceUInt32 GetSubscriberEvfBit();
	virtual void ProcessTasks(SceUInt32 taskBits);
	virtual void OnBroadcastFlags(SceUInt32 flags);
	virtual int HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					SceUInt32 *outTaskBits);
	virtual bool ShouldRetryResult(const TelephonyResult &res);
	virtual const char *GetLogTagName(int tagId) const;

	TelephonyResult VerifyPin(const TelPinCode &pin);
	TelephonyResult UnblockPin(const TelPukCode &puk, const TelPinCode &newPin);
	TelephonyResult GetPinStatus(int *outStatus);
	TelephonyResult GetPinRetryCount(int *outRetries);
	TelephonyResult ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin);
	TelephonyResult SetPinEnabled(const bool *enable, const TelPinCode &pin);
	TelephonyResult GetPinEnabled(bool *outEnabled);
	TelephonyResult DepersonalizeControlKey(const paf::string &key);
	TelephonyResult UnblockControlKey(const paf::string &key);
	TelephonyResult GetControlKeyStatus(SceBbmcControlKeyStatus *outStatus);
	TelephonyResult GetPlmnModeBit(int *outModeBit);
	TelephonyResult RequestFileRefresh();

private:
	TelephonyResult SetupSetPinValue(SceUInt32 kind, const void *buffer, SceSize size);
	TelephonyResult QueryPlmnModeBitUnlocked(int *outModeBit);

	paf::thread::Mutex m_stateMutex;
	bool m_modemFatal;
	paf::string m_pinString;
	SceBbmcUnblockPinParam m_unblockParam;
	SceBbmcControlKeyStatus m_ckStatus;
	SceUInt32 m_setupKind;
	const void *m_setupBuffer;
	SceSize m_setupSize;
	SceBbmcControlKeyParam m_ckParam;
	SceBbmcFileRefreshDetail m_refreshDetail;
};

/* SMS send/list/read/ack/delete handler (0x813D5ADC..0x813D64C7) */
class TelBbmcSmsHandler : public TelBbmcHandler, public TelBbmcLogClient {
public:
	explicit TelBbmcSmsHandler(TelBbmcSharedData *sharedData);
	virtual ~TelBbmcSmsHandler();

	virtual bool CanProcessTimerRetry();
	virtual SceUInt32 GetFeatureId();
	virtual SceUInt32 GetEventMask();
	virtual SceUInt32 GetSubscriberEvfBit();
	virtual void ProcessTasks(SceUInt32 taskBits);
	virtual void OnBroadcastFlags(SceUInt32 flags);
	virtual int HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					SceUInt32 *outTaskBits);
	virtual bool ShouldRetryResult(const TelephonyResult &res);
	virtual const char *GetLogTagName(int tagId) const;

	TelephonyResult SendSms(const TelSmsProtocolDataUnit &pdu);
	TelephonyResult ReadNextSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outMemoryIndex);
	TelephonyResult DeleteSms(const SceUInt32 *memoryIndex);

private:
	TelephonyResult AckSms(bool deleteMessage);
	TelephonyResult ReadSmsAtIndex();
	TelephonyResult FetchSmsList();

	paf::thread::Mutex m_stateMutex;
	SceUInt8 m_sendBuffer[256];
	SceSize m_sendLength;
	SceUInt8 m_readBuffer[256];
	int m_readLength;
	SceUInt32 m_listCount;
	SceUInt32 m_memoryIndex;
};

class TelBbmcAdapter;

/* Subscriber event-flag worker thread (0x813D64C8..0x813D6A73) */
class TelBbmcSubscriberThread : public TelThread {
public:
	TelBbmcSubscriberThread(const char *name, int priority, SceSize stackSize);
	virtual ~TelBbmcSubscriberThread();

	virtual TelephonyResult StopThread();
	virtual int Run(void *userArg);
	virtual TelephonyResult StartSubscriber(TelBbmcAdapter *adapter);
	virtual bool StartRetryTimer();
	virtual void StopRetryTimer();

	void AddHandler(TelBbmcHandler *handler);

private:
	void DispatchHandlers(SceUInt32 evfBits);
	TelephonyResult SubscribeAll();
	static int TimerCallback(int notifyId, int notifyCount, int notifyArg, void *common);

	paf::thread::Semaphore *m_startSema;
	paf::vector<TelBbmcHandler *> m_handlers;
	SceUID m_eventFlag;
	SceUID m_timerCallback;
	SceUID m_retryTimer;
};

/* Abstract adapter interface implemented by TelBbmcAdapter & TelBbmcAdapterLoopback */
class TelBbmcAdapterInterface : public TelObservable {
public:
	TelBbmcAdapterInterface();
	virtual ~TelBbmcAdapterInterface();

	virtual TelephonyResult Initialize() = 0;
	virtual TelephonyResult Terminate() = 0;
	virtual TelephonyResult TurnOnDefault() = 0;
	virtual TelephonyResult TurnOn(bool lpmEnabled) = 0;
	virtual TelephonyResult TurnOff(bool forceStop) = 0;
	virtual TelephonyResult VerifyPin(const TelPinCode &pin) = 0;
	virtual TelephonyResult UnblockPin(const TelPukCode &puk, const TelPinCode &newPin) = 0;
	virtual TelephonyResult GetPinStatus(int *outStatus) = 0;
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber) = 0;
	virtual TelephonyResult SetLowPowerModeSync(bool enableLpm) = 0;
	virtual TelephonyResult SetLowPowerModeAsync(bool enableLpm) = 0;
	virtual TelephonyResult ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin) = 0;
	virtual TelephonyResult SetPinEnabled(const bool *enable, const TelPinCode &pin) = 0;
	virtual TelephonyResult GetPinEnabled(bool *outEnabled) = 0;
	virtual TelephonyResult SendSms(const TelSmsProtocolDataUnit &pdu) = 0;
	virtual TelephonyResult ReadSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outIndex) = 0;
	virtual TelephonyResult DeleteSms(const SceUInt32 *memoryIndex) = 0;
	virtual TelephonyResult GetPinRetryCount(SceUInt16 *outRetries) = 0;
	virtual TelephonyResult GetSignalLevel(TelAntennaLevelThreshold *outLevel) = 0;
	virtual TelephonyResult GetNetworkInfo(TelNetworkInfo *outInfo) = 0;
	virtual TelephonyResult GetRoamingInfo(TelRoamingInfo *outRoaming) = 0;
	virtual TelephonyResult GetServingPlmn(TelephonyPlmn *outPlmn) = 0;
	virtual TelephonyResult SetApnCredentials(const TelephonyOperator &credentials) = 0;
	virtual TelephonyResult GetImsi(TelImeiInfo *outImsi) = 0;
	virtual TelephonyResult StartNetworkScan() = 0;
	virtual TelephonyResult GetNetworkScanResult(TelNetScanInfoList *outList) = 0;
	virtual TelephonyResult AbortNetworkScan() = 0;
	virtual TelephonyResult RegisterNetworkManual(const TelephonyPlmn &plmn,
						      int radioCapability) = 0;
	virtual TelephonyResult RegisterNetworkAuto() = 0;
	virtual TelephonyResult SetRssiThresholds(const paf::vector<int> &thresholds) = 0;
	virtual TelephonyResult GetImei(paf::string *outImei) = 0;
	virtual TelephonyResult GetIccid(paf::string *outIccid) = 0;
	virtual TelephonyResult GetHomePlmn(TelephonyPlmn *outPlmn) = 0;
	virtual TelephonyResult CheckSimCondition() = 0;
	virtual TelephonyResult GetControlKeyState(int *outState) = 0;
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType) = 0;
	virtual TelephonyResult GetControlKeyVerifyRetries(int *outRetries) = 0;
	virtual TelephonyResult GetControlKeyUnblockRetries(int *outRetries) = 0;
	virtual TelephonyResult DepersonalizeControlKey(const TelControlKey &key) = 0;
	virtual TelephonyResult UnblockControlKey(const TelControlKey &key) = 0;
	virtual TelephonyResult FetchDeviceInformation() = 0;
	virtual TelephonyResult FetchDefaultProfileSettings() = 0;
	virtual TelephonyResult SetRoamingControlBit(bool enable) = 0;
	virtual TelephonyResult SetPacketControlBit(bool enable) = 0;
	virtual TelephonyResult ClearSuspendControlBit() = 0;
	virtual TelephonyResult SetSuspendControlBit() = 0;
	virtual TelephonyResult GetPacketBearerInfo(TelPacketBearerInfo *outBearer) = 0;
	virtual TelephonyResult GetCallEndReasonInfo(TelCallEndReasonInfo *outReason) = 0;
	virtual TelephonyResult ReleasePdpContext() = 0;
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion) = 0;
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion) = 0;
	virtual TelephonyResult CheckComVersion(bool *outMatches) = 0;
	virtual TelephonyResult SyncGeneralHandler() = 0;
	virtual TelephonyResult GetPlmnModeBit(int *outModeBit) = 0;
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport) = 0;
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport) = 0;
	virtual TelephonyResult StartUsbExtIf() = 0;
	virtual TelephonyResult StopUsbExtIf() = 0;
	virtual TelephonyResult SetShellDebugMedia(int media) = 0;
	virtual TelephonyResult SetModemDebugMedia(int media) = 0;
	virtual TelephonyResult SetDriverDebugMedia(int media) = 0;
	virtual TelephonyResult GetFirmwareRevision(paf::string *outRevision) = 0;
};

/* Real hardware BBMC adapter (0x813CB470..0x813CFBA9) */
class TelBbmcAdapter : public TelBbmcAdapterInterface {
public:
	TelBbmcAdapter();
	virtual ~TelBbmcAdapter();

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult TurnOnDefault();
	virtual TelephonyResult TurnOn(bool lpmEnabled);
	virtual TelephonyResult TurnOff(bool forceStop);
	virtual TelephonyResult VerifyPin(const TelPinCode &pin);
	virtual TelephonyResult UnblockPin(const TelPukCode &puk, const TelPinCode &newPin);
	virtual TelephonyResult GetPinStatus(int *outStatus);
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber);
	virtual TelephonyResult SetLowPowerModeSync(bool enableLpm);
	virtual TelephonyResult SetLowPowerModeAsync(bool enableLpm);
	virtual TelephonyResult ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin);
	virtual TelephonyResult SetPinEnabled(const bool *enable, const TelPinCode &pin);
	virtual TelephonyResult GetPinEnabled(bool *outEnabled);
	virtual TelephonyResult SendSms(const TelSmsProtocolDataUnit &pdu);
	virtual TelephonyResult ReadSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outIndex);
	virtual TelephonyResult DeleteSms(const SceUInt32 *memoryIndex);
	virtual TelephonyResult GetPinRetryCount(SceUInt16 *outRetries);
	virtual TelephonyResult GetSignalLevel(TelAntennaLevelThreshold *outLevel);
	virtual TelephonyResult GetNetworkInfo(TelNetworkInfo *outInfo);
	virtual TelephonyResult GetRoamingInfo(TelRoamingInfo *outRoaming);
	virtual TelephonyResult GetServingPlmn(TelephonyPlmn *outPlmn);
	virtual TelephonyResult SetApnCredentials(const TelephonyOperator &credentials);
	virtual TelephonyResult GetImsi(TelImeiInfo *outImsi);
	virtual TelephonyResult StartNetworkScan();
	virtual TelephonyResult GetNetworkScanResult(TelNetScanInfoList *outList);
	virtual TelephonyResult AbortNetworkScan();
	virtual TelephonyResult RegisterNetworkManual(const TelephonyPlmn &plmn,
						      int radioCapability);
	virtual TelephonyResult RegisterNetworkAuto();
	virtual TelephonyResult SetRssiThresholds(const paf::vector<int> &thresholds);
	virtual TelephonyResult GetImei(paf::string *outImei);
	virtual TelephonyResult GetIccid(paf::string *outIccid);
	virtual TelephonyResult GetHomePlmn(TelephonyPlmn *outPlmn);
	virtual TelephonyResult CheckSimCondition();
	virtual TelephonyResult GetControlKeyState(int *outState);
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType);
	virtual TelephonyResult GetControlKeyVerifyRetries(int *outRetries);
	virtual TelephonyResult GetControlKeyUnblockRetries(int *outRetries);
	virtual TelephonyResult DepersonalizeControlKey(const TelControlKey &key);
	virtual TelephonyResult UnblockControlKey(const TelControlKey &key);
	virtual TelephonyResult FetchDeviceInformation();
	virtual TelephonyResult FetchDefaultProfileSettings();
	virtual TelephonyResult SetRoamingControlBit(bool enable);
	virtual TelephonyResult SetPacketControlBit(bool enable);
	virtual TelephonyResult ClearSuspendControlBit();
	virtual TelephonyResult SetSuspendControlBit();
	virtual TelephonyResult GetPacketBearerInfo(TelPacketBearerInfo *outBearer);
	virtual TelephonyResult GetCallEndReasonInfo(TelCallEndReasonInfo *outReason);
	virtual TelephonyResult ReleasePdpContext();
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult CheckComVersion(bool *outMatches);
	virtual TelephonyResult SyncGeneralHandler();
	virtual TelephonyResult GetPlmnModeBit(int *outModeBit);
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport);
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport);
	virtual TelephonyResult StartUsbExtIf();
	virtual TelephonyResult StopUsbExtIf();
	virtual TelephonyResult SetShellDebugMedia(int media);
	virtual TelephonyResult SetModemDebugMedia(int media);
	virtual TelephonyResult SetDriverDebugMedia(int media);
	virtual TelephonyResult GetFirmwareRevision(paf::string *outRevision);

	virtual TelephonyResult RequestFileRefresh();
	virtual void ClearVoiceNumber();
	virtual void SetGeneralDebugFlags(int flags);
	virtual TelephonyResult SelectDebugOutMedia(int channel, int media);

	TelephonyResult HandleHandlerEvent(int eventId, int eventArg);
	static int WriteDebugString(const char *text);

private:
	TelBbmcSharedData *m_sharedData;
	TelBbmcGeneralHandler *m_generalHandler;
	TelBbmcSimHandler *m_simHandler;
	TelBbmcSmsHandler *m_smsHandler;
	TelBbmcPacketHandler *m_packetHandler;
	paf::vector<TelBbmcSubscriberThread *> m_subscribers;
};

/* Loopback configuration read from tel_config.ini */
struct TelBbmcLoopbackConfig {
	bool simInserted;
	int pinStatus;
	paf::string pinCode;
	paf::string pukCode;
	int pinRetries;
	int simLockState;
	int simBlockType;
	int initialCheckEnabled;
	paf::string unlockCode;
	paf::string unblockCode;
	int ckRetries;
};

/* Loopback state machine core (0x813D8478..0x813D965A) */
class TelBbmcAdapterLoopbackCore {
public:
	TelBbmcAdapterLoopbackCore();
	virtual ~TelBbmcAdapterLoopbackCore();

	void ApplyConfig(const TelBbmcLoopbackConfig &config);
	TelephonyResult PushEvent(int eventId, int eventArg);
	void Step();
	void InjectSequenceEvent(int actionCode, int actionValue);
	TelephonyResult PopEvent(TelBbmcNotifyItem *outItem);

	TelephonyResult TurnOn();
	TelephonyResult TurnOff();
	TelephonyResult SetLowPowerMode(bool enableLpm);
	int GetCurrentState();
	TelephonyResult VerifyPin(const TelPinCode &pin, bool silent);
	TelephonyResult UnblockPin(const TelPukCode &puk, const TelPinCode &newPin);
	TelephonyResult GetPinStatus(int *outStatus);
	TelephonyResult ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin);
	TelephonyResult GetPinEnabled(bool *outEnabled);
	TelephonyResult SetPinEnabled(const bool *enable, const TelPinCode &pin);
	TelephonyResult GetPinRetryCount(SceUInt16 *outRetries);
	TelephonyResult CheckSimCondition();
	TelephonyResult GetControlKeyState(int *outState);
	TelephonyResult GetControlKeyBlockType(int *outBlockType);
	TelephonyResult GetControlKeyRetries(int *outRetries);
	TelephonyResult DepersonalizeControlKey(const TelControlKey &key);
	TelephonyResult UnblockControlKey(const TelControlKey &key);
	void MarkDeviceInfoReady();

private:
	TelBbmcNotifyQueue m_notifyQueue;
	paf::thread::Mutex m_mutex;
	TelBbmcLoopbackConfig m_config;
	bool m_failBoot;
	bool m_overheated;
	int m_overheatTicks;
	int m_currentState;
	int m_targetState;
	int m_pinEnabledState;
	int m_lpmDesired;
	bool m_devInfoReady;
	bool m_awaitingOnlineReady;
	bool m_devInfoRequested;
};

/* Loopback 100ms periodic thread (0x813D965C..0x813D982B) */
class TelBbmcAdapterPeriodicThread : public TelThread {
public:
	TelBbmcAdapterPeriodicThread(const char *name, SceSize stackSize);
	virtual ~TelBbmcAdapterPeriodicThread();

	void StartPeriodic(TelMessageThread *targetThread, TelBbmcAdapterLoopbackCore *core,
			   bool autoRecvSms);
	virtual int Run(void *userArg);

private:
	TelBbmcAdapterLoopbackCore *m_core;
	bool m_autoRecvSms;
};

/* Loopback BBMC adapter for testing without hardware (0x813D6B20..0x813D8477) */
class TelBbmcAdapterLoopback : public TelBbmcAdapterInterface, public TelMessageThread {
public:
	TelBbmcAdapterLoopback();
	virtual ~TelBbmcAdapterLoopback();

	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult TurnOnDefault();
	virtual TelephonyResult TurnOn(bool lpmEnabled);
	virtual TelephonyResult TurnOff(bool forceStop);
	virtual TelephonyResult VerifyPin(const TelPinCode &pin);
	virtual TelephonyResult UnblockPin(const TelPukCode &puk, const TelPinCode &newPin);
	virtual TelephonyResult GetPinStatus(int *outStatus);
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber);
	virtual TelephonyResult SetLowPowerModeSync(bool enableLpm);
	virtual TelephonyResult SetLowPowerModeAsync(bool enableLpm);
	virtual TelephonyResult ChangePin(const TelPinCode &oldPin, const TelPinCode &newPin);
	virtual TelephonyResult SetPinEnabled(const bool *enable, const TelPinCode &pin);
	virtual TelephonyResult GetPinEnabled(bool *outEnabled);
	virtual TelephonyResult SendSms(const TelSmsProtocolDataUnit &pdu);
	virtual TelephonyResult ReadSms(TelSmsProtocolDataUnit *outPdu, SceUInt32 *outIndex);
	virtual TelephonyResult DeleteSms(const SceUInt32 *memoryIndex);
	virtual TelephonyResult GetPinRetryCount(SceUInt16 *outRetries);
	virtual TelephonyResult GetSignalLevel(TelAntennaLevelThreshold *outLevel);
	virtual TelephonyResult GetNetworkInfo(TelNetworkInfo *outInfo);
	virtual TelephonyResult GetRoamingInfo(TelRoamingInfo *outRoaming);
	virtual TelephonyResult GetServingPlmn(TelephonyPlmn *outPlmn);
	virtual TelephonyResult SetApnCredentials(const TelephonyOperator &credentials);
	virtual TelephonyResult GetImsi(TelImeiInfo *outImsi);
	virtual TelephonyResult StartNetworkScan();
	virtual TelephonyResult GetNetworkScanResult(TelNetScanInfoList *outList);
	virtual TelephonyResult AbortNetworkScan();
	virtual TelephonyResult RegisterNetworkManual(const TelephonyPlmn &plmn,
						      int radioCapability);
	virtual TelephonyResult RegisterNetworkAuto();
	virtual TelephonyResult SetRssiThresholds(const paf::vector<int> &thresholds);
	virtual TelephonyResult GetImei(paf::string *outImei);
	virtual TelephonyResult GetIccid(paf::string *outIccid);
	virtual TelephonyResult GetHomePlmn(TelephonyPlmn *outPlmn);
	virtual TelephonyResult CheckSimCondition();
	virtual TelephonyResult GetControlKeyState(int *outState);
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType);
	virtual TelephonyResult GetControlKeyVerifyRetries(int *outRetries);
	virtual TelephonyResult GetControlKeyUnblockRetries(int *outRetries);
	virtual TelephonyResult DepersonalizeControlKey(const TelControlKey &key);
	virtual TelephonyResult UnblockControlKey(const TelControlKey &key);
	virtual TelephonyResult FetchDeviceInformation();
	virtual TelephonyResult FetchDefaultProfileSettings();
	virtual TelephonyResult SetRoamingControlBit(bool enable);
	virtual TelephonyResult SetPacketControlBit(bool enable);
	virtual TelephonyResult ClearSuspendControlBit();
	virtual TelephonyResult SetSuspendControlBit();
	virtual TelephonyResult GetPacketBearerInfo(TelPacketBearerInfo *outBearer);
	virtual TelephonyResult GetCallEndReasonInfo(TelCallEndReasonInfo *outReason);
	virtual TelephonyResult ReleasePdpContext();
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult CheckComVersion(bool *outMatches);
	virtual TelephonyResult SyncGeneralHandler();
	virtual TelephonyResult GetPlmnModeBit(int *outModeBit);
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport);
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport);
	virtual TelephonyResult StartUsbExtIf();
	virtual TelephonyResult StopUsbExtIf();
	virtual TelephonyResult SetShellDebugMedia(int media);
	virtual TelephonyResult SetModemDebugMedia(int media);
	virtual TelephonyResult SetDriverDebugMedia(int media);
	virtual TelephonyResult GetFirmwareRevision(paf::string *outRevision);

	virtual void OnHandleEvent(const TelEventInfo &event);

private:
	void ForwardEvent(const TelEventInfo &event);
	void GenerateAutoRecvSms();
	void ProcessPeriodicTick();

	TelBbmcAdapterLoopbackCore m_core;
	paf::list<TelSmsProtocolDataUnit> m_smsInbox;
	TelBbmcAdapterPeriodicThread m_periodicThread;
	int m_signalLevel;
	int m_networkType;
	int m_roamingState;
	int m_bearerType;
	bool m_simInserted;
	SceUInt16 m_mcc;
	SceUInt16 m_mnc;
	TelSmsPduGenerator *m_pduGenerator;
	bool m_autoRecvSms;
	int m_unusedCounter;
	int m_unusedFlag;
	int m_seqIndex;
	int m_seqCount;
	int m_seqCountdown;
	int m_seqScale;
	int m_registrationState;
};

/* Adapter factory (0x813CFBAC..0x813CFC19) */
enum TelBbmcAdapterKind {
	TEL_BBMC_ADAPTER_HARDWARE = 0,
	TEL_BBMC_ADAPTER_NONE = 1,
	TEL_BBMC_ADAPTER_LOOPBACK = 2
};

class TelBbmcAdapterFactory {
public:
	static TelephonyResult Create(int adapterKind, TelBbmcAdapterInterface **outAdapter);
};

/* Helper bitcount function used by TelBbmcLogManager (0x813DBCDC) */
int telCountSetBits(SceUInt32 mask);

} /* namespace telephony */

#endif /* TELEPHONY_BBMC_ADAPTER_H */
