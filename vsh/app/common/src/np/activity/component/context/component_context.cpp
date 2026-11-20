/*
 * A context: its components, its connection to the activity server and the commands run on it
 * (Sony's file: its checks name component/context/component_context.cpp)
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../common/activity_debug.h"
#include "../../common/activity_memory.h"
#include "../../common/activity_mutex.h"
#include "../../common/activity_simple_lock.h"
#include "../../context_manager.h"
#include "../../np_activity.h"
#include "../auth/component_auth.h"
#include "../command_executor/component_command_executor.h"
#include "../http/component_http.h"
#include "../utility/component_utility.h"
#include "../xml_generator/component_xml_generator.h"
#include "../xml_parser/component_xml_parser.h"
#include "../../command/command_add_comment.h"
#include "../../command/command_add_status_list.h"
#include "../../command/command_add_vote_good_job.h"
#include "../../command/command_check_status_update.h"
#include "../../command/command_get_account_information.h"
#include "../../command/command_get_friend_status_timeline.h"
#include "../../command/command_get_friend_status_timeline_by_title.h"
#include "../../command/command_get_static_resource.h"
#include "../../command/command_get_status_detail.h"
#include "../../command/command_get_status_info.h"
#include "../../command/command_get_status_info_with_callback.h"
#include "../../command/command_get_status_timeline_by_account.h"
#include "../../command/command_get_status_timeline_by_account_and_title.h"
#include "../../command/command_remove_comment.h"
#include "../../command/command_remove_status.h"
#include "../../command/command_remove_vote_good_job.h"
#include "../../command/command_set_account_information.h"
#include "component_context.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\context\\component_context.cpp"

ComponentContext::ComponentContext() :
	m_next(NULL), m_prev(NULL), m_state(CONTEXT_STATE_IDLE), m_staticResource(NULL),
	m_staticResourceSize(0), m_componentMutex(NULL), m_manager(NULL), m_executor(NULL),
	m_auth(NULL), m_http(NULL), m_xmlParser(NULL), m_xmlGenerator(NULL), m_utility(NULL)
{
}

void ComponentContext::FreeStaticResourceBuffer()
{
	if (m_staticResource != NULL) {
		activityFree(m_staticResource);
		m_staticResource = NULL;
		m_staticResourceSize = 0;
	}
}

ComponentContext::~ComponentContext()
{
	FreeStaticResourceBuffer();
}

int ComponentContext::Initialize()
{
	ActivityMutex *mutex;

	mutex = new ActivityMutex("SceNpActivityNetContext");
	if (mutex == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	m_mutex = mutex;

	m_componentMutex = new ActivityMutex("SceNpActivityNetComponent");
	if (m_componentMutex == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;

	m_utility = m_manager->CreateUtility();
	if (m_utility == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	m_utility->m_mutex = m_componentMutex;
	return 0;
}

int ComponentContext::Finalize()
{
	int ret = 0;

	{
		ActivitySimpleLock lock(GetMutex());

		switch (m_state) {
		case CONTEXT_STATE_IDLE:
			break;
		case CONTEXT_STATE_OPEN:
			ret = CloseConnection();
			break;
		case CONTEXT_STATE_OPENING:
		case CONTEXT_STATE_CLOSING:
		case CONTEXT_STATE_RUNNING:
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		default:
#line 151
			ACTIVITY_CHECK("Finalize");
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		}
		m_state = CONTEXT_STATE_IDLE;
	}

	m_manager->DestroyComponent(m_utility);
	m_utility = NULL;
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
	if (m_componentMutex != NULL) {
		delete m_componentMutex;
		m_componentMutex = NULL;
	}
	return ret;
}

int ComponentContext::AllocStaticResourceBuffer()
{
	if (m_staticResource == NULL) {
		m_staticResource = activityMalloc(CONTEXT_STATIC_RESOURCE_BUFFER_SIZE);
		if (m_staticResource == NULL)
			return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
		m_staticResourceSize = CONTEXT_STATIC_RESOURCE_BUFFER_SIZE;
	}
	return 0;
}

int ComponentContext::ExecuteAbort()
{
	ActivitySimpleLock lock(GetMutex());

	switch (m_state) {
	case CONTEXT_STATE_IDLE:
	case CONTEXT_STATE_CLOSING:
		break;
	case CONTEXT_STATE_OPENING:
	case CONTEXT_STATE_OPEN:
	case CONTEXT_STATE_RUNNING:
		SetAbort();
		if (m_executor != NULL)
			m_executor->SetAbort();
		if (m_auth != NULL)
			m_auth->SetAbort();
		if (m_http != NULL)
			m_http->SetAbort();
		if (m_xmlParser != NULL)
			m_xmlParser->SetAbort();
		if (m_xmlGenerator != NULL)
			m_xmlGenerator->SetAbort();
		if (m_utility != NULL)
			m_utility->SetAbort();
		break;
	default:
#line 234
		ACTIVITY_CHECK("ExecuteAbort");
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	}
	return 0;
}

inline void ComponentContext::setState(int state)
{
	ActivitySimpleLock lock(GetMutex());

	m_state = state;
}

/* the components a connection uses: made by OpenConnection */
inline void ComponentContext::destroyComponents()
{
	m_manager->DestroyComponent(m_executor);
	m_executor = NULL;
	m_manager->DestroyComponent(m_auth);
	m_auth = NULL;
	m_manager->DestroyComponent(m_http);
	m_http = NULL;
	m_manager->DestroyComponent(m_xmlParser);
	m_xmlParser = NULL;
	m_manager->DestroyComponent(m_xmlGenerator);
	m_xmlGenerator = NULL;
}

