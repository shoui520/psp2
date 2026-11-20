/*
 * The command that reads the timeline of the user's friends' statuses (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply fills one list.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetFriendStatusTimeline : public AbstractCommand, public StatusTimelineObserver {
public:
	CommandGetFriendStatusTimeline(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetFriendStatusTimeline();

	int SetParameter(const SceNpId *npId, const NpActivityRange *range,
		NpActivityStatus *statuses, int max, int *count);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusTimeline(NpActivityStatusList *lists);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityRange *m_range;
	NpActivityStatus *m_statuses;
	int m_max;
	int *m_count;
	NpActivityStatusList m_list;	/* the reply's one list, over the caller's statuses */
};

#endif
