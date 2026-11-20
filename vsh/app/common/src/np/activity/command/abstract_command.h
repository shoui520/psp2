/*
 * The base of the library's commands, 72 bytes: one request to the activity server, its response
 * read and parsed, run by the command executor (Sony's file: command/abstract_command.cpp, with
 * its getFullTicketAndMalloc and checkChatRestriction checks; ParseStreamData from
 * command_get_static_resource.cpp's check; the other names are ours). A command observes its
 * reply's result through the parser's subject.
 */
#ifndef NP_ACTIVITY_COMMAND_ABSTRACT_COMMAND_H
#define NP_ACTIVITY_COMMAND_ABSTRACT_COMMAND_H

#include <scetypes.h>
#include "../common/activity_object.h"
#include "../component/xml_parser/observer/abstract_xml_parser_subject.h"
#include "../np_activity.h"

class ComponentAuth;
class ComponentHttp;
class ComponentXmlParser;
class ComponentXmlGenerator;
class ComponentUtility;

/* a request's URL and body buffers */
#define ABSTRACT_COMMAND_URL_SIZE	512
#define ABSTRACT_COMMAND_REQUEST_SIZE	5120

/* the info a status or a comment carries, at most */
#define ABSTRACT_COMMAND_INFO_MAX	256

/* a status type or an account setting value a request may not carry */
#define ABSTRACT_COMMAND_VALUE_INVALID	100

/* a reply's result code n is the library's error this + n */
#define ABSTRACT_COMMAND_RESULT_ERROR_BASE	((int)0x80558A00)

class AbstractCommand : public ActivityObject, public ResultObserver {
public:
	AbstractCommand(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~AbstractCommand();

	virtual int SendRequest()
	{
		return -1;
	}
	/* begins the reply's parse */
	virtual int BeginParse()
	{
		return -1;
	}
	virtual int ReceiveData(void **data, SceSize *size, int *done);
	/* *done 1 once the reply's result is in */
	virtual int ParseStreamData(void *data, SceSize size, int *done);
	virtual int EndParse()
	{
		return -1;
	}
	virtual int EndRequest();
	/* the reply's result as the library's error, 0 when it has none */
	virtual int GetResult();
	virtual SceUInt64 GetContentLength();
	/* 0: the command's request cannot go over the connection the context opened */
	virtual int IsAvailable();
	virtual int UpdateResult(XmlParserResult *result);

protected:
	int isValidRange(const NpActivityRange *range);
	int isValidMax(const int *max);
	int isValidDetailParam(const NpActivityDetailParam *param);
	int isValidOwner(const NpActivityOwner *owner);
	int isValidStatusKey(const NpActivityStatusKey *key);
	int isValidCommentKey(const NpActivityCommentKey *key);
	int isValidStatusInfo(const NpActivityStatusInfo *info);
	int isValidCommentPost(const NpActivityCommentPost *comment);
	/* the public and editable values (ABSTRACT_COMMAND_VALUE_INVALID in neither) */
	int isValidAccountSetting(const int *setting);

public:
	virtual SceSize GetUrlBufferSize();
	virtual SceSize GetRequestBufferSize();

protected:

	/* the signed-in user's ticket, base64, in a buffer the command frees */
	int getFullTicketAndMalloc(char **ticket, SceSize *size);
	/* the function server's ticket: not built in (NP_ACTIVITY_ERROR_INTERNAL) */
	int getMiniTicketAndMalloc(char **ticket, SceSize *size);
	int checkChatRestriction();

	ComponentAuth *m_auth;
	ComponentHttp *m_http;
	ComponentXmlParser *m_xmlParser;
	ComponentXmlGenerator *m_xmlGenerator;
	ComponentUtility *m_utility;
	int m_api;			/* UTILITY_API_*: the server the request goes to */
	int m_hasResult;		/* 1 once the reply's result is in */
	XmlParserResult m_result;
	char *m_ticket;
	SceSize m_ticketSize;
	char *m_request;		/* the request's body */
	SceSize m_requestSize;
	SceSize m_requestLength;
	char *m_url;
	SceSize m_urlSize;
	SceSize m_urlLength;
};

#endif
