/*
 * tel_general_logic_mediator2.cpp: general telephony mediator event dispatch,
 * general info listener broadcast, power/app/livespace callbacks, and
 * internal async API handlers (3.65-CEX: 0x81255F12..0x812561BD,
 * 0x8125A9B4..0x8125AAB3, 0x8125C6EC..0x8125E9B5).
 */
#include <std/string.h>
#include <scetypes.h>
#include "shell.h"
#include "tel_general_internal.h"

extern "C" {
int sceAppMgrGetStatusByAppId(SceUID appId, void *status);
}

namespace telephony {

/* Layout of sceAppMgrGetStatusByAppId output (0x80 bytes) */
struct TelAppStatusQuery {
	SceUInt32 header[3];
	char titleId[116];
};

static const int kPowerCbResumeComplete = 0x40000;

/* ------------------------------------------------------------------
 * Network report line reader (0x8125A9B4)
 * ------------------------------------------------------------------ */

TelephonyResult TelGeneralLogicMediator::ReadNetworkReportLine(paf::string *outLine)
{
	*outLine = "";
	if (m_networkReportCache.empty() && m_networkReportOffset == 0) {
		paf::string netReport;
		paf::string devReport;
		if (m_bbmcAdapter != NULL) {
			m_bbmcAdapter->GetNetworkStatusReport(&netReport);
			m_bbmcAdapter->GetDeviceInfoReport(&devReport);
		}
		m_networkReportCache = netReport;
		if (!devReport.empty()) {
			m_networkReportCache.append("\n");
			m_networkReportCache.append(devReport.c_str());
		}
	}

	if (m_networkReportOffset >= m_networkReportCache.size()) {
		m_networkReportCache = "";
		m_networkReportOffset = 0;
		return TelephonyResult(TEL_OK);
	}

	const char *base = m_networkReportCache.c_str();
	SceSize totalLen = m_networkReportCache.size();
	SceSize start = m_networkReportOffset;
	SceSize pos = start;
	while (pos < totalLen && base[pos] != '\n')
		++pos;

	outLine->assign(base + start, pos - start);
	m_networkReportOffset = (pos < totalLen) ? (pos + 1) : totalLen;
	return TelephonyResult(TEL_OK);
}

/* ------------------------------------------------------------------
 * General Info snapshot & listener broadcast (0x81255F12, 0x8125D35E)
 * ------------------------------------------------------------------ */

/* 0x81255F12 */
void TelGeneralLogicMediator::BroadcastGeneralInfo()
{
	TelephonyGeneralInfo genInfo;
	genInfo.networkType = 0;
	genInfo.registration = 0;
	genInfo.operatorNames.name = L"";
	genInfo.operatorNames.shortName = L"";
	genInfo.signalLevel = 0;
	genInfo.roaming = 0;
	genInfo.simCondition = m_cachedSimCondition;
	genInfo.flightMode = m_cachedFlightMode;
	genInfo.heatCondition = m_action.HasErrorFlag(TEL_ERR_BIT_HIGH_TEMP) ? 1 : 0;

	int bearerMapped = 0;
	m_cachedBearerInfo.GetMappedBearerType(&bearerMapped);
	if (bearerMapped != 0)
		genInfo.networkType = bearerMapped;
	else
		m_cachedNetworkInfo.GetMappedNetworkType(&genInfo.networkType);

	m_cachedNetworkInfo.GetSignalLevel(&genInfo.signalLevel);

	TelephonyPlmn servingPlmn;
	m_cachedNetworkInfo.GetPlmn(&servingPlmn);
	TelephonyResult nameRes = TelNetworkCodeUtil::LookupOperatorNames(
		servingPlmn, &genInfo.operatorNames.name, &genInfo.operatorNames.shortName);
	if (!nameRes.IsSuccess()) {
		genInfo.operatorNames.name = L"";
		genInfo.operatorNames.shortName = L"";
	}

	m_cachedNetworkInfo.GetMappedRoamingState(&genInfo.roaming);
	if (m_netCtl != NULL) {
		bool domesticRoaming = false;
		m_netCtl->IsDomesticRoamingAllowed(&domesticRoaming);
		if (domesticRoaming)
			genInfo.roaming = 1;
	}

	/* Snapshot listeners under lock to avoid deadlock on re-entrant removal */
	m_listenerMutex.Lock();
	paf::vector<TelGeneralListenerRegistration> snapshot = m_generalListeners;
	m_listenerMutex.Unlock();

	for (SceSize idx = 0; idx < snapshot.size(); ++idx) {
		if (snapshot[idx].callback != NULL) {
			snapshot[idx].callback(TEL_EV_LISTENER_GENERAL_INFO, 0, &genInfo,
					       (int)sizeof(genInfo));
		}
	}

	TelEventInfo netStateEv(TEL_EV_NOTIFY_NET_STATE_UPDATE, -1, genInfo.registration);
	m_action.NotifyMediatorEvent(netStateEv);
}

/* 0x8125D35E */
void TelGeneralLogicMediator::RefreshNetworkAndBearerCache()
{
	if (m_bbmcAdapter == NULL)
		return;

	m_bbmcAdapter->GetNetworkInfo(&m_cachedNetworkInfo);
	m_bbmcAdapter->GetPacketBearerInfo(&m_cachedBearerInfo);

	bool flightOn = false;
	if (GetFlightMode(&flightOn).IsSuccess())
		m_cachedFlightMode = flightOn ? 1 : 0;

	if (m_cachedSimCondition != 3 && m_cachedSimCondition != 4) {
		int pinStatus = 0;
		if (m_bbmcAdapter->GetPinStatus(&pinStatus).IsSuccess()) {
			switch (pinStatus) {
			case 0:
			case 4:
				m_cachedSimCondition = 1;
				break;
			case 1:
			case 2:
			case 3:
				m_cachedSimCondition = 5;
				break;
			default:
				break;
			}
		}
	}

	BroadcastGeneralInfo();
}

/* ------------------------------------------------------------------
 * Shell, Power, Livespace, and App callbacks (0x8125D3E2..0x8125D735)
 * ------------------------------------------------------------------ */

/* 0x8125D3E2 */
void TelGeneralLogicMediator::AppActivatedCallback(int code, const void *eventData,
						   SceSize size, void *userData)
{
	(void)code;
	(void)size;
	TelGeneralLogicMediator *self = static_cast<TelGeneralLogicMediator *>(userData);
	if (self == NULL || eventData == NULL)
		return;

	const ShellAppEvent *appEv = static_cast<const ShellAppEvent *>(eventData);
	int appId = appEv->appId;

	TelAppStatusQuery status;
	sce_paf_memset(&status, 0, sizeof(status));
	sceAppMgrGetStatusByAppId(appId, &status);

	const char *opTitleId = NULL;
	if (self->m_netCtl != NULL)
		self->m_netCtl->GetOperatorAppTitleId(&opTitleId);

	if (opTitleId != NULL && sce_paf_strcmp(status.titleId, opTitleId) == 0) {
		self->m_operatorAppId = appId;
		self->m_operatorAppLaunched = true;
		TelBbmcSharedData::SetStaticInt(524330, 1);
	} else if (sce_paf_strcmp(status.titleId, "NPXS10062") == 0) {
		self->m_telRegAppId = appId;
		TelBbmcSharedData::SetStaticInt(524331, 1);
	}
}

/* 0x8125D480 */
void TelGeneralLogicMediator::HandleAppDeactivated(int code, int appId)
{
	(void)code;
	if (m_operatorAppId == appId)
		TelBbmcSharedData::SetStaticInt(524330, 2);
	if (m_telRegAppId == appId)
		TelBbmcSharedData::SetStaticInt(524331, 2);
}

/* 0x8125D5BE */
void TelGeneralLogicMediator::AppDeactivatedCallback(int code, const void *eventData,
						     SceSize size, void *userData)
{
	(void)size;
	TelGeneralLogicMediator *self = static_cast<TelGeneralLogicMediator *>(userData);
	if (self == NULL || eventData == NULL)
		return;

	const ShellAppEvent *appEv = static_cast<const ShellAppEvent *>(eventData);
	self->HandleAppDeactivated(code, appEv->appId);
}

/* 0x8125D5DE */
void TelGeneralLogicMediator::AppEndedCallback(int code, const void *eventData,
					       SceSize size, void *userData)
{
	(void)code;
	(void)size;
	TelGeneralLogicMediator *self = static_cast<TelGeneralLogicMediator *>(userData);
	if (self == NULL || eventData == NULL)
		return;

	const ShellAppEvent *appEv = static_cast<const ShellAppEvent *>(eventData);
	int appId = appEv->appId;

	if (self->m_operatorAppId == appId) {
		self->m_operatorAppId = -1;
		TelBbmcSharedData::SetStaticInt(524330, 3);
	}
	if (self->m_telRegAppId == appId) {
		TelEventInfo deactEv(TEL_EV_INTERNAL_DEACTIVATE_TEL_REG, -1,
				     self->m_debugForceFlag);
		self->PostMessage(deactEv);
		self->m_debugForceFlag = 0;
		self->m_telRegAppId = -1;
		TelBbmcSharedData::SetStaticInt(524331, 3);
	}
}

/* 0x8125D680 */
int TelGeneralLogicMediator::PowerCallback(int notifyId, int notifyCount, int powerInfo,
					   void *common)
{
	(void)notifyId;
	(void)notifyCount;
	TelGeneralLogicMediator *self = static_cast<TelGeneralLogicMediator *>(common);
	if ((powerInfo & kPowerCbResumeComplete) != 0 && self != NULL) {
		TelEventInfo resumeEv(TEL_EV_INTERNAL_RESUME, -1, -1);
		self->PostMessage(resumeEv);
	}
	return 0;
}

/* 0x8125D6DE */
void TelGeneralLogicMediator::LivespaceCallback(int code, const void *eventData,
						SceSize size, void *userData)
{
	(void)code;
	(void)eventData;
	(void)size;
	TelGeneralLogicMediator *self = static_cast<TelGeneralLogicMediator *>(userData);
	if (self != NULL) {
		TelEventInfo readyEv(TEL_EV_INTERNAL_LIVESPACE_READY, -1, -1);
		self->PostMessage(readyEv);
	}
}

/* ------------------------------------------------------------------
 * Observer notification and message thread dispatch (0x8125C6EC..0x8125E9B5)
 * ------------------------------------------------------------------ */

void TelGeneralLogicMediator::OnNotify(const TelEventInfo &event)
{
	PostMessage(event);
}

void TelGeneralLogicMediator::OnHandleEvent(const TelEventInfo &event)
{
	DispatchMediatorEvent(event);
}

/* 0x8125C6EC..0x8125D35D: async internal API dispatch helpers */
void TelGeneralLogicMediator::HandleInternalAsyncApi(const TelEventInfo &event)
{
	int eventId = event.GetEventId();
	m_smcMutex.Lock();
	m_action.SetEvent(event);

	switch (eventId) {
	case TEL_EV_INTERNAL_VERIFY_PIN:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_VERIFY_PIN);
		m_stateMachine.OnEventMediatorInternalVerifyPin();
		break;
	case TEL_EV_INTERNAL_UNBLOCK_PIN:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_UNBLOCK_PIN);
		m_stateMachine.OnEventMediatorInternalUnblockPin();
		break;
	case TEL_EV_INTERNAL_SET_PIN_PROTECTION:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_PIN_PROTECTION);
		m_stateMachine.OnEventMediatorInternalSetPinProtection();
		break;
	case TEL_EV_INTERNAL_CHANGE_PIN_CODE:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_CHANGE_PIN_CODE);
		m_stateMachine.OnEventMediatorInternalChangePinCode();
		break;
	case TEL_EV_INTERNAL_SET_APN_INFO:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_APN_INFO);
		m_stateMachine.OnEventMediatorInternalSetApnInfo();
		break;
	case TEL_EV_INTERNAL_SET_APN_INFO_BILLING:
		if (event.GetCategory() == 20) {
			const TelSetApnBillingEvent &billingEv =
				static_cast<const TelSetApnBillingEvent &>(event);
			m_action.internalBillingApn = billingEv.GetApnInfo();
			m_action.internalBillingSaveFlag = (event.GetEventResult() != 0);
		}
		m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_APN_INFO_BILLING);
		m_stateMachine.OnEventMediatorInternalSetApnInfoWithBillingPlan();
		break;
	case TEL_EV_INTERNAL_RESTORE_APN_INFO:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_RESTORE_APN_INFO);
		m_stateMachine.OnEventMediatorInternalRestoreApnInfo();
		break;
	case TEL_EV_INTERNAL_REMOVE_CURRENT_APN_INFO:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_REMOVE_CURRENT_APN_INFO);
		m_stateMachine.OnEventMediatorInternalRemoveCurrentApnInfo();
		break;
	case TEL_EV_INTERNAL_DEACTIVATE_SIM_LOCK:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_DEACTIVATE_SIM_LOCK);
		m_stateMachine.OnEventMediatorInternalDeactivateSimLock();
		break;
	case TEL_EV_INTERNAL_UNBLOCK_SIM_LOCK:
		m_action.SetCurrentApiId(TEL_API_INTERNAL_UNBLOCK_SIM_LOCK);
		m_stateMachine.OnEventMediatorInternalUnblockSimLock();
		break;
	default:
		break;
	}

	m_smcMutex.Unlock();
}

