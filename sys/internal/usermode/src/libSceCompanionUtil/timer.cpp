/*
 * A timer: a period and its next expiry on the process clock. A timer never started does not
 * expire; one that expired stays expired until it is started again.
 */
#include <kernel/processmgr.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

SceUInt64 CTimer::GetTime(void)
{
	return sceKernelGetProcessTimeWide();
}

void CTimer::Start(SceUInt32 seconds)
{
	m_period = 1000000ULL * seconds;
	m_deadline = sceKernelGetProcessTimeWide() + m_period;
}

bool CTimer::IsExpired(void)
{
	if (m_period == 0)
		return false;
	if (m_deadline == 0)
		return true;
	if (sceKernelGetProcessTimeWide() >= m_deadline) {
		m_deadline = 0;
		return true;
	}
	return false;
}

} /* namespace SceCompanionUtil */
