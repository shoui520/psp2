/*
 * The command that reads the timelines of the user's friends' statuses in titles.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_friend_status_timeline_by_title.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetFriendStatusTimelineByTitle::CommandGetFriendStatusTimelineByTitle(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_titleIds(NULL), m_count(0), m_range(NULL), m_lists(NULL)
{
	m_api = UTILITY_API_GET_TIMELINE_BY_TITLE;
}

CommandGetFriendStatusTimelineByTitle::~CommandGetFriendStatusTimelineByTitle()
{
}

int CommandGetFriendStatusTimelineByTitle::SetParameter(const SceNpId *npId,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	const NpActivityRange *range, NpActivityStatusList *lists)
{
	m_count = count;
	m_npId = npId;
	m_titleIds = titleIds;
	m_range = range;
	m_lists = lists;
	if (npId == NULL || titleIds == NULL || range == NULL || lists == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (isValidRange(range))
		return 0;
	return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
}

int CommandGetFriendStatusTimelineByTitle::SendRequest()
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
	ret = m_xmlGenerator->CreateGetFriendStatusTimelineByTitleRequest(m_npId, m_ticket, m_ticketSize,
		m_titleIds, m_count, m_range,
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

int CommandGetFriendStatusTimelineByTitle::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusTimeline(m_lists, m_count);
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

int CommandGetFriendStatusTimelineByTitle::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusTimelineObserver(this);
}

void CommandGetFriendStatusTimelineByTitle::UpdateStatusTimeline(NpActivityStatusList *lists)
{
}

SceSize CommandGetFriendStatusTimelineByTitle::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetFriendStatusTimelineByTitleRequestSize(m_ticketSize, m_count,
			m_range);
	return AbstractCommand::GetRequestBufferSize();
}
