/*
 * The context manager: makes the library's contexts and the components each context uses, by
 * the versions the library was built with. No binary names these classes; the names are ours
 * (the lock manager's mutex is "SceNpActivityNetCtxManager").
 */
#ifndef NP_ACTIVITY_CONTEXT_MANAGER_H
#define NP_ACTIVITY_CONTEXT_MANAGER_H

#include <stddef.h>
#include "common/activity_object.h"

class ComponentContext;
class ComponentCommandExecutor;
class ComponentAuth;
class ComponentHttp;
class ComponentXmlParser;
class ComponentXmlGenerator;
class ComponentUtility;

class AbstractContextManager : public ActivityObject {
public:
	AbstractContextManager();
	virtual ~AbstractContextManager();

	virtual ComponentContext *CreateContext()
	{
		return NULL;
	}
	virtual ComponentCommandExecutor *CreateCommandExecutor()
	{
		return NULL;
	}
	virtual ComponentAuth *CreateAuth()
	{
		return NULL;
	}
	virtual ComponentHttp *CreateHttp()
	{
		return NULL;
	}
	virtual ComponentXmlParser *CreateXmlParser()
	{
		return NULL;
	}
	virtual ComponentXmlGenerator *CreateXmlGenerator()
	{
		return NULL;
	}
	virtual ComponentUtility *CreateUtility()
	{
		return NULL;
	}

	void DestroyComponent(ActivityObject *component);

protected:
	ComponentContext *m_context;	/* never set: CreateContext makes a context while it is NULL */
};

class ContextManager : public AbstractContextManager {
public:
	ContextManager();
	virtual ~ContextManager();

	virtual ComponentContext *CreateContext();
	virtual ComponentCommandExecutor *CreateCommandExecutor();
	virtual ComponentAuth *CreateAuth();
	virtual ComponentHttp *CreateHttp();
	virtual ComponentXmlParser *CreateXmlParser();
	virtual ComponentXmlGenerator *CreateXmlGenerator();
	virtual ComponentUtility *CreateUtility();
};

#endif
