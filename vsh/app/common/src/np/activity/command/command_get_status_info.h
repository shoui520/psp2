/*
 * The command that reads the details of statuses into an array (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply's handler fills the
 * array itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusInfo : public AbstractCommand, public StatusInfoObserver {
public:
	CommandGetStatusInfo(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusInfo();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *keys, SceSize count,
		const int *locale, NpActivityStatusDetail *details, SceSize max, int *numDetails);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusKey *m_keys;
	SceSize m_count;
	const int *m_locale;
	NpActivityStatusDetail *m_details;
	SceSize m_max;
	int *m_numDetails;
};

#endif
