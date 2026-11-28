/*
 * tel_general_internal.h: internal class declarations for the general
 * telephony logic layer (modem/service interfaces, action executor, API/event
 * parameter containers, COM failure counter, LPM manager, SMC state machine,
 * and full/mini general mediators).
 */
#ifndef TELEPHONY_GENERAL_INTERNAL_H
#define TELEPHONY_GENERAL_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include "../tel_internal.h"

namespace telephony {

/* API identifiers indexed inside TelGeneralLogicActionApiParam (1..54) */
enum TelGeneralApiId {
	TEL_API_NONE = 0,
	TEL_API_TURN_ON = 1,
	TEL_API_GET_STATE = 2,
	TEL_API_SET_FLIGHT_MODE = 3,
	TEL_API_GET_FLIGHT_MODE = 4,
	TEL_API_GET_PHONE_NUMBER = 5,
	TEL_API_GET_ICCID = 6,
	TEL_API_VERIFY_PIN = 7,
	TEL_API_UNBLOCK_PIN = 8,
	TEL_API_SET_PIN_PROTECTION = 9,
	TEL_API_GET_PIN_PROTECTION = 10,
	TEL_API_GET_PIN_STATUS = 11,
	TEL_API_GET_PIN_REMAIN = 12,
	TEL_API_CHANGE_PIN_CODE = 13,
	TEL_API_GET_SIGNAL_LEVEL = 14,
	TEL_API_GET_NETWORK_INFO = 15,
	TEL_API_GET_ROAMING_INFO = 16,
	TEL_API_GET_MCC_MNC_OF_IMSI = 17,
	TEL_API_GET_MCC_MNC_OF_NETWORK = 18,
	TEL_API_SET_APN_INFO = 19,
	TEL_API_RESTORE_APN_INFO = 20,
	TEL_API_GET_OPERATOR_PROFILE_COUNT = 21,
	TEL_API_GET_OPERATOR_PROFILE = 22,
	TEL_API_GET_APN_INFO_LIST_COUNT = 23,
	TEL_API_GET_APN_INFO_LIST_ITEM = 24,
	TEL_API_REMOVE_CURRENT_APN_INFO = 25,
	TEL_API_START_NETWORK_SCAN = 26,
	TEL_API_ABORT_NETWORK_SCAN = 27,
	TEL_API_GET_NETWORK_SCAN_INFO_COUNT = 28,
	TEL_API_GET_NETWORK_SCAN_INFO = 29,
	TEL_API_SET_NET_REGISTER_AUTO = 30,
	TEL_API_SET_NET_REGISTER_INFO = 31,
	TEL_API_GET_NET_REGISTRATION_INFO = 32,
	TEL_API_SET_DATA_ROAMING_PERMISSION = 34,
	TEL_API_SET_DATA_ROAMING_DEFINITION = 35,
	TEL_API_CHECK_SIM_CONDITION = 36,
	TEL_API_IS_OPEN_SIM_MODEL = 37,
	TEL_API_GET_CONDITION_ERROR = 38,
	TEL_API_DEACTIVATE_SIM_LOCK = 39,
	TEL_API_UNBLOCK_SIM_LOCK = 40,
	TEL_API_GET_SIM_LOCK_DEACTIVATE_REMAIN = 41,
	TEL_API_GET_SIM_LOCK_UNBLOCK_REMAIN = 42,
	TEL_API_INTERNAL_VERIFY_PIN = 43,
	TEL_API_INTERNAL_UNBLOCK_PIN = 44,
	TEL_API_INTERNAL_SET_PIN_PROTECTION = 45,
	TEL_API_INTERNAL_CHANGE_PIN_CODE = 46,
	TEL_API_INTERNAL_SET_APN_INFO = 47,
	TEL_API_INTERNAL_SET_APN_INFO_BILLING = 48,
	TEL_API_INTERNAL_RESTORE_APN_INFO = 49,
	TEL_API_INTERNAL_REMOVE_CURRENT_APN_INFO = 50,
	TEL_API_INTERNAL_DEACTIVATE_SIM_LOCK = 51,
	TEL_API_INTERNAL_UNBLOCK_SIM_LOCK = 52,
	TEL_API_CHECK_IMSI_FOR_OPERATOR_CODE = 54,
	TEL_API_COUNT = 55
};

/* Condition error bitmask flags tracked by TelGeneralLogicAction */
enum TelGeneralErrorBit {
	TEL_ERR_BIT_COM_FAILURE = 0x001,
	TEL_ERR_BIT_SIM_DISABLED = 0x002,
	TEL_ERR_BIT_SIM_BLOCKED = 0x004,
	TEL_ERR_BIT_HIGH_TEMP = 0x008,
	TEL_ERR_BIT_SIM_UNUSABLE = 0x010,
	TEL_ERR_BIT_COM_BOOT_TEMP = 0x020,
	TEL_ERR_BIT_LOW_BATTERY = 0x040,
	TEL_ERR_BIT_COM_BOOT_OTHER = 0x080,
	TEL_ERR_BIT_ATTRIBUTE = 0x200,
	TEL_ERR_BIT_BBMC = 0x400
};

/* State codes recorded in TelGeneralLogicAction::m_stateCode */
enum TelGeneralSmStateCode {
	TEL_SM_STATE_INITIAL = 0,
	TEL_SM_STATE_BOOTING = 512,
	TEL_SM_STATE_BOOTED = 769,
	TEL_SM_STATE_INITIALIZING = 1026,
	TEL_SM_STATE_IDLE = 1283,
	TEL_SM_STATE_ERROR = 1540,
	TEL_SM_STATE_FATAL = 1796,
	TEL_SM_STATE_SHUTDOWN = 2304
};

/* Event IDs dispatched through TelGeneralLogicMediator's message thread */
enum TelGeneralEventId {
	TEL_EV_BBMC_COM_BOOT_OK = 517,
	TEL_EV_BBMC_COM_BOOT_NG = 518,
	TEL_EV_BBMC_SETUP_READY = 519,
	TEL_EV_BBMC_COM_DOWN_AT_SUSPEND = 520,
	TEL_EV_BBMC_NETWORK_READY = 521,
	TEL_EV_BBMC_LPM_FROM_NETWORK_READY = 522,

	TEL_EV_SIM_BLOCKED_NORMAL_OP = 772,
	TEL_EV_SIM_PERMANENTLY_BLOCKED = 773,
	TEL_EV_EXCEED_HI_TEMP = 774,
	TEL_EV_FALL_DOWN_LOW_TEMP = 775,
	TEL_EV_ENTER_LOW_BATT = 779,
	TEL_EV_EXIT_LOW_BATT = 780,
	TEL_EV_REGISTRATION_REJECT = 781,
	TEL_EV_INTERNAL_TURN_ON = 784,
	TEL_EV_MEDIATOR_COM_FAILURE = 785,
	TEL_EV_INTERNAL_DEACTIVATE_TEL_REG = 787,
	TEL_EV_INTERNAL_START_NETWORK_SCAN = 791,
	TEL_EV_INTERNAL_VERIFY_PIN = 796,
	TEL_EV_INTERNAL_UNBLOCK_PIN = 797,
	TEL_EV_INTERNAL_SET_PIN_PROTECTION = 798,
	TEL_EV_INTERNAL_CHANGE_PIN_CODE = 799,
	TEL_EV_INTERNAL_SET_APN_INFO = 800,
	TEL_EV_INTERNAL_RESTORE_APN_INFO = 801,
	TEL_EV_INTERNAL_REMOVE_CURRENT_APN_INFO = 803,
	TEL_EV_INTERNAL_DEACTIVATE_SIM_LOCK = 804,
	TEL_EV_INTERNAL_UNBLOCK_SIM_LOCK = 805,
	TEL_EV_INTERNAL_EXIT_FROM_ERROR = 811,
	TEL_EV_INTERNAL_SET_APN_INFO_BILLING = 812,
	TEL_EV_INTERNAL_CAN_NOT_USE_SIM = 814,
	TEL_EV_INTERNAL_SHUTDOWN = 815,
	TEL_EV_INTERNAL_SIM_LOCK_ERROR = 816,
	TEL_EV_INTERNAL_POWER_TICK_EXPIRED = 818,
	TEL_EV_INTERNAL_POWER_TICK_START = 819,
	TEL_EV_INTERNAL_POWER_TICK_STOP = 820,
	TEL_EV_INTERNAL_RESUME = 821,
	TEL_EV_INTERNAL_RE_TURN_ON = 822,
	TEL_EV_INTERNAL_TURN_OFF_SOFT = 823,
	TEL_EV_INTERNAL_BBMC_ERROR = 824,
	TEL_EV_INTERNAL_ENABLE_WWAN = 825,
	TEL_EV_INTERNAL_LIVESPACE_READY = 826,
	TEL_EV_COM_FW_UPDATE_NOTICE = 827,
	TEL_EV_INTERNAL_SET_DATA_ROAMING_PERM = 828,

	TEL_EV_BBMC_COM_FAILURE = 1025,
	TEL_EV_BBMC_STATUS_UPDATE = 1026,
	TEL_EV_BBMC_NETINFO = 1027,
	TEL_EV_BBMC_SIM_ENABLED = 1028,
	TEL_EV_BBMC_SIM_DISABLED = 1029,
	TEL_EV_BBMC_PIN_STATUS_READY = 1033,
	TEL_EV_BBMC_PIN_NOT_REQUIRED = 1034,
	TEL_EV_BBMC_PIN_REQUIRED = 1037,
	TEL_EV_BBMC_PUK_REQUIRED = 1038,
	TEL_EV_BBMC_PIN_PERM_BLOCKED = 1039,
	TEL_EV_BBMC_CHANGE_PIN_FIN = 1056,
	TEL_EV_BBMC_CHANGE_PIN_FAILED = 1057,
	TEL_EV_BBMC_PIN_PROTECTION_FIN = 1058,
	TEL_EV_BBMC_PIN_PROTECTION_FAILED = 1059,
	TEL_EV_BBMC_NET_PROFILE_FIN = 1061,
	TEL_EV_BBMC_NET_PROFILE_FAILED = 1062,
	TEL_EV_BBMC_NET_SCAN_FIN = 1063,
	TEL_EV_BBMC_NET_SCAN_FAILED = 1064,
	TEL_EV_BBMC_NET_REGISTER_FIN = 1067,
	TEL_EV_BBMC_NET_REGISTER_FAILED = 1068,
	TEL_EV_NOTIFY_CONDITION_ERROR = 1070,
	TEL_EV_NOTIFY_NET_STATE_UPDATE = 1074,
	TEL_EV_BBMC_NEED_DEVICE_INFO = 1076,
	TEL_EV_BBMC_PACKET_CALL_END_REASON = 1077,
	TEL_EV_BBMC_SIM_LOCK_INFO = 1083,
	TEL_EV_BBMC_PLMN_MODE_BIT = 1086,
	TEL_EV_LISTENER_GENERAL_INFO = 1089
};

/* Parameter payload for event 812 (SetApnInfo with billing plan) */
class TelSetApnBillingEvent : public TelEventInfo {
public:
	TelSetApnBillingEvent(const TelOperatorApnInfo &apnInfo, bool saveToRegistry)
		: TelEventInfo(TEL_EV_INTERNAL_SET_APN_INFO_BILLING, -1,
			       saveToRegistry ? 1 : 0),
		  m_apnInfo(apnInfo)
	{
		SetCategory(20);
	}
	virtual ~TelSetApnBillingEvent() {}

