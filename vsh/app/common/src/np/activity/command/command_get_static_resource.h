/*
 * The command that reads the activity service's static resource: its host and trophy icon URLs (Sony's file: command/command_get_static_resource.cpp's checks). Its fields and
 * methods are written with command_get_static_resource.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATIC_RESOURCE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATIC_RESOURCE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"
#include "static_resource.h"

class CommandGetStaticResource : public AbstractCommand {
public:
	CommandGetStaticResource(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStaticResource();

	int SetParameter(StaticResource *resource);
};

#endif
