/*
 * The context manager's base: no component of any kind until a manager makes one (no binary
 * names this file; the names are ours).
 */
#include <stddef.h>
#include "context_manager.h"

AbstractContextManager::AbstractContextManager() : m_context(NULL)
{
}

AbstractContextManager::~AbstractContextManager()
{
}

void AbstractContextManager::DestroyComponent(ActivityObject *component)
{
	delete component;
}
