/*
 * The command that reads which timelines have new statuses (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_check_status_update.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_CHECK_STATUS_UPDATE_H
#define NP_ACTIVITY_COMMAND_COMMAND_CHECK_STATUS_UPDATE_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandCheckStatusUpdate : public AbstractCommand {
public:
	CommandCheckStatusUpdate(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandCheckStatusUpdate();

	int SetParameter(const SceNpId *npId, const SceUInt64 *since,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		NpActivityUpdates *updates);
};

#endif
