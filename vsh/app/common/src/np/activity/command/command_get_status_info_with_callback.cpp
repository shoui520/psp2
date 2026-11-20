/*
 * The command that reads the details of statuses, one callback each.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "command_get_status_info_with_callback.h"
#include "../common/activity_memory.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_generator/component_xml_generator.h"
#include "../component/xml_parser/component_xml_parser.h"

CommandGetStatusInfoWithCallback::CommandGetStatusInfoWithCallback(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: AbstractCommand(auth, http, xmlParser, xmlGenerator, utility), m_npId(NULL),
	  m_keys(NULL), m_count(0), m_locale(NULL), m_work(NULL), m_state(NULL), m_callback(NULL),
	  m_arg(NULL)
{
	m_api = UTILITY_API_GET_STATUS_INFO;
}

CommandGetStatusInfoWithCallback::~CommandGetStatusInfoWithCallback()
{
}

int CommandGetStatusInfoWithCallback::SetParameter(const SceNpId *npId, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, void *work, NpActivityDetailState *state,
	NpActivityStatusDetailCallback callback, void *arg)
{
	SceSize i;

	m_locale = locale;
	m_npId = npId;
	m_keys = keys;
	m_count = count;
	m_work = work;
	m_state = state;
	m_callback = callback;
	m_arg = arg;
	if (npId == NULL || keys == NULL || work == NULL || state == NULL || locale == NULL ||
	    callback == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < count; i++) {
		if (!isValidStatusKey(&keys[i]))
			return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	state->state = 0;
	return 0;
}

int CommandGetStatusInfoWithCallback::SendRequest()
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

int CommandGetStatusInfoWithCallback::BeginParse()
{
	int ret;

	if (m_npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_xmlParser->BeginStatusInfo(&m_state->state, NULL, 0, 1,
		(NpActivityStatusDetail *)m_work);
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

int CommandGetStatusInfoWithCallback::EndParse()
{
	m_xmlParser->EndParse();
	m_xmlParser->RemoveResultObserver(this);
	return m_xmlParser->RemoveStatusInfoObserver(this);
}

void CommandGetStatusInfoWithCallback::UpdateStatusInfo(NpActivityStatusDetail *detail)
{
	if (m_callback != NULL)
		m_callback(detail, m_arg);
}

SceSize CommandGetStatusInfoWithCallback::GetRequestBufferSize()
{
	if (m_xmlGenerator != NULL)
		return m_xmlGenerator->GetGetStatusInfoRequestSize(m_ticketSize, m_count);
	return AbstractCommand::GetRequestBufferSize();
}
