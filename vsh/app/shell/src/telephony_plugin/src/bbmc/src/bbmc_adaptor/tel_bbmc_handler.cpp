/*
 * tel_bbmc_handler.cpp: TelBbmcHandler base class
 * (3.65-CEX: 0x813D2E10..0x813D3407).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <kernel/threadmgr.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

/* 0x813D2E10 */
TelBbmcHandler::TelBbmcHandler(TelBbmcSharedData *sharedData)
	: m_taskMutex("SceTelBbmcHandler", 0),
	  m_subscriberEvf(-1),
	  m_pendingTasks(0),
	  m_broadcastFlags(0),
	  m_retryTasks(0),
	  m_syncTimedOut(false),
	  m_subscriberId(-1),
	  m_callbackId(-1),
	  m_notifyQueue("SceTelBbmcNotifyQ", 64),
	  m_maxRetries(10),
	  m_remainingRetries(0),
	  m_retryDelayMs(20),
	  m_clearedEvents(0),
	  m_sharedData(sharedData)
{
	TelBbmcSharedStatus::Acquire();
	m_notifyQueue.Activate();
	sce_paf_memset(m_taskRetryCounters, 0, sizeof(m_taskRetryCounters));
}

/* 0x813D2F22 */
TelBbmcHandler::~TelBbmcHandler()
{
	m_notifyQueue.Deactivate();
	TelBbmcSharedStatus::Release();
}

