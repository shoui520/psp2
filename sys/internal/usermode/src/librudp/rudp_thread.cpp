/*
 * rudp_thread.cpp: worker thread base class, global RudpStatus, and PRX module entrypoints.
 */
#include "rudp_internal.h"

RudpThread::RudpThread()
	: m_stackSize(4096),
	  m_priority(0x10000100),
	  m_affinityMask(0),
	  m_reservedThread(0),
	  m_running(false),
	  m_started(false)
{
	rudpMutexInit(&m_mutex, 0);
	rudpCondInit(&m_cond, &m_mutex);
}

RudpThread::~RudpThread()
{
	rudpCondTerm(&m_cond);
	rudpMutexTerm(&m_mutex);
}

RudpResult RudpThread::setStackSize(uint32_t stackSize)
{
	if (stackSize < 4096) {
		stackSize = 4096;
	}
	m_stackSize = stackSize;
	return RudpResult(0);
}

RudpResult RudpThread::setPriority(int priority)
{
	m_priority = priority;
	return RudpResult(0);
}

RudpResult RudpThread::setAffinityMask(uint64_t affinityMask)
{
	m_affinityMask = affinityMask;
	return RudpResult(0);
}

RudpResult RudpThread::start()
{
	RudpMutexLock lock(&m_mutex);
	RudpResult res;
	m_started = false;
	if (rudpThreadCreate(&m_threadId, threadMain, this, m_stackSize, m_priority, 0, m_affinityMask) >= 0) {
		while (!m_started) {
			if (rudpCondTimedWait(&m_cond, &m_mutex, 1000000ULL) == RUDP_OSAL_ETIMEDOUT) {
				break;
			}
		}
		res = m_running ? 0 : SCE_RUDP_ERROR_THREAD;
	} else {
		res = SCE_RUDP_ERROR_THREAD;
	}
	return res;
}

RudpResult RudpThread::stop()
{
	RudpMutexLock lock(&m_mutex);
	RudpResult res;
	int exitStatus = 0;

	if (m_running) {
		m_running = false;
		m_started = false;
		rudpMutexUnlock(&m_mutex);
		res = rudpThreadJoin(m_threadId, &exitStatus);
		rudpMutexLock(&m_mutex);
	}
	return res;
}

int RudpThread::threadMain(void *arg)
{
	RudpThread *self = (RudpThread *)arg;
	rudpMutexLock(&self->m_mutex);
	self->m_started = true;
	self->m_running = true;
	rudpCondSignal(&self->m_cond);
	rudpMutexUnlock(&self->m_mutex);
	while (self->m_running) {
		self->run();
	}
	return 0;
}

RudpStatus::RudpStatus()
{
	rudpMemset(&m_data, 0, sizeof(m_data));
}

RudpStatus::~RudpStatus()
{
}

void RudpStatus::noop()
{
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	rudpSetComponentName("SceLibrudp");
	int ret = rudpOsalInit(0);
	if (ret >= 0) {
		ret = rudpMutexInit(&g_rudpMutex, 1);
		if (ret < 0) {
			rudpOsalTerm();
		}
	}
	return (ret >= 0) ? 0 : -1;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	rudpMutexTerm(&g_rudpMutex);
	rudpOsalTerm();
	return 0;
}
