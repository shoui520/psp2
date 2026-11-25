/*
 * tel_general_logic_mediator.cpp: full general telephony mediator implementing
 * TelGeneralLogicMediatorBase, TelGeneralLogicModemInterface (17 slots), and
 * TelGeneralLogicServiceInterface (82 slots)
 * (3.65-CEX: 0x81255774..0x8125B139).
 */
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <appmgr/appmgr.h>
#include "shell.h"
#include "tel_general_internal.h"

extern "C" {
int scePowerRegisterCallback(SceUID cbid);
int scePowerUnregisterCallback(SceUID cbid);
int sceAppMgrGetStatusByName(const char *name, void *status);
int SceNetInternal_0DFC23D6(int processId, int mode, int flags);
}

namespace telephony {

/* Layout of sceAppMgrGetStatusByName / sceAppMgrGetStatusByAppId output (0x80 bytes) */
struct TelAppMgrStatusRecord {
	SceUInt32 header[3];
	char titleId[40];
	SceUID processId;
	SceUInt8 tail[72];
};

static SceUID s_powerCallbackId = -1;

static const int kBbmcFeatureUnavailable = -2144139505;	/* 0x8033070F */
static const int kTelFeatureUnavailable = -2146424770;	/* 0x8010203E */

/* ------------------------------------------------------------------
 * Construction, destruction, and lifecycle (0x81255774..0x81255EF7)
 * ------------------------------------------------------------------ */

/* 0x81255774 */
TelGeneralLogicMediator::TelGeneralLogicMediator(TelBbmcAdapterInterface *bbmcAdapter,
						 TelGeneralLogicNetCtl *netCtl)
	: TelGeneralLogicMediatorBase("SceTelGenLogicMediatorObsv"),
	  TelMessageThread("SceTelGeneralLogicMediator", 0x1000010E, 0x4000),
	  m_smcMutex("SceTelGenLogicMediatorSmc", 0),
	  m_listenerMutex("SceTelGenLogicMediatorCb", 0),
	  m_bbmcAdapter(bbmcAdapter),
	  m_netCtl(netCtl),
	  m_cachedSimCondition(0),
	  m_cachedFlightMode(0),
	  m_debugDiagFlag(0),
	  m_operatorAppLaunched(false),
	  m_debugForceFlag(0),
	  m_stateMachine(m_action),
	  m_telRegAppId(-1),
	  m_operatorAppId(-1),
	  m_scanActive(false),
	  m_debugLogStage(0),
	  m_debugLogEnabled(false),
	  m_networkReportCache(""),
	  m_networkReportOffset(0)
{
}

/* 0x81255950 */
TelGeneralLogicMediator::~TelGeneralLogicMediator()
{
	m_generalListeners.clear();
}

/* 0x81255AD6 */
TelephonyResult TelGeneralLogicMediator::Initialize()
{
	m_action.Initialize(this, m_bbmcAdapter, m_netCtl);
	m_action.AddObserver(this);

	TelephonyResult res = TelTimerManager::Initialize();

	SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, AppEndedCallback, this);
	SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, AppActivatedCallback, this);
	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, AppDeactivatedCallback, this);

	s_powerCallbackId = -1;
	SceUID cbId = sceKernelCreateCallback("SceTelehony", 0, PowerCallback, this);
	if (cbId >= 0) {
		if (scePowerRegisterCallback(cbId) >= 0)
			s_powerCallbackId = cbId;
		else
			sceKernelDeleteCallback(cbId);
	}

	SceShell_7FE4E732(SHELL_EVENT_POWER, LivespaceCallback, this);
	StartThread(NULL);
	return res;
}

/* 0x81255BE6 */
TelephonyResult TelGeneralLogicMediator::Terminate()
{
	m_action.Terminate();
	m_action.RemoveObserver(this);
	TelTimerManager::Terminate();

	SceShell_D4C7D2B7(SHELL_EVENT_POWER, LivespaceCallback, this);

	if (s_powerCallbackId != -1) {
		scePowerUnregisterCallback(s_powerCallbackId);
		sceKernelDeleteCallback(s_powerCallbackId);
		s_powerCallbackId = -1;
	}

	SceShell_E3D3A162(SHELL_APP_EVENT_ENDED, AppEndedCallback, this);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, AppActivatedCallback, this);
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, AppDeactivatedCallback, this);

	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->RemoveObserver(this);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL)
		attrMgr->RemoveObserver(this);

	StopThread();
	return TelephonyResult(TEL_OK);
}

