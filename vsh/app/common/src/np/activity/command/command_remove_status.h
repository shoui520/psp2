/*
 * The command that removes one of the user's statuses (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply carries its result only.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_STATUS_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_STATUS_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveStatus : public AbstractCommand {
public:
	CommandRemoveStatus(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveStatus();

	int SetParameter(const SceNpId *npId, int statusId);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	int m_statusId;
};

#endif
