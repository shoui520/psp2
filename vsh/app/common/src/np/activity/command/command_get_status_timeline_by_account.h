/*
 * The command that reads the statuses of users or titles, one list per owner (the file name
 * follows command_get_static_resource.cpp; the class name is ours). The reply's handler fills
 * the lists itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_TIMELINE_BY_ACCOUNT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusTimelineByAccount : public AbstractCommand, public StatusTimelineObserver {
public:
	CommandGetStatusTimelineByAccount(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusTimelineByAccount();

	int SetParameter(const SceNpId *npId, const NpActivityOwner *owners,
		SceSize count, int *max, NpActivityStatusList *lists);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityOwner *m_owners;
	SceSize m_count;
	int *m_max;
	NpActivityStatusList *m_lists;
};

#endif
