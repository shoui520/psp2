/*
 * The base of the library's commands (Sony's file: command/abstract_command.cpp).
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "abstract_command.h"
#include "../common/activity_debug.h"
#include "../common/activity_memory.h"
#include "../component/auth/component_auth.h"
#include "../component/http/component_http.h"
#include "../component/utility/component_utility.h"
#include "../component/xml_parser/component_xml_parser.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "command\\abstract_command.cpp"

AbstractCommand::AbstractCommand(ComponentAuth *auth, ComponentHttp *http,
	ComponentXmlParser *xmlParser, ComponentXmlGenerator *xmlGenerator,
	ComponentUtility *utility)
	: m_auth(auth), m_http(http), m_xmlParser(xmlParser), m_xmlGenerator(xmlGenerator),
	  m_utility(utility), m_api(UTILITY_API_NONE), m_hasResult(0), m_ticket(NULL),
	  m_ticketSize(0), m_request(NULL), m_requestSize(0), m_requestLength(0), m_url(NULL),
	  m_urlSize(0), m_urlLength(0)
{
	sceClibMemset(&m_result, 0, sizeof(m_result));
}

AbstractCommand::~AbstractCommand()
{
	if (m_ticket != NULL) {
		activityFree(m_ticket);
		m_ticket = NULL;
	}
	if (m_request != NULL) {
		activityFree(m_request);
		m_request = NULL;
	}
	if (m_url != NULL) {
		activityFree(m_url);
		m_url = NULL;
	}
}

int AbstractCommand::ReceiveData(void **data, SceSize *size, int *done)
{
	return m_http->ReadData(data, size, done);
}

int AbstractCommand::ParseStreamData(void *data, SceSize size, int *done)
{
	int ret;

	ret = m_xmlParser->Parse((const char *)data, size, done);
	if (!ACTIVITY_FAILED(ret) && done != NULL)
		*done = m_hasResult;
	return ret;
}

int AbstractCommand::EndRequest()
{
	return m_http->EndRequest();
}

int AbstractCommand::GetResult()
{
	if (m_result.result != 0)
		return m_result.result + ABSTRACT_COMMAND_RESULT_ERROR_BASE;
	return 0;
}

SceUInt64 AbstractCommand::GetContentLength()
{
	return m_http->GetContentLength();
}

int AbstractCommand::IsAvailable()
{
	if (ComponentUtility::IsFunctionServer())
		return m_http->GetProtocol() == m_utility->GetProtocol(m_api);
	return m_http->GetSecureProtocol() == m_utility->GetSecureProtocol(m_api);
}

int AbstractCommand::UpdateResult(XmlParserResult *result)
{
	if (result != NULL)
		sceClibMemcpy_safe(&m_result, result, sizeof(m_result));
	m_hasResult = 1;
	return 1;
}

int AbstractCommand::isValidRange(const NpActivityRange *range)
{
	if (range == NULL)
		return 0;
	if (range->start == 0)
		return 0;
	return range->count != 0;
}

int AbstractCommand::isValidMax(const int *max)
{
	return max != NULL && *max != 0;
}

int AbstractCommand::isValidDetailParam(const NpActivityDetailParam *param)
{
	if (param == NULL)
		return 0;
	if (param->comments.start == 0)
		return 0;
	if (param->comments.count == 0)
		return 0;
	if (param->voters.start == 0)
		return 0;
	return param->voters.count != 0;
}

int AbstractCommand::isValidOwner(const NpActivityOwner *owner)
{
	return owner != NULL && (SceUInt32)owner->type < 2;
}

int AbstractCommand::isValidStatusKey(const NpActivityStatusKey *key)
{
	if (key == NULL)
		return 0;
	if (key->owner.type != NP_ACTIVITY_OWNER_USER && key->owner.type != NP_ACTIVITY_OWNER_TITLE)
		return 0;
	if (key->statusId < 0)
		return 0;
	return 1;
}

int AbstractCommand::isValidCommentKey(const NpActivityCommentKey *key)
{
	if (key == NULL)
		return 0;
	if (key->owner.type != NP_ACTIVITY_OWNER_USER && key->owner.type != NP_ACTIVITY_OWNER_TITLE)
		return 0;
	if (key->statusId < 0)
		return 0;
	if (key->commentId < 0)
		return 0;
	return 1;
}

int AbstractCommand::isValidStatusInfo(const NpActivityStatusInfo *info)
{
	if (info == NULL)
		return 0;
	if (info->type == ABSTRACT_COMMAND_VALUE_INVALID)
		return 0;
	return info->infoSize <= ABSTRACT_COMMAND_INFO_MAX;
}

int AbstractCommand::isValidCommentPost(const NpActivityCommentPost *comment)
{
	return comment != NULL && comment->infoSize <= ABSTRACT_COMMAND_INFO_MAX;
}

int AbstractCommand::isValidAccountSetting(const int *setting)
{
	if (setting == NULL)
		return 0;
	if (setting[0] == ABSTRACT_COMMAND_VALUE_INVALID)
		return 0;
	return setting[1] != ABSTRACT_COMMAND_VALUE_INVALID;
}

SceSize AbstractCommand::GetUrlBufferSize()
{
	return ABSTRACT_COMMAND_URL_SIZE;
}

SceSize AbstractCommand::GetRequestBufferSize()
{
	return ABSTRACT_COMMAND_REQUEST_SIZE;
}

#line 529
int AbstractCommand::getFullTicketAndMalloc(char **ticket, SceSize *size)
{
	if (m_auth != NULL)
		return m_auth->GetFullTicketBase64Str(ticket, size);

	ACTIVITY_CHECK("getFullTicketAndMalloc");
	return NP_ACTIVITY_ERROR_INTERNAL;
}

int AbstractCommand::getMiniTicketAndMalloc(char **ticket, SceSize *size)
{
	return NP_ACTIVITY_ERROR_INTERNAL;
}

#line 577
int AbstractCommand::checkChatRestriction()
{
	if (m_auth != NULL)
		return m_auth->CheckChatRestriction();

	ACTIVITY_CHECK("checkChatRestriction");
	return NP_ACTIVITY_ERROR_INTERNAL;
}
