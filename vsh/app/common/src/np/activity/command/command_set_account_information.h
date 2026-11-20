/*
 * The command that reads sets a user's activity account setting (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_set_account_information.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_SET_ACCOUNT_INFORMATION_H
#define NP_ACTIVITY_COMMAND_COMMAND_SET_ACCOUNT_INFORMATION_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandSetAccountInformation : public AbstractCommand {
public:
	CommandSetAccountInformation(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandSetAccountInformation();

	int SetParameter(const SceNpId *npId, const void *info);
};

#endif
