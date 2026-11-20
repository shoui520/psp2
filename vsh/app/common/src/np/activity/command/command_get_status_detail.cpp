/*
 * The command that reads one status's details, comments and good jobs.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_status_detail.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetStatusDetail::CommandGetStatusDetail(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_key(NULL), m_param(NULL), m_info(NULL), m_comments(NULL), m_maxComments(0),
	  m_numComments(NULL), m_voters(NULL), m_maxVoters(0), m_numVoters(NULL)
{
	m_api = UTILITY_API_GET_STATUS_DETAIL;
}

CommandGetStatusDetail::~CommandGetStatusDetail()
{
}

int CommandGetStatusDetail::SetParameter(const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityDetailParam *param, NpActivityStatusInfo *info,
	NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
	SceNpId *voters, int maxVoters, SceUInt32 *numVoters)
{
	m_info = info;
	m_npId = npId;
	m_key = key;
	m_param = param;
	m_comments = comments;
	m_maxComments = maxComments;
	m_numComments = numComments;
	m_isVoted = isVoted;
	m_voters = voters;
	m_maxVoters = maxVoters;
	m_numVoters = numVoters;
	if (npId == NULL || key == NULL || param == NULL || info == NULL || comments == NULL ||
	    numComments == NULL || isVoted == NULL || voters == NULL || numVoters == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (!isValidDetailParam(param))
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(info, 0, sizeof(*info));
	*numComments = 0;
	*numVoters = 0;
	*isVoted = 0;
	return 0;
}

int CommandGetStatusDetail::SendRequest()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = checkChatRestriction();
	if (ACTIVITY_FAILED(ret))
		return ret;
	if (ComponentUtility::IsFunctionServer())
		ret = getMiniTicketAndMalloc(&m_ticket, &m_ticketSize);
	else
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
	ret = m_xmlGenerator->CreateGetStatusDetailRequest(m_npId, m_ticket, m_ticketSize, m_key, m_param,
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

int CommandGetStatusDetail::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusDetail(m_info, m_comments, m_maxComments,
		m_numComments, m_isVoted, m_voters, m_maxVoters, m_numVoters, 0, NULL);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusDetailObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandGetStatusDetail::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusDetailObserver(this);
}

SceSize CommandGetStatusDetail::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetStatusDetailRequestSize(m_ticketSize, m_param);
	return AbstractCommand::GetRequestBufferSize();
}