	const TelOperatorApnInfo &GetApnInfo() const { return m_apnInfo; }

private:
	TelOperatorApnInfo m_apnInfo;
};

/*
 * Modem interface (vtable layout 0x81537678, 17 virtual slots [0..16]).
 * Matches TelephonyModemVtable in plugins.h for all shell callers.
 */
class TelGeneralLogicModemInterface {
public:
	virtual ~TelGeneralLogicModemInterface() {}
	/* slot 2 (+0x08) */
	virtual int StartModem() = 0;
	/* slot 3 (+0x0C) */
	virtual int StopModemSoft() = 0;
	/* slot 4 (+0x10) */
	virtual int StopModemForce() = 0;
	/* slot 5 (+0x14) */
	virtual int AddGeneralListener(TelephonyGeneralListener listener, void *userData) = 0;
	/* slot 6 (+0x18) */
	virtual int RemoveGeneralListener(TelephonyGeneralListener listener, void *userData) = 0;
	/* slot 7 (+0x1C) */
	virtual int GetPhoneNumberString(char *outNumber) = 0;
	/* slot 8 (+0x20) */
	virtual int NeedsInitialCheck() = 0;
	/* slot 9 (+0x24) */
	virtual int CheckSimConditionCode() = 0;
	/* slot 10 (+0x28) */
	virtual int GetControlKeyStateCode() = 0;
	/* slot 11 (+0x2C) */
	virtual int GetControlKeyBlockTypeCode() = 0;
	/* slot 12 (+0x30) */
	virtual int GetAntennaDisplayType() = 0;
	/* slot 13 (+0x34) */
	virtual int CheckContentDownloadCode(int serviceType) = 0;
	/* slot 14 (+0x38) */
	virtual TelephonyResult CheckImsiForOperatorCode(int *inOutOperatorCode,
							 bool *outMatched) = 0;
	/* slot 15 (+0x3C) */
	virtual int GetCountryCode(paf::string *outCountry) = 0;
	/* slot 16 (+0x40) */
	virtual int GetProductTargetOperatorCode(int *outOperator) = 0;
};

/*
 * Service interface (vtable layout 0x815376C4, 82 virtual slots [0..81]).
 * Matches TelephonyServiceVtable in plugins.h and shell_svc_telephony.cpp.
 */
class TelGeneralLogicServiceInterface {
public:
	virtual ~TelGeneralLogicServiceInterface() {}
	/* slot 2 (+0x008) */
	virtual int StopServiceForce() = 0;
	/* slot 3 (+0x00C) */
	virtual TelephonyResult GetGeneralLogicState(int *outState) = 0;
	/* slot 4 (+0x010) */
	virtual TelephonyResult SetFlightMode(int enabled) = 0;
	/* slot 5 (+0x014) */
	virtual TelephonyResult GetFlightMode(bool *outEnabled) = 0;
	/* slot 6 (+0x018) */
	virtual TelephonyResult SetEnableWwan(bool enabled) = 0;
	/* slot 7 (+0x01C) */
	virtual TelephonyResult GetEnableWwan(bool *outEnabled) = 0;
	/* slot 8 (+0x020) */
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber) = 0;
	/* slot 9 (+0x024) */
	virtual TelephonyResult GetImei(paf::string *outImei) = 0;
	/* slot 10 (+0x028) */
	virtual TelephonyResult GetIccid(paf::string *outIccid) = 0;
	/* slot 11 (+0x02C) */
	virtual TelephonyResult VerifyPin(const TelPinCode &pin) = 0;
	/* slot 12 (+0x030) */
	virtual TelephonyResult UnblockPin(const TelUnblockPinInfo &unblockInfo) = 0;
	/* slot 13 (+0x034) */
	virtual TelephonyResult SetPinProtection(bool enable, const TelPinCode &pin) = 0;
	/* slot 14 (+0x038) */
	virtual TelephonyResult GetPinProtection(bool *outEnabled) = 0;
	/* slot 15 (+0x03C) */
	virtual TelephonyResult GetPinStatus(int *outStatus) = 0;
	/* slot 16 (+0x040) */
	virtual TelephonyResult GetPinRemain(int *outRemain) = 0;
	/* slot 17 (+0x044) */
	virtual TelephonyResult ChangePinCode(const TelPinCode &oldPin,
					      const TelPinCode &newPin) = 0;
	/* slot 18 (+0x048) */
	virtual TelephonyResult SetEventCallback(TelGeneralEventCallback callback,
						 void *userData) = 0;
	/* slot 19 (+0x04C) */
	virtual TelephonyResult SetLegacyCallback(TelGeneralLegacyCallback callback,
						  void *userData) = 0;
	/* slot 20 (+0x050) */
	virtual TelephonyResult SetApnInfo(const TelephonyOperator &apnInfo,
					   bool saveToRegistry) = 0;
	/* slot 21 (+0x054) */
	virtual TelephonyResult SetApnInfoWithBillingPlan(const TelOperatorApnInfo &apnInfo,
							  bool saveToRegistry) = 0;
	/* slot 22 (+0x058) */
	virtual TelephonyResult GetUnsupportedProfile() = 0;
	/* slot 23 (+0x05C) */
	virtual TelephonyResult RestoreApnInfo() = 0;
	/* slot 24 (+0x060) */
	virtual TelephonyResult GetMccMncOfImsi(TelephonyPlmn *outPlmn) = 0;
	/* slot 25 (+0x064) */
	virtual TelephonyResult GetMccMncOfNetwork(TelephonyPlmn *outPlmn) = 0;
	/* slot 26 (+0x068) */
	virtual TelephonyResult GetSimActivationState(int *outActivationState) = 0;
	/* slot 27 (+0x06C) */
	virtual TelephonyResult GetProductTargetOperator(int *outOperator) = 0;
	/* slot 28 (+0x070) */
	virtual TelephonyResult GetOperatorProfileCount(int *outCount) = 0;
	/* slot 29 (+0x074) */
	virtual TelephonyResult GetOperatorProfile(int index,
						   TelOperatorProfile *outProfile) = 0;
	/* slot 30 (+0x078) */
	virtual TelephonyResult GetApnInfoListCount(int *outCount) = 0;
	/* slot 31 (+0x07C) */
	virtual TelephonyResult GetApnInfoListItem(int index,
						   TelOperatorApnInfo *outApnInfo) = 0;
	/* slot 32 (+0x080) */
	virtual TelephonyResult GetCurrentApnInfo(TelephonyOperator *outApnInfo) = 0;
	/* slot 33 (+0x084) */
	virtual TelephonyResult RemoveCurrentApnInfo() = 0;
	/* slot 34 (+0x088) */
	virtual TelephonyResult StartNetworkScan() = 0;
	/* slot 35 (+0x08C) */
	virtual TelephonyResult AbortNetworkScan() = 0;
	/* slot 36 (+0x090) */
	virtual TelephonyResult GetNetworkScanInfoCount(int *outCount) = 0;
	/* slot 37 (+0x094) */
	virtual TelephonyResult GetNetworkScanInfo(int index,
						   TelNetScanInfo *outScanInfo) = 0;
	/* slot 38 (+0x098) */
	virtual TelephonyResult SetNetRegisterAuto() = 0;
	/* slot 39 (+0x09C) */
	virtual TelephonyResult SetNetRegisterInfo(const TelNetScanInfo &scanInfo) = 0;
	/* slot 40 (+0x0A0) */
	virtual TelephonyResult GetNetRegistrationInfo(TelNetworkInfo *outInfo) = 0;
	/* slot 41 (+0x0A4) */
	virtual TelephonyResult GetSavedNetworkSelection(TelNetworkInfo *outInfo) = 0;
	/* slot 42 (+0x0A8) */
	virtual TelephonyResult SetRegistrationAppNetMode(int enableRestricted) = 0;
	/* slot 43 (+0x0AC) */
	virtual TelephonyResult SetDataRoamingPermission(bool allow) = 0;
	/* slot 44 (+0x0B0) */
	virtual TelephonyResult GetDataRoamingPermission(bool *outAllowed) = 0;
	/* slot 45 (+0x0B4) */
	virtual TelephonyResult SetDataRoamingDefinition(int definition) = 0;
	/* slot 46 (+0x0B8) */
	virtual TelephonyResult CheckSimCondition() = 0;
	/* slot 47 (+0x0BC) */
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport) = 0;
	/* slot 48 (+0x0C0) */
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport) = 0;
	/* slot 49 (+0x0C4) */
	virtual TelephonyResult GetIsComExist(int *outExists) = 0;
	/* slot 50 (+0x0C8) */
	virtual TelephonyResult GetIsApnInit(int *outInitialized) = 0;
	/* slot 51 (+0x0CC) */
	virtual TelephonyResult IsOpenSimModel(bool *outOpenModel) = 0;
	/* slot 52 (+0x0D0) */
	virtual TelephonyResult GetNetworkSelectionMode(int *outMode) = 0;
	/* slot 53 (+0x0D4) */
	virtual TelephonyResult GetConditionError(TelephonyResult *outConditionError) = 0;
	/* slot 54 (+0x0D8) */
	virtual TelephonyResult DeactivateSimLock(const TelCarrierLockKey &key) = 0;
	/* slot 55 (+0x0DC) */
	virtual TelephonyResult UnblockSimLock(const TelControlKey &key) = 0;
	/* slot 56 (+0x0E0) */
	virtual TelephonyResult GetSimLockDeactivateRemain(int *outRemain) = 0;
	/* slot 57 (+0x0E4) */
	virtual TelephonyResult GetSimLockUnblockRemain(int *outRemain) = 0;
	/* slot 58 (+0x0E8) */
	virtual TelephonyResult GetControlKeyState(int *outState) = 0;
	/* slot 59 (+0x0EC) */
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType) = 0;
	/* slot 60 (+0x0F0) */
	virtual TelephonyResult CheckImsiForOperatorCode(int *inOutOperatorCode,
							 bool *outMatched) = 0;
	/* slot 61 (+0x0F4) */
	virtual TelephonyResult GetCurrentSignalLevel(int *outLevel) = 0;
	/* slot 62 (+0x0F8) */
	virtual TelephonyResult SetBillingPlan(const int *billingPlan) = 0;
	/* slot 63 (+0x0FC) */
	virtual TelephonyResult GetBillingPlan(int *outBillingPlan) = 0;
	/* slot 64 (+0x100) */
	virtual TelephonyResult SetDebugLogEnabled(bool enabled) = 0;
	/* slot 65 (+0x104) */
	virtual TelephonyResult ReadDebugLogLine(paf::string *outLine) = 0;
	/* slot 66 (+0x108) */
	virtual TelephonyResult SetDebugDiagFlag(const int *flagValue) = 0;
	/* slot 67 (+0x10C) */
	virtual TelephonyResult GetDebugDiagFlag(int *outFlagValue) = 0;
	/* slot 68 (+0x110) */
	virtual TelephonyResult SetDebugForceFlag(const int *flagValue) = 0;
	/* slot 69 (+0x114) */
	virtual TelephonyResult GetDebugForceFlag(int *outFlagValue) = 0;
	/* slot 70 (+0x118) */
	virtual TelephonyResult IsSimActivated(int *outActivated) = 0;
	/* slot 71 (+0x11C) */
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion) = 0;
	/* slot 72 (+0x120) */
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion) = 0;
	/* slot 73 (+0x124) */
	virtual TelephonyResult CheckComVersion(bool *outMatches) = 0;
	/* slot 74 (+0x128) */
	virtual TelephonyResult ReleasePdpContext() = 0;
	/* slot 75 (+0x12C) */
	virtual TelephonyResult StartApnListDownload() = 0;
	/* slot 76 (+0x130) */
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType) = 0;
	/* slot 77 (+0x134) */
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes) = 0;
	/* slot 78 (+0x138) */
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType) = 0;
	/* slot 79 (+0x13C) */
	virtual TelephonyResult GetModemFirmwareVersion(paf::string *outVersion) = 0;
	/* slot 80 (+0x140) */
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl) = 0;
	/* slot 81 (+0x144) */
	virtual TelephonyResult NotifyPluginState(const char *pluginName, int stateValue) = 0;
};

