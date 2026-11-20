/*
 * The command that reads the details of statuses into an array.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_status_info.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetStatusInfo::CommandGetStatusInfo(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_keys(NULL), m_count(0), m_locale(NULL), m_details(NULL), m_max(0), m_numDetails(NULL)
{
	m_api = UTILITY_API_GET_STATUS_INFO;
}

CommandGetStatusInfo::~CommandGetStatusInfo()
{
}

int CommandGetStatusInfo::SetParameter(const SceNpId *npId, const NpActivityStatusKey *keys, SceSize count,
	const int *locale, NpActivityStatusDetail *details, SceSize max, int *numDetails)
{
	SceSize i;

	m_locale = locale;
	m_npId = npId;
	m_keys = keys;
	m_count = count;
	m_details = details;
	m_max = max;
	m_numDetails = numDetails;
	if (npId == NULL || keys == NULL || details == NULL || numDetails == NULL || locale == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < count; i++) {
		if (!isValidStatusKey(&keys[i]))
			return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	*numDetails = 0;
	return 0;
}

int CommandGetStatusInfo::SendRequest()
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
	ret = m_xmlGenerator->CreateGetStatusInfoRequest(m_npId, m_ticket, m_ticketSize, m_keys, m_count, m_locale,
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

int CommandGetStatusInfo::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusInfo(m_numDetails, m_details, m_max, 0, NULL);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusInfoObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandGetStatusInfo::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusInfoObserver(this);
}

SceSize CommandGetStatusInfo::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetStatusInfoRequestSize(m_ticketSize, m_count);
	return AbstractCommand::GetRequestBufferSize();
}
