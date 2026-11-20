/*
 * The base of the library's components: each can be told to abort what it is doing. No binary
 * names these classes; the names are ours (Sony's other base classes are abstract_*, as
 * command/abstract_command.cpp).
 */
#ifndef NP_ACTIVITY_COMPONENT_ABSTRACT_COMPONENT_H
#define NP_ACTIVITY_COMPONENT_ABSTRACT_COMPONENT_H

#include <stddef.h>
#include "../common/activity_mutex.h"

class AbstractComponent : public ActivityObject {
public:
	AbstractComponent();
	virtual ~AbstractComponent();

	virtual void SetAbort();
	virtual void ClearAbort();
	virtual int IsAborted();
	virtual ActivityMutex *GetMutex()
	{
		return NULL;
	}

	/* result, or NP_ACTIVITY_ERROR_ABORTED once the component was told to abort */
	int CheckAbort(int result);

protected:
	int m_aborted;
};

/* a component that shares its context's component mutex */
class Component : public AbstractComponent {
public:
	Component() : m_mutex(NULL), m_utility(NULL)
	{
	}
	virtual ~Component()
	{
	}

	virtual ActivityMutex *GetMutex()
	{
		return m_mutex;
	}

	ActivityMutex *m_mutex;		/* the context's component mutex */
	class ComponentUtility *m_utility;	/* the context's utility */
};

#endif
