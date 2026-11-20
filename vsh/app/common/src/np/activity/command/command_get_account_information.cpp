/*
 * The command that reads a user's account setting.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_account_information.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetAccountInformation::CommandGetAccountInformation(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_setting(NULL)
{
	m_api = UTILITY_API_GET_ACCOUNT_SETTING;
}

CommandGetAccountInformation::~CommandGetAccountInformation()
{
}

int CommandGetAccountInformation::SetParameter(const SceNpId *npId, int *setting)
{
	m_npId = npId;
	m_setting = setting;
	if (npId == NULL || setting == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibMemset(setting, 0, ACCOUNT_SETTING_SIZE);
	return 0;
}

int CommandGetAccountInformation::SendRequest()
{
	int ret;

	if (m_npId == NULL || m_setting == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
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
	ret = m_xmlGenerator->CreateGetAccountSettingRequest(m_npId, m_ticket, m_ticketSize,
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

int CommandGetAccountInformation::BeginParse()
{
	int ret;

	if (m_npId == NULL || m_setting == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginAccountInformation(m_setting);
	if (!ACTIVITY_FAILED(ret)) {
		ret = m_xmlParser->AddResultObserver(this);
		if (ret < 0) {
			m_xmlParser->EndParse();
			return ret;
		}
		ret = m_xmlParser->AddAccountInformationObserver(this);
		if (ret < 0) {
			m_xmlParser->RemoveResultObserver(this);
			m_xmlParser->EndParse();
		}
	}
	return ret;
}

int CommandGetAccountInformation::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveAccountInformationObserver(this);
}

SceSize CommandGetAccountInformation::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetAccountSettingRequestSize(m_ticketSize);
	return AbstractCommand::GetRequestBufferSize();
}
