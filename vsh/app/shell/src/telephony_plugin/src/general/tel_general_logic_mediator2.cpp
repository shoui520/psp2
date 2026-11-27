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

/* ------------------------------------------------------------------
 * Dedicated mediator event handlers (0x8125BE8A..0x8125D35D, 0x8125D736)
 * ------------------------------------------------------------------ */

/* 0x8125BE8A */
void TelGeneralLogicMediator::HandleBbmcComBootOk(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcComBootOk();
	m_smcMutex.Unlock();
}

/* 0x8125BED4 */
void TelGeneralLogicMediator::HandleBbmcComBootNg(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcComBootNg();
	m_smcMutex.Unlock();
}

/* 0x8125BF20 */
void TelGeneralLogicMediator::HandleMediatorComFailure(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventMediatorComFailure();
	m_smcMutex.Unlock();
}

/* 0x8125BFC2 */
void TelGeneralLogicMediator::HandleBbmcNeedUpdatingDeviceInfo(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcNeedUpdatingDeviceInfo();
	NotifyObservers(event);
	m_smcMutex.Unlock();
}

/* 0x8125C00C */
void TelGeneralLogicMediator::HandleBbmcSetupReady(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcSetupReady();
	m_smcMutex.Unlock();
}

/* 0x8125C056 */
void TelGeneralLogicMediator::HandleBbmcNetworkReady(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcNetworkReady();
	m_smcMutex.Unlock();
}

/* 0x8125C0A0 */
void TelGeneralLogicMediator::HandleBbmcPinRequired(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPinRequired();
	m_smcMutex.Unlock();
}

/* 0x8125C136 */
void TelGeneralLogicMediator::HandleBbmcPukRequired(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPukRequired();
	m_smcMutex.Unlock();
}

/* 0x8125C1CC */
void TelGeneralLogicMediator::HandleBbmcPinNotRequired(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPinNotRequired();
	m_smcMutex.Unlock();
}

/* 0x8125C262 */
void TelGeneralLogicMediator::HandleBbmcPinPermanentlyBlocked(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPinPermanentlyBlocked();
	m_smcMutex.Unlock();
}

/* 0x8125C2FA */
void TelGeneralLogicMediator::HandleBbmcNetScanFin(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_scanActive = false;
	m_stateMachine.OnEventBbmcNetScanFin();
	m_smcMutex.Unlock();
}

/* 0x8125C374 */
void TelGeneralLogicMediator::HandleBbmcNetScanFailed(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_scanActive = false;
	m_stateMachine.OnEventBbmcNetScanFailed();
	m_smcMutex.Unlock();
}

/* 0x8125C3EE */
void TelGeneralLogicMediator::HandleBbmcPinStatusReady(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPinStatusReady();
	m_smcMutex.Unlock();
}

/* 0x8125C486 */
void TelGeneralLogicMediator::HandleBbmcExceedHiTemp(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcExceedHiTemp();
	NotifyObservers(event);
	m_smcMutex.Unlock();
}

/* 0x8125C4E6 */
void TelGeneralLogicMediator::HandleBbmcPacketCallEndReasonUpdate(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventBbmcPacketCallEndReasonUpdate();
	m_smcMutex.Unlock();
}

/* 0x8125C5E4 */
void TelGeneralLogicMediator::HandleBbmcComFwUpdateNotice(const TelEventInfo &event)
{
	NotifyObservers(event);
}

/* 0x8125C6EC */
void TelGeneralLogicMediator::HandleInternalVerifyPin(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_VERIFY_PIN);
	m_stateMachine.OnEventMediatorInternalVerifyPin();
	m_smcMutex.Unlock();
}

/* 0x8125C7E0 */
void TelGeneralLogicMediator::HandleInternalUnblockPin(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_UNBLOCK_PIN);
	m_stateMachine.OnEventMediatorInternalUnblockPin();
	m_smcMutex.Unlock();
}

/* 0x8125C924 */
void TelGeneralLogicMediator::HandleInternalSetPinProtection(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_PIN_PROTECTION);
	m_stateMachine.OnEventMediatorInternalSetPinProtection();
	m_smcMutex.Unlock();
}

/* 0x8125CA3C */
void TelGeneralLogicMediator::HandleInternalChangePinCode(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_CHANGE_PIN_CODE);
	m_stateMachine.OnEventMediatorInternalChangePinCode();
	m_smcMutex.Unlock();
}

/* 0x8125CBF6 */
void TelGeneralLogicMediator::HandleInternalSetApnInfo(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_APN_INFO);
	m_stateMachine.OnEventMediatorInternalSetApnInfo();
	m_smcMutex.Unlock();
}