/* 0x81255CCC */
TelephonyResult TelGeneralLogicMediator::StartLogic()
{
	TelEventInfo startEv(TEL_EV_INTERNAL_TURN_ON, -1, -1);
	PostMessage(startEv);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicMediator::GetOperatorAppTitleId(const char **outTitleId)
{
	if (m_netCtl == NULL) {
		*outTitleId = NULL;
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}
	return m_netCtl->GetOperatorAppTitleId(outTitleId);
}

TelephonyModem *TelGeneralLogicMediator::GetModemInterface()
{
	return reinterpret_cast<TelephonyModem *>(
		static_cast<TelGeneralLogicModemInterface *>(this));
}

TelephonyService *TelGeneralLogicMediator::GetServiceInterface()
{
	return reinterpret_cast<TelephonyService *>(
		static_cast<TelGeneralLogicServiceInterface *>(this));
}

/* ------------------------------------------------------------------
 * TelGeneralLogicModemInterface methods (0x81255D24..0x81256697)
 * ------------------------------------------------------------------ */

/* 0x81255D24 */
int TelGeneralLogicMediator::ExecuteTurnOnInternal()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_TURN_ON);
	TelBbmcSharedData::SetStaticInt(524328, 1);
	m_stateMachine.TurnOn();
	TelephonyResult res = m_action.GetApiResult(TEL_API_TURN_ON);
	TelBbmcSharedData::SetStaticInt(524328, 65);
	(void)res.GetCode();
	m_smcMutex.Unlock();
	return 0;
}

/* 0x81255DDE */
int TelGeneralLogicMediator::StartModem()
{
	return ExecuteTurnOnInternal();
}

/* 0x81255E1E */
int TelGeneralLogicMediator::StopModemSoft()
{
	int err = 0;
	if (m_bbmcAdapter != NULL)
		err = m_bbmcAdapter->TurnOff(false).GetCode();
	TelEventInfo ev(TEL_EV_INTERNAL_TURN_OFF_SOFT, -1, -1);
	PostMessage(ev);
	return err;
}

/* 0x81255E98 */
int TelGeneralLogicMediator::StopModemForce()
{
	int err = 0;
	if (m_bbmcAdapter != NULL)
		err = m_bbmcAdapter->TurnOff(true).GetCode();
	TelEventInfo ev(TEL_EV_INTERNAL_SHUTDOWN, -1, -1);
	PostMessage(ev);
	return err;
}

int TelGeneralLogicMediator::StopServiceForce()
{
	return StopModemForce();
}

/* 0x812561BE */
int TelGeneralLogicMediator::AddGeneralListener(TelephonyGeneralListener listener,
						void *userData)
{
	if (listener == NULL)
		return TEL_ERR_INVALID_ARG;

	m_listenerMutex.Lock();
	TelGeneralListenerRegistration reg;
	reg.callback = listener;
	reg.userData = userData;
	m_generalListeners.push_back(reg);
	m_listenerMutex.Unlock();

	BroadcastGeneralInfo();
	return 0;
}

/* 0x81256322 */
int TelGeneralLogicMediator::RemoveGeneralListener(TelephonyGeneralListener listener,
						   void *userData)
{
	m_listenerMutex.Lock();
	for (paf::vector<TelGeneralListenerRegistration>::iterator it = m_generalListeners.begin();
	     it != m_generalListeners.end(); ++it) {
		if (it->callback == listener && it->userData == userData) {
			m_generalListeners.erase(it);
			break;
		}
	}
	m_listenerMutex.Unlock();
	return 0;
}

/* 0x81256416 */
int TelGeneralLogicMediator::CheckSimConditionCode()
{
	if (m_bbmcAdapter == NULL)
		return TEL_ERR_NOT_INITIALIZED;
	return m_bbmcAdapter->CheckSimCondition().GetCode();
}

/* 0x81256458 */
int TelGeneralLogicMediator::GetControlKeyStateCode()
{
	int state = 0;
	GetControlKeyState(&state);
	return state;
}

/* 0x812564A6 */
TelephonyResult TelGeneralLogicMediator::GetControlKeyState(int *outState)
{
	int state = 0;
	TelephonyResult res = (m_bbmcAdapter != NULL)
		? m_bbmcAdapter->GetControlKeyState(&state)
		: TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	*outState = state;
	return res;
}

/* 0x812564F8 */
int TelGeneralLogicMediator::GetControlKeyBlockTypeCode()
{
	int blockType = 0;
	GetControlKeyBlockType(&blockType);
	return blockType;
}

/* 0x81256546 */
TelephonyResult TelGeneralLogicMediator::GetControlKeyBlockType(int *outBlockType)
{
	int blockType = 0;
	TelephonyResult res = (m_bbmcAdapter != NULL)
		? m_bbmcAdapter->GetControlKeyBlockType(&blockType)
		: TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	*outBlockType = blockType;
	return res;
}

/* 0x81256598 */
int TelGeneralLogicMediator::GetAntennaDisplayType()
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return 0;

	TelAttributeValue attr(TEL_ATTR_ANTENNA_DISP_TYPE);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	return res.IsSuccess() ? attr.intValue : 0;
}

/* 0x81256640 */
int TelGeneralLogicMediator::CheckContentDownloadCode(int serviceType)
{
	return CheckContentDownloadAllowed(serviceType).GetCode();
}

/* 0x81256A5C */
int TelGeneralLogicMediator::GetPhoneNumberString(char *outNumber)
{
	if (outNumber == NULL)
		return -2146424823;	/* 0x80102009 */

	if (m_bbmcAdapter == NULL)
		return TEL_ERR_NOT_INITIALIZED;

	TelPhoneNumber number;
	TelephonyResult res = m_bbmcAdapter->GetPhoneNumber(&number);
	if (res.IsSuccess())
		sce_paf_strncpy(outNumber, number.GetDigits(), 33);
	return res.GetCode();
}

