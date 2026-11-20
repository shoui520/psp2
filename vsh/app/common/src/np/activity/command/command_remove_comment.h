/*
 * The command that removes a comment (the file name follows
 * command_get_static_resource.cpp; the class name is ours). The reply names the status.
 */
#ifndef NP_ACTIVITY_COMMAND_COMMAND_REMOVE_COMMENT_H
#define NP_ACTIVITY_COMMAND_COMMAND_REMOVE_COMMENT_H

#include <np/np_common.h>
#include "abstract_command.h"
#include "../np_activity.h"

class CommandRemoveComment : public AbstractCommand, public StatusIdObserver {
public:
	CommandRemoveComment(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~CommandRemoveComment();

	int SetParameter(const SceNpId *npId, const NpActivityCommentKey *key,
		NpActivityRequest *request);

	virtual int SendRequest();
	virtual int BeginParse();
	virtual int EndParse();
	virtual void UpdateStatusId(XmlParserStatusId *status);
	virtual SceSize GetRequestBufferSize();

private:
	const SceNpId *m_npId;
	const NpActivityCommentKey *m_key;
	NpActivityRequest *m_removeRequest;
	XmlParserStatusId m_reply;
};

#endif