/* ------------------------------------------------------------------
 * API & event parameter containers (0x8126904C..0x812693C3)
 * ------------------------------------------------------------------ */

class TelGeneralLogicActionApiParam {
public:
	TelGeneralLogicActionApiParam();
	virtual ~TelGeneralLogicActionApiParam();

	TelephonyResult SetCurrentApiId(int apiId);
	int GetCurrentApiId() const;
	void SetApiResult(int apiId, const TelephonyResult &result);
	TelephonyResult GetApiResult(int apiId) const;

	/* General state / flight mode */
	int stateResult;
	int flightModeInput;
	bool flightModeOutput;

	/* Device / SIM identity */
	TelPhoneNumber phoneNumber;
	paf::string iccid;

	/* PIN / PUK operations */
	TelPinCode verifyPinCode;
	TelUnblockPinInfo unblockPinInfo;
	TelPinCode protectionPinCode;
	bool pinProtectionInput;
	bool pinProtectionOutput;
	int pinStatusOutput;
	int pinRemainOutput;
	TelPinCode changeOldPin;
	TelPinCode changeNewPin;

	/* Signal / network / roaming / PLMN */
	TelAntennaLevelThreshold signalLevel;
	TelNetworkInfo networkInfo;
	TelRoamingInfo roamingInfo;
	TelephonyPlmn imsiPlmn;
	TelephonyPlmn networkPlmn;

	/* APN & operator profiles */
	TelephonyOperator apnCredentials;
	bool saveApnToRegistry;
	int operatorProfileCount;
	int operatorProfileIndex;
	TelOperatorProfile operatorProfile;
	int apnListCount;
	int apnListIndex;
	TelOperatorApnInfo apnListItem;

	/* Network scan & registration */
	int networkScanCount;
	int networkScanIndex;
	TelNetScanInfo networkScanItem;
	TelNetScanInfo manualRegisterTarget;
	TelNetworkInfo netRegistrationInfo;

	/* Roaming / SIM condition / SIM lock */
	bool dataRoamingPermission;
	int dataRoamingDefinition;
	bool openSimModel;
	TelephonyResult conditionError;
	TelCarrierLockKey deactivateSimLockKey;
	TelControlKey unblockSimLockKey;
	int simLockDeactivateRemain;
	int simLockUnblockRemain;

	/* Internal async worker inputs */
	TelPinCode internalVerifyPin;
	TelUnblockPinInfo internalUnblockPin;
	TelPinCode internalProtectionPin;
	bool internalProtectionEnable;
	paf::vector<TelPinCode> internalChangePins;
	TelephonyOperator internalApnCredentials;
	bool internalApnSaveFlag;
	TelOperatorApnInfo internalBillingApn;
	bool internalBillingSaveFlag;
	TelCarrierLockKey internalDeactivateKey;
	TelControlKey internalUnblockKey;

	/* IMSI operator check */
	int checkOperatorCode;
	bool checkOperatorMatched;

private:
	int m_currentApiId;
	TelephonyResult m_apiResults[TEL_API_COUNT];
};

class TelGeneralLogicActionEventParam {
public:
	TelGeneralLogicActionEventParam();
	virtual ~TelGeneralLogicActionEventParam();

	void SetEvent(const TelEventInfo &event);
	void GetEvent(TelEventInfo *outEvent) const;

private:
	TelEventInfo m_event;
};

/* ------------------------------------------------------------------
 * COM failure rolling-window counter (0x812693C4..0x8126958B)
 * ------------------------------------------------------------------ */

class TelGeneralComFailureCounter {
public:
	TelGeneralComFailureCounter();
	~TelGeneralComFailureCounter();

	void SetThreshold(int maxFailures, int windowSeconds);
	TelephonyResult RecordFailure(int reasonCode);
	void Reset();
	int GetFailureCodeAt(unsigned int index) const;
	bool IsThresholdExceeded() const { return m_thresholdExceeded; }
	unsigned int GetFailureCount() const { return (unsigned int)m_failureCodes.size(); }

private:
	SceUInt64 m_windowUsec;
	unsigned int m_maxFailures;
	SceUInt64 m_windowStartUsec;
	bool m_thresholdExceeded;
	paf::vector<int> m_failureCodes;
};

/* ------------------------------------------------------------------
 * Low Power Mode manager (0x8126958C..0x81269BFF)
 * ------------------------------------------------------------------ */

class TelGeneralLpmManager : public TelObservable {
public:
	explicit TelGeneralLpmManager(TelBbmcAdapterInterface *bbmcAdapter);
	virtual ~TelGeneralLpmManager();

	TelephonyResult ApplyWwanEnableChange(bool wwanEnabled);
	TelephonyResult ApplyFlightModeChange(bool flightModeOn);
	TelephonyResult ApplyCurrentAttributes();
	TelephonyResult SetLowPowerModeImmediate(bool enableLpm);
	TelephonyResult SetForcedLpm(bool forced);
	TelephonyResult GetCurrentLpmState(bool *outIsLpm);

private:
	TelephonyResult SetBbmcLpm(bool enableLpm, bool syncCall);

	TelBbmcAdapterInterface *m_bbmcAdapter;
	paf::thread::Mutex m_stateMutex;
	bool m_forcedLpm;
	bool m_currentLpm;
};

/* ------------------------------------------------------------------
 * General Logic Action executor (0x81261068..0x81268F93)
 * ------------------------------------------------------------------ */

