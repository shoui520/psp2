/*
 * The command that reads some users' timelines of some titles (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_status_timeline_by_account_and_title.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_AND_TITLE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_AND_TITLE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusTimelineByAccountAndTitle : public AbstractCommand {
public:
	CommandGetStatusTimelineByAccountAndTitle(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusTimelineByAccountAndTitle();

	int SetParameter(const SceNpId *npId,
		const NpActivityOwner *owners, SceSize count,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max, NpActivityStatusList *lists);
};

#endif
