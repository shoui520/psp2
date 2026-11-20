/*
 * The command that asks whether statuses changed since a time.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_check_status_update.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandCheckStatusUpdate::CommandCheckStatusUpdate(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_since(NULL), m_titleIds(NULL), m_count(0), m_updates(NULL)
{
	m_api = UTILITY_API_CHECK_STATUS;
}

CommandCheckStatusUpdate::~CommandCheckStatusUpdate()
{
}

int CommandCheckStatusUpdate::SetParameter(const SceNpId *npId, const SceUInt64 *since,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	NpActivityUpdates *updates)
{
	m_count = count;
	m_npId = npId;
	m_since = since;
	m_titleIds = titleIds;
	m_updates = updates;
	if (npId == NULL || since == NULL || updates == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (count != 0 && titleIds == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(updates, 0, sizeof(*updates));
	return 0;
}

int CommandCheckStatusUpdate::SendRequest()
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
	ret = m_xmlGenerator->CreateCheckStatusRequest(m_npId, m_ticket, m_ticketSize,
		(const SceRtcTick *)m_since, m_titleIds, m_count,
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

int CommandCheckStatusUpdate::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginCheckStatus(m_updates);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddCheckStatusObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandCheckStatusUpdate::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveCheckStatusObserver(this);
}

SceSize CommandCheckStatusUpdate::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetCheckStatusRequestSize(m_ticketSize, m_count);
	return AbstractCommand::GetRequestBufferSize();
}