class TelGeneralLogicAction : public TelGeneralLogicActionApiParam,
			      public TelGeneralLogicActionEventParam,
			      public TelObservable {
public:
	TelGeneralLogicAction();
	virtual ~TelGeneralLogicAction();

	TelephonyResult Initialize(TelObserver *mediatorObserver,
				   TelBbmcAdapterInterface *bbmcAdapter,
				   TelGeneralLogicNetCtl *netCtl);
	TelephonyResult Terminate();

	void NotifyMediatorEvent(const TelEventInfo &event);
	TelephonyResult SetEventCallback(TelGeneralEventCallback callback, void *userData);
	TelephonyResult SetLegacyCallback(TelGeneralLegacyCallback callback, void *userData);
	void DispatchToCallbacks(const TelEventInfo &event);

	bool HasErrorFlag(SceUInt32 mask);
	bool HasAnyError();
	void SetErrorFlag(SceUInt32 mask);
	void ClearErrorFlag(SceUInt32 mask);
	int GetStateCode();
	void SetStateCode(int stateCode);
	TelephonyResult GetHighestPriorityError();
	void NotifyConditionError();

	/* State entry & setup helpers */
	void EnterStateCode(int stateCode);
	void EnterIdleState();
	void EnterFatalState();
	void EnterErrorState();
	void ForceLpmMode();
	bool IsSimPermanentlyBlockedNow();

	/* BBMC event handlers called by SMC states */
	void HandleBbmcComBootOk();
	void HandleBbmcComBootNg();
	void HandleBbmcSetupReady();
	void HandleBbmcNetworkReady();
	void HandleBbmcLpmFromNetworkReady();
	void HandleBbmcComDownAtSuspend();
	void HandleBbmcComFailure();
	void HandleBbmcStatusUpdate();
	void HandleBbmcNetinfo();
	void HandleBbmcSimEnabled();
	void HandleBbmcSimDisabled();
	void HandleBbmcPinStatusReady();
	void HandleBbmcPinNotRequired();
	void HandleBbmcPinRequired();
	void HandleBbmcPukRequired();
	void HandleBbmcPinPermanentlyBlocked();
	void HandleBbmcSimBlockedNormalOperation();
	void HandleBbmcSimPermanentlyBlocked();
	void HandleBbmcSimLockInfo();
	void HandleBbmcPlmnModeBit();
	void HandleBbmcEnterLowBatt();
	void HandleBbmcExitLowBatt();
	void HandleBbmcExceedHiTemp();
	void HandleBbmcFallDownLowTemp();
	void HandleBbmcRegistrationReject();
	void HandleBbmcNeedUpdatingDeviceInfo();
	void HandleBbmcNetScanFin();
	void HandleBbmcNetScanFailed();
	void HandleBbmcPacketCallEndReasonUpdate();
	void HandleBbmcForwardEvent();

	/* Mediator internal event handlers called by SMC states */
	void HandleAttributeError();
	void HandleMediatorComFailure();
	void HandleMediatorInternalShutdown();
	void HandleMediatorInternalSimLockError();
	void HandleMediatorInternalBbmcError();
	void HandleMediatorInternalCanNotUseSim();
	void HandleMediatorInternalExitFromErrorState();
	void HandleMediatorInternalReTurnOn();
	void HandleMediatorInternalResume();
	void HandleMediatorInternalEnableWwan();
	void HandleMediatorInternalLivespaceReady();
	void HandleMediatorInternalDeactivateTelReg();
	void HandleMediatorInternalStartNetworkScan();
	void HandleMediatorInternalVerifyPin();
	void HandleMediatorInternalUnblockPin();
	void HandleMediatorInternalSetPinProtection();
	void HandleMediatorInternalChangePinCode();
	void HandleMediatorInternalSetApnInfo();
	void HandleMediatorInternalSetApnInfoWithBillingPlan();
	void HandleMediatorInternalRestoreApnInfo();
	void HandleMediatorInternalRemoveCurrentApnInfo();
	void HandleMediatorInternalSetDataRoamingPermission();
	void HandleMediatorInternalDeactivateSimLock();
	void HandleMediatorInternalUnblockSimLock();

	/* Public API action handlers called by SMC states */
	void ActionTurnOn();
	void ActionGetGeneralLogicState();
	void ActionSetFlightMode();
	void ActionGetFlightMode();
	void ActionGetPhoneNumber();
	void ActionGetIccid();
	void ActionVerifyPin();
	void ActionUnblockPin();
	void ActionSetPinProtection();
	void ActionGetPinProtection();
	void ActionGetPinStatus();
	void ActionGetPinRemain();
	void ActionChangePinCode();
	void ActionGetSignalLevel();
	void ActionGetNetworkInfo();
	void ActionGetRoamingInfo();
	void ActionGetMccMncOfImsi();
	void ActionGetMccMncOfNetwork();
	void ActionSetApnInfo();
	void ActionRestoreApnInfo();
	void ActionGetOperatorProfileCount();
	void ActionGetOperatorProfile();
	void ActionGetApnInfoListCount();
	void ActionGetApnInfoListItem();
	void ActionRemoveCurrentApnInfo();
	void ActionStartNetworkScan();
	void ActionAbortNetworkScan();
	void ActionGetNetworkScanInfoCount();
	void ActionGetNetworkScanInfo();
	void ActionSetNetRegisterAuto();
	void ActionSetNetRegisterInfo();
	void ActionGetNetRegistrationInfo();
	void ActionSetDataRoamingPermission();
	void ActionSetDataRoamingDefinition();
	void ActionCheckSimCondition();
	void ActionIsOpenSimModel();
	void ActionGetConditionError();
	void ActionDeactivateSimLock();
	void ActionUnblockSimLock();
	void ActionGetSimLockDeactivateRemain();
	void ActionGetSimLockUnblockRemain();
	void ActionCheckImsiForOperatorCode();

	/* Fallback handler for API calls rejected in the current SMC state */
	void HandleUnexpectedTransition();

	/* Direct helper used by TelGeneralLogicMediator */
	TelephonyResult SetBbmcTurnOff(bool forceStop);
	void NotifyCachedErrorIfPresent();

	const TelephonyPlmn &GetCachedImsiPlmn() const { return m_imsiPlmn; }
	const paf::vector<TelOperatorProfile> &GetOperatorProfiles() const { return m_operatorProfiles; }
	TelGeneralLpmManager *GetLpmManager() const { return m_lpmManager; }

private:
	TelephonyResult LoadImsiAndProfiles();
	TelephonyResult LoadApnListForImsi();
	TelephonyResult ApplyInitialApnSettings();
	TelephonyResult ApplyDataRoamingControl();
	TelephonyResult ApplyRssiThresholds();
	TelephonyResult ApplyNetworkSelection();
	TelephonyResult RegisterNetworkAutoAndSave();
	TelephonyResult SaveApnToRegistry(const TelephonyOperator &apnCredentials,
					  int billingPlan, bool writeBillingPlan);
	void CheckAndNotifyComFirmwareUpdate();
	void TryExitErrorState();

	paf::thread::Mutex m_callbackMutex;
	TelGeneralEventCallback m_eventCallback;
	void *m_callbackUserData;
	TelGeneralLegacyCallback m_legacyCallback;
	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelGeneralLogicNetCtl *m_netCtl;
	TelephonyPlmn m_imsiPlmn;
	paf::vector<TelOperatorProfile> m_operatorProfiles;
	TelApnInfoList m_apnInfoList;
	TelNetScanInfoList m_scanInfoList;
	paf::vector<TelephonyPlmn> m_supportedPlmns;
	TelGeneralComFailureCounter m_comFailureCounter;
	paf::thread::Mutex m_stateMutex;
	int m_stateCode;
	SceUInt32 m_errorFlags;
	TelGeneralLpmManager *m_lpmManager;
	bool m_simInserted;
	bool m_pinReady;
	bool m_setupReady;
	bool m_networkReady;
	bool m_livespaceReady;
	bool m_comFwChecked;
	bool m_deviceInfoReady;
	int m_dataRoamingDef;
};

/* ------------------------------------------------------------------
 * SMC State Machine (0x8125F4B0..0x81260037)
 * ------------------------------------------------------------------ */

class TelGeneralLogicStateMachineContext;

class TelGeneralLogicStateMachineState : public statemap::State {
public:
	TelGeneralLogicStateMachineState(const char *name, int stateId)
		: statemap::State(name, stateId)
	{
	}
	virtual ~TelGeneralLogicStateMachineState() {}

	virtual void Entry(TelGeneralLogicStateMachineContext &context) { (void)context; }
	virtual void Exit(TelGeneralLogicStateMachineContext &context) { (void)context; }