/* 0x81259474 */
int TelGeneralLogicMediator::NeedsInitialCheck()
{
	int exists = 0;
	GetIsComExist(&exists);
	return exists;
}

/* 0x8125A098 */
int TelGeneralLogicMediator::GetCountryCode(paf::string *outCountry)
{
	TelephonyPlmn plmn;
	TelephonyResult res = GetMccMncOfImsi(&plmn);
	if (res.IsSuccess() && plmn.GetCountryCode(outCountry))
		return 0;
	return TEL_ERR_NOT_FOUND;
}

/* 0x8125A120 */
int TelGeneralLogicMediator::GetProductTargetOperatorCode(int *outOperator)
{
	return telGetProductTargetOperator(outOperator).GetCode();
}

/* ------------------------------------------------------------------
 * TelGeneralLogicServiceInterface methods (0x81256698..0x8125B139)
 * ------------------------------------------------------------------ */

/* 0x81256698 */
TelephonyResult TelGeneralLogicMediator::GetGeneralLogicState(int *outState)
{
	*outState = m_action.GetStateCode();
	return TelephonyResult(TEL_OK);
}

/* 0x812566AE */
TelephonyResult TelGeneralLogicMediator::SetFlightMode(int enabled)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_FLIGHT_MODE);
	m_action.flightModeInput = enabled;
	m_stateMachine.SetFlightMode();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_FLIGHT_MODE);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81256762 */
TelephonyResult TelGeneralLogicMediator::GetFlightMode(bool *outEnabled)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue attr(TEL_ATTR_FLIGHT_MODE);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	if (res.IsSuccess())
		*outEnabled = (attr.intValue != 0);
	return res;
}

/* 0x8125681A */
TelephonyResult TelGeneralLogicMediator::SetEnableWwan(bool enabled)
{
	TelEventInfo ev(TEL_EV_INTERNAL_ENABLE_WWAN, -1, enabled ? 1 : 0);
	PostMessage(ev);
	return TelephonyResult(TEL_OK);
}

/* 0x8125687C */
TelephonyResult TelGeneralLogicMediator::GetEnableWwan(bool *outEnabled)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue attr(TEL_ATTR_ENABLE_WWAN);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	if (res.IsSuccess())
		*outEnabled = (attr.intValue != 0);
	return res;
}

/* 0x81256934 */
TelephonyResult TelGeneralLogicMediator::GetPhoneNumber(TelPhoneNumber *outNumber)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_PHONE_NUMBER);
	m_stateMachine.GetPhoneNumber();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_PHONE_NUMBER);
	if (res.IsSuccess() && outNumber != NULL)
		*outNumber = m_action.phoneNumber;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81256AD8 */
TelephonyResult TelGeneralLogicMediator::GetImei(paf::string *outImei)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue attr(TEL_ATTR_IDSTORAGE_MODEM_ID);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	if (outImei != NULL)
		*outImei = attr.stringValue;
	return res;
}

/* 0x81256BBC */
TelephonyResult TelGeneralLogicMediator::GetIccid(paf::string *outIccid)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_ICCID);
	m_stateMachine.GetIccid();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_ICCID);
	if (res.IsSuccess() && outIccid != NULL)
		*outIccid = m_action.iccid;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81256C82 */
TelephonyResult TelGeneralLogicMediator::VerifyPin(const TelPinCode &pin)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_VERIFY_PIN);
	m_action.verifyPinCode = pin;
	m_stateMachine.VerifyPin();
	TelephonyResult res = m_action.GetApiResult(TEL_API_VERIFY_PIN);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81256DDE */
TelephonyResult TelGeneralLogicMediator::UnblockPin(const TelUnblockPinInfo &unblockInfo)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_UNBLOCK_PIN);
	m_action.unblockPinInfo = unblockInfo;
	m_stateMachine.UnblockPin();
	TelephonyResult res = m_action.GetApiResult(TEL_API_UNBLOCK_PIN);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81256FA0 */
TelephonyResult TelGeneralLogicMediator::SetPinProtection(bool enable, const TelPinCode &pin)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_PIN_PROTECTION);
	m_action.pinProtectionInput = enable;
	m_action.protectionPinCode = pin;
	m_stateMachine.SetPinProtection();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_PIN_PROTECTION);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257116 */
TelephonyResult TelGeneralLogicMediator::GetPinProtection(bool *outEnabled)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_PIN_PROTECTION);
	m_stateMachine.GetPinProtection();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_PIN_PROTECTION);
	if (res.IsSuccess() && outEnabled != NULL)
		*outEnabled = m_action.pinProtectionOutput;
	m_smcMutex.Unlock();
	return res;
}

/* 0x812571D6 */
TelephonyResult TelGeneralLogicMediator::GetPinStatus(int *outStatus)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_PIN_STATUS);
	m_stateMachine.GetPinStatus();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_PIN_STATUS);
	if (res.IsSuccess() && outStatus != NULL)
		*outStatus = m_action.pinStatusOutput;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257296 */