void ComponentContext::clearAbortAll()
{
	ClearAbort();
	if (m_utility != NULL)
		m_utility->ClearAbort();
	if (m_executor != NULL)
		m_executor->ClearAbort();
	if (m_auth != NULL)
		m_auth->ClearAbort();
	if (m_http != NULL)
		m_http->ClearAbort();
	if (m_xmlParser != NULL)
		m_xmlParser->ClearAbort();
	if (m_xmlGenerator != NULL)
		m_xmlGenerator->ClearAbort();
}

/*
 * The end of a command, with the context's mutex held: the context is open again, and a
 * command that failed after an abort fails with NP_ACTIVITY_ERROR_ABORTED, unless the server
 * gave its own error.
 */
inline int ComponentContext::endCommandLocked(int result)
{
	if (m_state == CONTEXT_STATE_RUNNING)
		m_state = CONTEXT_STATE_OPEN;
	if (IsAborted() && ACTIVITY_FAILED(result) && result != NP_ACTIVITY_ERROR_ABORTED &&
	    ((result & 0x80558900) != 0x80558900 || (result & 0x80558A00) != 0x80558A00))
		result = NP_ACTIVITY_ERROR_ABORTED;
	clearAbortAll();
	return result;
}

__attribute__((noinline)) int ComponentContext::endCommand(int result)
{
	ActivitySimpleLock lock(GetMutex());

	return endCommandLocked(result);
}

