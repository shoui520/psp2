/*
 * The command that reads the friends' timeline of some titles (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_friend_status_timeline_by_title.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_BY_TITLE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_BY_TITLE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetFriendStatusTimelineByTitle : public AbstractCommand {
public:
	CommandGetFriendStatusTimelineByTitle(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetFriendStatusTimelineByTitle();

	int SetParameter(const SceNpId *npId,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		const NpActivityRange *range, NpActivityStatusList *lists);
};

#endif
