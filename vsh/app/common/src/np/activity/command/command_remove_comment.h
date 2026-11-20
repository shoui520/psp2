/*
 * The command that reads deletes a comment (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_remove_comment.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_COMMENT_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_COMMENT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveComment : public AbstractCommand {
public:
	CommandRemoveComment(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveComment();

	int SetParameter(const SceNpId *npId, const NpActivityCommentKey *key,
		NpActivityRequest *request);
};

#endif
