/*
 * The command that posts a comment on a status (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply names the comment.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_ADD_COMMENT_H
#define NP_ACTIVITY_COMMAND_COMMAND_ADD_COMMENT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandAddComment : public AbstractCommand, public StatusIdCommentObserver {
public:
	CommandAddComment(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandAddComment();

	int SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityCommentPost *comment, NpActivityCommentRequest *request);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusIdComment(XmlParserStatusIdComment *comment);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityStatusKey *m_key;
	const NpActivityCommentPost *m_comment;
	NpActivityCommentRequest *m_commentRequest;
	XmlParserStatusIdComment m_reply;
};

#endif
