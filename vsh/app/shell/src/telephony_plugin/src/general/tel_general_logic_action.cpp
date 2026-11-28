/*
 * tel_general_logic_action.cpp: general telephony SMC action executor
 * handling BBMC events, SIM/PIN/PUK/CK operations, APN/profile/roaming/scan
 * configuration, error-mask prioritization, and callback dispatch
 * (3.65-CEX: 0x81261068..0x81268F93).
 */
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include "src/livespace_db/lsdb_keys.h"
#include "tel_general_internal.h"

extern "C" {
int SceNetInternal_7493FB0C(int mode, int flags);
}

namespace telephony {

struct TelErrorPriorityEntry {
	SceUInt32 mask;
	int errorCode;
};

/* 0x81516014: 10-entry priority table mapping active error flags to condition error codes */
static const TelErrorPriorityEntry s_conditionErrorTable[] = {
	{ TEL_ERR_BIT_COM_FAILURE,    TEL_COND_ERR_COM_FAILURE },
	{ TEL_ERR_BIT_COM_BOOT_TEMP,  TEL_COND_ERR_COM_BOOT_TEMP },
	{ TEL_ERR_BIT_COM_BOOT_OTHER, TEL_COND_ERR_COM_BOOT_OTHER },
	{ TEL_ERR_BIT_LOW_BATTERY,    TEL_COND_ERR_LOW_BATTERY },
	{ TEL_ERR_BIT_SIM_DISABLED,   TEL_COND_ERR_SIM_DISABLED },
	{ TEL_ERR_BIT_SIM_BLOCKED,    TEL_COND_ERR_SIM_BLOCKED },
	{ TEL_ERR_BIT_SIM_UNUSABLE,   TEL_COND_ERR_SIM_UNUSABLE },
	{ TEL_ERR_BIT_HIGH_TEMP,      TEL_COND_ERR_HIGH_TEMP },
	{ TEL_ERR_BIT_ATTRIBUTE,      TEL_COND_ERR_ATTRIBUTE },
	{ TEL_ERR_BIT_BBMC,           TEL_COND_ERR_BBMC }
};

static const int kComFailureMaxCount = 3;
static const int kComFailureWindowSeconds = 600;
static const int kComBootTempMinErr = -2146424776;	/* 0x80102038 / 0x80102838 range */
static const int kComBootTempMaxErr = -2146424775;	/* 0x80102039 */
static const int kBbmcBootTempLow = -2146422728;	/* 0x80102838 */
static const int kBbmcBootTempHigh = -2146422727;	/* 0x80102839 */
static const int kInvalidStateError = -2146424821;	/* 0x8010200B */

/* ------------------------------------------------------------------
 * Construction, initialization, callbacks, and state/error helpers
 * ------------------------------------------------------------------ */

/* 0x81261068 */
TelGeneralLogicAction::TelGeneralLogicAction()
	: TelObservable("SceTelGenLogicActionObsv"),
	  m_callbackMutex("SceTelGenLogicActionCb", 0),
	  m_eventCallback(NULL),
	  m_callbackUserData(NULL),
	  m_legacyCallback(NULL),
	  m_bbmcAdapter(NULL),
	  m_netCtl(NULL),
	  m_stateMutex("SceTelGenLogicActionSt", 0),
	  m_stateCode(TEL_SM_STATE_INITIAL),
	  m_errorFlags(0),
	  m_lpmManager(NULL),
	  m_simInserted(false),
	  m_pinReady(false),
	  m_setupReady(false),
	  m_networkReady(false),
	  m_livespaceReady(false),
	  m_comFwChecked(false),
	  m_deviceInfoReady(false),
	  m_dataRoamingDef(0)
{
}

/* 0x8126120A */
TelGeneralLogicAction::~TelGeneralLogicAction()
{
	Terminate();
}

/* 0x8126145A */
TelephonyResult TelGeneralLogicAction::Initialize(TelObserver *mediatorObserver,
						  TelBbmcAdapterInterface *bbmcAdapter,
						  TelGeneralLogicNetCtl *netCtl)
{
	m_bbmcAdapter = bbmcAdapter;
	m_netCtl = netCtl;

	if (m_lpmManager == NULL) {
		void *mem = telMalloc(sizeof(TelGeneralLpmManager));
		if (mem == NULL)
			return TelephonyResult(TEL_ERR_NO_MEMORY);
		m_lpmManager = new (mem) TelGeneralLpmManager(m_bbmcAdapter);
	}

	m_comFailureCounter.SetThreshold(kComFailureMaxCount, kComFailureWindowSeconds);
	if (m_bbmcAdapter != NULL && mediatorObserver != NULL)
		m_bbmcAdapter->AddObserver(mediatorObserver);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL && mediatorObserver != NULL)
		attrMgr->AddObserver(mediatorObserver);

	telLoadSupportedOperatorPlmns(&m_supportedPlmns);
	return TelephonyResult(TEL_OK);
}

/* 0x81261538 */
TelephonyResult TelGeneralLogicAction::Terminate()
{
	if (m_lpmManager != NULL) {
		m_lpmManager->~TelGeneralLpmManager();
		telFree(m_lpmManager);
		m_lpmManager = NULL;
	}
	m_operatorProfiles.clear();
	m_apnInfoList.GetEntries().clear();
	m_scanInfoList.GetEntries().clear();
	m_supportedPlmns.clear();
	return TelephonyResult(TEL_OK);
}

/* 0x81261608 */
void TelGeneralLogicAction::NotifyMediatorEvent(const TelEventInfo &event)
{
	NotifyObservers(event);
}