/* 0x813D2FF6 */
TelephonyResult TelBbmcHandler::Unsubscribe()
{
	if (m_subscriberId >= 0) {
		sceBbmcUnsubscribeFeature(m_subscriberId);
		m_subscriberId = -1;
	}
	if (m_callbackId >= 0) {
		sceKernelDeleteCallback(m_callbackId);
		m_callbackId = -1;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813D3024 */
TelephonyResult TelBbmcHandler::Subscribe(SceUID subscriberEvf)
{
	TelephonyResult res(TEL_OK);
	m_subscriberEvf = subscriberEvf;

	m_callbackId = sceKernelCreateCallback("SceTelBbmcHandlerCb", 0,
					       TelBbmcHandler::CallbackEntry, this);
	if (m_callbackId < 0) {
		res.SetCode(TEL_ERR_IO_FAILED);
		return res;
	}

	int subId = sceBbmcSubscribeFeatureId(GetFeatureId(), GetEventMask(), m_callbackId);
	if (subId < 0) {
		res.SetCode(TEL_ERR_IO_FAILED);
		Unsubscribe();
		return res;
	}

	m_subscriberId = subId;
	return res;
}

/* 0x813D30E8 */
TelephonyResult TelBbmcHandler::PopNotify(TelBbmcNotifyItem *outItem)
{
	if (!m_notifyQueue.Pop(outItem))
		return TelephonyResult(TEL_ERR_NOT_FOUND);
	return TelephonyResult(TEL_OK);
}

/* 0x813D3116 */
void TelBbmcHandler::HandleEvfWake(SceUInt32 evfBits)
{
	if (((GetSubscriberEvfBit() | 0xC0000000U) & evfBits) == 0)
		return;

	m_taskMutex.Lock();
	SceUInt32 tasks = m_pendingTasks;
	m_pendingTasks = 0;
	m_taskMutex.Unlock();

	if ((evfBits & 0x80000000U) != 0)
		tasks |= 0x20000000U;
	if ((evfBits & 0x40000000U) != 0 && CanProcessTimerRetry()) {
		tasks |= m_retryTasks;
		m_retryTasks = 0;
	}

	ProcessTasks(tasks);
}

/* 0x813D3180 */
void TelBbmcHandler::DispatchBroadcast(SceUInt32 flags)
{
	if (flags != 0)
		OnBroadcastFlags(flags);
}

/* 0x813D318C */
bool TelBbmcHandler::ShouldRetryResult(const TelephonyResult &res)
{
	return res.GetCode() == TEL_ERR_BBMC_DRIVER;
}

/* 0x813D31A8 */
void TelBbmcHandler::ResetClearedEvents()
{
	m_clearedEvents = 0;
}

/* 0x813D31B0 */
SceUInt32 TelBbmcHandler::GetClearedEvents()
{
	return m_clearedEvents;
}

/* 0x813D31B6 */
TelephonyResult TelBbmcHandler::PushNotify(int eventId, int eventArg)
{
	TelBbmcNotifyItem item;
	item.eventId = eventId;
	item.eventArg = eventArg;
	return m_notifyQueue.Push(item);
}

/* 0x813D31E6 */
int TelBbmcHandler::PostTask(SceUInt32 taskBit)
{
	m_taskMutex.Lock();
	m_pendingTasks |= taskBit;
	int ret = sceKernelSetEventFlag(m_subscriberEvf, GetSubscriberEvfBit());
	m_taskMutex.Unlock();
	return ret;
}

/* 0x813D321C */
void TelBbmcHandler::SignalSyncError(SceUInt32 eventMask, int status)
{
	m_sync.Signal(eventMask, status);
}

/* 0x813D3226 */
void TelBbmcHandler::ClearAndSignalEvent(SceUInt32 eventMask)
{
	if (eventMask != 0) {
		sceBbmcClearEvent(m_subscriberId, eventMask);
		m_clearedEvents |= eventMask;
	}
	m_sync.Signal(eventMask, 0);
}

/* 0x813D324E */
void TelBbmcHandler::BeginSyncWait(SceUInt32 waitMask)
{
	m_sync.BeginWait(waitMask);
}

/* 0x813D325A */
SceUInt32 TelBbmcHandler::WaitSync(SceUInt32 *outLastEvent, int *outStatus, int timeoutUsec)
{
	m_syncTimedOut = false;
	if (m_sync.Wait(timeoutUsec))
		m_syncTimedOut = true;
	if (outLastEvent != NULL)
		*outLastEvent = m_sync.GetLastEventMask();
	if (outStatus != NULL)
		*outStatus = m_sync.GetLastStatus();
	return m_sync.GetMatchedMask();
}

/* 0x813D3296 */
void TelBbmcHandler::ResetRetryCount(int count)
{
	if (count < 0)
		count = m_maxRetries;
	m_remainingRetries = count;
}

/* 0x813D32AA */
bool TelBbmcHandler::ConsumeRetryAndDelay(const TelephonyResult &res)
{
	if (m_remainingRetries <= 0)
		return false;

	if (!ShouldRetryResult(res)) {
		m_remainingRetries = 0;
		return false;
	}

	TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
	if (sharedStatus != NULL && sharedStatus->GetFlag(TEL_BBMC_FLAG_SUSPENDING)) {
		while (sharedStatus->GetFlag(TEL_BBMC_FLAG_SUSPENDING))
			sceKernelDelayThread((SceUInt32)(m_retryDelayMs * 1000));
		return true;
	}

	--m_remainingRetries;
	sceKernelDelayThread((SceUInt32)(m_retryDelayMs * 1000));
	return true;
}

/* 0x813D3326 */
void TelBbmcHandler::SetTaskRetryBudget(SceUInt32 taskBit, int retries)
{
	int bitIndex = telFindFirstSetBit(taskBit);
	if (retries < 0)
		retries = m_maxRetries;
	m_taskRetryCounters[bitIndex][0] = (SceInt8)retries;
	m_taskRetryCounters[bitIndex][1] = 0;
}

/* 0x813D334C */
bool TelBbmcHandler::ScheduleTaskRetry(SceUInt32 taskBit)
{
	int bitIndex = telFindFirstSetBit(taskBit);
	if (m_taskRetryCounters[bitIndex][0] <= 0)
		return false;
	m_retryTasks |= taskBit;
	--m_taskRetryCounters[bitIndex][0];
	return true;
}

/* 0x813D337C */
int TelBbmcHandler::CallbackEntry(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	TelBbmcHandler *self = (TelBbmcHandler *)common;
	if (self == NULL)
		return 0;

	SceUInt32 taskBits = 0;
	int handleRet = self->HandleCallbackEvent(notifyCount, (SceUInt32)notifyArg, &taskBits);
	taskBits |= 0x40000000U;

	self->m_taskMutex.Lock();
	self->m_pendingTasks |= taskBits;
	sceKernelSetEventFlag(self->m_subscriberEvf, self->GetSubscriberEvfBit());
	self->m_taskMutex.Unlock();

	int resumeRet = 0;
	if (self->m_callbackId >= 0)
		resumeRet = sceBbmcResumeSubscriberCallback(self->m_subscriberId);
	return (handleRet != 0) ? handleRet : resumeRet;
}

} /* namespace telephony */
