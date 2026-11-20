/*
 * The command that reads statuses by key, each passed to a callback (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_status_info_with_callback.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_WITH_CALLBACK_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_WITH_CALLBACK_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusInfoWithCallback : public AbstractCommand {
public:
	CommandGetStatusInfoWithCallback(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusInfoWithCallback();

	int SetParameter(const SceNpId *npId,
		const NpActivityStatusKey *keys, SceSize count, const int *locale, void *work,
		NpActivityDetailState *state, NpActivityStatusDetailCallback callback, void *arg);
};

#endif