TelephonyResult TelGeneralLogicMediator::GetPinRemain(int *outRemain)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_PIN_REMAIN);
	m_stateMachine.GetPinRemain();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_PIN_REMAIN);
	if (res.IsSuccess() && outRemain != NULL)
		*outRemain = m_action.pinRemainOutput;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257356 */
TelephonyResult TelGeneralLogicMediator::ChangePinCode(const TelPinCode &oldPin,
						       const TelPinCode &newPin)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_CHANGE_PIN_CODE);
	m_action.changeOldPin = oldPin;
	m_action.changeNewPin = newPin;
	m_stateMachine.ChangePinCode();
	TelephonyResult res = m_action.GetApiResult(TEL_API_CHANGE_PIN_CODE);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257B9C */
TelephonyResult TelGeneralLogicMediator::SetEventCallback(TelGeneralEventCallback callback,
							  void *userData)
{
	m_action.SetEventCallback(callback, userData);
	return TelephonyResult(TEL_OK);
}

/* 0x81257BE6 */
TelephonyResult TelGeneralLogicMediator::SetLegacyCallback(TelGeneralLegacyCallback callback,
							   void *userData)
{
	m_action.SetLegacyCallback(callback, userData);
	return TelephonyResult(TEL_OK);
}

/* 0x81257C30 */
TelephonyResult TelGeneralLogicMediator::SetApnInfo(const TelephonyOperator &apnInfo,
						    bool saveToRegistry)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_APN_INFO);
	m_action.apnCredentials = apnInfo;
	m_action.saveApnToRegistry = saveToRegistry;
	m_stateMachine.SetApnInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_APN_INFO);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257E14 */
TelephonyResult TelGeneralLogicMediator::SetApnInfoWithBillingPlan(
	const TelOperatorApnInfo &apnInfo, bool saveToRegistry)
{
	TelSetApnBillingEvent billingEv(apnInfo, saveToRegistry);
	PostMessage(billingEv);
	return TelephonyResult(TEL_OK);
}

/* 0x8142B948 */
TelephonyResult TelGeneralLogicMediator::GetUnsupportedProfile()
{
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

/* 0x81257EEA */
TelephonyResult TelGeneralLogicMediator::RestoreApnInfo()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_RESTORE_APN_INFO);
	m_stateMachine.RestoreApnInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_RESTORE_APN_INFO);
	m_smcMutex.Unlock();
	return res;
}

/* 0x8125786C */
TelephonyResult TelGeneralLogicMediator::GetMccMncOfImsi(TelephonyPlmn *outPlmn)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_MCC_MNC_OF_IMSI);
	m_stateMachine.GetMccMncOfImsi();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_MCC_MNC_OF_IMSI);
	if (res.IsSuccess() && outPlmn != NULL)
		*outPlmn = m_action.imsiPlmn;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257A04 */
TelephonyResult TelGeneralLogicMediator::GetMccMncOfNetwork(TelephonyPlmn *outPlmn)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_MCC_MNC_OF_NETWORK);
	m_stateMachine.GetMccMncOfNetwork();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_MCC_MNC_OF_NETWORK);
	if (res.IsSuccess() && outPlmn != NULL)
		*outPlmn = m_action.networkPlmn;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81257FFA */
TelephonyResult TelGeneralLogicMediator::GetSimActivationState(int *outActivationState)
{
	int state = 0;
	if (m_netCtl != NULL)
		m_netCtl->GetSimActivationState(&state);
	*outActivationState = state;
	return TelephonyResult(TEL_OK);
}

/* 0x8125803E */
TelephonyResult TelGeneralLogicMediator::GetProductTargetOperator(int *outOperator)
{
	return telGetProductTargetOperator(outOperator);
}

/* 0x8125807C */
TelephonyResult TelGeneralLogicMediator::GetOperatorProfileCount(int *outCount)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_OPERATOR_PROFILE_COUNT);
	m_stateMachine.GetOperatorProfileCount();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_OPERATOR_PROFILE_COUNT);
	if (res.IsSuccess() && outCount != NULL)
		*outCount = m_action.operatorProfileCount;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258140 */
TelephonyResult TelGeneralLogicMediator::GetOperatorProfile(int index,
							    TelOperatorProfile *outProfile)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_OPERATOR_PROFILE);
	m_action.operatorProfileIndex = index;
	m_stateMachine.GetOperatorProfile();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_OPERATOR_PROFILE);
	if (res.IsSuccess() && outProfile != NULL)
		*outProfile = m_action.operatorProfile;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258498 */
TelephonyResult TelGeneralLogicMediator::GetApnInfoListCount(int *outCount)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_APN_INFO_LIST_COUNT);
	m_stateMachine.GetApnInfoListCount();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_APN_INFO_LIST_COUNT);
	if (res.IsSuccess() && outCount != NULL)
		*outCount = m_action.apnListCount;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258558 */
TelephonyResult TelGeneralLogicMediator::GetApnInfoListItem(int index,
							    TelOperatorApnInfo *outApnInfo)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_APN_INFO_LIST_ITEM);
	m_action.apnListIndex = index;
	m_stateMachine.GetApnInfoListItem();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_APN_INFO_LIST_ITEM);
	if (res.IsSuccess() && outApnInfo != NULL)
		*outApnInfo = m_action.apnListItem;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258694 */
