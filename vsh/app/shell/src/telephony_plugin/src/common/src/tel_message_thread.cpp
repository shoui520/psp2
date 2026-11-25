/*
 * tel_message_thread.cpp: TelMessageQueue and TelMessageThread
 * (3.65-CEX: 0x813DF9B8..0x813DFDF7, 0x81454104..0x814544FF).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_common.h"

namespace telephony {

/* 0x81454104 */
TelMessageQueue::TelMessageQueue(const char *name, SceSize maxCount)
	: m_maxCount(maxCount), m_active(false),
	  m_mutex(name, 0),
	  m_notFullCond(m_mutex, name, 0),
	  m_notEmptyCond(m_mutex, name, 0)
{
}

TelMessageQueue::~TelMessageQueue()
{
}

TelephonyResult TelMessageQueue::Activate()
{
	m_mutex.Lock();
	m_active = true;
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelMessageQueue::Deactivate()
{
	m_mutex.Lock();
	m_active = false;
	m_queues[0].clear();
	m_queues[1].clear();
	m_notFullCond.Notify();
	m_notEmptyCond.Notify();
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x81454390 */
TelephonyResult TelMessageQueue::Push(const TelEventInfo &event, int priority)
{
	m_mutex.Lock();
	while (m_queues[priority].size() == m_maxCount && m_active)
		m_notFullCond.Wait(0);

	if (!m_active) {
		m_mutex.Unlock();
		return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
	}

	paf::common::SharedPtr<TelObject> cloned = event.Clone();
	paf::common::SharedPtr<TelEventInfo> clonedEvent(
		static_cast<TelEventInfo *>(cloned.get()));
	(void)clonedEvent;
	/* Store a fresh heap clone owned by the queue */
	TelEventInfo *copy = new TelEventInfo(event.GetEventId(), event.GetEventArg(),
					      event.GetEventResult());
	if (copy != NULL)
		copy->SetTimerHandle(event.GetTimerHandle());
	m_queues[priority].push_back(paf::common::SharedPtr<TelEventInfo>(copy));

	m_notEmptyCond.Notify();
	m_mutex.Unlock();
	return TelephonyResult(TEL_OK);
}

/* 0x814542F0 */
TelephonyResult TelMessageQueue::WaitForMessage()
{
	m_mutex.Lock();
	while (m_active) {
		for (int priority = 0; priority < 2; ++priority) {
			if (m_queues[priority].size() != 0) {
				m_mutex.Unlock();
				return TelephonyResult(TEL_OK);
			}
		}
		m_notEmptyCond.Wait(0);
	}
	m_mutex.Unlock();
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

/* 0x814541F0 */
bool TelMessageQueue::Pop(paf::common::SharedPtr<TelEventInfo> *outEvent, int priority)
{
	m_mutex.Lock();
	if (m_queues[priority].size() == 0) {
		m_mutex.Unlock();
		return false;
	}

	*outEvent = m_queues[priority][0];
	m_queues[priority].erase(m_queues[priority].begin());
	m_notFullCond.Notify();
	m_mutex.Unlock();
	return true;
}

/* 0x813DF9B8 */
TelMessageThread::TelMessageThread(const char *name, int priority, SceSize stackSize)
	: TelThread(name, priority, stackSize), m_queue(NULL)
{
}

/* 0x813DF9DC */
TelMessageThread::~TelMessageThread()
{
}

/* 0x813DFA36 */
TelephonyResult TelMessageThread::StartThread(void *userArg)
{
	m_queue = new TelMessageQueue("SceTelCmnMsgThrQ", 128);
	if (m_queue == NULL)
		return TelephonyResult(TEL_OK);

	m_queue->Activate();
	TelephonyResult res = TelThread::StartThread(userArg);
	if (res.GetCode() == TEL_OK) {
		TelEventInfo startEvent(TEL_EVENT_THREAD_START, -1, -1);
		PostMessage(startEvent);
	}
	return res;
}

/* 0x813DFB02 */
TelephonyResult TelMessageThread::StopThread()
{
	TelEventInfo stopEvent(TEL_EVENT_THREAD_STOP, -1, -1);
	PostMessage(stopEvent);
	JoinThread();
	if (m_queue != NULL) {
		m_queue->Deactivate();
		delete m_queue;
		m_queue = NULL;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813DFBE8 */
TelephonyResult TelMessageThread::DispatchEvent(const TelEventInfo &event)
{
	OnHandleEvent(event);
	return TelephonyResult(TEL_OK);
}

/* 0x813DFBFE */
TelephonyResult TelMessageThread::PostMessage(const TelEventInfo &event)
{
	if (m_queue != NULL)
		m_queue->Push(event, 1);
	return TelephonyResult(TEL_OK);
}

/* 0x813DFC3C */
TelephonyResult TelMessageThread::PostUrgentMessage(const TelEventInfo &event)
{
	if (m_queue != NULL)
		m_queue->Push(event, 0);
	return TelephonyResult(TEL_OK);
}

/* 0x813DFDEA */
void TelMessageThread::OnReceiveMessage(const TelEventInfo &event)
{
	PostMessage(event);
}

/* 0x813DFC7C */
void TelMessageThread::OnStop()
{
	CancelThread();
}

/* 0x813DFC86 */
int TelMessageThread::Run(void *userArg)
{
	(void)userArg;
	while (!IsThreadCanceled()) {
		TelephonyResult waitRes = m_queue->WaitForMessage();
		if (waitRes.GetCode() == TEL_ERR_NOT_SUPPORTED)
			break;

		for (int priority = 0; priority < 2; ++priority) {
			paf::common::SharedPtr<TelEventInfo> event;
			if (m_queue->Pop(&event, priority) && event.get() != NULL) {
				if (event->GetEventId() == TEL_EVENT_THREAD_START)
					OnStart();
				OnHandleEvent(*event);
				if (event->GetEventId() == TEL_EVENT_THREAD_STOP)
					OnStop();
			}
		}
	}
	return 0;
}

} /* namespace telephony */
