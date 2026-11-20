/*
 * The command that posts statuses of the signed-in user.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_add_status_list.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandAddStatusList::CommandAddStatusList(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_infos(NULL), m_count(0), m_results(NULL)
{
	m_api = UTILITY_API_SET_MULTIPLE_STATUS;
}

CommandAddStatusList::~CommandAddStatusList()
{
}

int CommandAddStatusList::SetParameter(const SceNpId *npId, const NpActivityStatusInfo *infos,
	SceSize count, NpActivityPostResult *results)
{
	SceSize i;

	m_results = results;
	m_npId = npId;
	m_infos = infos;
	m_count = count;
	if (npId == NULL || infos == NULL || results == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < count; i++) {
		if (!isValidStatusInfo(&infos[i]))
			return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

int CommandAddStatusList::SendRequest()
{
	int ret;

	if (m_npId == NULL || m_infos == NULL || m_count == 0 || m_results == NULL)
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
	ret = m_xmlGenerator->CreateAddStatusListRequest(m_npId, m_ticket, m_ticketSize, m_infos,
		m_count, m_request, m_requestSize, &m_requestLength);
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

int CommandAddStatusList::BeginParse()
{
	int ret;

	if (m_npId == NULL || m_infos == NULL || m_count == 0 || m_results == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginAddStatusList(m_results, m_count);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddStatusListObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandAddStatusList::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusListObserver(this);
}

SceSize CommandAddStatusList::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetAddStatusListRequestSize(m_ticketSize, m_count);
	return AbstractCommand::GetRequestBufferSize();
}