TelephonyResult TelGeneralLogicMediator::GetCurrentApnInfo(TelephonyOperator *outApnInfo)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue apnAttr(TEL_ATTR_APN_NAME);
	TelephonyResult res = attrMgr->GetAttribute(&apnAttr);
	TelAttributeValue userAttr(TEL_ATTR_APN_USER_ID);
	res = attrMgr->GetAttribute(&userAttr);
	TelAttributeValue passAttr(TEL_ATTR_APN_PASSWORD);
	res = attrMgr->GetAttribute(&passAttr);

	if (outApnInfo != NULL) {
		outApnInfo->SetCredentials(apnAttr.stringValue, userAttr.stringValue,
					   passAttr.stringValue);
	}
	return res;
}

/* 0x8125895C */
TelephonyResult TelGeneralLogicMediator::RemoveCurrentApnInfo()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_REMOVE_CURRENT_APN_INFO);
	m_stateMachine.RemoveCurrentApnInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_REMOVE_CURRENT_APN_INFO);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258A6C */
TelephonyResult TelGeneralLogicMediator::StartNetworkScan()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_START_NETWORK_SCAN);
	m_stateMachine.StartNetworkScan();
	TelephonyResult res = m_action.GetApiResult(TEL_API_START_NETWORK_SCAN);
	if (res.IsSuccess())
		m_scanActive = true;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258B1A */
TelephonyResult TelGeneralLogicMediator::AbortNetworkScan()
{
	TelephonyResult res = (m_bbmcAdapter != NULL)
		? m_bbmcAdapter->AbortNetworkScan()
		: TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	m_scanActive = false;
	return res;
}

/* 0x81258B76 */
TelephonyResult TelGeneralLogicMediator::GetNetworkScanInfoCount(int *outCount)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_NETWORK_SCAN_INFO_COUNT);
	m_stateMachine.GetNetworkScanInfoCount();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_NETWORK_SCAN_INFO_COUNT);
	if (res.IsSuccess() && outCount != NULL)
		*outCount = m_action.networkScanCount;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258C36 */
TelephonyResult TelGeneralLogicMediator::GetNetworkScanInfo(int index,
							    TelNetScanInfo *outScanInfo)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_NETWORK_SCAN_INFO);
	m_action.networkScanIndex = index;
	m_stateMachine.GetNetworkScanInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_NETWORK_SCAN_INFO);
	if (res.IsSuccess() && outScanInfo != NULL)
		*outScanInfo = m_action.networkScanItem;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258D02 */
TelephonyResult TelGeneralLogicMediator::SetNetRegisterAuto()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_NET_REGISTER_AUTO);
	m_stateMachine.SetNetRegisterAuto();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_NET_REGISTER_AUTO);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258DB0 */
TelephonyResult TelGeneralLogicMediator::SetNetRegisterInfo(const TelNetScanInfo &scanInfo)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_NET_REGISTER_INFO);
	m_action.manualRegisterTarget = scanInfo;
	m_stateMachine.SetNetRegisterInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_NET_REGISTER_INFO);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258E88 */
TelephonyResult TelGeneralLogicMediator::GetNetRegistrationInfo(TelNetworkInfo *outInfo)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_NET_REGISTRATION_INFO);
	m_stateMachine.GetNetRegistrationInfo();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_NET_REGISTRATION_INFO);
	if (res.IsSuccess() && outInfo != NULL)
		*outInfo = m_action.netRegistrationInfo;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81258FAA */
TelephonyResult TelGeneralLogicMediator::GetSavedNetworkSelection(TelNetworkInfo *outInfo)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue mccAttr(TEL_ATTR_MCC);
	TelephonyResult res = attrMgr->GetAttribute(&mccAttr);
	SceUInt16 mcc = res.IsSuccess() ? (SceUInt16)mccAttr.intValue : 0;

	TelAttributeValue mncAttr(TEL_ATTR_MNC);
	res = attrMgr->GetAttribute(&mncAttr);
	SceUInt16 mnc = res.IsSuccess() ? (SceUInt16)mncAttr.intValue : 0;

	if (outInfo != NULL)
		outInfo->SetPlmnCodes(mcc, mnc);

	TelAttributeValue ratAttr(TEL_ATTR_RAT);
	res = attrMgr->GetAttribute(&ratAttr);
	if (outInfo != NULL)
		outInfo->SetNetworkType(ratAttr.intValue);

	return res;
}

