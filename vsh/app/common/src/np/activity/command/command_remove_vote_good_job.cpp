/*
 * The command that takes back a good job given to a status.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_remove_vote_good_job.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandRemoveVoteGoodJob::CommandRemoveVoteGoodJob(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_key(NULL), m_removeRequest(NULL)
{
	m_api = UTILITY_API_DELETE_VOTE;
}

CommandRemoveVoteGoodJob::~CommandRemoveVoteGoodJob()
{
}

int CommandRemoveVoteGoodJob::SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
	NpActivityRequest *request)
{
	m_npId = npId;
	m_key = key;
	m_removeRequest = request;
	if (npId == NULL || key == NULL || request == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (!isValidStatusKey(key))
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(request, 0, sizeof(*request));
	return 0;
}

int CommandRemoveVoteGoodJob::SendRequest()
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
	ret = m_xmlGenerator->CreateRemoveVoteGoodJobRequest(m_npId, m_ticket, m_ticketSize, m_key,
		m_request, m_requestSize, &m_requestLength);
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

int CommandRemoveVoteGoodJob::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusId(&m_reply);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusIdObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandRemoveVoteGoodJob::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusIdObserver(this);
}

void CommandRemoveVoteGoodJob::UpdateStatusId(XmlParserStatusId *status)
{
	sceClibMemcpy_safe(&m_removeRequest->updateTime, &status->updateTime,
		sizeof(m_removeRequest->updateTime));
}

SceSize CommandRemoveVoteGoodJob::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetRemoveVoteGoodJobRequestSize(m_ticketSize);
	return AbstractCommand::GetRequestBufferSize();
}
