/*
 * The command that reads comments on a status (the file name follows command_get_static_resource.cpp; the class name is ours). Its fields and
 * methods are written with command_add_comment.cpp.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_COMMENT_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_COMMENT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddComment : public AbstractCommand {
public:
	CommandAddComment(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandAddComment();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityCommentPost *comment, NpActivityCommentRequest *request);
};

#endif
