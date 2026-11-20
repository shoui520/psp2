/*
 * The command that asks whether the statuses of the user, the friends or the titles changed since
 * a time (the file name follows command_get_static_resource.cpp; the class name is ours). The
 * reply's handler fills the updates itself.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_CHECK_STATUS_UPDATE_H
#define NP_ACTIVITY_COMMAND_COMMAND_CHECK_STATUS_UPDATE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandCheckStatusUpdate : public AbstractCommand, public CheckStatusObserver {
public:
	CommandCheckStatusUpdate(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandCheckStatusUpdate();

	int SetParameter(const SceNpId *npId, const SceUInt64 *since,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		NpActivityUpdates *updates);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const SceUInt64 *m_since;
	const char (*m_titleIds)[NP_ACTIVITY_TITLE_ID_SIZE];
	SceSize m_count;
	NpActivityUpdates *m_updates;
};

#endif
