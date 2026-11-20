/*
 * The command that reads a user's account setting: whether the statuses are public, and
 * editable (the file name follows command_get_static_resource.cpp; the class name is ours). The
 * reply's handler fills the setting itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_ACCOUNT_INFORMATION_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_ACCOUNT_INFORMATION_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

/* the setting: whether the statuses are public, and editable */
#define ACCOUNT_SETTING_SIZE	(2 * sizeof(int))

class CommandGetAccountInformation : public AbstractCommand, public AccountInformationObserver {
public:
	CommandGetAccountInformation(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetAccountInformation();

	int SetParameter(const SceNpId *npId, int *setting);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	int *m_setting;
};

#endif
