/*
 * The command that reads votes "good job" on a status (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_add_vote_good_job.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_VOTE_GOOD_JOB_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_VOTE_GOOD_JOB_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddVoteGoodJob : public AbstractCommand {
public:
	CommandAddVoteGoodJob(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandAddVoteGoodJob();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		NpActivityRequest *request);
};

#endif
