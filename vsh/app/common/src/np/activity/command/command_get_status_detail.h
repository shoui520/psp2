/*
 * The command that reads a status with its comments and votes (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_status_detail.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_DETAIL_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_DETAIL_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusDetail : public AbstractCommand {
public:
	CommandGetStatusDetail(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusDetail();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityDetailParam *param, NpActivityStatusInfo *info,
		NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
		SceNpId *voters, int maxVoters, SceUInt32 *numVoters);
};

#endif
