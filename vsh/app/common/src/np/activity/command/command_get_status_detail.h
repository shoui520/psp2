/*
 * The command that reads one status's details, its comments and the users who gave it a good
 * job (the file name follows command_get_static_resource.cpp; the class name is ours). The
 * reply's handler fills them itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_DETAIL_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_DETAIL_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusDetail : public AbstractCommand, public StatusDetailObserver {
public:
	CommandGetStatusDetail(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusDetail();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityDetailParam *param, NpActivityStatusInfo *info,
		NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
		SceNpId *voters, int maxVoters, SceUInt32 *numVoters);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusKey *m_key;
	const NpActivityDetailParam *m_param;
	NpActivityStatusInfo *m_info;
	NpActivityComment *m_comments;
	int m_maxComments;
	int *m_numComments;
	int *m_isVoted;
	SceNpId *m_voters;
	int m_maxVoters;
	SceUInt32 *m_numVoters;
};

#endif