/* 0x8125CD56 */
void TelGeneralLogicMediator::HandleInternalSetApnInfoWithBillingPlan(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	if (event.GetCategory() == 20) {
		const TelSetApnBillingEvent &billingEv =
			static_cast<const TelSetApnBillingEvent &>(event);
		m_action.internalBillingApn = billingEv.GetApnInfo();
		m_action.internalBillingSaveFlag = (event.GetEventResult() != 0);
	}
	m_action.SetCurrentApiId(TEL_API_INTERNAL_SET_APN_INFO_BILLING);
	m_stateMachine.OnEventMediatorInternalSetApnInfoWithBillingPlan();
	m_smcMutex.Unlock();
}

/* 0x8125CF0E */
void TelGeneralLogicMediator::HandleInternalRestoreApnInfo(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_RESTORE_APN_INFO);
	m_stateMachine.OnEventMediatorInternalRestoreApnInfo();
	m_smcMutex.Unlock();
}

/* 0x8125CFBC */
void TelGeneralLogicMediator::HandleInternalRemoveCurrentApnInfo(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_REMOVE_CURRENT_APN_INFO);
	m_stateMachine.OnEventMediatorInternalRemoveCurrentApnInfo();
	m_smcMutex.Unlock();
}

/* 0x8125D06C */
void TelGeneralLogicMediator::HandleInternalDeactivateSimLock(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_DEACTIVATE_SIM_LOCK);
	m_stateMachine.OnEventMediatorInternalDeactivateSimLock();
	m_smcMutex.Unlock();
}

/* 0x8125D166 */
void TelGeneralLogicMediator::HandleInternalUnblockSimLock(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_action.SetCurrentApiId(TEL_API_INTERNAL_UNBLOCK_SIM_LOCK);
	m_stateMachine.OnEventMediatorInternalUnblockSimLock();
	m_smcMutex.Unlock();
}

/* 0x8125D262 */
void TelGeneralLogicMediator::HandleInternalResume(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventMediatorInternalResume();
	m_smcMutex.Unlock();
}

/* 0x8125D302 */
void TelGeneralLogicMediator::HandleAttributeError(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventAttributeError();
	m_smcMutex.Unlock();
}

/* 0x8125D736 */
void TelGeneralLogicMediator::HandleInternalSimLockError(const TelEventInfo &event)
{
	m_smcMutex.Lock();
	m_action.SetEvent(event);
	m_stateMachine.OnEventMediatorInternalSimLockError();
	m_smcMutex.Unlock();
}