/* 0x81261688 */
TelephonyResult TelGeneralLogicAction::SetEventCallback(TelGeneralEventCallback callback,
							void *userData)
{
	m_callbackMutex.Lock();
	m_eventCallback = callback;
	m_callbackUserData = userData;
	m_callbackMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x812616B2 */
TelephonyResult TelGeneralLogicAction::SetLegacyCallback(TelGeneralLegacyCallback callback,
							 void *userData)
{
	m_callbackMutex.Lock();
	m_legacyCallback = callback;
	m_callbackUserData = userData;
	m_callbackMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

void TelGeneralLogicAction::DispatchToCallbacks(const TelEventInfo &event)
{
	m_callbackMutex.Lock();
	TelGeneralLegacyEventRecord record;
	record.category = event.GetCategory();
	record.eventId = event.GetEventId();
	record.eventArg = event.GetEventArg();
	record.eventResult = event.GetEventResult();

	if (m_eventCallback != NULL)
		m_eventCallback(event, m_callbackUserData);
	if (m_legacyCallback != NULL)
		m_legacyCallback(&record, m_callbackUserData);
	m_callbackMutex.Unlock();
}

/* 0x81261778 */
bool TelGeneralLogicAction::HasErrorFlag(SceUInt32 mask)
{
	m_stateMutex.Lock();
	bool isSet = (m_errorFlags & mask) != 0;
	m_stateMutex.Unlock();
	return isSet;
}

bool TelGeneralLogicAction::HasAnyError()
{
	m_stateMutex.Lock();
	bool hasErr = (m_errorFlags != 0);
	m_stateMutex.Unlock();
	return hasErr;
}

void TelGeneralLogicAction::SetErrorFlag(SceUInt32 mask)
{
	m_stateMutex.Lock();
	m_errorFlags |= mask;
	m_stateMutex.Unlock();
}

void TelGeneralLogicAction::ClearErrorFlag(SceUInt32 mask)
{
	m_stateMutex.Lock();
	m_errorFlags &= ~mask;
	m_stateMutex.Unlock();
}

/* 0x812617D4 */
int TelGeneralLogicAction::GetStateCode()
{
	m_stateMutex.Lock();
	int code = m_stateCode;
	m_stateMutex.Unlock();
	return code;
}

void TelGeneralLogicAction::SetStateCode(int stateCode)
{
	m_stateMutex.Lock();
	m_stateCode = stateCode;
	m_stateMutex.Unlock();
}

/* 0x812617F2 */
TelephonyResult TelGeneralLogicAction::GetHighestPriorityError()
{
	TelephonyResult res(TEL_OK);
	m_stateMutex.Lock();
	SceSize count = sizeof(s_conditionErrorTable) / sizeof(s_conditionErrorTable[0]);
	for (SceSize idx = 0; idx < count; ++idx) {
		if ((m_errorFlags & s_conditionErrorTable[idx].mask) != 0) {
			res.SetCode(s_conditionErrorTable[idx].errorCode);
			break;
		}
	}
	m_stateMutex.Unlock();
	return res;
}

/* 0x81268DDE */
void TelGeneralLogicAction::NotifyConditionError()
{
	TelephonyResult err = GetHighestPriorityError();
	TelEventInfo notifyEv(TEL_EV_NOTIFY_CONDITION_ERROR, -1, err.GetCode());
	DispatchToCallbacks(notifyEv);
}

void TelGeneralLogicAction::NotifyCachedErrorIfPresent()
{
	if (HasAnyError())
		NotifyConditionError();
}

/* 0x81268D9E */
TelephonyResult TelGeneralLogicAction::SetBbmcTurnOff(bool forceStop)
{
	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->TurnOff(forceStop);
	return TelephonyResult(TEL_OK);
}

/* 0x81268EDC */
void TelGeneralLogicAction::ForceLpmMode()
{
	if (m_lpmManager != NULL)
		m_lpmManager->SetLowPowerModeImmediate(true);
}

/* 0x81268F2C */
bool TelGeneralLogicAction::IsSimPermanentlyBlockedNow()
{
	if (m_bbmcAdapter == NULL)
		return false;
	int ckState = 1;
	TelephonyResult res = m_bbmcAdapter->GetControlKeyState(&ckState);
	return res.IsSuccess() && (ckState == 3);
}

/* 0x81261866: default handler when an API call is rejected by the current SMC state */
void TelGeneralLogicAction::HandleUnexpectedTransition()
{
	int curApi = GetCurrentApiId();
	if (curApi > TEL_API_NONE && curApi < TEL_API_COUNT) {
		TelephonyResult err(kInvalidStateError);
		SetApiResult(curApi, err);
	}
}

/* ------------------------------------------------------------------
 * Internal setup helpers (IMSI, profiles, APN, RSSI, roaming, net selection)
 * ------------------------------------------------------------------ */

/* 0x812619E0: apply RSSI thresholds from TelAntennaThresholdInfoFactory */
TelephonyResult TelGeneralLogicAction::ApplyRssiThresholds()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_OK);

	TelAntennaThresholdInfoFactory factory;
	TelAntennaThresholdInfo info;
	TelephonyResult res = factory.Create(1, &info);
	if (!res.IsSuccess())
		return res;

	paf::vector<int> thresholds;
	res = info.GetThresholds(&thresholds);
	if (res.IsSuccess() && !thresholds.empty())
		res = m_bbmcAdapter->SetRssiThresholds(thresholds);
	return res;
}

/* 0x81261E08: configure BBMC data roaming control bit from registry + domestic override */
TelephonyResult TelGeneralLogicAction::ApplyDataRoamingControl()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_OK);

	bool userRoamingAllowed = true;
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL) {
		TelAttributeValue roamAttr(TEL_ATTR_ENABLE_DATA_ROAMING);
		TelephonyResult res = attrMgr->GetAttribute(&roamAttr);
		if (res.IsSuccess())
			userRoamingAllowed = (roamAttr.intValue != 0);
	}

	bool domesticAllowed = false;
	if (m_netCtl != NULL) {
		m_netCtl->UpdateRoamingOverride();
		m_netCtl->IsDomesticRoamingAllowed(&domesticAllowed);
	}

	bool enableRoaming = userRoamingAllowed || domesticAllowed;
	return m_bbmcAdapter->SetRoamingControlBit(enableRoaming);
}

/* 0x81261CC6: query IMSI, resolve PLMN, load operator profiles and APN list */
TelephonyResult TelGeneralLogicAction::LoadImsiAndProfiles()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	TelephonyResult res = m_bbmcAdapter->FetchDeviceInformation();
	if (!res.IsSuccess()) {
		TelEventInfo errEv(TEL_EV_INTERNAL_CAN_NOT_USE_SIM, -1, res.GetCode());
		NotifyMediatorEvent(errEv);
		return res;
	}

	TelImeiInfo imsiInfo;
	res = m_bbmcAdapter->GetImsi(&imsiInfo);
	if (!res.IsSuccess()) {
		TelEventInfo errEv(TEL_EV_INTERNAL_CAN_NOT_USE_SIM, -1, res.GetCode());
		NotifyMediatorEvent(errEv);
		return res;
	}

	TelMccMncUtil mccUtil;
	res = mccUtil.ResolveFromImsi(&m_imsiPlmn, imsiInfo.GetValue());
	if (!res.IsSuccess()) {
		if (m_bbmcAdapter->GetHomePlmn(&m_imsiPlmn).IsSuccess()) {
			mccUtil.ResolveFromNumeric(&m_imsiPlmn, m_imsiPlmn.GetMcc(),
						   m_imsiPlmn.GetMnc());
		}
	}

	int targetOperator = TEL_OPERATOR_DEFAULT;
	telGetProductTargetOperator(&targetOperator);

	m_operatorProfiles.clear();
	TelOperatorProfileFactory profileFactory(targetOperator, m_imsiPlmn, 0);
	profileFactory.LoadProfiles(&m_operatorProfiles);

	m_deviceInfoReady = true;
	return LoadApnListForImsi();
}

TelephonyResult TelGeneralLogicAction::LoadApnListForImsi()
{
	m_apnInfoList.GetEntries().clear();
	TelApnInfoListFactory listFactory;
	return listFactory.Create(m_imsiPlmn, &m_apnInfoList);
}

