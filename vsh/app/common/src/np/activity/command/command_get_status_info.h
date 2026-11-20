/*
 * The command that reads statuses by key (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_status_info.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusInfo : public AbstractCommand {
public:
	CommandGetStatusInfo(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusInfo();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *keys,
		SceSize count, const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusInfo *infos);
};

#endif
