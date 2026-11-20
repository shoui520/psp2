/*
 * The command that reads the statuses of users or titles in titles, one list per owner (the file
 * name follows command_get_static_resource.cpp; the class name is ours). The reply's handler
 * fills the lists itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_AND_TITLE_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_AND_TITLE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusTimelineByAccountAndTitle : public AbstractCommand, public StatusTimelineObserver {
public:
	CommandGetStatusTimelineByAccountAndTitle(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusTimelineByAccountAndTitle();

	int SetParameter(const SceNpId *npId, const NpActivityOwner *owners,
		SceSize count, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max,
		NpActivityStatusList *lists);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityOwner *m_owners;
	SceSize m_count;
	const char (*m_titleIds)[NP_ACTIVITY_TITLE_ID_SIZE];
	int *m_max;
	NpActivityStatusList *m_lists;
};

#endif