/* 0x8125D7E4: main event switch routing BBMC, attribute, and internal events into SMC */
void TelGeneralLogicMediator::DispatchMediatorEvent(const TelEventInfo &event)
{
	int eventId = event.GetEventId();

	/* Internal async API worker events */
	if ((eventId >= TEL_EV_INTERNAL_VERIFY_PIN &&
	     eventId <= TEL_EV_INTERNAL_RESTORE_APN_INFO) ||
	    (eventId >= TEL_EV_INTERNAL_REMOVE_CURRENT_APN_INFO &&
	     eventId <= TEL_EV_INTERNAL_UNBLOCK_SIM_LOCK) ||
	    eventId == TEL_EV_INTERNAL_SET_APN_INFO_BILLING) {
		HandleInternalAsyncApi(event);
		return;
	}

	if (eventId == TEL_EV_INTERNAL_TURN_ON) {
		ExecuteTurnOnInternal();
		return;
	}

	/* GUI notifications forwarded directly to observers (TelGeneralLogicGui) */
	if (eventId == TEL_EV_COM_FW_UPDATE_NOTICE ||
	    eventId == TEL_GUI_EVENT_ERROR_DIALOG ||
	    eventId == TEL_GUI_EVENT_SIM_STATE_RESET) {
		NotifyObservers(event);
		return;
	}

	m_smcMutex.Lock();
	m_action.SetEvent(event);

	switch (eventId) {
	/* BBMC power / readiness lifecycle (517..522) */
	case TEL_EV_BBMC_COM_BOOT_OK:
		m_stateMachine.OnEventBbmcComBootOk();
		break;
	case TEL_EV_BBMC_COM_BOOT_NG:
		m_stateMachine.OnEventBbmcComBootNg();
		break;
	case TEL_EV_BBMC_SETUP_READY:
		m_stateMachine.OnEventBbmcSetupReady();
		break;
	case TEL_EV_BBMC_COM_DOWN_AT_SUSPEND:
		m_stateMachine.OnEventBbmcComDownAtSuspend();
		break;
	case TEL_EV_BBMC_NETWORK_READY:
		m_stateMachine.OnEventBbmcNetworkReady();
		break;
	case TEL_EV_BBMC_LPM_FROM_NETWORK_READY:
		m_stateMachine.OnEventBbmcLpmFromNetworkReady();
		break;

	/* SIM lock, thermal, low-battery, and mediator internal events (772..828) */
	case TEL_EV_SIM_BLOCKED_NORMAL_OP:
		m_cachedSimCondition = 3;
		m_stateMachine.OnEventBbmcSimBlockedNormalOperation();
		break;
	case TEL_EV_SIM_PERMANENTLY_BLOCKED:
		m_cachedSimCondition = 4;
		m_stateMachine.OnEventBbmcSimPermanentlyBlocked();
		break;
	case TEL_EV_EXCEED_HI_TEMP:
		m_stateMachine.OnEventBbmcExceedHiTemp();
		NotifyObservers(event);
		break;
	case TEL_EV_FALL_DOWN_LOW_TEMP:
		m_stateMachine.OnEventBbmcFallDownLowTemp();
		break;
	case TEL_EV_ENTER_LOW_BATT:
		m_stateMachine.OnEventBbmcEnterLowBatt();
		break;
	case TEL_EV_EXIT_LOW_BATT:
		m_stateMachine.OnEventBbmcExitLowBatt();
		break;
	case TEL_EV_REGISTRATION_REJECT:
		m_stateMachine.OnEventBbmcRegistrationReject();
		NotifyObservers(event);
		break;
	case TEL_EV_MEDIATOR_COM_FAILURE:
		m_stateMachine.OnEventMediatorComFailure();
		break;
	case TEL_EV_INTERNAL_DEACTIVATE_TEL_REG:
		m_stateMachine.OnEventMediatorInternalDeactivateTelReg();
		break;
	case TEL_EV_INTERNAL_START_NETWORK_SCAN:
		m_stateMachine.OnEventMediatorInternalStartNetworkScan();
		break;
	case TEL_EV_INTERNAL_EXIT_FROM_ERROR:
		m_stateMachine.OnEventMediatorInternalExitFromErrorState();
		break;
	case TEL_EV_INTERNAL_CAN_NOT_USE_SIM:
		m_stateMachine.OnEventMediatorInternalCanNotUseSim();
		break;
	case TEL_EV_INTERNAL_SHUTDOWN:
		m_stateMachine.OnEventMediatorInternalShutdown();
		break;
	case TEL_EV_INTERNAL_SIM_LOCK_ERROR:
		m_stateMachine.OnEventMediatorInternalSimLockError();
		break;
	case TEL_EV_INTERNAL_RESUME:
		m_stateMachine.OnEventMediatorInternalResume();
		break;
	case TEL_EV_INTERNAL_RE_TURN_ON:
		m_stateMachine.OnEventMediatorInternalReTurnOn();
		break;
	case TEL_EV_INTERNAL_BBMC_ERROR:
		m_stateMachine.OnEventMediatorInternalBbmcError();
		break;
	case TEL_EV_INTERNAL_ENABLE_WWAN:
		m_stateMachine.OnEventMediatorInternalEnableWwan();
		break;
	case TEL_EV_INTERNAL_LIVESPACE_READY:
		m_stateMachine.OnEventMediatorInternalLivespaceReady();
		break;
	case TEL_EV_INTERNAL_SET_DATA_ROAMING_PERM:
		m_stateMachine.OnEventMediatorInternalSetDataRoamingPermission();
		break;

	/* BBMC status, SIM, PIN, profile, scan, and registration events (1025..1086) */
	case TEL_EV_BBMC_COM_FAILURE:
		m_stateMachine.OnEventBbmcComFailure();
		break;
	case TEL_EV_BBMC_STATUS_UPDATE:
		m_stateMachine.OnEventBbmcStatusUpdate();
		break;
	case TEL_EV_BBMC_NETINFO:
		m_stateMachine.OnEventBbmcNetinfo();
		break;
	case TEL_EV_BBMC_SIM_ENABLED:
		m_cachedSimCondition = 1;
		m_stateMachine.OnEventSimEnabled();
		NotifyObservers(event);
		break;
	case TEL_EV_BBMC_SIM_DISABLED:
		m_cachedSimCondition = 0;
		m_stateMachine.OnEventSimDisabled();
		NotifyObservers(event);
		break;
	case TEL_EV_BBMC_PIN_STATUS_READY:
		m_stateMachine.OnEventBbmcPinStatusReady();
		break;
	case TEL_EV_BBMC_PIN_NOT_REQUIRED:
		m_stateMachine.OnEventBbmcPinNotRequired();
		break;
	case TEL_EV_BBMC_PIN_REQUIRED:
		m_stateMachine.OnEventBbmcPinRequired();
		break;
	case TEL_EV_BBMC_PUK_REQUIRED:
		m_stateMachine.OnEventBbmcPukRequired();
		break;
	case TEL_EV_BBMC_PIN_PERM_BLOCKED:
		m_stateMachine.OnEventBbmcPinPermanentlyBlocked();
		break;
	case TEL_EV_BBMC_CHANGE_PIN_FIN:
		m_stateMachine.OnEventBbmcChangePinCodeFin();
		break;
	case TEL_EV_BBMC_CHANGE_PIN_FAILED:
		m_stateMachine.OnEventBbmcChangePinCodeFailed();
		break;
	case TEL_EV_BBMC_PIN_PROTECTION_FIN:
		m_stateMachine.OnEventBbmcPinProtectionFin();
		break;
	case TEL_EV_BBMC_PIN_PROTECTION_FAILED:
		m_stateMachine.OnEventBbmcPinProtectionFailed();
		break;
	case TEL_EV_BBMC_NET_PROFILE_FIN:
		m_stateMachine.OnEventBbmcNetProfileSettingFin();
		break;
	case TEL_EV_BBMC_NET_PROFILE_FAILED:
		m_stateMachine.OnEventBbmcNetProfileSettingFailed();
		break;
	case TEL_EV_BBMC_NET_SCAN_FIN:
		m_scanActive = false;
		m_stateMachine.OnEventBbmcNetScanFin();
		break;
	case TEL_EV_BBMC_NET_SCAN_FAILED:
		m_scanActive = false;
		m_stateMachine.OnEventBbmcNetScanFailed();
		break;
	case TEL_EV_BBMC_NET_REGISTER_FIN:
		m_stateMachine.OnEventBbmcNetRegisterSettingFin();
		break;
	case TEL_EV_BBMC_NET_REGISTER_FAILED:
		m_stateMachine.OnEventBbmcNetRegisterSettingFailed();
		break;
	case TEL_EV_BBMC_NEED_DEVICE_INFO:
		m_stateMachine.OnEventBbmcNeedUpdatingDeviceInfo();
		NotifyObservers(event);
		break;
	case TEL_EV_BBMC_PACKET_CALL_END_REASON:
		m_stateMachine.OnEventBbmcPacketCallEndReasonUpdate();
		break;
	case TEL_EV_BBMC_SIM_LOCK_INFO:
		m_stateMachine.OnEventBbmcSimLockInfo();
		break;
	case TEL_EV_BBMC_PLMN_MODE_BIT:
		m_stateMachine.OnEventBbmcPlmnModeBit();
		break;

	/* Attribute manager error event (1795) */
	case TEL_EVENT_ATTRIBUTE_ERROR:
		m_stateMachine.OnEventAttributeError();
		break;

	default:
		m_stateMachine.OnEventNotifyCallback();
		break;
	}

	m_smcMutex.Unlock();

	/* Refresh cached network/bearer/SIM state and broadcast to shell listeners */
	if (eventId == TEL_EV_BBMC_NETINFO ||
	    eventId == TEL_EV_BBMC_STATUS_UPDATE ||
	    eventId == TEL_EV_BBMC_NETWORK_READY ||
	    eventId == TEL_EV_BBMC_LPM_FROM_NETWORK_READY ||
	    eventId == TEL_EV_BBMC_SIM_ENABLED ||
	    eventId == TEL_EV_BBMC_SIM_DISABLED ||
	    eventId == TEL_EV_EXCEED_HI_TEMP ||
	    eventId == TEL_EV_FALL_DOWN_LOW_TEMP ||
	    eventId == TEL_EV_SIM_BLOCKED_NORMAL_OP ||
	    eventId == TEL_EV_SIM_PERMANENTLY_BLOCKED) {
		RefreshNetworkAndBearerCache();
	}
}

} /* namespace telephony */