TelephonyResult TelGeneralLogicAction::SaveApnToRegistry(const TelephonyOperator &apnCredentials,
							 int billingPlan, bool writeBillingPlan)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	paf::string apnName;
	paf::string userId;
	paf::string password;
	apnCredentials.GetCredentials(&apnName, &userId, &password);

	TelAttributeValue apnAttr(TEL_ATTR_APN_NAME);
	apnAttr.SetString(apnName);
	TelephonyResult res = attrMgr->SetAttribute(apnAttr);
	if (!res.IsSuccess())
		return res;

	TelAttributeValue userAttr(TEL_ATTR_APN_USER_ID);
	userAttr.SetString(userId);
	res = attrMgr->SetAttribute(userAttr);
	if (!res.IsSuccess())
		return res;

	TelAttributeValue passAttr(TEL_ATTR_APN_PASSWORD);
	passAttr.SetString(password);
	res = attrMgr->SetAttribute(passAttr);
	if (!res.IsSuccess())
		return res;

	TelAttributeValue initAttr(TEL_ATTR_IS_APN_INIT);
	initAttr.SetInt(1);
	res = attrMgr->SetAttribute(initAttr);

	if (writeBillingPlan && res.IsSuccess()) {
		TelAttributeValue planAttr(TEL_ATTR_BILLING_PLAN);
		planAttr.SetInt(billingPlan);
		res = attrMgr->SetAttribute(planAttr);
	}
	return res;
}

/* 0x8126268E: validate SIM against IMSI history and push initial APN to BBMC */
TelephonyResult TelGeneralLogicAction::ApplyInitialApnSettings()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	/* Compare current SIM IMSI against stored IMSI to detect SIM swap */
	TelImeiInfo currentImsi;
	m_bbmcAdapter->GetImsi(&currentImsi);

	TelAttributeValue savedImsiAttr(TEL_ATTR_IMSI);
	attrMgr->GetAttribute(&savedImsiAttr);

	bool simChanged = (savedImsiAttr.stringValue != currentImsi.GetValue());
	if (simChanged && !currentImsi.GetValue().empty()) {
		TelAttributeValue newImsiAttr(TEL_ATTR_IMSI);
		newImsiAttr.SetString(currentImsi.GetValue());
		attrMgr->SetAttribute(newImsiAttr);

		TelAttributeValue initReset(TEL_ATTR_IS_APN_INIT);
		initReset.SetInt(0);
		attrMgr->SetAttribute(initReset);
	}

	/* Check operator-specific SIM match */
	if (m_netCtl != NULL && !m_netCtl->IsSimCardMatchOperator(m_imsiPlmn)) {
		TelEventInfo errEv(TEL_EV_INTERNAL_CAN_NOT_USE_SIM, -1, TEL_ERR_SIM_INVALID);
		NotifyMediatorEvent(errEv);
		return TelephonyResult(TEL_ERR_SIM_INVALID);
	}

	TelAttributeValue isApnInitAttr(TEL_ATTR_IS_APN_INIT);
	attrMgr->GetAttribute(&isApnInitAttr);

	TelephonyOperator activeApn;
	if (isApnInitAttr.intValue != 0 && !simChanged) {
		TelAttributeValue apnAttr(TEL_ATTR_APN_NAME);
		TelAttributeValue userAttr(TEL_ATTR_APN_USER_ID);
		TelAttributeValue passAttr(TEL_ATTR_APN_PASSWORD);
		attrMgr->GetAttribute(&apnAttr);
		attrMgr->GetAttribute(&userAttr);
		attrMgr->GetAttribute(&passAttr);
		activeApn.SetCredentials(apnAttr.stringValue, userAttr.stringValue,
					 passAttr.stringValue);
	} else {
		/* Select first auto-selectable APN from the PLMN APN list */
		const paf::vector<TelOperatorApnInfo> &entries = m_apnInfoList.GetEntries();
		for (SceSize idx = 0; idx < entries.size(); ++idx) {
			if (entries[idx].IsAutoSelectable()) {
				entries[idx].GetApnCredentials(&activeApn);
				SaveApnToRegistry(activeApn, entries[idx].GetBillingPlan(), true);
				break;
			}
		}
	}

	TelephonyResult res = m_bbmcAdapter->SetApnCredentials(activeApn);
	if (!res.IsSuccess()) {
		TelEventInfo bbmcErr(TEL_EV_INTERNAL_BBMC_ERROR, -1, res.GetCode());
		NotifyMediatorEvent(bbmcErr);
	}
	return res;
}

/* 0x812631F6: apply saved automatic or manual network registration mode */
TelephonyResult TelGeneralLogicAction::ApplyNetworkSelection()
{
	if (m_bbmcAdapter == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr == NULL)
		return TelephonyResult(TEL_OK);

	TelAttributeValue modeAttr(TEL_ATTR_NETWORK_SELECTION);
	TelephonyResult res = attrMgr->GetAttribute(&modeAttr);
	if (!res.IsSuccess() || modeAttr.intValue == 0)
		return m_bbmcAdapter->RegisterNetworkAuto();

	TelAttributeValue mccAttr(TEL_ATTR_MCC);
	TelAttributeValue mncAttr(TEL_ATTR_MNC);
	TelAttributeValue ratAttr(TEL_ATTR_RAT);
	attrMgr->GetAttribute(&mccAttr);
	attrMgr->GetAttribute(&mncAttr);
	attrMgr->GetAttribute(&ratAttr);

	TelephonyPlmn targetPlmn;
	targetPlmn.SetMccMnc((SceUInt16)mccAttr.intValue, (SceUInt16)mncAttr.intValue);
	return m_bbmcAdapter->RegisterNetworkManual(targetPlmn, ratAttr.intValue);
}

/* 0x812646D8: check COM modem firmware archive version once Livespace is ready */
void TelGeneralLogicAction::CheckAndNotifyComFirmwareUpdate()
{
	if (m_comFwChecked || !m_livespaceReady || m_bbmcAdapter == NULL)
		return;

	bool versionMatches = true;
	TelephonyResult res = m_bbmcAdapter->CheckComVersion(&versionMatches);
	if (res.IsSuccess()) {
		m_comFwChecked = true;
		if (!versionMatches) {
			TelEventInfo fwEv(TEL_EV_COM_FW_UPDATE_NOTICE, -1, 0);
			NotifyMediatorEvent(fwEv);
		}
	}
}

/* 0x81264488: leave Error state once all error bit flags have been cleared */
void TelGeneralLogicAction::TryExitErrorState()
{
	if (!HasAnyError()) {
		TelEventInfo exitEv(TEL_EV_INTERNAL_EXIT_FROM_ERROR, -1, 0);
		NotifyMediatorEvent(exitEv);
	}
}

/* ------------------------------------------------------------------
 * State entry actions
 * ------------------------------------------------------------------ */

void TelGeneralLogicAction::EnterStateCode(int stateCode)
{
	SetStateCode(stateCode);
	TelBbmcSharedData::SetStaticInt(524327, (SceUInt32)stateCode);
}

void TelGeneralLogicAction::EnterIdleState()
{
	EnterStateCode(TEL_SM_STATE_IDLE);
	CheckAndNotifyComFirmwareUpdate();
}

