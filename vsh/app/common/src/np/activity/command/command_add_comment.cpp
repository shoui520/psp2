/*
 * The command that posts a comment on a status.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_add_comment.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandAddComment::CommandAddComment(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_key(NULL), m_comment(NULL), m_commentRequest(NULL)
{
	m_api = UTILITY_API_SET_COMMENT;
}

CommandAddComment::~CommandAddComment()
{
}

int CommandAddComment::SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityCommentPost *comment, NpActivityCommentRequest *request)
{
	m_commentRequest = request;
	m_npId = npId;
	m_key = key;
	m_comment = comment;
	if (npId == NULL || key == NULL || comment == NULL || request == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (!isValidStatusKey(key))
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (!isValidCommentPost(comment))
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(request, 0, sizeof(*request));
	return 0;
}

int CommandAddComment::SendRequest()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = checkChatRestriction();
	if (ACTIVITY_FAILED(ret))
		return ret;
	ret = getFullTicketAndMalloc(&m_ticket, &m_ticketSize);
	if (ACTIVITY_FAILED(ret))
		return ret;

	m_requestSize = GetRequestBufferSize();
	m_request = (char *)activityMalloc(m_requestSize);
	if (m_request == NULL) {
		activityFree(m_ticket);
		m_ticket = NULL;
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(m_request, 0, m_requestSize);
	ret = m_xmlGenerator->CreateAddCommentRequest(m_npId, m_ticket, m_ticketSize, m_key,
		m_comment, m_request, m_requestSize, &m_requestLength);
	if (ACTIVITY_FAILED(ret)) {
		activityFree(m_ticket);
		m_ticket = NULL;
		activityFree(m_request);
		m_request = NULL;
		return ret;
	}

	m_urlSize = GetUrlBufferSize();
	m_url = (char *)activityMalloc(m_urlSize);
	if (m_url == NULL) {
		activityFree(m_ticket);
		m_ticket = NULL;
		activityFree(m_request);
		m_request = NULL;
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(m_url, 0, m_urlSize);
	m_urlLength = sceClibSnprintf(m_url, m_urlSize, "%s%s%s", m_utility->GetScheme(m_api),
		m_utility->GetHost(m_api), m_utility->GetPath(m_api));
	ret = m_http->SendRequest(HTTP_METHOD_POST, m_url, m_urlLength, m_request,
		m_requestLength);
	activityFree(m_ticket);
	m_ticket = NULL;
	activityFree(m_request);
	m_request = NULL;
	activityFree(m_url);
	m_url = NULL;
	return ret;
}

int CommandAddComment::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusIdComment(&m_reply);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusIdCommentObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandAddComment::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusIdCommentObserver(this);
}

void CommandAddComment::UpdateStatusIdComment(XmlParserStatusIdComment *comment)
{
	m_commentRequest->commentId = comment->commentId;
	sceClibMemcpy_safe(&m_commentRequest->creationTime, &comment->creationTime,
		sizeof(m_commentRequest->creationTime));
	sceClibMemcpy_safe(&m_commentRequest->updateTime, &comment->updateTime,
		sizeof(m_commentRequest->updateTime));
}

SceSize CommandAddComment::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetAddCommentRequestSize(m_ticketSize);
	return AbstractCommand::GetRequestBufferSize();
}
