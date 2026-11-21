/*
 * Which thread is inside the system callback: the event thread owns the lock, and marks when it
 * calls the callback, so that the exports refuse to stop the library from inside it.
 */
#include <kernel/threadmgr.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

void CCallbackLock::Init(void)
{
	m_threadId = -1;
	m_inCallback = false;
}

void CCallbackLock::Term(void)
{
}

void CCallbackLock::SetOwner(void)
{
	m_threadId = sceKernelGetThreadId();
}

void CCallbackLock::ClearOwner(void)
{
	m_threadId = -1;
}

void CCallbackLock::Enter(void)
{
	m_inCallback = true;
}

void CCallbackLock::Leave(void)
{
	m_inCallback = false;
}

bool CCallbackLock::IsInCallback(void)
{
	if (m_threadId == -1)
		return false;
	if (m_inCallback)
		return sceKernelGetThreadId() == m_threadId;
	return false;
}

} /* namespace SceCompanionUtil */
