/*
 * The command that reads the timelines of the user's friends' statuses in titles, one list per
 * title (the file name follows command_get_static_resource.cpp; the class name is ours). The
 * reply's handler fills the lists itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_BY_TITLE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_FRIEND_STATUS_TIMELINE_BY_TITLE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetFriendStatusTimelineByTitle : public AbstractCommand, public StatusTimelineObserver {
public:
	CommandGetFriendStatusTimelineByTitle(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetFriendStatusTimelineByTitle();

	int SetParameter(const SceNpId *npId,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		const NpActivityRange *range, NpActivityStatusList *lists);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusTimeline(NpActivityStatusList *lists);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const char (*m_titleIds)[NP_ACTIVITY_TITLE_ID_SIZE];
	SceSize m_count;
	const NpActivityRange *m_range;
	NpActivityStatusList *m_lists;
};

#endif
