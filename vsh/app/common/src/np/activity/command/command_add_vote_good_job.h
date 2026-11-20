/*
 * The command that gives a status a good job (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply names the status.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_VOTE_GOOD_JOB_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_VOTE_GOOD_JOB_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddVoteGoodJob : public AbstractCommand, public StatusIdObserver {
public:
	CommandAddVoteGoodJob(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandAddVoteGoodJob();

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
	NpActivityRequest *m_voteRequest;
	XmlParserStatusId m_reply;
};

#endif
