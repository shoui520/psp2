/*
 * The command that reads a user's activity account setting (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_get_account_information.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_ACCOUNT_INFORMATION_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_ACCOUNT_INFORMATION_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetAccountInformation : public AbstractCommand {
public:
	CommandGetAccountInformation(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetAccountInformation();

	int SetParameter(const SceNpId *npId, void *info);
};

#endif