void TelGeneralLogicAction::EnterErrorState()
{
	EnterStateCode(TEL_SM_STATE_ERROR);
	NotifyConditionError();
}

void TelGeneralLogicAction::EnterFatalState()
{
	EnterStateCode(TEL_SM_STATE_FATAL);
	NotifyConditionError();
}

/* ------------------------------------------------------------------
 * BBMC event handlers (0x81263540..0x812646D7)
 * ------------------------------------------------------------------ */

void TelGeneralLogicAction::HandleBbmcComBootOk()
{
	m_comFailureCounter.Reset();
	ClearErrorFlag(TEL_ERR_BIT_COM_FAILURE | TEL_ERR_BIT_COM_BOOT_TEMP |
		       TEL_ERR_BIT_COM_BOOT_OTHER);
	ApplyRssiThresholds();
}

/* 0x81263924 */
void TelGeneralLogicAction::HandleBbmcComBootNg()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	int errCode = ev.GetEventResult();

	if (errCode == kBbmcBootTempLow || errCode == kBbmcBootTempHigh ||
	    errCode == kComBootTempMinErr || errCode == kComBootTempMaxErr) {
		SetErrorFlag(TEL_ERR_BIT_COM_BOOT_TEMP);
	} else {
		SetErrorFlag(TEL_ERR_BIT_COM_BOOT_OTHER);
	}
	DispatchToCallbacks(ev);
}

