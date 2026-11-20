/*
 * The command that reads the activity service's static resource: its host and trophy icon URLs
 * (Sony's file: command/command_get_static_resource.cpp's checks; the class name is ours). The
 * reply's text is kept in the resource's data for the signature check.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATIC_RESOURCE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATIC_RESOURCE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"
#include "static_resource.h"

class CommandGetStaticResource : public AbstractCommand, public StaticResourceObserver {
public:
	CommandGetStaticResource(ComponentAuth *auth, ComponentHttp *http,
		ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
		ComponentUtility *utility);
	virtual ~CommandGetStaticResource();

	int SetParameter(StaticResource *resource);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual int ParseStreamData(void *data, SceSize size, int *done);
	virtual void UpdateStaticResource(StaticResource *resource);

private:
	StaticResource *m_resource;
};

#endif
