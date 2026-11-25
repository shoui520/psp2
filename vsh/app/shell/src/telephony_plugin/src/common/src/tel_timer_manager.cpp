/*
 * tel_timer_manager.cpp: TelTimerManager singleton backed by sceKernelCreateTimer
 * (3.65-CEX: 0x813E4576..0x813E4C2F).
 */
#include <stddef.h>
#include <scetypes.h>
#include <common/main_thread_call_list.h>
#include <kernel/threadmgr.h>
#include "../tel_common.h"

extern "C" {
extern int sceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId);
extern int sceKernelCheckCallback(void);
}

namespace telephony {

namespace {

static TelTimerManager *g_timerManager = NULL;

} /* namespace */

/* 0x813E4576 */
TelTimerManager::TelTimerManager()
	: m_callbackId(-1), m_mutex("SceTelCmnTimerManager", 0)
{
}

TelTimerManager::~TelTimerManager()
{
}

/* 0x813E4C16 */
int TelTimerManager::TimerCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyCount;
	(void)notifyArg;
	(void)common;
	if (g_timerManager != NULL)
		g_timerManager->OnTimerFired((SceUID)notifyId);
	return 0;
}

void TelTimerManager::MainThreadPoll(void *arg)
{
	(void)arg;
	sceKernelCheckCallback();
}

/* 0x813E45B4 */
TelephonyResult TelTimerManager::Initialize()
{
	if (g_timerManager != NULL)
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	g_timerManager = new TelTimerManager();
	if (g_timerManager == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	g_timerManager->m_mutex.Lock();
	g_timerManager->m_callbackId = sceKernelCreateCallback(
		"SceTelTimerCb", 0, TimerCallback, g_timerManager);
	paf::common::MainThreadCallList::Register(MainThreadPoll, g_timerManager);
	g_timerManager->m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813E4692 */
TelephonyResult TelTimerManager::CancelAllTimers()
{
	m_mutex.Lock();
	for (paf::map<SceUID, TelTimerEntry>::iterator it = m_timers.begin();
	     it != m_timers.end(); ++it) {
		sceKernelDeleteTimer(it->first);
	}
	m_timers.clear();
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813E4718 */
TelephonyResult TelTimerManager::Terminate()
{
	if (g_timerManager == NULL)
		return TelephonyResult(TEL_OK);

	TelephonyResult res = g_timerManager->CancelAllTimers();
	g_timerManager->m_mutex.Lock();
	sceKernelDeleteCallback(g_timerManager->m_callbackId);
	paf::common::MainThreadCallList::Unregister(MainThreadPoll, g_timerManager);
	g_timerManager->m_mutex.Unlock();

	delete g_timerManager;
	g_timerManager = NULL;
	return res;
}

/* 0x813E47DC */
TelTimerManager *TelTimerManager::GetInstance()
{
	return g_timerManager;
}

/* 0x813E47E8 */
TelephonyResult TelTimerManager::StartTimer(SceUInt32 msec, TelObserver *observer,
					    SceUID *outTimerId, const TelEventInfo &event)
{
	m_mutex.Lock();
	paf::string timerName = telFormatString("SceTelTimer%x", event.GetEventId());
	SceUID timerId = sceKernelCreateTimer(timerName.c_str(), 0, NULL);
	*outTimerId = timerId;
	if (timerId < 0) {
		m_mutex.Unlock();
		return TelephonyResult(timerId);
	}

	if (sceKernelRegisterCallbackToEvent(timerId, m_callbackId) != 0) {
		m_mutex.Unlock();
		return TelephonyResult(*outTimerId);
	}

	SceKernelSysClock interval;
	interval.quad = (SceUInt64)msec * 1000ULL;
	int err = sceKernelSetTimerEvent(timerId, 1, &interval, 0);
	if (err != 0) {
		sceKernelDeleteTimer(timerId);
		m_mutex.Unlock();
		return TelephonyResult(err);
	}

	err = sceKernelStartTimer(timerId);
	if (err != 0) {
		sceKernelDeleteTimer(timerId);
		m_mutex.Unlock();
		return TelephonyResult(err);
	}

	TelEventInfo *cloned = new TelEventInfo(event.GetEventId(), event.GetEventArg(),
						event.GetEventResult());
	if (cloned != NULL)
		cloned->SetTimerHandle(timerId);

	TelTimerEntry entry;
	entry.event = paf::common::SharedPtr<TelEventInfo>(cloned);
	entry.observer = observer;
	m_timers[timerId] = entry;

	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x813E4AD2 */
TelephonyResult TelTimerManager::CancelTimer(SceUID timerId)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	paf::map<SceUID, TelTimerEntry>::iterator it = m_timers.find(timerId);
	if (it != m_timers.end()) {
		m_timers.erase(it);
		int err = sceKernelDeleteTimer(timerId);
		if (err != 0)
			res = TelephonyResult(err);
	}
	m_mutex.Unlock();
	return res;
}

/* 0x813E4B84 */
void TelTimerManager::OnTimerFired(SceUID timerId)
{
	m_mutex.Lock();
	paf::map<SceUID, TelTimerEntry>::iterator it = m_timers.find(timerId);
	if (it == m_timers.end()) {
		m_mutex.Unlock();
		return;
	}

	TelObserver *observer = it->second.observer;
	paf::common::SharedPtr<TelEventInfo> event = it->second.event;
	m_mutex.Unlock();

	if (observer != NULL && event.get() != NULL)
		observer->OnNotify(*event);
	CancelTimer(timerId);
}

} /* namespace telephony */
