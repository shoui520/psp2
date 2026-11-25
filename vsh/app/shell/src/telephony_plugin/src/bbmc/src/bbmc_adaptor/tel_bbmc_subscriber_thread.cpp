/*
 * tel_bbmc_subscriber_thread.cpp: BBMC driver event-flag subscriber thread
 * and 100ms retry timer for telephony_plugin (3.65-CEX: 0x813D64C8..0x813D6A73).
 */
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

static const SceUInt32 kBbmcSubscriberStopBit = 0x20000000U;
static const SceUInt32 kBbmcSubscriberTimerBit = 0x40000000U;
static const SceUInt32 kBbmcSubscriberPollBit = 0x80000000U;
static const SceUInt32 kBbmcWaitTimeoutUsec = 5000000U;
static const SceUInt64 kBbmcRetryTimerUsec = 100000ULL;

TelBbmcSubscriberThread::TelBbmcSubscriberThread(const char *name, int priority, SceSize stackSize)
	: TelThread(name, priority, stackSize),
	  m_startSema(NULL),
	  m_handlers(),
	  m_eventFlag(-1),
	  m_timerCallback(-1),
	  m_retryTimer(-1)
{
}

TelBbmcSubscriberThread::~TelBbmcSubscriberThread()
{
}

TelephonyResult TelBbmcSubscriberThread::StartSubscriber(TelBbmcAdapter *adapter)
{
	TelephonyResult res(TEL_OK);

	m_startSema = new paf::thread::Semaphore("SceTelBbmcSubscriberSema", 0, 1, NULL);
	if (m_startSema == NULL) {
		res.SetCode(TEL_ERR_BBMC_DRIVER);
		return res;
	}

	res = StartThread(adapter);
	if (res.IsSuccess()) {
		m_startSema->Acquire();
		if (IsThreadCanceled() || m_eventFlag < 0)
			res.SetCode(TEL_ERR_BBMC_DRIVER);
	}

	delete m_startSema;
	m_startSema = NULL;
	return res;
}

TelephonyResult TelBbmcSubscriberThread::StopThread()
{
	TelephonyResult res(TEL_OK);

	CancelThread();
	if (m_eventFlag >= 0)
		sceKernelSetEventFlag(m_eventFlag, kBbmcSubscriberStopBit);
	JoinThread();
	return res;
}

void TelBbmcSubscriberThread::AddHandler(TelBbmcHandler *handler)
{
	m_handlers.push_back(handler);
}

void TelBbmcSubscriberThread::DispatchHandlers(SceUInt32 evfBits)
{
	SceUInt32 broadcastFlags = 0;

	for (SceSize index = 0; index < m_handlers.size(); ++index) {
		TelBbmcHandler *handler = m_handlers[index];
		handler->HandleEvfWake(evfBits);

		if (handler->CanProcessTimerRetry()) {
			if (handler->GetPendingRetryTasks() != 0)
				StartRetryTimer();
		} else {
			StopRetryTimer();
		}

		broadcastFlags |= handler->TakeBroadcastFlags();
	}

	if (broadcastFlags != 0) {
		for (SceSize index = 0; index < m_handlers.size(); ++index)
			m_handlers[index]->DispatchBroadcast(broadcastFlags);
	}
}

TelephonyResult TelBbmcSubscriberThread::SubscribeAll()
{
	TelephonyResult res(TEL_OK);

	for (SceSize index = 0; index < m_handlers.size(); ++index) {
		res = m_handlers[index]->Subscribe(m_eventFlag);
		if (!res.IsSuccess()) {
			for (SceSize rollback = 0; rollback < index; ++rollback)
				m_handlers[rollback]->Unsubscribe();
			return res;
		}
	}

	return res;
}

int TelBbmcSubscriberThread::Run(void *userArg)
{
	TelBbmcAdapter *adapter = static_cast<TelBbmcAdapter *>(userArg);

	m_eventFlag = sceKernelCreateEventFlag("SceTelBbmcSubscriberEvf",
					       SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (m_eventFlag < 0) {
		m_startSema->Release();
		return m_eventFlag;
	}

	m_timerCallback = sceKernelCreateCallback("SceTelBbmcTimerCb", 0,
						  &TelBbmcSubscriberThread::TimerCallback, this);
	if (m_timerCallback < 0) {
		sceKernelDeleteEventFlag(m_eventFlag);
		m_eventFlag = -1;
		m_startSema->Release();
		return m_timerCallback;
	}

	TelephonyResult subRes = SubscribeAll();
	if (!subRes.IsSuccess())
		CancelThread();

	m_startSema->Release();

	int heartbeatCountdown = 6;
	while (!IsThreadCanceled()) {
		SceUInt32 outBits = 0;
		SceUInt timeout = kBbmcWaitTimeoutUsec;
		int waitRet = sceKernelWaitEventFlagCB(m_eventFlag, 0xFFFFFFFFU,
						       SCE_KERNEL_EVF_WAITMODE_OR |
						       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						       &outBits, &timeout);
		if (waitRet == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			outBits |= kBbmcSubscriberPollBit;
			if (--heartbeatCountdown <= 0)
				heartbeatCountdown = 6;
		}

		if (outBits != 0)
			DispatchHandlers(outBits);

		for (SceSize index = 0; index < m_handlers.size(); ++index) {
			TelBbmcNotifyItem item;
			while (m_handlers[index]->PopNotify(&item).IsSuccess()) {
				if (adapter != NULL)
					adapter->HandleHandlerEvent(item.eventId, item.eventArg);
			}
		}
	}

	StopRetryTimer();

	for (SceSize index = 0; index < m_handlers.size(); ++index)
		m_handlers[index]->Unsubscribe();

	sceKernelDeleteCallback(m_timerCallback);
	SceUID evfId = m_eventFlag;
	m_eventFlag = -1;
	sceKernelDeleteEventFlag(evfId);
	return 0;
}

bool TelBbmcSubscriberThread::StartRetryTimer()
{
	if (m_retryTimer >= 0)
		return false;

	SceUID timerId = sceKernelCreateTimer("SceTelBbmcTimer", 0, NULL);
	if (timerId < 0)
		return false;

	if (sceKernelRegisterCallbackToEvent(timerId, m_timerCallback) >= 0) {
		SceKernelSysClock interval;
		interval.quad = kBbmcRetryTimerUsec;
		if (sceKernelSetTimerEvent(timerId, 1, &interval, 0) >= 0 &&
		    sceKernelStartTimer(timerId) >= 0) {
			m_retryTimer = timerId;
			return true;
		}
	}

	sceKernelDeleteTimer(timerId);
	return false;
}

void TelBbmcSubscriberThread::StopRetryTimer()
{
	if (m_retryTimer >= 0) {
		sceKernelDeleteTimer(m_retryTimer);
		m_retryTimer = -1;
	}
}

int TelBbmcSubscriberThread::TimerCallback(int notifyId, int notifyCount, int notifyArg,
					   void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;

	TelBbmcSubscriberThread *self = static_cast<TelBbmcSubscriberThread *>(common);
	if (self != NULL) {
		self->StopRetryTimer();
		if (self->m_eventFlag >= 0)
			sceKernelSetEventFlag(self->m_eventFlag, kBbmcSubscriberTimerBit);
	}
	return 0;
}

} /* namespace telephony */