/* 0x81259196 */
TelephonyResult TelGeneralLogicMediator::SetRegistrationAppNetMode(int enableRestricted)
{
	TelAppMgrStatusRecord status;
	sce_paf_memset(&status, 0, sizeof(status));

	int appErr = sceAppMgrGetStatusByName("NPXS10084", &status);
	if (appErr != 0)
		appErr = sceAppMgrGetStatusByName("NPXS10037", &status);

	if (appErr == 0) {
		int netMode = (enableRestricted != 0) ? 2 : 0;
		int netErr = SceNetInternal_0DFC23D6(status.processId, netMode, 0);
		if (netErr != 0)
			return TelephonyResult(netErr);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x812591FE */
TelephonyResult TelGeneralLogicMediator::SetDataRoamingPermission(bool allow)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_DATA_ROAMING_PERMISSION);
	m_action.dataRoamingPermission = allow;
	m_stateMachine.SetDataRoamingPermission();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_DATA_ROAMING_PERMISSION);
	m_smcMutex.Unlock();
	return res;
}

/* 0x812592FE */
TelephonyResult TelGeneralLogicMediator::GetDataRoamingPermission(bool *outAllowed)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL) {
		*outAllowed = true;
		return TelephonyResult(TEL_OK);
	}

	TelAttributeValue attr(TEL_ATTR_ENABLE_DATA_ROAMING);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	int enabled = attr.intValue;
	if (!res.IsSuccess()) {
		enabled = 1;
		res.SetCode(TEL_OK);
	}
	*outAllowed = (enabled != 0);
	return res;
}

/* 0x812593C0 */
TelephonyResult TelGeneralLogicMediator::SetDataRoamingDefinition(int definition)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_SET_DATA_ROAMING_DEFINITION);
	m_action.dataRoamingDefinition = definition;
	m_stateMachine.SetDataRoamingDefinition();
	TelephonyResult res = m_action.GetApiResult(TEL_API_SET_DATA_ROAMING_DEFINITION);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259690 */
TelephonyResult TelGeneralLogicMediator::CheckSimCondition()
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_CHECK_SIM_CONDITION);
	m_stateMachine.CheckSimCondition();
	TelephonyResult res = m_action.GetApiResult(TEL_API_CHECK_SIM_CONDITION);
	m_smcMutex.Unlock();
	return res;
}

/* 0x8125973C */
TelephonyResult TelGeneralLogicMediator::GetNetworkStatusReport(paf::string *outReport)
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	return m_bbmcAdapter->GetNetworkStatusReport(outReport);
}

/* 0x8125974C */
TelephonyResult TelGeneralLogicMediator::GetDeviceInfoReport(paf::string *outReport)
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	return m_bbmcAdapter->GetDeviceInfoReport(outReport);
}

/* 0x812594C4 */
TelephonyResult TelGeneralLogicMediator::GetIsComExist(int *outExists)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL) {
		*outExists = 0;
		return TelephonyResult(TEL_OK);
	}

	TelAttributeValue attr(TEL_ATTR_IS_COM_EXIST);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	*outExists = (res.IsSuccess() && attr.intValue != 0) ? 1 : 0;
	return res;
}

/* 0x812595D6 */
TelephonyResult TelGeneralLogicMediator::GetIsApnInit(int *outInitialized)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL) {
		*outInitialized = 0;
		return TelephonyResult(TEL_OK);
	}

	TelAttributeValue attr(TEL_ATTR_IS_APN_INIT);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	*outInitialized = (res.IsSuccess() && attr.intValue != 0) ? 1 : 0;
	return res;
}

/* 0x8125975C */
TelephonyResult TelGeneralLogicMediator::IsOpenSimModel(bool *outOpenModel)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_IS_OPEN_SIM_MODEL);
	m_stateMachine.IsOpenSimModel();
	TelephonyResult res = m_action.GetApiResult(TEL_API_IS_OPEN_SIM_MODEL);
	if (res.IsSuccess() && outOpenModel != NULL)
		*outOpenModel = m_action.openSimModel;
	m_smcMutex.Unlock();
	return res;
}

/* 0x8125981C */
TelephonyResult TelGeneralLogicMediator::GetNetworkSelectionMode(int *outMode)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL) {
		*outMode = 0;
		return TelephonyResult(TEL_OK);
	}

	TelAttributeValue attr(TEL_ATTR_NETWORK_SELECTION);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	*outMode = (res.IsSuccess() && attr.intValue != 0) ? 1 : 0;
	return res;
}

/* 0x812598D6 */
TelephonyResult TelGeneralLogicMediator::GetConditionError(TelephonyResult *outConditionError)
{
	if (outConditionError != NULL)
		*outConditionError = m_action.GetHighestPriorityError();
	return TelephonyResult(TEL_OK);
}

/* 0x81259916 */
TelephonyResult TelGeneralLogicMediator::DeactivateSimLock(const TelCarrierLockKey &key)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_DEACTIVATE_SIM_LOCK);
	m_action.deactivateSimLockKey = key;
	m_stateMachine.DeactivateSimLock();
	TelephonyResult res = m_action.GetApiResult(TEL_API_DEACTIVATE_SIM_LOCK);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259AB6 */
TelephonyResult TelGeneralLogicMediator::UnblockSimLock(const TelControlKey &key)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_UNBLOCK_SIM_LOCK);
	m_action.unblockSimLockKey = key;
	m_stateMachine.UnblockSimLock();
	TelephonyResult res = m_action.GetApiResult(TEL_API_UNBLOCK_SIM_LOCK);
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259C4C */
TelephonyResult TelGeneralLogicMediator::GetSimLockDeactivateRemain(int *outRemain)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_SIM_LOCK_DEACTIVATE_REMAIN);
	m_stateMachine.GetSimLockDeactivateRemain();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_SIM_LOCK_DEACTIVATE_REMAIN);
	if (res.IsSuccess() && outRemain != NULL)
		*outRemain = m_action.simLockDeactivateRemain;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259D0C */
