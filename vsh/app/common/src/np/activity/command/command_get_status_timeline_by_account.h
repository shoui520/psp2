/*
 * The command that reads some users' timelines (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_status_timeline_by_account.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusTimelineByAccount : public AbstractCommand {
public:
	CommandGetStatusTimelineByAccount(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusTimelineByAccount();

	int SetParameter(const SceNpId *npId, const NpActivityOwner *owners,
		SceSize count, int *max, NpActivityStatusList *lists);
};

#endif
