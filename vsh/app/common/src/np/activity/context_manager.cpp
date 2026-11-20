/*
 * The context manager: makes a context and its components, each by the version the library
 * was built with (no binary names this file; the names are ours).
 */
#include <stddef.h>
#include "context_manager.h"
#include "component/context/component_context.h"
#include "component/command_executor/component_command_executor.h"
#include "component/auth/component_auth.h"
#include "component/http/component_http.h"
#include "component/xml_parser/component_xml_parser.h"
#include "component/xml_generator/component_xml_generator.h"
#include "component/utility/component_utility.h"

/* the components, by the version table's types */
#define COMPONENT_TYPE_CONTEXT		0
#define COMPONENT_TYPE_COMMAND_EXECUTOR	1
#define COMPONENT_TYPE_AUTH		2
#define COMPONENT_TYPE_HTTP		3
#define COMPONENT_TYPE_XML_PARSER	4
#define COMPONENT_TYPE_XML_GENERATOR	5
#define COMPONENT_TYPE_UTILITY		6
#define COMPONENT_TYPE_END		7

/* a component's version: 0 none, 1 this build's (the only one there is) */
typedef struct ComponentVersion {
	int type;
	int version;
} ComponentVersion;

static const ComponentVersion s_componentVersions[] = {
	{ COMPONENT_TYPE_CONTEXT, 1 },
	{ COMPONENT_TYPE_COMMAND_EXECUTOR, 1 },
	{ COMPONENT_TYPE_AUTH, 1 },
	{ COMPONENT_TYPE_HTTP, 1 },
	{ COMPONENT_TYPE_XML_PARSER, 1 },
	{ COMPONENT_TYPE_XML_GENERATOR, 1 },
	{ COMPONENT_TYPE_UTILITY, 1 },
	{ COMPONENT_TYPE_END, 1 },
};

static inline int getComponentVersion(int type)
{
	int i;

	for (i = 0; s_componentVersions[i].type != COMPONENT_TYPE_END; i++) {
		if (s_componentVersions[i].type == type)
			return s_componentVersions[i].version;
	}
	return 1;
}

ContextManager::ContextManager()
{
}

ContextManager::~ContextManager()
{
}

/* each component: none for version 0, else this build's */
#define CREATE_COMPONENT(type, Class)				\
	switch (getComponentVersion(type)) {			\
	case 0:							\
		return NULL;					\
	case 1:							\
		return new Class;				\
	default:						\
		return new Class;				\
	}

ComponentContext *ContextManager::CreateContext()
{
	int version = getComponentVersion(COMPONENT_TYPE_CONTEXT);

	if (m_context != NULL)
		return NULL;
	switch (version) {
	case 0:
		return NULL;
	case 1:
		return new ComponentContext;
	default:
		return new ComponentContext;
	}
}

ComponentCommandExecutor *ContextManager::CreateCommandExecutor()
{
	CREATE_COMPONENT(COMPONENT_TYPE_COMMAND_EXECUTOR, ComponentCommandExecutor);
}

ComponentAuth *ContextManager::CreateAuth()
{
	CREATE_COMPONENT(COMPONENT_TYPE_AUTH, ComponentAuth);
}

ComponentHttp *ContextManager::CreateHttp()
{
	CREATE_COMPONENT(COMPONENT_TYPE_HTTP, ComponentHttp);
}

ComponentXmlParser *ContextManager::CreateXmlParser()
{
	CREATE_COMPONENT(COMPONENT_TYPE_XML_PARSER, ComponentXmlParser);
}

ComponentXmlGenerator *ContextManager::CreateXmlGenerator()
{
	CREATE_COMPONENT(COMPONENT_TYPE_XML_GENERATOR, ComponentXmlGenerator);
}

ComponentUtility *ContextManager::CreateUtility()
{
	CREATE_COMPONENT(COMPONENT_TYPE_UTILITY, ComponentUtility);
}