	/* Transitions (exact Sony names in SMC alphabetical order) */
	virtual void AbortNetworkScan(TelGeneralLogicStateMachineContext &context);
	virtual void ChangePinCode(TelGeneralLogicStateMachineContext &context);
	virtual void CheckImsiForOperatorCode(TelGeneralLogicStateMachineContext &context);
	virtual void CheckSimCondition(TelGeneralLogicStateMachineContext &context);
	virtual void DeactivateSimLock(TelGeneralLogicStateMachineContext &context);
	virtual void GetFlightMode(TelGeneralLogicStateMachineContext &context);
	virtual void GetApnInfoListCount(TelGeneralLogicStateMachineContext &context);
	virtual void GetApnInfoListItem(TelGeneralLogicStateMachineContext &context);
	virtual void GetConditionError(TelGeneralLogicStateMachineContext &context);
	virtual void GetGeneralLogicState(TelGeneralLogicStateMachineContext &context);
	virtual void GetIccid(TelGeneralLogicStateMachineContext &context);
	virtual void GetMccMncOfImsi(TelGeneralLogicStateMachineContext &context);
	virtual void GetMccMncOfNetwork(TelGeneralLogicStateMachineContext &context);
	virtual void GetNetRegistrationInfo(TelGeneralLogicStateMachineContext &context);
	virtual void GetNetworkInfo(TelGeneralLogicStateMachineContext &context);
	virtual void GetNetworkScanInfo(TelGeneralLogicStateMachineContext &context);
	virtual void GetNetworkScanInfoCount(TelGeneralLogicStateMachineContext &context);
	virtual void GetOperatorProfile(TelGeneralLogicStateMachineContext &context);
	virtual void GetOperatorProfileCount(TelGeneralLogicStateMachineContext &context);
	virtual void GetPhoneNumber(TelGeneralLogicStateMachineContext &context);
	virtual void GetPinProtection(TelGeneralLogicStateMachineContext &context);
	virtual void GetPinRemain(TelGeneralLogicStateMachineContext &context);
	virtual void GetPinStatus(TelGeneralLogicStateMachineContext &context);
	virtual void GetRoamingInfo(TelGeneralLogicStateMachineContext &context);
	virtual void GetSignalLevel(TelGeneralLogicStateMachineContext &context);
	virtual void GetSimLockDeactivateRemain(TelGeneralLogicStateMachineContext &context);
	virtual void GetSimLockUnblockRemain(TelGeneralLogicStateMachineContext &context);
	virtual void IsOpenSimModel(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventAttributeError(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcChangePinCodeFailed(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcChangePinCodeFin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcComBootNg(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcComBootOk(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcComDownAtSuspend(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcComFailure(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcEnterLowBatt(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcExceedHiTemp(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcExitLowBatt(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcFallDownLowTemp(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcLpmFromNetworkReady(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNeedUpdatingDeviceInfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetProfileSettingFailed(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetProfileSettingFin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetRegisterSettingFailed(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetRegisterSettingFin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetScanFailed(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetScanFin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetinfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcNetworkReady(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPacketCallEndReasonUpdate(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinNotRequired(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinPermanentlyBlocked(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinProtectionFailed(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinProtectionFin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinRequired(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPinStatusReady(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPlmnModeBit(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcRegistrationReject(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcSetupReady(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcSimBlockedNormalOperation(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcSimLockInfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcSimPermanentlyBlocked(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventBbmcStatusUpdate(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorComFailure(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalBbmcError(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalCanNotUseSim(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalChangePinCode(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalDeactivateSimLock(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalDeactivateTelReg(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalEnableWwan(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalExitFromErrorState(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalLivespaceReady(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalReTurnOn(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalRemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalRestoreApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalResume(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalSetApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalSetApnInfoWithBillingPlan(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalSetDataRoamingPermission(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalSetPinProtection(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalShutdown(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalSimLockError(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalStartNetworkScan(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalUnblockPin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalUnblockSimLock(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventMediatorInternalVerifyPin(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventNotifyCallback(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventSimDisabled(TelGeneralLogicStateMachineContext &context);
	virtual void OnEventSimEnabled(TelGeneralLogicStateMachineContext &context);
	virtual void RemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void RestoreApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void SetApnInfo(TelGeneralLogicStateMachineContext &context);
	virtual void SetDataRoamingDefinition(TelGeneralLogicStateMachineContext &context);
	virtual void SetDataRoamingPermission(TelGeneralLogicStateMachineContext &context);
	virtual void SetFlightMode(TelGeneralLogicStateMachineContext &context);
	virtual void SetNetRegisterAuto(TelGeneralLogicStateMachineContext &context);
	virtual void SetNetRegisterInfo(TelGeneralLogicStateMachineContext &context);
	virtual void SetPinProtection(TelGeneralLogicStateMachineContext &context);
	virtual void StartNetworkScan(TelGeneralLogicStateMachineContext &context);
	virtual void TurnOn(TelGeneralLogicStateMachineContext &context);
	virtual void UnblockPin(TelGeneralLogicStateMachineContext &context);
	virtual void UnblockSimLock(TelGeneralLogicStateMachineContext &context);
	virtual void VerifyPin(TelGeneralLogicStateMachineContext &context);
	virtual void Default(TelGeneralLogicStateMachineContext &context);
};

class TelGeneralLogicStateMachineContext : public statemap::FSMContext {
public:
	explicit TelGeneralLogicStateMachineContext(TelGeneralLogicAction &owner);
	virtual ~TelGeneralLogicStateMachineContext() {}

	virtual void enterStartState();

	TelGeneralLogicAction &getOwner() { return m_owner; }
	TelGeneralLogicStateMachineState &getState()
	{
		return *static_cast<TelGeneralLogicStateMachineState *>(m_state);
	}

	void AbortNetworkScan() { setTransition("AbortNetworkScan"); getState().AbortNetworkScan(*this); setTransition(NULL); }
	void ChangePinCode() { setTransition("ChangePinCode"); getState().ChangePinCode(*this); setTransition(NULL); }
	void CheckImsiForOperatorCode() { setTransition("CheckImsiForOperatorCode"); getState().CheckImsiForOperatorCode(*this); setTransition(NULL); }
	void CheckSimCondition() { setTransition("CheckSimCondition"); getState().CheckSimCondition(*this); setTransition(NULL); }
	void DeactivateSimLock() { setTransition("DeactivateSimLock"); getState().DeactivateSimLock(*this); setTransition(NULL); }
	void GetFlightMode() { setTransition("GetFlightMode"); getState().GetFlightMode(*this); setTransition(NULL); }
	void GetApnInfoListCount() { setTransition("GetApnInfoListCount"); getState().GetApnInfoListCount(*this); setTransition(NULL); }
	void GetApnInfoListItem() { setTransition("GetApnInfoListItem"); getState().GetApnInfoListItem(*this); setTransition(NULL); }
	void GetConditionError() { setTransition("GetConditionError"); getState().GetConditionError(*this); setTransition(NULL); }
	void GetGeneralLogicState() { setTransition("GetGeneralLogicState"); getState().GetGeneralLogicState(*this); setTransition(NULL); }
	void GetIccid() { setTransition("GetIccid"); getState().GetIccid(*this); setTransition(NULL); }
	void GetMccMncOfImsi() { setTransition("GetMccMncOfImsi"); getState().GetMccMncOfImsi(*this); setTransition(NULL); }
	void GetMccMncOfNetwork() { setTransition("GetMccMncOfNetwork"); getState().GetMccMncOfNetwork(*this); setTransition(NULL); }
	void GetNetRegistrationInfo() { setTransition("GetNetRegistrationInfo"); getState().GetNetRegistrationInfo(*this); setTransition(NULL); }
	void GetNetworkInfo() { setTransition("GetNetworkInfo"); getState().GetNetworkInfo(*this); setTransition(NULL); }
	void GetNetworkScanInfo() { setTransition("GetNetworkScanInfo"); getState().GetNetworkScanInfo(*this); setTransition(NULL); }
	void GetNetworkScanInfoCount() { setTransition("GetNetworkScanInfoCount"); getState().GetNetworkScanInfoCount(*this); setTransition(NULL); }
	void GetOperatorProfile() { setTransition("GetOperatorProfile"); getState().GetOperatorProfile(*this); setTransition(NULL); }
	void GetOperatorProfileCount() { setTransition("GetOperatorProfileCount"); getState().GetOperatorProfileCount(*this); setTransition(NULL); }
	void GetPhoneNumber() { setTransition("GetPhoneNumber"); getState().GetPhoneNumber(*this); setTransition(NULL); }
	void GetPinProtection() { setTransition("GetPinProtection"); getState().GetPinProtection(*this); setTransition(NULL); }
	void GetPinRemain() { setTransition("GetPinRemain"); getState().GetPinRemain(*this); setTransition(NULL); }
	void GetPinStatus() { setTransition("GetPinStatus"); getState().GetPinStatus(*this); setTransition(NULL); }
	void GetRoamingInfo() { setTransition("GetRoamingInfo"); getState().GetRoamingInfo(*this); setTransition(NULL); }
	void GetSignalLevel() { setTransition("GetSignalLevel"); getState().GetSignalLevel(*this); setTransition(NULL); }
	void GetSimLockDeactivateRemain() { setTransition("GetSimLockDeactivateRemain"); getState().GetSimLockDeactivateRemain(*this); setTransition(NULL); }
	void GetSimLockUnblockRemain() { setTransition("GetSimLockUnblockRemain"); getState().GetSimLockUnblockRemain(*this); setTransition(NULL); }
	void IsOpenSimModel() { setTransition("IsOpenSimModel"); getState().IsOpenSimModel(*this); setTransition(NULL); }
	void OnEventAttributeError() { setTransition("OnEventAttributeError"); getState().OnEventAttributeError(*this); setTransition(NULL); }
	void OnEventBbmcChangePinCodeFailed() { setTransition("OnEventBbmcChangePinCodeFailed"); getState().OnEventBbmcChangePinCodeFailed(*this); setTransition(NULL); }
	void OnEventBbmcChangePinCodeFin() { setTransition("OnEventBbmcChangePinCodeFin"); getState().OnEventBbmcChangePinCodeFin(*this); setTransition(NULL); }
	void OnEventBbmcComBootNg() { setTransition("OnEventBbmcComBootNg"); getState().OnEventBbmcComBootNg(*this); setTransition(NULL); }
	void OnEventBbmcComBootOk() { setTransition("OnEventBbmcComBootOk"); getState().OnEventBbmcComBootOk(*this); setTransition(NULL); }
	void OnEventBbmcComDownAtSuspend() { setTransition("OnEventBbmcComDownAtSuspend"); getState().OnEventBbmcComDownAtSuspend(*this); setTransition(NULL); }
	void OnEventBbmcComFailure() { setTransition("OnEventBbmcComFailure"); getState().OnEventBbmcComFailure(*this); setTransition(NULL); }
	void OnEventBbmcEnterLowBatt() { setTransition("OnEventBbmcEnterLowBatt"); getState().OnEventBbmcEnterLowBatt(*this); setTransition(NULL); }
	void OnEventBbmcExceedHiTemp() { setTransition("OnEventBbmcExceedHiTemp"); getState().OnEventBbmcExceedHiTemp(*this); setTransition(NULL); }
	void OnEventBbmcExitLowBatt() { setTransition("OnEventBbmcExitLowBatt"); getState().OnEventBbmcExitLowBatt(*this); setTransition(NULL); }
	void OnEventBbmcFallDownLowTemp() { setTransition("OnEventBbmcFallDownLowTemp"); getState().OnEventBbmcFallDownLowTemp(*this); setTransition(NULL); }
	void OnEventBbmcLpmFromNetworkReady() { setTransition("OnEventBbmcLpmFromNetworkReady"); getState().OnEventBbmcLpmFromNetworkReady(*this); setTransition(NULL); }
	void OnEventBbmcNeedUpdatingDeviceInfo() { setTransition("OnEventBbmcNeedUpdatingDeviceInfo"); getState().OnEventBbmcNeedUpdatingDeviceInfo(*this); setTransition(NULL); }
	void OnEventBbmcNetProfileSettingFailed() { setTransition("OnEventBbmcNetProfileSettingFailed"); getState().OnEventBbmcNetProfileSettingFailed(*this); setTransition(NULL); }
	void OnEventBbmcNetProfileSettingFin() { setTransition("OnEventBbmcNetProfileSettingFin"); getState().OnEventBbmcNetProfileSettingFin(*this); setTransition(NULL); }
	void OnEventBbmcNetRegisterSettingFailed() { setTransition("OnEventBbmcNetRegisterSettingFailed"); getState().OnEventBbmcNetRegisterSettingFailed(*this); setTransition(NULL); }
	void OnEventBbmcNetRegisterSettingFin() { setTransition("OnEventBbmcNetRegisterSettingFin"); getState().OnEventBbmcNetRegisterSettingFin(*this); setTransition(NULL); }
	void OnEventBbmcNetScanFailed() { setTransition("OnEventBbmcNetScanFailed"); getState().OnEventBbmcNetScanFailed(*this); setTransition(NULL); }
	void OnEventBbmcNetScanFin() { setTransition("OnEventBbmcNetScanFin"); getState().OnEventBbmcNetScanFin(*this); setTransition(NULL); }
	void OnEventBbmcNetinfo() { setTransition("OnEventBbmcNetinfo"); getState().OnEventBbmcNetinfo(*this); setTransition(NULL); }
	void OnEventBbmcNetworkReady() { setTransition("OnEventBbmcNetworkReady"); getState().OnEventBbmcNetworkReady(*this); setTransition(NULL); }
	void OnEventBbmcPacketCallEndReasonUpdate() { setTransition("OnEventBbmcPacketCallEndReasonUpdate"); getState().OnEventBbmcPacketCallEndReasonUpdate(*this); setTransition(NULL); }
	void OnEventBbmcPinNotRequired() { setTransition("OnEventBbmcPinNotRequired"); getState().OnEventBbmcPinNotRequired(*this); setTransition(NULL); }
	void OnEventBbmcPinPermanentlyBlocked() { setTransition("OnEventBbmcPinPermanentlyBlocked"); getState().OnEventBbmcPinPermanentlyBlocked(*this); setTransition(NULL); }
	void OnEventBbmcPinProtectionFailed() { setTransition("OnEventBbmcPinProtectionFailed"); getState().OnEventBbmcPinProtectionFailed(*this); setTransition(NULL); }
	void OnEventBbmcPinProtectionFin() { setTransition("OnEventBbmcPinProtectionFin"); getState().OnEventBbmcPinProtectionFin(*this); setTransition(NULL); }
	void OnEventBbmcPinRequired() { setTransition("OnEventBbmcPinRequired"); getState().OnEventBbmcPinRequired(*this); setTransition(NULL); }
	void OnEventBbmcPinStatusReady() { setTransition("OnEventBbmcPinStatusReady"); getState().OnEventBbmcPinStatusReady(*this); setTransition(NULL); }
	void OnEventBbmcPlmnModeBit() { setTransition("OnEventBbmcPlmnModeBit"); getState().OnEventBbmcPlmnModeBit(*this); setTransition(NULL); }
	void OnEventBbmcPukRequired() { setTransition("OnEventBbmcPukRequired"); getState().OnEventBbmcPukRequired(*this); setTransition(NULL); }
	void OnEventBbmcRegistrationReject() { setTransition("OnEventBbmcRegistrationReject"); getState().OnEventBbmcRegistrationReject(*this); setTransition(NULL); }
	void OnEventBbmcSetupReady() { setTransition("OnEventBbmcSetupReady"); getState().OnEventBbmcSetupReady(*this); setTransition(NULL); }
	void OnEventBbmcSimBlockedNormalOperation() { setTransition("OnEventBbmcSimBlockedNormalOperation"); getState().OnEventBbmcSimBlockedNormalOperation(*this); setTransition(NULL); }
	void OnEventBbmcSimLockInfo() { setTransition("OnEventBbmcSimLockInfo"); getState().OnEventBbmcSimLockInfo(*this); setTransition(NULL); }
	void OnEventBbmcSimPermanentlyBlocked() { setTransition("OnEventBbmcSimPermanentlyBlocked"); getState().OnEventBbmcSimPermanentlyBlocked(*this); setTransition(NULL); }
	void OnEventBbmcStatusUpdate() { setTransition("OnEventBbmcStatusUpdate"); getState().OnEventBbmcStatusUpdate(*this); setTransition(NULL); }
	void OnEventMediatorComFailure() { setTransition("OnEventMediatorComFailure"); getState().OnEventMediatorComFailure(*this); setTransition(NULL); }
	void OnEventMediatorInternalBbmcError() { setTransition("OnEventMediatorInternalBbmcError"); getState().OnEventMediatorInternalBbmcError(*this); setTransition(NULL); }
	void OnEventMediatorInternalCanNotUseSim() { setTransition("OnEventMediatorInternalCanNotUseSim"); getState().OnEventMediatorInternalCanNotUseSim(*this); setTransition(NULL); }
	void OnEventMediatorInternalChangePinCode() { setTransition("OnEventMediatorInternalChangePinCode"); getState().OnEventMediatorInternalChangePinCode(*this); setTransition(NULL); }
	void OnEventMediatorInternalDeactivateSimLock() { setTransition("OnEventMediatorInternalDeactivateSimLock"); getState().OnEventMediatorInternalDeactivateSimLock(*this); setTransition(NULL); }
	void OnEventMediatorInternalDeactivateTelReg() { setTransition("OnEventMediatorInternalDeactivateTelReg"); getState().OnEventMediatorInternalDeactivateTelReg(*this); setTransition(NULL); }
	void OnEventMediatorInternalEnableWwan() { setTransition("OnEventMediatorInternalEnableWwan"); getState().OnEventMediatorInternalEnableWwan(*this); setTransition(NULL); }
	void OnEventMediatorInternalExitFromErrorState() { setTransition("OnEventMediatorInternalExitFromErrorState"); getState().OnEventMediatorInternalExitFromErrorState(*this); setTransition(NULL); }
	void OnEventMediatorInternalLivespaceReady() { setTransition("OnEventMediatorInternalLivespaceReady"); getState().OnEventMediatorInternalLivespaceReady(*this); setTransition(NULL); }
	void OnEventMediatorInternalReTurnOn() { setTransition("OnEventMediatorInternalReTurnOn"); getState().OnEventMediatorInternalReTurnOn(*this); setTransition(NULL); }
	void OnEventMediatorInternalRemoveCurrentApnInfo() { setTransition("OnEventMediatorInternalRemoveCurrentApnInfo"); getState().OnEventMediatorInternalRemoveCurrentApnInfo(*this); setTransition(NULL); }
	void OnEventMediatorInternalRestoreApnInfo() { setTransition("OnEventMediatorInternalRestoreApnInfo"); getState().OnEventMediatorInternalRestoreApnInfo(*this); setTransition(NULL); }
	void OnEventMediatorInternalResume() { setTransition("OnEventMediatorInternalResume"); getState().OnEventMediatorInternalResume(*this); setTransition(NULL); }
	void OnEventMediatorInternalSetApnInfo() { setTransition("OnEventMediatorInternalSetApnInfo"); getState().OnEventMediatorInternalSetApnInfo(*this); setTransition(NULL); }
	void OnEventMediatorInternalSetApnInfoWithBillingPlan() { setTransition("OnEventMediatorInternalSetApnInfoWithBillingPlan"); getState().OnEventMediatorInternalSetApnInfoWithBillingPlan(*this); setTransition(NULL); }
	void OnEventMediatorInternalSetDataRoamingPermission() { setTransition("OnEventMediatorInternalSetDataRoamingPermission"); getState().OnEventMediatorInternalSetDataRoamingPermission(*this); setTransition(NULL); }
	void OnEventMediatorInternalSetPinProtection() { setTransition("OnEventMediatorInternalSetPinProtection"); getState().OnEventMediatorInternalSetPinProtection(*this); setTransition(NULL); }
	void OnEventMediatorInternalShutdown() { setTransition("OnEventMediatorInternalShutdown"); getState().OnEventMediatorInternalShutdown(*this); setTransition(NULL); }
	void OnEventMediatorInternalSimLockError() { setTransition("OnEventMediatorInternalSimLockError"); getState().OnEventMediatorInternalSimLockError(*this); setTransition(NULL); }
	void OnEventMediatorInternalStartNetworkScan() { setTransition("OnEventMediatorInternalStartNetworkScan"); getState().OnEventMediatorInternalStartNetworkScan(*this); setTransition(NULL); }
	void OnEventMediatorInternalUnblockPin() { setTransition("OnEventMediatorInternalUnblockPin"); getState().OnEventMediatorInternalUnblockPin(*this); setTransition(NULL); }
	void OnEventMediatorInternalUnblockSimLock() { setTransition("OnEventMediatorInternalUnblockSimLock"); getState().OnEventMediatorInternalUnblockSimLock(*this); setTransition(NULL); }
	void OnEventMediatorInternalVerifyPin() { setTransition("OnEventMediatorInternalVerifyPin"); getState().OnEventMediatorInternalVerifyPin(*this); setTransition(NULL); }
	void OnEventNotifyCallback() { setTransition("OnEventNotifyCallback"); getState().OnEventNotifyCallback(*this); setTransition(NULL); }
	void OnEventSimDisabled() { setTransition("OnEventSimDisabled"); getState().OnEventSimDisabled(*this); setTransition(NULL); }
	void OnEventSimEnabled() { setTransition("OnEventSimEnabled"); getState().OnEventSimEnabled(*this); setTransition(NULL); }
	void RemoveCurrentApnInfo() { setTransition("RemoveCurrentApnInfo"); getState().RemoveCurrentApnInfo(*this); setTransition(NULL); }
	void RestoreApnInfo() { setTransition("RestoreApnInfo"); getState().RestoreApnInfo(*this); setTransition(NULL); }
	void SetApnInfo() { setTransition("SetApnInfo"); getState().SetApnInfo(*this); setTransition(NULL); }
	void SetDataRoamingDefinition() { setTransition("SetDataRoamingDefinition"); getState().SetDataRoamingDefinition(*this); setTransition(NULL); }
	void SetDataRoamingPermission() { setTransition("SetDataRoamingPermission"); getState().SetDataRoamingPermission(*this); setTransition(NULL); }
	void SetFlightMode() { setTransition("SetFlightMode"); getState().SetFlightMode(*this); setTransition(NULL); }
	void SetNetRegisterAuto() { setTransition("SetNetRegisterAuto"); getState().SetNetRegisterAuto(*this); setTransition(NULL); }
	void SetNetRegisterInfo() { setTransition("SetNetRegisterInfo"); getState().SetNetRegisterInfo(*this); setTransition(NULL); }
	void SetPinProtection() { setTransition("SetPinProtection"); getState().SetPinProtection(*this); setTransition(NULL); }
	void StartNetworkScan() { setTransition("StartNetworkScan"); getState().StartNetworkScan(*this); setTransition(NULL); }
	void TurnOn() { setTransition("TurnOn"); getState().TurnOn(*this); setTransition(NULL); }
	void UnblockPin() { setTransition("UnblockPin"); getState().UnblockPin(*this); setTransition(NULL); }
	void UnblockSimLock() { setTransition("UnblockSimLock"); getState().UnblockSimLock(*this); setTransition(NULL); }
	void VerifyPin() { setTransition("VerifyPin"); getState().VerifyPin(*this); setTransition(NULL); }

private:
	TelGeneralLogicAction &m_owner;
};

/* Registered general listener entry (event 1089) */
struct TelGeneralListenerRegistration {
	TelephonyGeneralListener callback;
	void *userData;
};

/* ------------------------------------------------------------------
 * Full General Logic Mediator (0x81255774..0x8125EF83)
 * ------------------------------------------------------------------ */

class TelGeneralLogicMediator : public TelGeneralLogicMediatorBase,
				public TelGeneralLogicModemInterface,
				public TelGeneralLogicServiceInterface,
				public TelObserver,
				public TelMessageThread {
public:
	TelGeneralLogicMediator(TelBbmcAdapterInterface *bbmcAdapter,
				TelGeneralLogicNetCtl *netCtl);
	virtual ~TelGeneralLogicMediator();

	/* TelGeneralLogicMediatorBase */
	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult StartLogic();
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyModem *GetModemInterface();
	virtual TelephonyService *GetServiceInterface();

	/* TelGeneralLogicModemInterface */
	virtual int StartModem();
	virtual int StopModemSoft();
	virtual int StopModemForce();
	virtual int AddGeneralListener(TelephonyGeneralListener listener, void *userData);
	virtual int RemoveGeneralListener(TelephonyGeneralListener listener, void *userData);
	virtual int GetPhoneNumberString(char *outNumber);
	virtual int NeedsInitialCheck();
	virtual int CheckSimConditionCode();
	virtual int GetControlKeyStateCode();
	virtual int GetControlKeyBlockTypeCode();
	virtual int GetAntennaDisplayType();
	virtual int CheckContentDownloadCode(int serviceType);
	virtual TelephonyResult CheckImsiForOperatorCode(int *inOutOperatorCode,
							 bool *outMatched);
	virtual int GetCountryCode(paf::string *outCountry);
	virtual int GetProductTargetOperatorCode(int *outOperator);

	/* TelGeneralLogicServiceInterface */
	virtual int StopServiceForce();
	virtual TelephonyResult GetGeneralLogicState(int *outState);
	virtual TelephonyResult SetFlightMode(int enabled);
	virtual TelephonyResult GetFlightMode(bool *outEnabled);
	virtual TelephonyResult SetEnableWwan(bool enabled);
	virtual TelephonyResult GetEnableWwan(bool *outEnabled);
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber);
	virtual TelephonyResult GetImei(paf::string *outImei);
	virtual TelephonyResult GetIccid(paf::string *outIccid);
	virtual TelephonyResult VerifyPin(const TelPinCode &pin);
	virtual TelephonyResult UnblockPin(const TelUnblockPinInfo &unblockInfo);
	virtual TelephonyResult SetPinProtection(bool enable, const TelPinCode &pin);
	virtual TelephonyResult GetPinProtection(bool *outEnabled);
	virtual TelephonyResult GetPinStatus(int *outStatus);
	virtual TelephonyResult GetPinRemain(int *outRemain);
	virtual TelephonyResult ChangePinCode(const TelPinCode &oldPin,
					      const TelPinCode &newPin);
	virtual TelephonyResult SetEventCallback(TelGeneralEventCallback callback,
						 void *userData);
	virtual TelephonyResult SetLegacyCallback(TelGeneralLegacyCallback callback,
						  void *userData);
	virtual TelephonyResult SetApnInfo(const TelephonyOperator &apnInfo,
					   bool saveToRegistry);
	virtual TelephonyResult SetApnInfoWithBillingPlan(const TelOperatorApnInfo &apnInfo,
							  bool saveToRegistry);
	virtual TelephonyResult GetUnsupportedProfile();
	virtual TelephonyResult RestoreApnInfo();
	virtual TelephonyResult GetMccMncOfImsi(TelephonyPlmn *outPlmn);
	virtual TelephonyResult GetMccMncOfNetwork(TelephonyPlmn *outPlmn);
	virtual TelephonyResult GetSimActivationState(int *outActivationState);
	virtual TelephonyResult GetProductTargetOperator(int *outOperator);
	virtual TelephonyResult GetOperatorProfileCount(int *outCount);
	virtual TelephonyResult GetOperatorProfile(int index,
						   TelOperatorProfile *outProfile);
	virtual TelephonyResult GetApnInfoListCount(int *outCount);
	virtual TelephonyResult GetApnInfoListItem(int index,
						   TelOperatorApnInfo *outApnInfo);
	virtual TelephonyResult GetCurrentApnInfo(TelephonyOperator *outApnInfo);
	virtual TelephonyResult RemoveCurrentApnInfo();
	virtual TelephonyResult StartNetworkScan();
	virtual TelephonyResult AbortNetworkScan();
	virtual TelephonyResult GetNetworkScanInfoCount(int *outCount);
	virtual TelephonyResult GetNetworkScanInfo(int index,
						   TelNetScanInfo *outScanInfo);
	virtual TelephonyResult SetNetRegisterAuto();
	virtual TelephonyResult SetNetRegisterInfo(const TelNetScanInfo &scanInfo);
	virtual TelephonyResult GetNetRegistrationInfo(TelNetworkInfo *outInfo);
	virtual TelephonyResult GetSavedNetworkSelection(TelNetworkInfo *outInfo);
	virtual TelephonyResult SetRegistrationAppNetMode(int enableRestricted);
	virtual TelephonyResult SetDataRoamingPermission(bool allow);
	virtual TelephonyResult GetDataRoamingPermission(bool *outAllowed);
	virtual TelephonyResult SetDataRoamingDefinition(int definition);
	virtual TelephonyResult CheckSimCondition();
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport);
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport);
	virtual TelephonyResult GetIsComExist(int *outExists);
	virtual TelephonyResult GetIsApnInit(int *outInitialized);
	virtual TelephonyResult IsOpenSimModel(bool *outOpenModel);
	virtual TelephonyResult GetNetworkSelectionMode(int *outMode);
	virtual TelephonyResult GetConditionError(TelephonyResult *outConditionError);
	virtual TelephonyResult DeactivateSimLock(const TelCarrierLockKey &key);
	virtual TelephonyResult UnblockSimLock(const TelControlKey &key);
	virtual TelephonyResult GetSimLockDeactivateRemain(int *outRemain);
	virtual TelephonyResult GetSimLockUnblockRemain(int *outRemain);
	virtual TelephonyResult GetControlKeyState(int *outState);
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType);
	virtual TelephonyResult GetCurrentSignalLevel(int *outLevel);
	virtual TelephonyResult SetBillingPlan(const int *billingPlan);
	virtual TelephonyResult GetBillingPlan(int *outBillingPlan);
	virtual TelephonyResult SetDebugLogEnabled(bool enabled);
	virtual TelephonyResult ReadDebugLogLine(paf::string *outLine);
	virtual TelephonyResult SetDebugDiagFlag(const int *flagValue);
	virtual TelephonyResult GetDebugDiagFlag(int *outFlagValue);
	virtual TelephonyResult SetDebugForceFlag(const int *flagValue);
	virtual TelephonyResult GetDebugForceFlag(int *outFlagValue);
	virtual TelephonyResult IsSimActivated(int *outActivated);
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult CheckComVersion(bool *outMatches);
	virtual TelephonyResult ReleasePdpContext();
	virtual TelephonyResult StartApnListDownload();
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType);
	virtual TelephonyResult GetModemFirmwareVersion(paf::string *outVersion);
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl);
	virtual TelephonyResult NotifyPluginState(const char *pluginName, int stateValue);

	/* TelObserver & TelMessageThread */
	virtual void OnNotify(const TelEventInfo &event);
	virtual void OnHandleEvent(const TelEventInfo &event);

	/* Internal helpers implemented in tel_general_logic_mediator.cpp / mediator2.cpp */
	TelephonyResult GetSignalLevel(TelAntennaLevelThreshold *outLevel);
	TelephonyResult GetNetworkInfo(TelNetworkInfo *outInfo);
	TelephonyResult GetRoamingInfo(TelRoamingInfo *outRoaming);
	void BroadcastGeneralInfo();
	void RefreshNetworkAndBearerCache();
	void HandleInternalAsyncApi(const TelEventInfo &event);
	TelephonyResult LookupOperatorProfileVersion(int operatorId, paf::string *outVersion);
	TelephonyResult BuildDiagnosticReport(paf::string *outReport);
	TelephonyResult ReadNetworkReportLine(paf::string *outLine);

	static int PowerCallback(int notifyId, int notifyCount, int powerInfo, void *common);
	static void LivespaceCallback(int code, const void *eventData, SceSize size, void *userData);
	static void AppActivatedCallback(int code, const void *eventData, SceSize size, void *userData);
	static void AppDeactivatedCallback(int code, const void *eventData, SceSize size, void *userData);
	static void AppEndedCallback(int code, const void *eventData, SceSize size, void *userData);

private:
	int ExecuteTurnOnInternal();
	void HandleAppDeactivated(int code, int appId);
	void HandleBbmcComBootOk(const TelEventInfo &event);
	void HandleBbmcComBootNg(const TelEventInfo &event);
	void HandleMediatorComFailure(const TelEventInfo &event);
	void HandleBbmcNeedUpdatingDeviceInfo(const TelEventInfo &event);
	void HandleBbmcSetupReady(const TelEventInfo &event);
	void HandleBbmcNetworkReady(const TelEventInfo &event);
	void HandleBbmcPinRequired(const TelEventInfo &event);
	void HandleBbmcPukRequired(const TelEventInfo &event);
	void HandleBbmcPinNotRequired(const TelEventInfo &event);
	void HandleBbmcPinPermanentlyBlocked(const TelEventInfo &event);
	void HandleBbmcNetScanFin(const TelEventInfo &event);
	void HandleBbmcNetScanFailed(const TelEventInfo &event);
	void HandleBbmcPinStatusReady(const TelEventInfo &event);
	void HandleBbmcExceedHiTemp(const TelEventInfo &event);
	void HandleBbmcPacketCallEndReasonUpdate(const TelEventInfo &event);
	void HandleBbmcComFwUpdateNotice(const TelEventInfo &event);
	void HandleInternalVerifyPin(const TelEventInfo &event);
	void HandleInternalUnblockPin(const TelEventInfo &event);
	void HandleInternalSetPinProtection(const TelEventInfo &event);
	void HandleInternalChangePinCode(const TelEventInfo &event);
	void HandleInternalSetApnInfo(const TelEventInfo &event);
	void HandleInternalSetApnInfoWithBillingPlan(const TelEventInfo &event);
	void HandleInternalRestoreApnInfo(const TelEventInfo &event);
	void HandleInternalRemoveCurrentApnInfo(const TelEventInfo &event);
	void HandleInternalDeactivateSimLock(const TelEventInfo &event);
	void HandleInternalUnblockSimLock(const TelEventInfo &event);
	void HandleInternalResume(const TelEventInfo &event);
	void HandleAttributeError(const TelEventInfo &event);
	void HandleInternalSimLockError(const TelEventInfo &event);

	paf::thread::Mutex m_smcMutex;
	paf::thread::Mutex m_listenerMutex;
	paf::vector<TelGeneralListenerRegistration> m_generalListeners;
	TelBbmcAdapterInterface *m_bbmcAdapter;
	TelGeneralLogicNetCtl *m_netCtl;
	TelNetworkInfo m_cachedNetworkInfo;
	TelPacketBearerInfo m_cachedBearerInfo;
	int m_cachedSimCondition;
	int m_cachedFlightMode;
	int m_debugDiagFlag;
	bool m_operatorAppLaunched;
	int m_debugForceFlag;
	TelGeneralLogicAction m_action;
	TelGeneralLogicStateMachineContext m_stateMachine;
	int m_telRegAppId;
	int m_operatorAppId;
	bool m_scanActive;
	bool m_powerTickActive;
	int m_debugLogStage;
	bool m_debugLogEnabled;
	paf::string m_networkReportCache;
	SceSize m_networkReportOffset;
};

/* ------------------------------------------------------------------
 * Minimal Stub General Logic Mediator for Wi-Fi units (0x8125EF84..0x8125F373)
 * ------------------------------------------------------------------ */

class TelGeneralLogicMediatorMini : public TelGeneralLogicMediatorBase,
				    public TelGeneralLogicModemInterface,
				    public TelGeneralLogicServiceInterface,
				    public TelObserver {
public:
	TelGeneralLogicMediatorMini();
	virtual ~TelGeneralLogicMediatorMini();

	/* TelGeneralLogicMediatorBase */
	virtual TelephonyResult Initialize();
	virtual TelephonyResult Terminate();
	virtual TelephonyResult StartLogic();
	virtual TelephonyResult GetOperatorAppTitleId(const char **outTitleId);
	virtual TelephonyModem *GetModemInterface();
	virtual TelephonyService *GetServiceInterface();

	/* TelGeneralLogicModemInterface */
	virtual int StartModem() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int StopModemSoft() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int StopModemForce() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int AddGeneralListener(TelephonyGeneralListener listener, void *userData);
	virtual int RemoveGeneralListener(TelephonyGeneralListener listener, void *userData);
	virtual int GetPhoneNumberString(char *outNumber);
	virtual int NeedsInitialCheck() { return 0; }
	virtual int CheckSimConditionCode() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int GetControlKeyStateCode() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int GetControlKeyBlockTypeCode() { return TEL_ERR_NOT_SUPPORTED; }
	virtual int GetAntennaDisplayType() { return 0; }
	virtual int CheckContentDownloadCode(int serviceType);
	virtual TelephonyResult CheckImsiForOperatorCode(int *inOutOperatorCode,
							 bool *outMatched);
	virtual int GetCountryCode(paf::string *outCountry);
	virtual int GetProductTargetOperatorCode(int *outOperator);

	/* TelGeneralLogicServiceInterface */
	virtual int StopServiceForce() { return TEL_ERR_NOT_SUPPORTED; }
	virtual TelephonyResult GetGeneralLogicState(int *outState);
	virtual TelephonyResult SetFlightMode(int enabled);
	virtual TelephonyResult GetFlightMode(bool *outEnabled);
	virtual TelephonyResult SetEnableWwan(bool enabled);
	virtual TelephonyResult GetEnableWwan(bool *outEnabled);
	virtual TelephonyResult GetPhoneNumber(TelPhoneNumber *outNumber);
	virtual TelephonyResult GetImei(paf::string *outImei);
	virtual TelephonyResult GetIccid(paf::string *outIccid);
	virtual TelephonyResult VerifyPin(const TelPinCode &pin);
	virtual TelephonyResult UnblockPin(const TelUnblockPinInfo &unblockInfo);
	virtual TelephonyResult SetPinProtection(bool enable, const TelPinCode &pin);
	virtual TelephonyResult GetPinProtection(bool *outEnabled);
	virtual TelephonyResult GetPinStatus(int *outStatus);
	virtual TelephonyResult GetPinRemain(int *outRemain);
	virtual TelephonyResult ChangePinCode(const TelPinCode &oldPin,
					      const TelPinCode &newPin);
	virtual TelephonyResult SetEventCallback(TelGeneralEventCallback callback,
						 void *userData);
	virtual TelephonyResult SetLegacyCallback(TelGeneralLegacyCallback callback,
						  void *userData);
	virtual TelephonyResult SetApnInfo(const TelephonyOperator &apnInfo,
					   bool saveToRegistry);
	virtual TelephonyResult SetApnInfoWithBillingPlan(const TelOperatorApnInfo &apnInfo,
							  bool saveToRegistry);
	virtual TelephonyResult GetUnsupportedProfile();
	virtual TelephonyResult RestoreApnInfo();
	virtual TelephonyResult GetMccMncOfImsi(TelephonyPlmn *outPlmn);
	virtual TelephonyResult GetMccMncOfNetwork(TelephonyPlmn *outPlmn);
	virtual TelephonyResult GetSimActivationState(int *outActivationState);
	virtual TelephonyResult GetProductTargetOperator(int *outOperator);
	virtual TelephonyResult GetOperatorProfileCount(int *outCount);
	virtual TelephonyResult GetOperatorProfile(int index,
						   TelOperatorProfile *outProfile);
	virtual TelephonyResult GetApnInfoListCount(int *outCount);
	virtual TelephonyResult GetApnInfoListItem(int index,
						   TelOperatorApnInfo *outApnInfo);
	virtual TelephonyResult GetCurrentApnInfo(TelephonyOperator *outApnInfo);
	virtual TelephonyResult RemoveCurrentApnInfo();
	virtual TelephonyResult StartNetworkScan();
	virtual TelephonyResult AbortNetworkScan();
	virtual TelephonyResult GetNetworkScanInfoCount(int *outCount);
	virtual TelephonyResult GetNetworkScanInfo(int index,
						   TelNetScanInfo *outScanInfo);
	virtual TelephonyResult SetNetRegisterAuto();
	virtual TelephonyResult SetNetRegisterInfo(const TelNetScanInfo &scanInfo);
	virtual TelephonyResult GetNetRegistrationInfo(TelNetworkInfo *outInfo);
	virtual TelephonyResult GetSavedNetworkSelection(TelNetworkInfo *outInfo);
	virtual TelephonyResult SetRegistrationAppNetMode(int enableRestricted);
	virtual TelephonyResult SetDataRoamingPermission(bool allow);
	virtual TelephonyResult GetDataRoamingPermission(bool *outAllowed);
	virtual TelephonyResult SetDataRoamingDefinition(int definition);
	virtual TelephonyResult CheckSimCondition();
	virtual TelephonyResult GetNetworkStatusReport(paf::string *outReport);
	virtual TelephonyResult GetDeviceInfoReport(paf::string *outReport);
	virtual TelephonyResult GetIsComExist(int *outExists);
	virtual TelephonyResult GetIsApnInit(int *outInitialized);
	virtual TelephonyResult IsOpenSimModel(bool *outOpenModel);
	virtual TelephonyResult GetNetworkSelectionMode(int *outMode);
	virtual TelephonyResult GetConditionError(TelephonyResult *outConditionError);
	virtual TelephonyResult DeactivateSimLock(const TelCarrierLockKey &key);
	virtual TelephonyResult UnblockSimLock(const TelControlKey &key);
	virtual TelephonyResult GetSimLockDeactivateRemain(int *outRemain);
	virtual TelephonyResult GetSimLockUnblockRemain(int *outRemain);
	virtual TelephonyResult GetControlKeyState(int *outState);
	virtual TelephonyResult GetControlKeyBlockType(int *outBlockType);
	virtual TelephonyResult GetCurrentSignalLevel(int *outLevel);
	virtual TelephonyResult SetBillingPlan(const int *billingPlan);
	virtual TelephonyResult GetBillingPlan(int *outBillingPlan);
	virtual TelephonyResult SetDebugLogEnabled(bool enabled);
	virtual TelephonyResult ReadDebugLogLine(paf::string *outLine);
	virtual TelephonyResult SetDebugDiagFlag(const int *flagValue);
	virtual TelephonyResult GetDebugDiagFlag(int *outFlagValue);
	virtual TelephonyResult SetDebugForceFlag(const int *flagValue);
	virtual TelephonyResult GetDebugForceFlag(int *outFlagValue);
	virtual TelephonyResult IsSimActivated(int *outActivated);
	virtual TelephonyResult GetLatestArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult GetArchiveVersion(SceUInt32 *outVersion);
	virtual TelephonyResult CheckComVersion(bool *outMatches);
	virtual TelephonyResult ReleasePdpContext();
	virtual TelephonyResult StartApnListDownload();
	virtual TelephonyResult CheckContentDownloadAllowed(int serviceType);
	virtual TelephonyResult GetDownloadSizeLimit(unsigned int serviceType,
						     SceInt64 *outLimitBytes);
	virtual TelephonyResult CheckRoamingDownloadAllowed(unsigned int serviceType);
	virtual TelephonyResult GetModemFirmwareVersion(paf::string *outVersion);
	virtual TelephonyResult GetUpdateListUrl(paf::string *outUrl);
	virtual TelephonyResult NotifyPluginState(const char *pluginName, int stateValue);

	/* TelObserver */
	virtual void OnNotify(const TelEventInfo &event);

private:
	paf::thread::Mutex m_callbackMutex;
	paf::vector<TelGeneralListenerRegistration> m_generalListeners;
	paf::wstring m_emptyOperatorName;
};

} /* namespace telephony */

#endif /* TELEPHONY_GENERAL_INTERNAL_H */