void TelGeneralLogicMediator::HandleInternalAsyncApi(const TelEventInfo &event)
{
	switch (event.GetEventId()) {
	case TEL_EV_INTERNAL_VERIFY_PIN:
		HandleInternalVerifyPin(event);
		break;
	case TEL_EV_INTERNAL_UNBLOCK_PIN:
		HandleInternalUnblockPin(event);
		break;
	case TEL_EV_INTERNAL_SET_PIN_PROTECTION:
		HandleInternalSetPinProtection(event);
		break;
	case TEL_EV_INTERNAL_CHANGE_PIN_CODE:
		HandleInternalChangePinCode(event);
		break;
	case TEL_EV_INTERNAL_SET_APN_INFO:
		HandleInternalSetApnInfo(event);
		break;
	case TEL_EV_INTERNAL_SET_APN_INFO_BILLING:
		HandleInternalSetApnInfoWithBillingPlan(event);
		break;
	case TEL_EV_INTERNAL_RESTORE_APN_INFO:
		HandleInternalRestoreApnInfo(event);
		break;
	case TEL_EV_INTERNAL_REMOVE_CURRENT_APN_INFO:
		HandleInternalRemoveCurrentApnInfo(event);
		break;
	case TEL_EV_INTERNAL_DEACTIVATE_SIM_LOCK:
		HandleInternalDeactivateSimLock(event);
		break;
	case TEL_EV_INTERNAL_UNBLOCK_SIM_LOCK:
		HandleInternalUnblockSimLock(event);
		break;
	default:
		break;
	}
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

	if (eventId == TEL_EV_COM_FW_UPDATE_NOTICE) {
		HandleBbmcComFwUpdateNotice(event);
		return;
	}

	/* GUI notifications forwarded directly to observers (TelGeneralLogicGui) */
	if (eventId == TEL_GUI_EVENT_ERROR_DIALOG ||
	    eventId == TEL_GUI_EVENT_SIM_STATE_RESET) {
		NotifyObservers(event);
		return;
	}

	switch (eventId) {
	case TEL_EV_BBMC_COM_BOOT_OK:
		HandleBbmcComBootOk(event);
		break;
	case TEL_EV_BBMC_COM_BOOT_NG:
		HandleBbmcComBootNg(event);
		break;
	case TEL_EV_BBMC_SETUP_READY:
		HandleBbmcSetupReady(event);
		break;
	case TEL_EV_BBMC_NETWORK_READY:
		HandleBbmcNetworkReady(event);
		RefreshNetworkAndBearerCache();
		break;
	case TEL_EV_EXCEED_HI_TEMP:
		HandleBbmcExceedHiTemp(event);
		RefreshNetworkAndBearerCache();
		break;
	case TEL_EV_MEDIATOR_COM_FAILURE:
		HandleMediatorComFailure(event);
		break;
	case TEL_EV_INTERNAL_SIM_LOCK_ERROR:
		HandleInternalSimLockError(event);
		break;
	case TEL_EV_INTERNAL_RESUME:
		HandleInternalResume(event);
		break;
	case TEL_EV_BBMC_PIN_STATUS_READY:
		HandleBbmcPinStatusReady(event);
		break;
	case TEL_EV_BBMC_PIN_NOT_REQUIRED:
		HandleBbmcPinNotRequired(event);
		break;
	case TEL_EV_BBMC_PIN_REQUIRED:
		HandleBbmcPinRequired(event);
		break;
	case TEL_EV_BBMC_PUK_REQUIRED:
		HandleBbmcPukRequired(event);
		break;
	case TEL_EV_BBMC_PIN_PERM_BLOCKED:
		HandleBbmcPinPermanentlyBlocked(event);
		break;
	case TEL_EV_BBMC_NET_SCAN_FIN:
		HandleBbmcNetScanFin(event);
		break;
	case TEL_EV_BBMC_NET_SCAN_FAILED:
		HandleBbmcNetScanFailed(event);
		break;
	case TEL_EV_BBMC_NEED_DEVICE_INFO:
		HandleBbmcNeedUpdatingDeviceInfo(event);
		break;
	case TEL_EV_BBMC_PACKET_CALL_END_REASON:
		HandleBbmcPacketCallEndReasonUpdate(event);
		break;
	case TEL_EVENT_ATTRIBUTE_ERROR:
		HandleAttributeError(event);
		break;
	default:
		m_smcMutex.Lock();
		m_action.SetEvent(event);
		switch (eventId) {
		case TEL_EV_BBMC_COM_DOWN_AT_SUSPEND:
			m_stateMachine.OnEventBbmcComDownAtSuspend();
			break;
		case TEL_EV_BBMC_LPM_FROM_NETWORK_READY:
			m_stateMachine.OnEventBbmcLpmFromNetworkReady();
			break;
		case TEL_EV_SIM_BLOCKED_NORMAL_OP:
			m_cachedSimCondition = 3;
			m_stateMachine.OnEventBbmcSimBlockedNormalOperation();
			break;
		case TEL_EV_SIM_PERMANENTLY_BLOCKED:
			m_cachedSimCondition = 4;
			m_stateMachine.OnEventBbmcSimPermanentlyBlocked();
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
		case TEL_EV_BBMC_NET_REGISTER_FIN:
			m_stateMachine.OnEventBbmcNetRegisterSettingFin();
			break;
		case TEL_EV_BBMC_NET_REGISTER_FAILED:
			m_stateMachine.OnEventBbmcNetRegisterSettingFailed();
			break;
		case TEL_EV_BBMC_SIM_LOCK_INFO:
			m_stateMachine.OnEventBbmcSimLockInfo();
			break;
		case TEL_EV_BBMC_PLMN_MODE_BIT:
			m_stateMachine.OnEventBbmcPlmnModeBit();
			break;
		default:
			m_stateMachine.OnEventNotifyCallback();
			break;
		}
		m_smcMutex.Unlock();

		if (eventId == TEL_EV_BBMC_NETINFO ||
		    eventId == TEL_EV_BBMC_STATUS_UPDATE ||
		    eventId == TEL_EV_BBMC_LPM_FROM_NETWORK_READY ||
		    eventId == TEL_EV_BBMC_SIM_ENABLED ||
		    eventId == TEL_EV_BBMC_SIM_DISABLED ||
		    eventId == TEL_EV_FALL_DOWN_LOW_TEMP ||
		    eventId == TEL_EV_SIM_BLOCKED_NORMAL_OP ||
		    eventId == TEL_EV_SIM_PERMANENTLY_BLOCKED) {
			RefreshNetworkAndBearerCache();
		}
		break;
	}
}

} /* namespace telephony */
