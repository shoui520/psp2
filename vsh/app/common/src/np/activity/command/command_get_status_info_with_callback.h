/*
 * The command that reads the details of statuses one at a time, handing each to a callback (the
 * file name follows command_get_static_resource.cpp; the class name is ours). The state's count
 * is the statuses read so far; the work area holds the status being read.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_WITH_CALLBACK_H
#define NP_ACTIVITY_COMMAND_COMMAND_GET_STATUS_INFO_WITH_CALLBACK_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandGetStatusInfoWithCallback : public AbstractCommand, public StatusInfoObserver {
public:
	CommandGetStatusInfoWithCallback(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandGetStatusInfoWithCallback();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *keys,
		SceSize count, const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusDetailCallback callback, void *arg);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusInfo(NpActivityStatusDetail *detail);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusKey *m_keys;
	SceSize m_count;
	const int *m_locale;
	void *m_work;			/* one NpActivityStatusDetail */
	NpActivityDetailState *m_state;
	NpActivityStatusDetailCallback m_callback;
	void *m_arg;
};

#endif
