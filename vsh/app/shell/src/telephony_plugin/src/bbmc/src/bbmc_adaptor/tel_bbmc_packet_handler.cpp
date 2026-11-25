/*
 * tel_bbmc_packet_handler.cpp: TelBbmcPacketHandler
 * (3.65-CEX: 0x813D355C..0x813D3E3F).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

namespace {

enum TelPacketLogTag {
	PKT_LOG_CER = 0,
	PKT_LOG_BT = 1,
	PKT_LOG_SCF = 2,
	PKT_LOG_CCF = 3,
	PKT_LOG_GCF = 4,
	PKT_LOG_RPDPC = 5
};

enum TelPacketTaskBit {
	PKT_TASK_CALL_ENDED = 0x001,
	PKT_TASK_BEARER_CHANGED = 0x002,
	PKT_TASK_CALL_STARTED = 0x004,
	PKT_TASK_GET_CTRL_FLAG = 0x008,
	PKT_TASK_SET_PACKET_BIT = 0x010,
	PKT_TASK_SET_ROAM_BIT = 0x020,
	PKT_TASK_SET_SUSPEND_BIT = 0x040,
	PKT_TASK_CLR_PACKET_BIT = 0x080,
	PKT_TASK_CLR_ROAM_BIT = 0x100,
	PKT_TASK_CLR_SUSPEND_BIT = 0x200
};

} /* namespace */

/* 0x813D355C */
TelBbmcPacketHandler::TelBbmcPacketHandler(TelBbmcSharedData *sharedData)
	: TelBbmcHandler(sharedData),
	  m_stateMutex("SceTelBbmcPacketHandler", 0),
	  m_controlFlags(0),
	  m_controlSetCount(0),
	  m_controlEventCount(0)
{
	sce_paf_memset(&m_callEndReason, 0, sizeof(m_callEndReason));
	sce_paf_memset(&m_bearerTech, 0, sizeof(m_bearerTech));
	Register(TEL_BBMC_LOG_CH_PACKET);
}

/* 0x813D35F0 */
TelBbmcPacketHandler::~TelBbmcPacketHandler()
{
	Unregister();
}

/* 0x813D3D98 */
bool TelBbmcPacketHandler::CanProcessTimerRetry()
{
	TelBbmcSharedStatus *status = TelBbmcSharedStatus::GetInstance();
	return status == NULL || !status->GetFlag(TEL_BBMC_FLAG_SUSPENDING);
}

SceUInt32 TelBbmcPacketHandler::GetFeatureId()
{
	return TEL_BBMC_FEATURE_PACKET;
}

/* 0x813D3A68 */
SceUInt32 TelBbmcPacketHandler::GetEventMask()
{
	return 0x3FU;
}

/* 0x813D3A6C */
SceUInt32 TelBbmcPacketHandler::GetSubscriberEvfBit()
{
	return 1U << GetFeatureId();
}

