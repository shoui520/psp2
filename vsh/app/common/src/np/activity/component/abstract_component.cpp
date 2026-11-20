/* The base of the library's components (no binary names this file; the name is ours) */
#include "../common/activity_simple_lock.h"
#include "../np_activity.h"
#include "abstract_component.h"

AbstractComponent::AbstractComponent() : m_aborted(0)
{
}

AbstractComponent::~AbstractComponent()
{
}

void AbstractComponent::SetAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 1;
}

void AbstractComponent::ClearAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 0;
}

int AbstractComponent::IsAborted()
{
	ActivitySimpleLock lock(GetMutex());

	return m_aborted;
}

int AbstractComponent::CheckAbort(int result)
{
	ActivitySimpleLock lock(GetMutex());

	if (m_aborted)
		return NP_ACTIVITY_ERROR_ABORTED;
	return result;
}
