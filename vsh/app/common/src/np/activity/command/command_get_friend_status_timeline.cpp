/*
 * The command that reads the timeline of the user's friends' statuses.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_friend_status_timeline.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetFriendStatusTimeline::CommandGetFriendStatusTimeline(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_range(NULL), m_statuses(NULL), m_max(0), m_count(NULL)
{
	m_api = UTILITY_API_GET_TIMELINE;
}

CommandGetFriendStatusTimeline::~CommandGetFriendStatusTimeline()
{
}

int CommandGetFriendStatusTimeline::SetParameter(const SceNpId *npId, const NpActivityRange *range,
	NpActivityStatus *statuses, int max, int *count)
{
	m_max = max;
	m_npId = npId;
	m_range = range;
	m_statuses = statuses;
	m_count = count;
	if (npId == NULL || range == NULL || statuses == NULL || count == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (!isValidRange(range))
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	*count = 0;
	return 0;
}

int CommandGetFriendStatusTimeline::SendRequest()
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
	ret = m_xmlGenerator->CreateGetFriendStatusTimelineRequest(m_npId, m_ticket, m_ticketSize, m_range,
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

int CommandGetFriendStatusTimeline::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	m_list.statuses = m_statuses;
	m_list.count = 0;
	m_list.max = m_max;
	ret = m_xmlParser->BeginStatusTimeline(&m_list, 1);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusTimelineObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandGetFriendStatusTimeline::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusTimelineObserver(this);
}

void CommandGetFriendStatusTimeline::UpdateStatusTimeline(NpActivityStatusList *lists)
{
	*m_count = lists->count;
}

SceSize CommandGetFriendStatusTimeline::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetFriendStatusTimelineRequestSize(m_ticketSize, m_range);
	return AbstractCommand::GetRequestBufferSize();
}
