/*
 * tel_bbmc_sync.cpp: TelBbmcSync request/response condition-variable helper
 * (3.65-CEX: 0x813D6A74..0x813D6B1F).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

/* 0x813D6A74 */
TelBbmcSync::TelBbmcSync()
	: m_mutex("SceTelBbmcSync", 0),
	  m_cond(m_mutex, "SceTelBbmcSync", 0),
	  m_activeCount(0),
	  m_waitMask(0),
	  m_matchedMask(0),
	  m_lastEventMask(0),
	  m_lastStatus(0)
{
}

/* 0x813D6AA8 */
TelBbmcSync::~TelBbmcSync()
{
}

/* 0x813D6AC0 */
bool TelBbmcSync::Wait(int timeoutUsec)
{
	bool timedOut = m_cond.Wait(timeoutUsec / 1000);
	m_waitMask = 0;
	return timedOut;
}

/* 0x813D6AE4 */
void TelBbmcSync::Signal(SceUInt32 eventMask, int status)
{
	m_mutex.Lock();
	SceUInt32 matched = m_waitMask & eventMask;
	m_lastEventMask = eventMask;
	if (matched == 0) {
		m_mutex.Unlock();
		return;
	}
	m_matchedMask = matched;
	if (status != 0)
		m_lastStatus = status;
	m_mutex.Unlock();
	m_cond.NotifyAll();
}

/* 0x813D6B16 */
void TelBbmcSync::BeginWait(SceUInt32 waitMask)
{
	m_waitMask = waitMask;
	m_lastStatus = 0;
}

} /* namespace telephony */
