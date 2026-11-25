/*
 * tel_general_logic_mediator_mini2.cpp: unsupported service method stubs for
 * TelGeneralLogicMediatorMini on Wi-Fi-only units
 * (3.65-CEX: 0x8125F23E..0x8125F373).
 */
#include "tel_general_internal.h"

namespace telephony {

TelephonyResult TelGeneralLogicMediatorMini::GetGeneralLogicState(int *outState)
{
	(void)outState;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetFlightMode(int enabled)
{
	(void)enabled;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetFlightMode(bool *outEnabled)
{
	(void)outEnabled;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetEnableWwan(bool enabled)
{
	(void)enabled;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetPhoneNumber(TelPhoneNumber *outNumber)
{
	(void)outNumber;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetImei(paf::string *outImei)
{
	(void)outImei;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetIccid(paf::string *outIccid)
{
	(void)outIccid;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::VerifyPin(const TelPinCode &pin)
{
	(void)pin;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::UnblockPin(const TelUnblockPinInfo &unblockInfo)
{
	(void)unblockInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetPinProtection(bool enable, const TelPinCode &pin)
{
	(void)enable;
	(void)pin;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetPinProtection(bool *outEnabled)
{
	(void)outEnabled;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetPinStatus(int *outStatus)
{
	(void)outStatus;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetPinRemain(int *outRemain)
{
	(void)outRemain;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::ChangePinCode(const TelPinCode &oldPin,
							   const TelPinCode &newPin)
{
	(void)oldPin;
	(void)newPin;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetEventCallback(TelGeneralEventCallback callback,
							      void *userData)
{
	(void)callback;
	(void)userData;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetLegacyCallback(TelGeneralLegacyCallback callback,
							       void *userData)
{
	(void)callback;
	(void)userData;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetApnInfo(const TelephonyOperator &apnInfo,
							bool saveToRegistry)
{
	(void)apnInfo;
	(void)saveToRegistry;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetApnInfoWithBillingPlan(
	const TelOperatorApnInfo &apnInfo, bool saveToRegistry)
{
	(void)apnInfo;
	(void)saveToRegistry;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetUnsupportedProfile()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::RestoreApnInfo()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetMccMncOfImsi(TelephonyPlmn *outPlmn)
{
	(void)outPlmn;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetMccMncOfNetwork(TelephonyPlmn *outPlmn)
{
	(void)outPlmn;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetSimActivationState(int *outActivationState)
{
	(void)outActivationState;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetProductTargetOperator(int *outOperator)
{
	(void)outOperator;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetOperatorProfileCount(int *outCount)
{
	(void)outCount;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetOperatorProfile(int index,
								TelOperatorProfile *outProfile)
{
	(void)index;
	(void)outProfile;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetApnInfoListCount(int *outCount)
{
	(void)outCount;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetApnInfoListItem(int index,
								TelOperatorApnInfo *outApnInfo)
{
	(void)index;
	(void)outApnInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetCurrentApnInfo(TelephonyOperator *outApnInfo)
{
	(void)outApnInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::RemoveCurrentApnInfo()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::StartNetworkScan()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::AbortNetworkScan()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetNetworkScanInfoCount(int *outCount)
{
	(void)outCount;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetNetworkScanInfo(int index,
								TelNetScanInfo *outScanInfo)
{
	(void)index;
	(void)outScanInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetNetRegisterAuto()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetNetRegisterInfo(const TelNetScanInfo &scanInfo)
{
	(void)scanInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetNetRegistrationInfo(TelNetworkInfo *outInfo)
{
	(void)outInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetSavedNetworkSelection(TelNetworkInfo *outInfo)
{
	(void)outInfo;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetRegistrationAppNetMode(int enableRestricted)
{
	(void)enableRestricted;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetDataRoamingPermission(bool allow)
{
	(void)allow;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetDataRoamingPermission(bool *outAllowed)
{
	(void)outAllowed;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetDataRoamingDefinition(int definition)
{
	(void)definition;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::CheckSimCondition()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetNetworkStatusReport(paf::string *outReport)
{
	(void)outReport;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetDeviceInfoReport(paf::string *outReport)
{
	(void)outReport;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetIsApnInit(int *outInitialized)
{
	(void)outInitialized;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::IsOpenSimModel(bool *outOpenModel)
{
	(void)outOpenModel;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetNetworkSelectionMode(int *outMode)
{
	(void)outMode;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetConditionError(TelephonyResult *outConditionError)
{
	(void)outConditionError;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::DeactivateSimLock(const TelCarrierLockKey &key)
{
	(void)key;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::UnblockSimLock(const TelControlKey &key)
{
	(void)key;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetSimLockDeactivateRemain(int *outRemain)
{
	(void)outRemain;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetSimLockUnblockRemain(int *outRemain)
{
	(void)outRemain;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetControlKeyState(int *outState)
{
	(void)outState;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetControlKeyBlockType(int *outBlockType)
{
	(void)outBlockType;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetCurrentSignalLevel(int *outLevel)
{
	(void)outLevel;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetBillingPlan(const int *billingPlan)
{
	(void)billingPlan;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetBillingPlan(int *outBillingPlan)
{
	(void)outBillingPlan;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetDebugLogEnabled(bool enabled)
{
	(void)enabled;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::ReadDebugLogLine(paf::string *outLine)
{
	(void)outLine;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetDebugDiagFlag(const int *flagValue)
{
	(void)flagValue;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetDebugDiagFlag(int *outFlagValue)
{
	(void)outFlagValue;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::SetDebugForceFlag(const int *flagValue)
{
	(void)flagValue;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetDebugForceFlag(int *outFlagValue)
{
	(void)outFlagValue;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::IsSimActivated(int *outActivated)
{
	(void)outActivated;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetLatestArchiveVersion(SceUInt32 *outVersion)
{
	(void)outVersion;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetArchiveVersion(SceUInt32 *outVersion)
{
	(void)outVersion;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::CheckComVersion(bool *outMatches)
{
	(void)outMatches;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::ReleasePdpContext()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::StartApnListDownload()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetModemFirmwareVersion(paf::string *outVersion)
{
	(void)outVersion;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::GetUpdateListUrl(paf::string *outUrl)
{
	(void)outUrl;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyResult TelGeneralLogicMediatorMini::NotifyPluginState(const char *pluginName,
							       int stateValue)
{
	(void)pluginName;
	(void)stateValue;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

} /* namespace telephony */