/* 0x813D36BA */
TelephonyResult TelBbmcPacketHandler::SetPacketControlBit(bool enable)
{
	TelephonyResult res(TEL_OK);
	m_sync.Lock();
	m_sync.IncrementActiveCount();
	BeginSyncWait(0x04U);

	SceUInt32 taskBit = enable ? PKT_TASK_SET_PACKET_BIT : PKT_TASK_CLR_PACKET_BIT;
	if (PostTask(taskBit) >= 0) {
		SceUInt32 lastEvent = 0;
		int status = 0;
		WaitSync(&lastEvent, &status, 30000000);
		if (status < 0)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		else if (m_syncTimedOut)
			res.SetCode(TEL_ERR_TIMEOUT);
	} else {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	}

	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

/* 0x813D377E */
TelephonyResult TelBbmcPacketHandler::SetRoamingControlBit(bool enable)
{
	TelephonyResult res(TEL_OK);
	m_sync.Lock();
	m_sync.IncrementActiveCount();
	BeginSyncWait(0x04U);

	SceUInt32 taskBit = enable ? PKT_TASK_SET_ROAM_BIT : PKT_TASK_CLR_ROAM_BIT;
	if (PostTask(taskBit) >= 0) {
		SceUInt32 lastEvent = 0;
		int status = 0;
		WaitSync(&lastEvent, &status, 30000000);
		if (status < 0)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		else if (m_syncTimedOut)
			res.SetCode(TEL_ERR_TIMEOUT);
	} else {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	}

	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

/* 0x813D3842 */
TelephonyResult TelBbmcPacketHandler::SetSuspendControlBit(bool enable)
{
	TelephonyResult res(TEL_OK);
	m_sync.Lock();
	m_sync.IncrementActiveCount();
	BeginSyncWait(0x04U);

	SceUInt32 taskBit = enable ? PKT_TASK_SET_SUSPEND_BIT : PKT_TASK_CLR_SUSPEND_BIT;
	if (PostTask(taskBit) >= 0) {
		SceUInt32 lastEvent = 0;
		int status = 0;
		WaitSync(&lastEvent, &status, 30000000);
		if (status < 0)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		else if (m_syncTimedOut)
			res.SetCode(TEL_ERR_TIMEOUT);
	} else {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	}

	m_sync.DecrementActiveCount();
	m_sync.Unlock();
	return res;
}

/* 0x813D3906 */
TelephonyResult TelBbmcPacketHandler::GetLastCallEndReason(SceBbmcCallEndReason *outReason)
{
	m_stateMutex.Lock();
	sce_paf_memcpy(outReason, &m_callEndReason, sizeof(m_callEndReason));
	m_stateMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813D392E */
TelephonyResult TelBbmcPacketHandler::GetCurrentBearerTech(SceBbmcBearerTech *outBearer)
{
	m_stateMutex.Lock();
	sce_paf_memcpy(outBearer, &m_bearerTech, sizeof(m_bearerTech));
	m_stateMutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813D3956 */
TelephonyResult TelBbmcPacketHandler::ReleasePdpContext()
{
	TelephonyResult res(TEL_OK);
	int ret = sceBbmcPacketReleasePDPContext(m_subscriberId);
	LogResult(PKT_LOG_RPDPC, ret);

	if (ret == 0)
		return res;
	if (ret == SCE_BBMC_ERROR_PDP_BUSY)
		res.SetCode(TEL_ERR_PDP_RELEASE_BUSY);
	else if (ret == SCE_BBMC_ERROR_PDP_STATE)
		res.SetCode(TEL_ERR_PDP_RELEASE_STATE);
	else
		res.SetCode(TEL_ERR_BBMC_DRIVER);
	return res;
}

/* 0x813D3A00 */
bool TelBbmcPacketHandler::IsPacketCallActive() const
{
	TelBbmcSharedStatus *status = TelBbmcSharedStatus::GetInstance();
	return status != NULL && status->GetFlag(TEL_BBMC_FLAG_PACKET_ACTIVE);
}

/* 0x813D3A14 */
SceUInt32 TelBbmcPacketHandler::GetControlFlagValue()
{
	m_stateMutex.Lock();
	SceUInt32 value = m_controlFlags;
	m_stateMutex.Unlock();
	return value;
}

/* 0x813D3A30 */
int TelBbmcPacketHandler::GetControlSetCount()
{
	m_stateMutex.Lock();
	int count = m_controlSetCount;
	m_stateMutex.Unlock();
	return count;
}

/* 0x813D3A4C */
int TelBbmcPacketHandler::GetControlEventCount()
{
	m_stateMutex.Lock();
	int count = m_controlEventCount;
	m_stateMutex.Unlock();
	return count;
}

/* 0x813D3A7C */
void TelBbmcPacketHandler::ProcessTasks(SceUInt32 taskBits)
{
	TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
	SceUInt32 tasks = taskBits;

	if ((tasks & PKT_TASK_CALL_STARTED) != 0) {
		if (sharedStatus != NULL)
			sharedStatus->SetFlag(TEL_BBMC_FLAG_PACKET_ACTIVE, true);
		tasks |= PKT_TASK_BEARER_CHANGED;
	}

	if ((tasks & PKT_TASK_CALL_ENDED) != 0) {
		if (sharedStatus != NULL)
			sharedStatus->SetFlag(TEL_BBMC_FLAG_PACKET_ACTIVE, false);
		tasks |= PKT_TASK_BEARER_CHANGED;
		m_stateMutex.Lock();
		int ret = sceBbmcPacketGetLastCallEndReason(m_subscriberId, &m_callEndReason);
		LogResult(PKT_LOG_CER, ret);
		if (ret >= 0)
			PushNotify(TEL_BBMC_EVENT_CALL_END_REASON, 0);
		m_stateMutex.Unlock();
	}

	if ((tasks & PKT_TASK_BEARER_CHANGED) != 0) {
		m_stateMutex.Lock();
		int ret = sceBbmcPacketGetCurrentBearerTech(m_subscriberId, &m_bearerTech);
		LogResult(PKT_LOG_BT, ret);
		if (ret >= 0)
			PushNotify(TEL_BBMC_EVENT_BEARER_TECH, 0);
		m_stateMutex.Unlock();
	}

	if ((tasks & (PKT_TASK_SET_PACKET_BIT | PKT_TASK_SET_ROAM_BIT |
		      PKT_TASK_SET_SUSPEND_BIT)) != 0) {
		SceUInt32 ctrlBits = 0;
		if ((tasks & PKT_TASK_SET_PACKET_BIT) != 0)
			ctrlBits |= 1U;
		if ((tasks & PKT_TASK_SET_ROAM_BIT) != 0)
			ctrlBits |= 2U;
		if ((tasks & PKT_TASK_SET_SUSPEND_BIT) != 0)
			ctrlBits |= 4U;

		int ret = sceBbmcPacketIfSetControlFlag(m_subscriberId, ctrlBits);
		LogResult(PKT_LOG_SCF, ret);
		if (ret != 0) {
			SignalSyncError(0x04U, ret);
		} else {
			m_stateMutex.Lock();
			++m_controlSetCount;
			m_stateMutex.Unlock();
		}
	}

	if ((tasks & (PKT_TASK_CLR_PACKET_BIT | PKT_TASK_CLR_ROAM_BIT |
		      PKT_TASK_CLR_SUSPEND_BIT)) != 0) {
		SceUInt32 ctrlBits = 0;
		if ((tasks & PKT_TASK_CLR_PACKET_BIT) != 0)
			ctrlBits |= 1U;
		if ((tasks & PKT_TASK_CLR_ROAM_BIT) != 0)
			ctrlBits |= 2U;
		if ((tasks & PKT_TASK_CLR_SUSPEND_BIT) != 0)
			ctrlBits |= 4U;

		int ret = sceBbmcPacketIfClearControlFlag(m_subscriberId, ctrlBits);
		LogResult(PKT_LOG_CCF, ret);
		if (ret != 0) {
			SignalSyncError(0x04U, ret);
		} else {
			m_stateMutex.Lock();
			++m_controlSetCount;
			m_stateMutex.Unlock();
		}
	}

	if ((tasks & PKT_TASK_GET_CTRL_FLAG) != 0) {
		m_stateMutex.Lock();
		int ret = sceBbmcPacketIfGetControlFlag(m_subscriberId, &m_controlFlags);
		LogResult(PKT_LOG_GCF, ret);
		m_stateMutex.Unlock();
	}
}

void TelBbmcPacketHandler::OnBroadcastFlags(SceUInt32 flags)
{
	(void)flags;
}

/* 0x813D3C96 */
int TelBbmcPacketHandler::HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					      SceUInt32 *outTaskBits)
{
	(void)notifyCount;
	SceUInt32 handled = 0;
	LogEventMask(eventBits);

	if ((eventBits & 0x01U) != 0) {
		handled |= 0x01U;
		*outTaskBits |= PKT_TASK_CALL_STARTED;
	}
	if ((eventBits & 0x02U) != 0) {
		handled |= 0x02U;
		*outTaskBits |= PKT_TASK_CALL_ENDED;
	}
	if ((eventBits & 0x04U) != 0) {
		handled |= 0x04U;
		*outTaskBits |= PKT_TASK_GET_CTRL_FLAG;
		m_stateMutex.Lock();
		++m_controlEventCount;
		m_stateMutex.Unlock();
	}
	if ((eventBits & 0x08U) != 0) {
		handled |= 0x08U;
		*outTaskBits |= PKT_TASK_BEARER_CHANGED;
	}
	if ((eventBits & 0x10U) != 0) {
		handled |= 0x10U;
		PushNotify(TEL_BBMC_EVENT_PACKET_RESTRICTED, TEL_ERR_PACKET_RESTRICTED);
	}
	if ((eventBits & 0x20U) != 0) {
		handled |= 0x20U;
		PushNotify(TEL_BBMC_EVENT_PACKET_UNRESTRICTED, 0);
	}

	if (handled != eventBits)
		handled = eventBits;
	ClearAndSignalEvent(handled);
	return 0;
}

/* 0x813D3DB6 */
const char *TelBbmcPacketHandler::GetLogTagName(int tagId) const
{
	switch (tagId) {
	case PKT_LOG_CER:
		return "CER";
	case PKT_LOG_BT:
		return "BT";
	case PKT_LOG_SCF:
		return "SCF";
	case PKT_LOG_CCF:
		return "CCF";
	case PKT_LOG_GCF:
		return "GCF";
	case PKT_LOG_RPDPC:
		return "RPDPC";
	default:
		return "";
	}
}

} /* namespace telephony */
