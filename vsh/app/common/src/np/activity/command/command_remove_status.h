/*
 * The command that reads deletes a status (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_remove_status.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_STATUS_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_STATUS_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveStatus : public AbstractCommand {
public:
	CommandRemoveStatus(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveStatus();

	int SetParameter(const SceNpId *npId, int statusId);
};

#endif
