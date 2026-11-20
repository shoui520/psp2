/*
 * The command that reads posts statuses (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_add_status_list.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_STATUS_LIST_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_STATUS_LIST_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddStatusList : public AbstractCommand {
public:
	CommandAddStatusList(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandAddStatusList();

	int SetParameter(const SceNpId *npId, const NpActivityStatusInfo *infos,
		SceSize count, NpActivityPostResult *results);
};

#endif