TelephonyResult TelGeneralLogicMediator::GetSimLockUnblockRemain(int *outRemain)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_GET_SIM_LOCK_UNBLOCK_REMAIN);
	m_stateMachine.GetSimLockUnblockRemain();
	TelephonyResult res = m_action.GetApiResult(TEL_API_GET_SIM_LOCK_UNBLOCK_REMAIN);
	if (res.IsSuccess() && outRemain != NULL)
		*outRemain = m_action.simLockUnblockRemain;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259DCC */
TelephonyResult TelGeneralLogicMediator::CheckImsiForOperatorCode(int *inOutOperatorCode,
								  bool *outMatched)
{
	m_smcMutex.Lock();
	m_action.SetCurrentApiId(TEL_API_CHECK_IMSI_FOR_OPERATOR_CODE);
	m_action.checkOperatorCode = (inOutOperatorCode != NULL) ? *inOutOperatorCode : 0;
	m_action.checkOperatorMatched = false;
	m_stateMachine.CheckImsiForOperatorCode();
	TelephonyResult res = m_action.GetApiResult(TEL_API_CHECK_IMSI_FOR_OPERATOR_CODE);
	if (res.IsSuccess() && outMatched != NULL)
		*outMatched = m_action.checkOperatorMatched;
	m_smcMutex.Unlock();
	return res;
}

/* 0x81259EA4 */
TelephonyResult TelGeneralLogicMediator::GetCurrentSignalLevel(int *outLevel)
{
	if (outLevel != NULL)
		m_cachedNetworkInfo.GetSignalLevel(outLevel);
	return TelephonyResult(TEL_OK);
}

/* 0x81259EE2 */
TelephonyResult TelGeneralLogicMediator::SetBillingPlan(const int *billingPlan)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue attr(TEL_ATTR_BILLING_PLAN);
	attr.SetInt((billingPlan != NULL) ? *billingPlan : 0);
	return attrMgr->SetAttribute(attr);
}

/* 0x81259FDA */
TelephonyResult TelGeneralLogicMediator::GetBillingPlan(int *outBillingPlan)
{
	*outBillingPlan = 0;
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue attr(TEL_ATTR_BILLING_PLAN);
	TelephonyResult res = attrMgr->GetAttribute(&attr);
	if (res.IsSuccess())
		*outBillingPlan = attr.intValue;
	return res;
}

/* 0x8125A170 */
TelephonyResult TelGeneralLogicMediator::SetDebugLogEnabled(bool enabled)
{
	if (m_debugLogEnabled && !enabled) {
		TelBbmcLogManager *logMgr = TelBbmcLogManager::GetInstance();
		if (logMgr != NULL)
			logMgr->ClearLog();
	}
	m_debugLogEnabled = enabled;
	return TelephonyResult(TEL_OK);
}

/* 0x8125AAB4 */
TelephonyResult TelGeneralLogicMediator::ReadDebugLogLine(paf::string *outLine)
{
	*outLine = "";
	if (m_debugLogStage == 0) {
		TelephonyResult res = ReadNetworkReportLine(outLine);
		if (res.IsSuccess() && outLine->empty() && m_debugLogEnabled) {
			TelBbmcLogManager *logMgr = TelBbmcLogManager::GetInstance();
			if (logMgr != NULL) {
				logMgr->RewindRead();
				char lineBuf[257];
				int readLen = logMgr->ReadFormatted(lineBuf, 255);
				if (readLen < 0)
					readLen = 0;
				if (readLen > 256)
					readLen = 256;
				lineBuf[readLen] = '\0';
				*outLine = lineBuf;
				if (!outLine->empty())
					m_debugLogStage = 1;
			}
		}
		return res;
	}

	if (m_debugLogStage == 1) {
		TelBbmcLogManager *logMgr = TelBbmcLogManager::GetInstance();
		if (logMgr != NULL) {
			char lineBuf[257];
			int readLen = logMgr->ReadFormatted(lineBuf, 255);
			if (readLen < 0)
				readLen = 0;
			if (readLen > 256)
				readLen = 256;
			lineBuf[readLen] = '\0';
			*outLine = lineBuf;
		}
		if (outLine->empty())
			m_debugLogStage = 0;
		return TelephonyResult(TEL_OK);
	}

	m_debugLogStage = 0;
	return TelephonyResult(TEL_OK);
}

/* 0x8125AC84 */
TelephonyResult TelGeneralLogicMediator::SetDebugDiagFlag(const int *flagValue)
{
	if (flagValue != NULL)
		m_debugDiagFlag = *flagValue;
	return TelephonyResult(TEL_OK);
}

/* 0x8125AC96 */
TelephonyResult TelGeneralLogicMediator::GetDebugDiagFlag(int *outFlagValue)
{
	if (outFlagValue != NULL)
		*outFlagValue = m_debugDiagFlag;
	return TelephonyResult(TEL_OK);
}