int ComponentContext::getStaticResource()
{
	ComponentHttp *http;
	CommandGetStaticResource *command;
	StaticResource *resource;
	int ret;

	m_http->SetSecureProtocol(UTILITY_PROTOCOL_HTTP);
	http = m_http;
	ret = http->Open(UTILITY_PROTOCOL_HTTP,
		m_utility->GetUserAgent(UTILITY_API_STATIC_RESOURCE),
		m_utility->GetHost(UTILITY_API_STATIC_RESOURCE),
		m_utility->GetPort(UTILITY_PROTOCOL_HTTP), 0);
	if (ACTIVITY_FAILED(ret)) {
		m_http->Close();
		return ret;
	}

	command = new CommandGetStaticResource(m_auth, m_http, m_xmlParser, m_xmlGenerator,
		m_utility);
	if (command == NULL) {
		m_http->Close();
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	resource = (StaticResource *)activityMalloc(sizeof(*resource));
	if (resource == NULL) {
		delete command;
		m_http->Close();
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(resource, 0, sizeof(*resource));

	ret = command->SetParameter(resource);
	if (!ACTIVITY_FAILED(ret)) {
		m_executor->SetCommand(command);
		ret = m_executor->Execute();
	}
	if (ACTIVITY_FAILED(ret)) {
		delete command;
		activityFree(resource);
		m_http->Close();
		return ret;
	}
	m_executor->ClearCommand();
	delete command;

	ret = m_http->Close();
	if (!ACTIVITY_FAILED(ret)) {
		if (!m_utility->IsValidStaticResource(resource->data, resource->dataSize,
		    resource->signature, sceClibStrnlen(resource->signature,
		    sizeof(resource->signature)))) {
			activityFree(resource);
			return NP_ACTIVITY_ERROR_INVALID_STATIC_RESOURCE;
		}

		switch (resource->status) {
		case STATIC_RESOURCE_STATUS_ALIVE:
			ret = m_utility->SetHost(resource->activityFqdn,
				sceClibStrnlen(resource->activityFqdn,
				sizeof(resource->activityFqdn)));
			if (!ACTIVITY_FAILED(ret))
				ret = m_utility->SetTrophyIconUrls(resource->trophyIconBaseUrl,
					resource->trophyIconBronze, resource->trophyIconSilver,
					resource->trophyIconGold, resource->trophyIconHidden,
					resource->trophyIconPlatinum);
			break;
		case STATIC_RESOURCE_STATUS_MAINTENANCE:
			ret = NP_ACTIVITY_ERROR_SERVICE_MAINTENANCE;
			break;
		case STATIC_RESOURCE_STATUS_END:
			ret = NP_ACTIVITY_ERROR_SERVICE_END;
			break;
		case STATIC_RESOURCE_STATUS_BEFORE:
			ret = NP_ACTIVITY_ERROR_SERVICE_BEFORE;
			break;
		default:
#line 2262
			ACTIVITY_CHECK("getStaticResource");
			ret = NP_ACTIVITY_ERROR_SERVICE_MAINTENANCE;
			break;
		}
	}
	activityFree(resource);
	return ret;
}

int ComponentContext::OpenConnection(int mode)
{
	ComponentHttp *http;
	int functionServer;
	int protocol;
	int ret = 0;

	{
		ActivitySimpleLock lock(GetMutex());

		switch (m_state) {
		case CONTEXT_STATE_IDLE:
			m_state = CONTEXT_STATE_OPENING;
			clearAbortAll();

			m_executor = m_manager->CreateCommandExecutor();
			if (m_executor == NULL) {
				setState(CONTEXT_STATE_IDLE);
				destroyComponents();
				return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
			}
			m_executor->m_mutex = m_componentMutex;

			m_auth = m_manager->CreateAuth();
			if (m_auth == NULL) {
				setState(CONTEXT_STATE_IDLE);
				destroyComponents();
				return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
			}
			m_auth->m_mutex = m_componentMutex;
			m_auth->m_utility = m_utility;

			m_http = m_manager->CreateHttp();
			if (m_http == NULL) {
				setState(CONTEXT_STATE_IDLE);
				destroyComponents();
				return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
			}
			m_http->m_mutex = m_componentMutex;
			m_http->SetBuffer(m_staticResource, m_staticResourceSize);

			m_xmlParser = m_manager->CreateXmlParser();
			if (m_xmlParser == NULL) {
				setState(CONTEXT_STATE_IDLE);
				destroyComponents();
				return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
			}
			m_xmlParser->m_mutex = m_componentMutex;
			m_xmlParser->m_utility = m_utility;

			m_xmlGenerator = m_manager->CreateXmlGenerator();
			if (m_xmlGenerator == NULL) {
				setState(CONTEXT_STATE_IDLE);
				destroyComponents();
				return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
			}
			m_xmlGenerator->m_mutex = m_componentMutex;
			m_xmlGenerator->m_utility = m_utility;
			break;
		case CONTEXT_STATE_OPENING:
		case CONTEXT_STATE_CLOSING:
		case CONTEXT_STATE_OPEN:
		case CONTEXT_STATE_RUNNING:
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		default:
#line 285
			ACTIVITY_CHECK("OpenConnection");
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		}
	}

	if (m_utility->IsHostExpired())
		ret = getStaticResource();
	if (ACTIVITY_FAILED(ret)) {
		ActivitySimpleLock lock(GetMutex());

		destroyComponents();
		setState(CONTEXT_STATE_IDLE);
		return endCommandLocked(ret);
	}

	m_http->SetSecureProtocol(mode);
	functionServer = ComponentUtility::IsFunctionServer();
	http = m_http;
	protocol = UTILITY_PROTOCOL_HTTPS;
	if (functionServer)
		protocol = mode;
	ret = http->Open(protocol, m_utility->GetUserAgent(UTILITY_API_NONE),
		m_utility->GetHost(UTILITY_API_NONE), m_utility->GetPort(protocol), 1);
	if (ret < 0) {
		m_http->Close();
		{
			ActivitySimpleLock lock(GetMutex());

			destroyComponents();
			m_state = CONTEXT_STATE_IDLE;
		}
	} else {
		setState(CONTEXT_STATE_OPEN);
	}
	return endCommand(ret);
}

int ComponentContext::CloseConnection()
{
	int ret;

	{
		ActivitySimpleLock lock(GetMutex());

		clearAbortAll();
		switch (m_state) {
		case CONTEXT_STATE_IDLE:
			return 0;
		case CONTEXT_STATE_OPENING:
		case CONTEXT_STATE_CLOSING:
		case CONTEXT_STATE_RUNNING:
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		case CONTEXT_STATE_OPEN:
			m_state = CONTEXT_STATE_CLOSING;
			break;
		default:
#line 426
			ACTIVITY_CHECK("CloseConnection");
			return NP_ACTIVITY_ERROR_INVALID_STATE;
		}
	}

	ret = m_http->Close();
	{
		ActivitySimpleLock lock(GetMutex());

		m_state = CONTEXT_STATE_IDLE;
		destroyComponents();
	}
	return endCommand(ret);
}

/* the NP ID a request is sent for: the caller's, else the signed-in user's */
inline int ComponentContext::getNpId(const SceNpId *npId, SceNpId *self, const SceNpId **id)
{
	int ret;

	if (npId != NULL) {
		*id = npId;
		return 0;
	}
	if (m_auth == NULL) {
		*id = NULL;
		return NP_ACTIVITY_ERROR_INTERNAL;
	}
	sceClibMemset(self, 0, sizeof(*self));
	ret = m_auth->GetMyNpId(self);
	*id = !ACTIVITY_FAILED(ret) ? self : NULL;
	return ret;
}

inline int ComponentContext::runCommand(AbstractCommand *command)
{
	int ret;

	m_executor->SetCommand(command);
	ret = m_executor->Execute();
	m_executor->ClearCommand();
	delete command;
	return endCommand(ret);
}

/*
 * A command's start: the context must be open with no command running. Stated at the line of
 * the method's check.
 */
#define CONTEXT_BEGIN_COMMAND(function)						\
	{									\
		ActivitySimpleLock lock(GetMutex());				\
										\
		switch (m_state) {						\
		case CONTEXT_STATE_OPEN:					\
			if (IsAborted())					\
				return endCommandLocked(0);			\
			m_state = CONTEXT_STATE_RUNNING;			\
			break;							\
		case CONTEXT_STATE_IDLE:					\
		case CONTEXT_STATE_OPENING:					\
		case CONTEXT_STATE_CLOSING:					\
		case CONTEXT_STATE_RUNNING:					\
			return NP_ACTIVITY_ERROR_INVALID_STATE;			\
		default:							\
			ACTIVITY_CHECK(function);				\
			return NP_ACTIVITY_ERROR_INVALID_STATE;			\
		}								\
	}

/* runs a Command made with the context's components, its parameters set with arguments */
#define CONTEXT_RUN_COMMAND(Command, arguments)					\
	Command *command;							\
	SceNpId self;								\
	const SceNpId *id;							\
	int ret;								\
										\
	command = new Command(m_auth, m_http, m_xmlParser, m_xmlGenerator, m_utility); \
	if (command == NULL)							\
		return endCommand(NP_ACTIVITY_ERROR_OUT_OF_MEMORY);		\
	if (!command->IsAvailable()) {						\
		delete command;							\
		return endCommand(NP_ACTIVITY_ERROR_NOT_SUPPORTED);		\
	}									\
	ret = getNpId(npId, &self, &id);					\
	if (ACTIVITY_FAILED(ret)) {								\
		delete command;							\
		return endCommand(ret);						\
	}									\
	ret = command->SetParameter arguments;					\
	if (ACTIVITY_FAILED(ret)) {								\
		delete command;							\
		return endCommand(ret);						\
	}									\
	return runCommand(command)

int ComponentContext::GetAccountInformation(const SceNpId *npId, void *info)
{
#line 477
	CONTEXT_BEGIN_COMMAND("GetAccountInformation");
	CONTEXT_RUN_COMMAND(CommandGetAccountInformation, (id, info));
}

int ComponentContext::SetAccountInformation(const SceNpId *npId, const void *info)
{
#line 568
	CONTEXT_BEGIN_COMMAND("SetAccountInformation");
	CONTEXT_RUN_COMMAND(CommandSetAccountInformation, (id, info));
}

int ComponentContext::CheckStatusUpdate(const SceNpId *npId, const SceUInt64 *since,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	NpActivityUpdates *updates)
{
#line 662
	CONTEXT_BEGIN_COMMAND("CheckStatusUpdate");
	CONTEXT_RUN_COMMAND(CommandCheckStatusUpdate, (id, since, titleIds, count, updates));
}

int ComponentContext::GetFriendStatusTimeline(const SceNpId *npId, const NpActivityRange *range,
	NpActivityStatus *statuses, int max, int *count)
{
#line 756
	CONTEXT_BEGIN_COMMAND("GetFriendStatusTimeline");
	CONTEXT_RUN_COMMAND(CommandGetFriendStatusTimeline, (id, range, statuses, max, count));
}

int ComponentContext::GetFriendStatusTimelineByTitle(const SceNpId *npId,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	const NpActivityRange *range, NpActivityStatusList *lists)
{
#line 850
	CONTEXT_BEGIN_COMMAND("GetFriendStatusTimelineByTitle");
	CONTEXT_RUN_COMMAND(CommandGetFriendStatusTimelineByTitle,
		(id, titleIds, count, range, lists));
}

int ComponentContext::GetStatusTimelineByAccount(const SceNpId *npId,
	const NpActivityOwner *owners, SceSize count, int *max, NpActivityStatusList *lists)
{
#line 944
	CONTEXT_BEGIN_COMMAND("GetStatusTimelineByAccount");
	CONTEXT_RUN_COMMAND(CommandGetStatusTimelineByAccount, (id, owners, count, max, lists));
}

int ComponentContext::GetStatusTimelineByAccountAndTitle(const SceNpId *npId,
	const NpActivityOwner *owners, SceSize count,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max, NpActivityStatusList *lists)
{
#line 1039
	CONTEXT_BEGIN_COMMAND("GetStatusTimelineByAccountAndTitle");
	CONTEXT_RUN_COMMAND(CommandGetStatusTimelineByAccountAndTitle,
		(id, owners, count, titleIds, max, lists));
}

int ComponentContext::GetStatusInfo(const SceNpId *npId, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, void *work, NpActivityDetailState *state,
	NpActivityStatusInfo *infos)
{
#line 1135
	CONTEXT_BEGIN_COMMAND("GetStatusInfo");
	CONTEXT_RUN_COMMAND(CommandGetStatusInfo, (id, keys, count, locale, work, state, infos));
}

int ComponentContext::GetStatusInfoWithCallback(const SceNpId *npId,
	const NpActivityStatusKey *keys, SceSize count, const int *locale, void *work,
	NpActivityDetailState *state, NpActivityStatusDetailCallback callback, void *arg)
{
#line 1232
	CONTEXT_BEGIN_COMMAND("GetStatusInfoWithCallback");
	CONTEXT_RUN_COMMAND(CommandGetStatusInfoWithCallback,
		(id, keys, count, locale, work, state, callback, arg));
}

int ComponentContext::GetStatusDetail(const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityDetailParam *param, NpActivityStatusInfo *info,
	NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
	SceNpId *voters, int maxVoters, SceUInt32 *numVoters)
{
#line 1440
	CONTEXT_BEGIN_COMMAND("GetStatusDetail");
	CONTEXT_RUN_COMMAND(CommandGetStatusDetail, (id, key, param, info, comments, maxComments,
		numComments, isVoted, voters, maxVoters, numVoters));
}

int ComponentContext::AddStatusList(const SceNpId *npId, const NpActivityStatusInfo *infos,
	SceSize count, NpActivityPostResult *results)
{
#line 1536
	CONTEXT_BEGIN_COMMAND("AddStatusList");
	CONTEXT_RUN_COMMAND(CommandAddStatusList, (id, infos, count, results));
}

int ComponentContext::RemoveStatus(const SceNpId *npId, int statusId)
{
#line 1627
	CONTEXT_BEGIN_COMMAND("RemoveStatus");
	CONTEXT_RUN_COMMAND(CommandRemoveStatus, (id, statusId));
}

int ComponentContext::AddComment(const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityCommentPost *comment, NpActivityCommentRequest *request)
{
#line 1720
	CONTEXT_BEGIN_COMMAND("AddComment");
	CONTEXT_RUN_COMMAND(CommandAddComment, (id, key, comment, request));
}

int ComponentContext::RemoveComment(const SceNpId *npId, const NpActivityCommentKey *key,
	NpActivityRequest *request)
{
#line 1812
	CONTEXT_BEGIN_COMMAND("RemoveComment");
	CONTEXT_RUN_COMMAND(CommandRemoveComment, (id, key, request));
}

int ComponentContext::AddVoteGoodJob(const SceNpId *npId, const NpActivityStatusKey *key,
	NpActivityRequest *request)
{
#line 1904
	CONTEXT_BEGIN_COMMAND("AddVoteGoodJob");
	CONTEXT_RUN_COMMAND(CommandAddVoteGoodJob, (id, key, request));
}

int ComponentContext::RemoveVoteGoodJob(const SceNpId *npId, const NpActivityStatusKey *key,
	NpActivityRequest *request)
{
#line 1996
	CONTEXT_BEGIN_COMMAND("RemoveVoteGoodJob");
	CONTEXT_RUN_COMMAND(CommandRemoveVoteGoodJob, (id, key, request));
}
