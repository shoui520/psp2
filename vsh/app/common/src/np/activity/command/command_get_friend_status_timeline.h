/*
 * The command that reads the friends' timeline (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_friend_status_timeline.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetFriendStatusTimeline : public AbstractCommand {
public:
	CommandGetFriendStatusTimeline(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetFriendStatusTimeline();

	int SetParameter(const SceNpId *npId, const NpActivityRange *range,
		NpActivityStatus *statuses, int max, int *count);
};

#endif
