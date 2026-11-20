/*
 * The command that takes back a good job given to a status (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply names the status.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_VOTE_GOOD_JOB_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_VOTE_GOOD_JOB_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveVoteGoodJob : public AbstractCommand, public StatusIdObserver {
public:
	CommandRemoveVoteGoodJob(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveVoteGoodJob();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		NpActivityRequest *request);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusId(XmlParserStatusId *status);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusKey *m_key;
	NpActivityRequest *m_removeRequest;
	XmlParserStatusId m_reply;
};

#endif
