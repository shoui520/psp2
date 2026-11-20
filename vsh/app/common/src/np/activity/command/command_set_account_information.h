/*
 * The command that sets the user's account setting: whether the statuses are public,
 * and editable (the file name follows command_get_static_resource.cpp; the class name is ours).
 * The reply carries its result only.
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

	int SetParameter(const SceNpId *npId, const int *setting);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const int *m_setting;
};

#endif