/* 0x8125ACA8 */
TelephonyResult TelGeneralLogicMediator::SetDebugForceFlag(const int *flagValue)
{
	if (flagValue != NULL)
		m_debugForceFlag = *flagValue;
	return TelephonyResult(TEL_OK);
}

/* 0x8125ACBA */
TelephonyResult TelGeneralLogicMediator::GetDebugForceFlag(int *outFlagValue)
{
	if (outFlagValue != NULL)
		*outFlagValue = m_debugForceFlag;
	return TelephonyResult(TEL_OK);
}

/* 0x8125ACCC */
TelephonyResult TelGeneralLogicMediator::IsSimActivated(int *outActivated)
{
	int activationState = 0;
	if (m_netCtl != NULL)
		m_netCtl->GetSimActivationState(&activationState);
	*outActivated = (activationState == 0) ? 1 : 0;
	return TelephonyResult(TEL_OK);
}

/* 0x8125AD1A */
TelephonyResult TelGeneralLogicMediator::GetLatestArchiveVersion(SceUInt32 *outVersion)
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	TelephonyResult res = m_bbmcAdapter->GetLatestArchiveVersion(outVersion);
	if (res.GetCode() == kBbmcFeatureUnavailable)
		res.SetCode(kTelFeatureUnavailable);
	return res;
}

/* 0x8125AD8A */
TelephonyResult TelGeneralLogicMediator::GetArchiveVersion(SceUInt32 *outVersion)
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	TelephonyResult res = m_bbmcAdapter->GetArchiveVersion(outVersion);
	if (res.GetCode() == kBbmcFeatureUnavailable)
		res.SetCode(kTelFeatureUnavailable);
	return res;
}

/* 0x8125ADFA */
TelephonyResult TelGeneralLogicMediator::CheckComVersion(bool *outMatches)
{
	*outMatches = false;
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	TelephonyResult res = m_bbmcAdapter->CheckComVersion(outMatches);
	if (res.GetCode() == kBbmcFeatureUnavailable)
		res.SetCode(kTelFeatureUnavailable);
	return res;
}

/* 0x8125AE6E */
TelephonyResult TelGeneralLogicMediator::ReleasePdpContext()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	return m_bbmcAdapter->ReleasePdpContext();
}

/* 0x8125AEB2 */
TelephonyResult TelGeneralLogicMediator::StartApnListDownload()
{
	return telStartApnInfoListDownloader();
}

/* 0x8125AEBE */
TelephonyResult TelGeneralLogicMediator::CheckContentDownloadAllowed(int serviceType)
{
	if (m_netCtl == NULL)
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	return m_netCtl->CheckContentDownloadAllowed(serviceType);
}

/* 0x8125AF04 */
TelephonyResult TelGeneralLogicMediator::GetDownloadSizeLimit(unsigned int serviceType,
							      SceInt64 *outLimitBytes)
{
	if (m_netCtl == NULL)
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	return m_netCtl->GetDownloadSizeLimit(serviceType, outLimitBytes);
}

/* 0x8125AF58 */
TelephonyResult TelGeneralLogicMediator::CheckRoamingDownloadAllowed(unsigned int serviceType)
{
	if (m_netCtl == NULL)
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	return m_netCtl->CheckRoamingDownloadAllowed(serviceType);
}

/* 0x8125AF9E */
TelephonyResult TelGeneralLogicMediator::GetModemFirmwareVersion(paf::string *outVersion)
{
	*outVersion = "";
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	TelephonyResult res = m_bbmcAdapter->GetFirmwareRevision(outVersion);
	if (res.IsSuccess()) {
		const char *delim = "] - ";
		const char *rawStr = outVersion->c_str();
		const char *found = sce_paf_strstr(rawStr, delim);
		if (found != NULL)
			*outVersion = paf::string(found + sce_paf_strlen(delim));
	}
	return res;
}

/* 0x8125B0CA */
TelephonyResult TelGeneralLogicMediator::GetUpdateListUrl(paf::string *outUrl)
{
	if (m_netCtl == NULL)
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	return m_netCtl->GetUpdateListUrl(outUrl);
}

/* 0x8125B110 */
TelephonyResult TelGeneralLogicMediator::NotifyPluginState(const char *pluginName, int stateValue)
{
	if (pluginName != NULL && sce_paf_strcmp(pluginName, "tel_initial_check_plugin") == 0)
		TelBbmcSharedData::SetStaticInt(524329, (SceUInt32)stateValue);
	return TelephonyResult(TEL_OK);
}

/* Factory helper called by TelLogicFactory::CreateAndStart */
TelGeneralLogicMediatorBase *telCreateGeneralLogicMediator(TelBbmcAdapterInterface *bbmcAdapter,
							   TelGeneralLogicNetCtl *netCtl)
{
	void *mem = telMalloc(sizeof(TelGeneralLogicMediator));
	return (mem != NULL) ? new (mem) TelGeneralLogicMediator(bbmcAdapter, netCtl) : NULL;
}

} /* namespace telephony */