/* 0x812635E4 */
void TelGeneralLogicAction::HandleBbmcPinStatusReady()
{
	m_pinReady = true;
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcPinNotRequired()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcPinRequired()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcPukRequired()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcPinPermanentlyBlocked()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x81262508 / SetupReady */
void TelGeneralLogicAction::HandleBbmcSetupReady()
{
	m_setupReady = true;
	ClearErrorFlag(TEL_ERR_BIT_SIM_DISABLED | TEL_ERR_BIT_SIM_UNUSABLE |
		       TEL_ERR_BIT_SIM_BLOCKED);

	LoadImsiAndProfiles();
	ApplyInitialApnSettings();
	ApplyDataRoamingControl();
	ApplyNetworkSelection();

	if (m_lpmManager != NULL)
		m_lpmManager->ApplyCurrentAttributes();

	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcNetworkReady()
{
	m_networkReady = true;
	ApplyDataRoamingControl();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcLpmFromNetworkReady()
{
	m_networkReady = false;
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcComDownAtSuspend()
{
	m_pinReady = false;
	m_setupReady = false;
	m_networkReady = false;
}

/* 0x81263768 */
void TelGeneralLogicAction::HandleBbmcComFailure()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);

	SetErrorFlag(TEL_ERR_BIT_COM_FAILURE);
	m_comFailureCounter.RecordFailure(ev.GetEventResult());

	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->TurnOff(true);

	DispatchToCallbacks(ev);

	if (m_comFailureCounter.IsThresholdExceeded()) {
		TelEventInfo fatalEv(TEL_EV_MEDIATOR_COM_FAILURE, -1, ev.GetEventResult());
		NotifyMediatorEvent(fatalEv);
	} else {
		TelEventInfo retryEv(TEL_EV_INTERNAL_RE_TURN_ON, -1, 0);
		NotifyMediatorEvent(retryEv);
	}
}

void TelGeneralLogicAction::HandleBbmcStatusUpdate()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcNetinfo()
{
	ApplyDataRoamingControl();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x81263A2C */
void TelGeneralLogicAction::HandleBbmcSimEnabled()
{
	m_simInserted = true;
	ClearErrorFlag(TEL_ERR_BIT_SIM_DISABLED | TEL_ERR_BIT_SIM_UNUSABLE);
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
	TryExitErrorState();
}

/* 0x81263C8C */
void TelGeneralLogicAction::HandleBbmcSimDisabled()
{
	m_simInserted = false;
	SetErrorFlag(TEL_ERR_BIT_SIM_DISABLED);
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcSimBlockedNormalOperation()
{
	SetErrorFlag(TEL_ERR_BIT_SIM_BLOCKED);
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcSimPermanentlyBlocked()
{
	SetErrorFlag(TEL_ERR_BIT_SIM_BLOCKED);
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcSimLockInfo()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x81262508 */
void TelGeneralLogicAction::HandleBbmcPlmnModeBit()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x81264202 */
void TelGeneralLogicAction::HandleBbmcEnterLowBatt()
{
	SetErrorFlag(TEL_ERR_BIT_LOW_BATTERY);
	ForceLpmMode();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x812642EC */
void TelGeneralLogicAction::HandleBbmcExitLowBatt()
{
	ClearErrorFlag(TEL_ERR_BIT_LOW_BATTERY);
	if (m_lpmManager != NULL)
		m_lpmManager->ApplyCurrentAttributes();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
	TryExitErrorState();
}

void TelGeneralLogicAction::HandleBbmcExceedHiTemp()
{
	SetErrorFlag(TEL_ERR_BIT_HIGH_TEMP);
	ForceLpmMode();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcFallDownLowTemp()
{
	ClearErrorFlag(TEL_ERR_BIT_HIGH_TEMP);
	if (m_lpmManager != NULL)
		m_lpmManager->ApplyCurrentAttributes();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
	TryExitErrorState();
}

void TelGeneralLogicAction::HandleBbmcRegistrationReject()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcNeedUpdatingDeviceInfo()
{
	LoadImsiAndProfiles();
}

/* 0x81263FCC */
void TelGeneralLogicAction::HandleBbmcNetScanFin()
{
	if (m_bbmcAdapter != NULL) {
		m_scanInfoList.GetEntries().clear();
		m_bbmcAdapter->GetNetworkScanResult(&m_scanInfoList);
	}
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcNetScanFailed()
{
	m_scanInfoList.GetEntries().clear();
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

void TelGeneralLogicAction::HandleBbmcPacketCallEndReasonUpdate()
{
	if (m_bbmcAdapter == NULL)
		return;
	TelCallEndReasonInfo callEndReason;
	if (!m_bbmcAdapter->GetCallEndReasonInfo(&callEndReason).IsSuccess())
		return;

	int endType = 0;
	int reasonCode = 0;
	callEndReason.GetFirstReason(&endType, &reasonCode);

	for (SceSize idx = 0; idx < m_operatorProfiles.size(); ++idx) {
		const paf::vector<TelCallEndReasonInfo> &filters =
			m_operatorProfiles[idx].GetCallEndReasons();
		for (SceSize filterIdx = 0; filterIdx < filters.size(); ++filterIdx) {
			if (filters[filterIdx].Matches(endType, reasonCode)) {
				if (m_netCtl != NULL) {
					int activationState = 1;
					m_netCtl->SetSimActivationState(&activationState);
				}
				break;
			}
		}
	}

	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* 0x81263BE0 */
void TelGeneralLogicAction::HandleBbmcForwardEvent()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	DispatchToCallbacks(ev);
}

/* ------------------------------------------------------------------
 * Mediator internal event handlers (0x81264588..0x81266B5B)
 * ------------------------------------------------------------------ */

void TelGeneralLogicAction::HandleAttributeError()
{
	SetErrorFlag(TEL_ERR_BIT_ATTRIBUTE);
}

void TelGeneralLogicAction::HandleMediatorComFailure()
{
	SetErrorFlag(TEL_ERR_BIT_COM_FAILURE);
	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->TurnOff(true);
}

void TelGeneralLogicAction::HandleMediatorInternalShutdown()
{
	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->TurnOff(true);
}

/* 0x81264610 */
void TelGeneralLogicAction::HandleMediatorInternalSimLockError()
{
	SetErrorFlag(TEL_ERR_BIT_SIM_BLOCKED);
}

void TelGeneralLogicAction::HandleMediatorInternalBbmcError()
{
	SetErrorFlag(TEL_ERR_BIT_BBMC);
}

void TelGeneralLogicAction::HandleMediatorInternalCanNotUseSim()
{
	SetErrorFlag(TEL_ERR_BIT_SIM_UNUSABLE);
}

void TelGeneralLogicAction::HandleMediatorInternalExitFromErrorState()
{
	NotifyConditionError();
}

/* 0x812618B6: re-turn-on after recoverable COM failure */
void TelGeneralLogicAction::HandleMediatorInternalReTurnOn()
{
	ClearErrorFlag(TEL_ERR_BIT_COM_FAILURE);
	ActionTurnOn();
}

/* 0x81264770: resume from suspend */
void TelGeneralLogicAction::HandleMediatorInternalResume()
{
	if (m_bbmcAdapter != NULL)
		m_bbmcAdapter->ClearSuspendControlBit();
	if (m_lpmManager != NULL)
		m_lpmManager->ApplyCurrentAttributes();
}

/* 0x81266650: enable/disable WWAN attribute + LPM update */
void TelGeneralLogicAction::HandleMediatorInternalEnableWwan()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	bool enableWwan = (ev.GetEventResult() != 0);

	TelephonyResult res(TEL_OK);
	if (m_lpmManager != NULL)
		res = m_lpmManager->ApplyWwanEnableChange(enableWwan);

	if (res.IsSuccess()) {
		TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
		if (attrMgr != NULL) {
			TelAttributeValue attr(TEL_ATTR_ENABLE_WWAN);
			attr.SetInt(enableWwan ? 1 : 0);
			res = attrMgr->SetAttribute(attr);
		}
	}

	TelEventInfo doneEv(res.IsSuccess() ? 1054 : 1055, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81263540 */
void TelGeneralLogicAction::HandleMediatorInternalLivespaceReady()
{
	m_livespaceReady = true;
	if (GetStateCode() == TEL_SM_STATE_IDLE)
		CheckAndNotifyComFirmwareUpdate();
}

/* 0x81264AD2 */
void TelGeneralLogicAction::HandleMediatorInternalDeactivateTelReg()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	if (ev.GetEventArg() == 2) {
		SceNetInternal_7493FB0C(0, 0);
		if (m_lpmManager != NULL)
			m_lpmManager->SetLowPowerModeImmediate(true);
		HandleMediatorInternalResume();
		if (m_lpmManager != NULL)
			m_lpmManager->ApplyCurrentAttributes();
	}
}

void TelGeneralLogicAction::HandleMediatorInternalStartNetworkScan()
{
	if (m_bbmcAdapter == NULL)
		return;
	TelephonyResult res = m_bbmcAdapter->StartNetworkScan();
	if (!res.IsSuccess()) {
		TelEventInfo failEv(TEL_EV_BBMC_NET_SCAN_FAILED, -1, res.GetCode());
		DispatchToCallbacks(failEv);
	}
}

/* 0x81264B74: async worker for VerifyPin (API 43) */
void TelGeneralLogicAction::HandleMediatorInternalVerifyPin()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->VerifyPin(internalVerifyPin);
	SetApiResult(TEL_API_INTERNAL_VERIFY_PIN, res);

	TelEventInfo doneEv(res.IsSuccess() ? 1041 : 1039, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81264D1A: async worker for UnblockPin (API 44) */
void TelGeneralLogicAction::HandleMediatorInternalUnblockPin()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL) {
		res = m_bbmcAdapter->UnblockPin(internalUnblockPin.GetPuk(),
						internalUnblockPin.GetNewPin());
	}
	SetApiResult(TEL_API_INTERNAL_UNBLOCK_PIN, res);

	TelEventInfo doneEv(1040, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81264E3C: async worker for SetPinProtection (API 45) */
void TelGeneralLogicAction::HandleMediatorInternalSetPinProtection()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->SetPinEnabled(&internalProtectionEnable, internalProtectionPin);
	SetApiResult(TEL_API_INTERNAL_SET_PIN_PROTECTION, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_PIN_PROTECTION_FIN
					    : TEL_EV_BBMC_PIN_PROTECTION_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81264F32: async worker for ChangePinCode (API 46) */
void TelGeneralLogicAction::HandleMediatorInternalChangePinCode()
{
	TelephonyResult res(TEL_ERR_INVALID_ARG);
	if (m_bbmcAdapter != NULL && internalChangePins.size() >= 2)
		res = m_bbmcAdapter->ChangePin(internalChangePins[0], internalChangePins[1]);
	SetApiResult(TEL_API_INTERNAL_CHANGE_PIN_CODE, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_CHANGE_PIN_FIN
					    : TEL_EV_BBMC_CHANGE_PIN_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x8126508E: async worker for SetApnInfo (API 47) */
void TelGeneralLogicAction::HandleMediatorInternalSetApnInfo()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL) {
		res = m_bbmcAdapter->SetApnCredentials(internalApnCredentials);
		if (res.IsSuccess() && internalApnSaveFlag)
			res = SaveApnToRegistry(internalApnCredentials, 0, false);
	}
	SetApiResult(TEL_API_INTERNAL_SET_APN_INFO, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_PROFILE_FIN
					    : TEL_EV_BBMC_NET_PROFILE_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x8126562C: async worker for SetApnInfoWithBillingPlan (API 48) */
void TelGeneralLogicAction::HandleMediatorInternalSetApnInfoWithBillingPlan()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL) {
		TelephonyOperator credentials;
		internalBillingApn.GetApnCredentials(&credentials);
		res = m_bbmcAdapter->SetApnCredentials(credentials);
		if (res.IsSuccess() && internalBillingSaveFlag) {
			res = SaveApnToRegistry(credentials,
						internalBillingApn.GetBillingPlan(), true);
		}
	}
	SetApiResult(TEL_API_INTERNAL_SET_APN_INFO_BILLING, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_PROFILE_FIN
					    : TEL_EV_BBMC_NET_PROFILE_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81265D04 / 0x81265DC0: async worker for RestoreApnInfo (API 49) */
void TelGeneralLogicAction::HandleMediatorInternalRestoreApnInfo()
{
	TelephonyResult res(TEL_ERR_NOT_FOUND);
	const paf::vector<TelOperatorApnInfo> &entries = m_apnInfoList.GetEntries();
	for (SceSize idx = 0; idx < entries.size(); ++idx) {
		if (entries[idx].IsAutoSelectable()) {
			TelephonyOperator credentials;
			entries[idx].GetApnCredentials(&credentials);
			if (m_bbmcAdapter != NULL)
				res = m_bbmcAdapter->SetApnCredentials(credentials);
			if (res.IsSuccess())
				res = SaveApnToRegistry(credentials, entries[idx].GetBillingPlan(), true);
			break;
		}
	}
	SetApiResult(TEL_API_INTERNAL_RESTORE_APN_INFO, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_PROFILE_FIN
					    : TEL_EV_BBMC_NET_PROFILE_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81265DC0: async worker for RemoveCurrentApnInfo (API 50) */
void TelGeneralLogicAction::HandleMediatorInternalRemoveCurrentApnInfo()
{
	TelephonyOperator emptyApn;
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->SetApnCredentials(emptyApn);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (res.IsSuccess() && attrMgr != NULL) {
		TelAttributeValue emptyApnAttr(TEL_ATTR_APN_NAME);
		emptyApnAttr.SetString("");
		attrMgr->SetAttribute(emptyApnAttr);

		TelAttributeValue emptyUserAttr(TEL_ATTR_APN_USER_ID);
		emptyUserAttr.SetString("");
		attrMgr->SetAttribute(emptyUserAttr);

		TelAttributeValue emptyPassAttr(TEL_ATTR_APN_PASSWORD);
		emptyPassAttr.SetString("");
		attrMgr->SetAttribute(emptyPassAttr);

		TelAttributeValue initAttr(TEL_ATTR_IS_APN_INIT);
		initAttr.SetInt(0);
		attrMgr->SetAttribute(initAttr);
	}
	SetApiResult(TEL_API_INTERNAL_REMOVE_CURRENT_APN_INFO, res);

	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_PROFILE_FIN
					    : TEL_EV_BBMC_NET_PROFILE_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x8126692C: async worker for SetDataRoamingPermission */
void TelGeneralLogicAction::HandleMediatorInternalSetDataRoamingPermission()
{
	TelEventInfo ev(0, -1, -1);
	GetEvent(&ev);
	bool allowRoaming = (ev.GetEventResult() != 0);

	TelephonyResult res(TEL_OK);
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL) {
		TelAttributeValue roamAttr(TEL_ATTR_ENABLE_DATA_ROAMING);
		roamAttr.SetInt(allowRoaming ? 1 : 0);
		res = attrMgr->SetAttribute(roamAttr);
	}
	if (res.IsSuccess())
		res = ApplyDataRoamingControl();

	TelEventInfo doneEv(res.IsSuccess() ? 1090 : 1091, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x812661E6: async worker for DeactivateSimLock (API 51) */
void TelGeneralLogicAction::HandleMediatorInternalDeactivateSimLock()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL) {
		TelControlKey ckKey(internalDeactivateKey.GetKey());
		res = m_bbmcAdapter->DepersonalizeControlKey(ckKey);
	}
	SetApiResult(TEL_API_INTERNAL_DEACTIVATE_SIM_LOCK, res);

	TelEventInfo doneEv(res.IsSuccess() ? 1079 : 1080, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x8126635E: async worker for UnblockSimLock (API 52) */
void TelGeneralLogicAction::HandleMediatorInternalUnblockSimLock()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->UnblockControlKey(internalUnblockKey);
	SetApiResult(TEL_API_INTERNAL_UNBLOCK_SIM_LOCK, res);

	if (res.IsSuccess()) {
		ClearErrorFlag(TEL_ERR_BIT_SIM_BLOCKED);
		TryExitErrorState();
	}

	TelEventInfo doneEv(res.IsSuccess() ? 1081 : 1082, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* ------------------------------------------------------------------
 * Public API actions called by SMC transitions (0x81266B5C..0x81268D9D)
 * ------------------------------------------------------------------ */

/* 0x812618B6: API 1 TurnOn */
void TelGeneralLogicAction::ActionTurnOn()
{
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL) {
		bool startInLpm = true;
		TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
		if (attrMgr != NULL) {
			TelAttributeValue wwanAttr(TEL_ATTR_ENABLE_WWAN);
			TelAttributeValue flightAttr(TEL_ATTR_FLIGHT_MODE);
			attrMgr->GetAttribute(&wwanAttr);
			attrMgr->GetAttribute(&flightAttr);
			if (wwanAttr.intValue != 0 && flightAttr.intValue == 0)
				startInLpm = false;
		}
		res = m_bbmcAdapter->TurnOn(startInLpm);
	}
	SetApiResult(TEL_API_TURN_ON, res);
}

/* 0x81266B5C: API 2 GetGeneralLogicState */
void TelGeneralLogicAction::ActionGetGeneralLogicState()
{
	m_stateMutex.Lock();
	stateResult = (m_stateCode & 0xFF);
	SetApiResult(TEL_API_GET_STATE, TelephonyResult(TEL_OK));
	m_stateMutex.Unlock();
}

/* 0x81266BB0: API 3 SetFlightMode */
void TelGeneralLogicAction::ActionSetFlightMode()
{
	bool flightOn = (flightModeInput != 0);
	TelephonyResult res(TEL_OK);

	if (m_lpmManager != NULL)
		res = m_lpmManager->ApplyFlightModeChange(flightOn);

	if (res.IsSuccess()) {
		TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
		if (attrMgr != NULL) {
			TelAttributeValue attr(TEL_ATTR_FLIGHT_MODE);
			attr.SetInt(flightOn ? 1 : 0);
			res = attrMgr->SetAttribute(attr);
		}
	}

	SetApiResult(TEL_API_SET_FLIGHT_MODE, res);
	TelEventInfo doneEv(res.IsSuccess() ? 1052 : 1053, -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81266D14: API 4 GetFlightMode */
void TelGeneralLogicAction::ActionGetFlightMode()
{
	SetApiResult(TEL_API_GET_FLIGHT_MODE, TelephonyResult(TEL_OK));
}

/* 0x81266D50: API 5 GetPhoneNumber */
void TelGeneralLogicAction::ActionGetPhoneNumber()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetPhoneNumber(&phoneNumber);
	SetApiResult(TEL_API_GET_PHONE_NUMBER, res);
}

/* 0x81266E20: API 6 GetIccid */
void TelGeneralLogicAction::ActionGetIccid()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetIccid(&iccid);
	SetApiResult(TEL_API_GET_ICCID, res);
}

/* 0x81266EB6: API 7 VerifyPin */
void TelGeneralLogicAction::ActionVerifyPin()
{
	internalVerifyPin = verifyPinCode;
	SetApiResult(TEL_API_VERIFY_PIN, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_VERIFY_PIN, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81266F1E: API 8 UnblockPin */
void TelGeneralLogicAction::ActionUnblockPin()
{
	internalUnblockPin = unblockPinInfo;
	SetApiResult(TEL_API_UNBLOCK_PIN, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_UNBLOCK_PIN, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81266FD2: API 9 SetPinProtection */
void TelGeneralLogicAction::ActionSetPinProtection()
{
	internalProtectionEnable = pinProtectionInput;
	internalProtectionPin = protectionPinCode;
	SetApiResult(TEL_API_SET_PIN_PROTECTION, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_SET_PIN_PROTECTION, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81267044: API 10 GetPinProtection */
void TelGeneralLogicAction::ActionGetPinProtection()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetPinEnabled(&pinProtectionOutput);
	SetApiResult(TEL_API_GET_PIN_PROTECTION, res);
}

/* 0x812670AA: API 11 GetPinStatus */
void TelGeneralLogicAction::ActionGetPinStatus()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetPinStatus(&pinStatusOutput);
	SetApiResult(TEL_API_GET_PIN_STATUS, res);
}

/* 0x8126710C: API 12 GetPinRemain */
void TelGeneralLogicAction::ActionGetPinRemain()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	SceUInt16 retries = 0;
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetPinRetryCount(&retries);
	pinRemainOutput = (int)retries;
	SetApiResult(TEL_API_GET_PIN_REMAIN, res);
}

/* 0x81267172: API 13 ChangePinCode */
void TelGeneralLogicAction::ActionChangePinCode()
{
	internalChangePins.clear();
	internalChangePins.push_back(changeOldPin);
	internalChangePins.push_back(changeNewPin);
	SetApiResult(TEL_API_CHANGE_PIN_CODE, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_CHANGE_PIN_CODE, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81267206: API 14 GetSignalLevel */
void TelGeneralLogicAction::ActionGetSignalLevel()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetSignalLevel(&signalLevel);
	SetApiResult(TEL_API_GET_SIGNAL_LEVEL, res);
}

/* 0x8126727E: API 15 GetNetworkInfo */
void TelGeneralLogicAction::ActionGetNetworkInfo()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetNetworkInfo(&networkInfo);
	SetApiResult(TEL_API_GET_NETWORK_INFO, res);
}

/* 0x8126735E: API 16 GetRoamingInfo */
void TelGeneralLogicAction::ActionGetRoamingInfo()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetRoamingInfo(&roamingInfo);
	SetApiResult(TEL_API_GET_ROAMING_INFO, res);
}

/* 0x8126753C: API 17 GetMccMncOfImsi */
void TelGeneralLogicAction::ActionGetMccMncOfImsi()
{
	imsiPlmn = m_imsiPlmn;
	SetApiResult(TEL_API_GET_MCC_MNC_OF_IMSI,
		     TelephonyResult(imsiPlmn.IsValid() ? (int)TEL_OK : (int)TEL_ERR_NOT_FOUND));
}

/* 0x812673D6: API 18 GetMccMncOfNetwork */
void TelGeneralLogicAction::ActionGetMccMncOfNetwork()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetServingPlmn(&networkPlmn);
	SetApiResult(TEL_API_GET_MCC_MNC_OF_NETWORK, res);
}

/* 0x81267700: API 19 SetApnInfo */
void TelGeneralLogicAction::ActionSetApnInfo()
{
	internalApnCredentials = apnCredentials;
	internalApnSaveFlag = saveApnToRegistry;
	SetApiResult(TEL_API_SET_APN_INFO, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_SET_APN_INFO, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x812677A2: API 20 RestoreApnInfo */
void TelGeneralLogicAction::ActionRestoreApnInfo()
{
	SetApiResult(TEL_API_RESTORE_APN_INFO, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_RESTORE_APN_INFO, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x812677DE: API 21 GetOperatorProfileCount */
void TelGeneralLogicAction::ActionGetOperatorProfileCount()
{
	operatorProfileCount = (int)m_operatorProfiles.size();
	SetApiResult(TEL_API_GET_OPERATOR_PROFILE_COUNT, TelephonyResult(TEL_OK));
}

/* 0x812678B2: API 22 GetOperatorProfile */
void TelGeneralLogicAction::ActionGetOperatorProfile()
{
	if (operatorProfileIndex < 0 ||
	    (SceSize)operatorProfileIndex >= m_operatorProfiles.size()) {
		SetApiResult(TEL_API_GET_OPERATOR_PROFILE, TelephonyResult(TEL_ERR_IO_FAILED));
		return;
	}
	operatorProfile = m_operatorProfiles[(SceSize)operatorProfileIndex];
	SetApiResult(TEL_API_GET_OPERATOR_PROFILE, TelephonyResult(TEL_OK));
}

/* 0x81267D08: API 23 GetApnInfoListCount */
void TelGeneralLogicAction::ActionGetApnInfoListCount()
{
	apnListCount = (int)m_apnInfoList.GetEntries().size();
	SetApiResult(TEL_API_GET_APN_INFO_LIST_COUNT, TelephonyResult(TEL_OK));
}

/* 0x81267DC6: API 24 GetApnInfoListItem */
void TelGeneralLogicAction::ActionGetApnInfoListItem()
{
	const paf::vector<TelOperatorApnInfo> &entries = m_apnInfoList.GetEntries();
	if (apnListIndex < 0 || (SceSize)apnListIndex >= entries.size()) {
		SetApiResult(TEL_API_GET_APN_INFO_LIST_ITEM, TelephonyResult(TEL_ERR_IO_FAILED));
		return;
	}
	apnListItem = entries[(SceSize)apnListIndex];
	SetApiResult(TEL_API_GET_APN_INFO_LIST_ITEM, TelephonyResult(TEL_OK));
}

/* 0x81267F10: API 25 RemoveCurrentApnInfo */
void TelGeneralLogicAction::ActionRemoveCurrentApnInfo()
{
	SetApiResult(TEL_API_REMOVE_CURRENT_APN_INFO, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_REMOVE_CURRENT_APN_INFO, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81267F4C: API 26 StartNetworkScan */
void TelGeneralLogicAction::ActionStartNetworkScan()
{
	SetApiResult(TEL_API_START_NETWORK_SCAN, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_START_NETWORK_SCAN, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81267FB0: API 27 AbortNetworkScan */
void TelGeneralLogicAction::ActionAbortNetworkScan()
{
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->AbortNetworkScan();

	TelEventInfo abortEv(TEL_EV_BBMC_NET_SCAN_FAILED, -1, res.GetCode());
	NotifyMediatorEvent(abortEv);
	SetApiResult(TEL_API_ABORT_NETWORK_SCAN, res);
}

/* 0x81268040: API 28 GetNetworkScanInfoCount */
void TelGeneralLogicAction::ActionGetNetworkScanInfoCount()
{
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL && m_scanInfoList.GetEntries().empty())
		res = m_bbmcAdapter->GetNetworkScanResult(&m_scanInfoList);
	networkScanCount = (int)m_scanInfoList.GetEntries().size();
	SetApiResult(TEL_API_GET_NETWORK_SCAN_INFO_COUNT, res);
}

/* 0x812680EE: API 29 GetNetworkScanInfo */
void TelGeneralLogicAction::ActionGetNetworkScanInfo()
{
	const paf::vector<TelNetScanInfo> &entries = m_scanInfoList.GetEntries();
	if (networkScanIndex < 0 || (SceSize)networkScanIndex >= entries.size()) {
		SetApiResult(TEL_API_GET_NETWORK_SCAN_INFO, TelephonyResult(TEL_ERR_IO_FAILED));
		return;
	}
	networkScanItem = entries[(SceSize)networkScanIndex];
	SetApiResult(TEL_API_GET_NETWORK_SCAN_INFO, TelephonyResult(TEL_OK));
}

/* 0x8126824E */
TelephonyResult TelGeneralLogicAction::RegisterNetworkAutoAndSave()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->RegisterNetworkAuto();

	if (res.IsSuccess()) {
		TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
		if (attrMgr != NULL) {
			TelAttributeValue modeAttr(TEL_ATTR_NETWORK_SELECTION);
			modeAttr.SetInt(1);
			res = attrMgr->SetAttribute(modeAttr);
		}
	}
	return res;
}

/* 0x81268350: API 30 SetNetRegisterAuto */
void TelGeneralLogicAction::ActionSetNetRegisterAuto()
{
	TelephonyResult res = RegisterNetworkAutoAndSave();

	SetApiResult(TEL_API_SET_NET_REGISTER_AUTO, res);
	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_REGISTER_FIN
					    : TEL_EV_BBMC_NET_REGISTER_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x8126840C: API 31 SetNetRegisterInfo */
void TelGeneralLogicAction::ActionSetNetRegisterInfo()
{
	TelephonyPlmn targetPlmn;
	manualRegisterTarget.GetPlmn(&targetPlmn);
	int radioCapability = manualRegisterTarget.GetRadioCapability();

	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->RegisterNetworkManual(targetPlmn, radioCapability);

	if (res.IsSuccess()) {
		TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
		if (attrMgr != NULL) {
			TelAttributeValue modeAttr(TEL_ATTR_NETWORK_SELECTION);
			modeAttr.SetInt(1);
			attrMgr->SetAttribute(modeAttr);

			TelAttributeValue mccAttr(TEL_ATTR_MCC);
			mccAttr.SetInt(targetPlmn.GetMcc());
			attrMgr->SetAttribute(mccAttr);

			TelAttributeValue mncAttr(TEL_ATTR_MNC);
			mncAttr.SetInt(targetPlmn.GetMnc());
			attrMgr->SetAttribute(mncAttr);

			TelAttributeValue ratAttr(TEL_ATTR_RAT);
			ratAttr.SetInt(radioCapability);
			attrMgr->SetAttribute(ratAttr);
		}
	}

	SetApiResult(TEL_API_SET_NET_REGISTER_INFO, res);
	TelEventInfo doneEv(res.IsSuccess() ? TEL_EV_BBMC_NET_REGISTER_FIN
					    : TEL_EV_BBMC_NET_REGISTER_FAILED,
			    -1, res.GetCode());
	DispatchToCallbacks(doneEv);
}

/* 0x81268868: API 32 GetNetRegistrationInfo */
void TelGeneralLogicAction::ActionGetNetRegistrationInfo()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetNetworkInfo(&netRegistrationInfo);
	SetApiResult(TEL_API_GET_NET_REGISTRATION_INFO, res);
}

/* 0x81268948: API 34 SetDataRoamingPermission */
void TelGeneralLogicAction::ActionSetDataRoamingPermission()
{
	SetApiResult(TEL_API_SET_DATA_ROAMING_PERMISSION, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_SET_DATA_ROAMING_PERM, -1,
			dataRoamingPermission ? 1 : 0);
	NotifyMediatorEvent(ev);
}

/* 0x81268984: API 35 SetDataRoamingDefinition */
void TelGeneralLogicAction::ActionSetDataRoamingDefinition()
{
	m_dataRoamingDef = dataRoamingDefinition;
	TelephonyResult res(TEL_OK);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->SetPacketControlBit(dataRoamingDefinition != 0);
	SetApiResult(TEL_API_SET_DATA_ROAMING_DEFINITION, res);
}

/* 0x812689E8: API 36 CheckSimCondition */
void TelGeneralLogicAction::ActionCheckSimCondition()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->CheckSimCondition();
	SetApiResult(TEL_API_CHECK_SIM_CONDITION, res);
}

/* 0x81268A44: API 37 IsOpenSimModel */
void TelGeneralLogicAction::ActionIsOpenSimModel()
{
	openSimModel = m_supportedPlmns.empty();
	SetApiResult(TEL_API_IS_OPEN_SIM_MODEL, TelephonyResult(TEL_OK));
}

/* 0x81268AA8: API 38 GetConditionError */
void TelGeneralLogicAction::ActionGetConditionError()
{
	conditionError = GetHighestPriorityError();
	SetApiResult(TEL_API_GET_CONDITION_ERROR, TelephonyResult(TEL_OK));
}

/* 0x81268B40: API 39 DeactivateSimLock */
void TelGeneralLogicAction::ActionDeactivateSimLock()
{
	internalDeactivateKey = deactivateSimLockKey;
	SetApiResult(TEL_API_DEACTIVATE_SIM_LOCK, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_DEACTIVATE_SIM_LOCK, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81268B94: API 40 UnblockSimLock */
void TelGeneralLogicAction::ActionUnblockSimLock()
{
	internalUnblockKey = unblockSimLockKey;
	SetApiResult(TEL_API_UNBLOCK_SIM_LOCK, TelephonyResult(TEL_OK));
	TelEventInfo ev(TEL_EV_INTERNAL_UNBLOCK_SIM_LOCK, -1, 0);
	NotifyMediatorEvent(ev);
}

/* 0x81268BE8: API 41 GetSimLockDeactivateRemain */
void TelGeneralLogicAction::ActionGetSimLockDeactivateRemain()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetControlKeyVerifyRetries(&simLockDeactivateRemain);
	SetApiResult(TEL_API_GET_SIM_LOCK_DEACTIVATE_REMAIN, res);
}

/* 0x81268C4C: API 42 GetSimLockUnblockRemain */
void TelGeneralLogicAction::ActionGetSimLockUnblockRemain()
{
	TelephonyResult res(TEL_ERR_NOT_INITIALIZED);
	if (m_bbmcAdapter != NULL)
		res = m_bbmcAdapter->GetControlKeyUnblockRetries(&simLockUnblockRemain);
	SetApiResult(TEL_API_GET_SIM_LOCK_UNBLOCK_REMAIN, res);
}

/* 0x81268CB0: API 54 CheckImsiForOperatorCode */
void TelGeneralLogicAction::ActionCheckImsiForOperatorCode()
{
	TelOperatorProfileFactory factory(checkOperatorCode, m_imsiPlmn, 0);
	paf::vector<TelOperatorProfile> profiles;
	TelephonyResult res = factory.LoadProfiles(&profiles);
	if (res.IsSuccess() && !profiles.empty())
		checkOperatorMatched = true;
	SetApiResult(TEL_API_CHECK_IMSI_FOR_OPERATOR_CODE, res);
}

} /* namespace telephony */
