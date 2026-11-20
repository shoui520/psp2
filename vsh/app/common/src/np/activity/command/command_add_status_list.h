/*
 * The command that posts statuses of the signed-in user (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply's handler fills the
 * results itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_STATUS_LIST_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_STATUS_LIST_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddStatusList : public AbstractCommand, public StatusListObserver {
public:
	CommandAddStatusList(ComponentAuth *auth, ComponentHttp *http,
		ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
		ComponentUtility *utility);
	virtual ~CommandAddStatusList();

	int SetParameter(const SceNpId *npId, const NpActivityStatusInfo *infos,
		SceSize count, NpActivityPostResult *results);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusInfo *m_infos;
	SceSize m_count;
	NpActivityPostResult *m_results;
};

#endif
