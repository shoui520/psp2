/*
 * The command that reads takes a "good job" vote back (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_remove_vote_good_job.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_VOTE_GOOD_JOB_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_VOTE_GOOD_JOB_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveVoteGoodJob : public AbstractCommand {
public:
	CommandRemoveVoteGoodJob(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveVoteGoodJob();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		NpActivityRequest *request);
};

#endif
