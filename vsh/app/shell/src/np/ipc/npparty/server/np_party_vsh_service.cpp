/*
 * np_party_vsh_service.cpp: SceNpParty VSH state manager, new-indicator worker
 * thread, push notification handler, and WebAPI session jobs.
 * 3.65-CEX: 0x81384E12 - 0x8138870F
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <common/char_encoding.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libmd5.h>
#include <json.h>

#include "np_party_vsh_internal.h"
#include "../../../imps/np_imps_svc.h"
#include "../../../imps/np_imps_util.h"
#include "../../../../livespace_db/livespace_db_internal.h"
#include "../../../../../plugins.h"

#define NP_PARTY_TITLE_ID			"NPXS10001"
#define NP_PARTY_ICON_PATH			"vs0:app/NPXS10001/sce_sys/icon0.png"
#define NP_PARTY_PRESENCE_URI_PREFIX		"pspy:presence?onlineid="
#define NP_PARTY_PUSH_SERVICE_NAME		"np:service"
#define NP_PARTY_PUSH_EVENT_JOIN_FRIEND		"np:service:session:party:join:to:friend"
#define NP_PARTY_PUSH_EVENT_REMOVE_FRIEND	"np:service:session:party:remove:to:friend"
#define NP_PARTY_WEBAPI_SERVICE_GROUP		"sessionInvitation"
#define NP_PARTY_VOICE_NOTICE_NAME		"PARTY_VOICE_API_NOTIFICATION"

#define NP_PARTY_MSG_ONE_FRIEND_HASH		1260454307u
#define NP_PARTY_MSG_MULTI_FRIEND_HASH		784269619u
#define NP_PARTY_MSG_FRIEND_JOINED_HASH		1917592874u
#define NP_PARTY_MSG_VOICE_DISABLED_HASH	(-1047175105)
#define NP_PARTY_MSG_VOICE_ENABLED_HASH		(-1819671632)

#define NP_PARTY_SDK_VERSION_COMM_ID_V2		0x01600000u
#define NP_PARTY_SDK_VERSION_PRESENCE_MIN	0x01500000u
#define NP_PARTY_SDK_VERSION_PRESENCE_DEP	0x03500000u

#define NP_PARTY_WEBAPI_METHOD_GET		0
#define NP_PARTY_WEBAPI_METHOD_DELETE		3
#define NP_PARTY_WEBAPI_ERR_HEADER_NOT_FOUND	(-2143084507)	/* 0x80431025 */
#define NP_PARTY_WEBAPI_DEFAULT_BUF_SIZE	4096u
#define NP_PARTY_FRIENDS_QUERY_LIMIT		10
#define NP_PARTY_NOTIFY_FLAG_SHOW_PARTY		0x20000u
#define NP_PARTY_IMPS_STATUS_ONLINE		3

struct NpWebApiResponseInfo {
	SceInt32 httpStatus;
	SceUInt32 reservedWords[3];
};

struct NpPushNotificationSender {
	SceUInt8 headerBytes[8];
	char onlineId[36];
};

typedef void (*NpPushNotificationCallback)(const void *pushContext,
					   const NpPushNotificationSender *sender,
					   const char *titleId, const char *dataType,
					   const char *payload, SceSize payloadSize,
					   void *userArg);

extern "C" {
extern int sceNpCmpNpId(const SceNpId *lhs, const SceNpId *rhs);
extern int SceNpWebApi_0D910413(int libCtxId, const char *apiGroup, const char *path,
				int method, const void *contentParam, SceSize contentLength,
				SceInt64 *outRequestId);
extern int sceNpWebApiSendRequest(SceInt64 requestId, const void *data, SceSize dataSize);
extern int sceNpWebApiSendRequest2(SceInt64 requestId, const void *data, SceSize dataSize,
				   NpWebApiResponseInfo *outResponse);
extern int sceNpWebApiGetHttpResponseHeaderValue(SceInt64 requestId, const char *fieldName,
						 char *outValue, SceSize maxValueSize);
extern int sceNpWebApiReadData(SceInt64 requestId, void *buffer, SceSize bufferSize);
extern int sceNpWebApiDeleteRequest(SceInt64 requestId);
extern int sceNpWebApiAbortRequest(SceInt64 requestId);
}

extern int shellNpGetWebApiLibCtx(int *outLibCtx);				/* 0x81003D20 */
extern paf::job::JobQueue *shellGetNpBackgroundJobQueue(void);			/* 0x813C5BE0 */
extern int shellNpPushRegisterService(const char *servicePrefix, int matchMode,
				      void *callback, void *userContext, int *outHandle);					/* 0x813AF03A */

/* New-indicator notification payload passed to SceNpPartyVSHNewIndicator thread */
struct PartyNewIndicatorState {
	PartyNewIndicatorState()
		: msgType(256),
		  msgResourceHash(0),
		  actType(2),
		  newFlag(true),
		  execMode(0),
		  pending(false),
		  lastRowid(0)
	{
	}

	LsdbNewEventKey lastEventKey;
	paf::string titleId;
	paf::string itemId;
	SceInt64 msgType;
	paf::string msgKeyName;
	SceUInt32 msgResourceHash;
	int actType;
	bool newFlag;
	paf::string iconPath;
	paf::string msgArgs[9];
	paf::string titleText;
	paf::string descText;
	paf::string execTitleId;
	paf::string execArg;
	paf::string timeUpdated;
	int execMode;
	LsdbNewEventKey submittedKey;
	SceInt64 lastRowid;
	bool pending;
	SceKernelLwMutexWork mutex;
	SceKernelLwCondWork cond;
};

/* Global SceNpParty VSH state (0x815C8FB8, 6288 bytes) */
struct NpPartyVshContext {
	SceUInt8 initialized;
	SceUInt8 headerAlignment[3];
	SceNpPartyInitParam gameInitParam;
	SceUID gameAppId;
	NpPartyGameHandlerFlags gameHandlers;
	SceUInt16 gameClientState;
	SceNpPartyState partyState;
	SceUInt32 stateAlignment;
	NpPartyAppRoomSnapshot room;
	SceNpPartyMemberVoiceInfo voiceStates[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt8 voiceStateCount;
	SceUInt8 voiceAlignment[3];
	NpPartyMemberSessionSlot sessions[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt8 sessionCount;
	SceUInt8 sessionAlignment[3];
	SceKernelLwMutexWork mutex;
	int voiceSuspendState;
	SceUInt32 voiceSuspendAlignment;
	SceKernelSysClock voiceTalkingTimestamps[SCE_NP_PARTY_MEMBER_NUM_MAX];
	char roomSessionId[NP_PARTY_ROOM_SESSION_ID_SIZE];
};

static NewEventAccessor *s_partyNewEventAccessor = NULL;			/* 0x815C8E40 */
static PartyNewIndicatorState s_partyNiState;					/* 0x815C8E48 */
static NpPartyVshContext s_partyVshCtx;						/* 0x815C8FB8 */

static SceUID s_partyNiThreadId = -1;						/* 0x81548A30 */
static int s_partyPushHandle = 0;						/* 0x81548A34 */
static paf::job::JobQueue *s_partyNiJobQueue = NULL;				/* 0x81548A38 */
static SceUInt32 s_partyImpsListenerId = 0;					/* 0x81548A3C */

static PartyNewIndicatorState s_partyNiTemplates[3];				/* 0x81548A40..0x81548D0F */
static bool s_partyNiTemplatesInitialized = false;

static void initPartyNiTemplate(PartyNewIndicatorState *tmpl, SceUInt32 msgHash)
{
	tmpl->titleId = NP_PARTY_TITLE_ID;
	tmpl->itemId = "0";
	tmpl->msgType = 256;
	tmpl->msgKeyName = "";
	tmpl->msgResourceHash = msgHash;
	tmpl->actType = 2;
	tmpl->newFlag = true;
	tmpl->iconPath = NP_PARTY_ICON_PATH;
	for (int i = 0; i < 9; ++i)
		tmpl->msgArgs[i] = "";
	tmpl->titleText = "";
	tmpl->descText = "";
	tmpl->execTitleId = NP_PARTY_TITLE_ID;
	tmpl->execArg = "";
	tmpl->timeUpdated = "";
	tmpl->execMode = 0;
}

static void ensurePartyNiTemplatesInitialized(void)
{
	if (s_partyNiTemplatesInitialized)
		return;
	initPartyNiTemplate(&s_partyNiTemplates[0], NP_PARTY_MSG_ONE_FRIEND_HASH);
	initPartyNiTemplate(&s_partyNiTemplates[1], NP_PARTY_MSG_MULTI_FRIEND_HASH);
	initPartyNiTemplate(&s_partyNiTemplates[2], NP_PARTY_MSG_FRIEND_JOINED_HASH);
	s_partyNiTemplatesInitialized = true;
}

/* 0x8138863C */
static void partyVshContextReset(NpPartyVshContext *ctx)
{
	ctx->initialized = 0;
	ctx->gameClientState = NP_PARTY_GAME_STATE_UNINITIALIZED;
	ctx->partyState = SCE_NP_PARTY_STATE_NOT_IN_PARTY;
	ctx->gameAppId = -1;
	sce_paf_memset(&ctx->gameInitParam, 0, sizeof(ctx->gameInitParam));
	sce_paf_memset(&ctx->gameHandlers, 0, sizeof(ctx->gameHandlers));
	sce_paf_memset(&ctx->room, 0, sizeof(ctx->room));
	sce_paf_memset(ctx->voiceStates, 0, sizeof(ctx->voiceStates) + sizeof(ctx->voiceStateCount) + sizeof(ctx->voiceAlignment));
	sce_paf_memset(ctx->sessions, 0, sizeof(ctx->sessions) + sizeof(ctx->sessionCount) + sizeof(ctx->sessionAlignment));
	sce_paf_memset(&ctx->mutex, 0, sizeof(ctx->mutex));
	sce_paf_memset(ctx->roomSessionId, 0, sizeof(ctx->roomSessionId));
	ctx->voiceSuspendState = NP_PARTY_VOICE_ACTIVE;
}

/* 0x813886B8 */
static int partyVshContextCreateMutex(NpPartyVshContext *ctx)
{
	return sceKernelCreateLwMutex(&ctx->mutex, "SceNpPartyVshSystemLwMutex",
				      SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
}

/* 0x813886DE */
static int partyVshContextLock(NpPartyVshContext *ctx)
{
	return sceKernelLockLwMutex(&ctx->mutex, 1, NULL);
}

/* 0x813886F0 */
static int partyVshContextUnlock(NpPartyVshContext *ctx)
{
	return sceKernelUnlockLwMutex(&ctx->mutex, 1);
}

/* 0x8138606E */
static int buildGameMemberInfo(const NpPartyAppMemberRecord *src, SceNpPartyMemberInfo *dst)
{
	if (src == NULL || dst == NULL)
		return NP_PARTY_VSH_ERROR_INVALID_ARGUMENT;
	if (s_partyVshCtx.gameClientState == NP_PARTY_GAME_STATE_UNINITIALIZED)
		return NP_PARTY_VSH_ERROR_GAME_NOT_INITIALIZED;

	sce_paf_memset(dst, 0, sizeof(*dst));
	dst->npId = src->memberInfo.npId;
	dst->memberId = src->memberInfo.memberId;
	dst->memberFlags = src->memberInfo.memberFlags;
	dst->reserved = src->memberInfo.reserved;
	dst->gamePresence = src->memberInfo.gamePresence;

	SceUInt32 sdkVersion = s_partyVshCtx.gameInitParam.sdkVersion;
	if (sdkVersion >= NP_PARTY_SDK_VERSION_COMM_ID_V2) {
		if (src->memberInfo.commId.data[0] != '\0')
			dst->commId = src->memberInfo.commId;
	} else {
		dst->commId = src->legacyCommId;
	}

	if (sdkVersion >= NP_PARTY_SDK_VERSION_PRESENCE_DEP) {
		sce_paf_memset(&dst->gamePresence, 0, sizeof(dst->gamePresence));
		sce_paf_memset(&dst->commId, 0, sizeof(dst->commId));
	}
	return 0;
}

static void buildGameMemberListLocked(SceNpPartyMemberList *outList)
{
	sce_paf_memset(outList, 0, sizeof(*outList));
	outList->memberNum = static_cast<SceUInt8>(s_partyVshCtx.room.memberNum);
	for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i)
		outList->members[i] = s_partyVshCtx.room.members[i].memberInfo.npId;
	if ((s_partyVshCtx.room.roomState & NP_PARTY_ROOM_PRIVATE_BIT) != 0)
		outList->privateParty = true;
}

/* 0x813852BE */
static int postPartyNewIndicator(const PartyNewIndicatorState *request)
{
	int ret = sceKernelTryLockLwMutex(&s_partyNiState.mutex, 1);
	if (ret != 0)
		return ret;

	s_partyNiState.titleId = request->titleId;
	s_partyNiState.itemId = request->itemId;
	s_partyNiState.msgType = request->msgType;
	s_partyNiState.msgKeyName = request->msgKeyName;
	s_partyNiState.msgResourceHash = request->msgResourceHash;
	s_partyNiState.actType = request->actType;
	s_partyNiState.newFlag = request->newFlag;
	s_partyNiState.iconPath = request->iconPath;
	for (int i = 0; i < 9; ++i)
		s_partyNiState.msgArgs[i] = request->msgArgs[i];
	s_partyNiState.titleText = request->titleText;
	s_partyNiState.descText = request->descText;
	s_partyNiState.execTitleId = request->execTitleId;
	s_partyNiState.execArg = request->execArg;
	s_partyNiState.timeUpdated = request->timeUpdated;
	s_partyNiState.execMode = request->execMode;
	s_partyNiState.pending = true;

	ret = sceKernelSignalLwCond(&s_partyNiState.cond);
	sceKernelUnlockLwMutex(&s_partyNiState.mutex, 1);
	return ret;
}

/* 0x81384EC2 */
static int partyNewIndicatorThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	int ret = 0;

	for (;;) {
		ret = sceKernelLockLwMutex(&s_partyNiState.mutex, 1, NULL);
		if (ret != 0)
			break;
		ret = sceKernelWaitLwCond(&s_partyNiState.cond, NULL);
		if (ret != 0)
			break;

		LsdbNewEvent event;
		event.titleId = s_partyNiState.titleId;
		event.itemId = s_partyNiState.itemId;
		event.msgType = s_partyNiState.msgType;
		event.actType = s_partyNiState.actType;
		event.newFlag = s_partyNiState.newFlag;
		event.iconPath = s_partyNiState.iconPath;
		for (int i = 0; i < 9; ++i)
			event.msgArg[i] = s_partyNiState.msgArgs[i];
		event.title = s_partyNiState.titleText;
		event.desc = s_partyNiState.descText;
		event.execTitleId = s_partyNiState.execTitleId;
		event.execArg = s_partyNiState.execArg;
		event.timeUpdated = s_partyNiState.timeUpdated;
		event.execMode = NP_PARTY_NOTIFY_FLAG_SHOW_PARTY;

		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		if (indicatorPlugin != NULL) {
			paf::wstring wideArg;
			paf::common::Utf8ToUtf16(s_partyNiState.msgArgs[0], &wideArg, 1);

			paf::IDParam msgId(s_partyNiState.msgResourceHash);
			const wchar_t *fmtText = indicatorPlugin->GetString(msgId);
			paf::wstring formatted = shellFormatText(fmtText, wideArg.c_str());

			paf::string utf8Desc;
			paf::common::Utf16ToUtf8(formatted, &utf8Desc);
			event.desc = utf8Desc;
		}

		SceLsdb_315B9FD6(&event, true);
		s_partyNiState.submittedKey.titleId = event.titleId;
		s_partyNiState.submittedKey.itemId = event.itemId;
		s_partyNiState.submittedKey.rowid = event.rowid;

		ret = sceKernelUnlockLwMutex(&s_partyNiState.mutex, 1);
		if (ret != 0)
			break;
	}

	s_partyNiThreadId = -1;
	sceKernelExitDeleteThread(0);
	return ret;
}

/* 0x813853B6 */
static int notifyFriendJoinedParty(const char *onlineId)
{
	if (onlineId == NULL)
		return 0;

	if (partyVshContextLock(&s_partyVshCtx) == 0) {
		bool alreadyInCurrentParty = false;
		for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
			if (sce_paf_memcmp(s_partyVshCtx.room.members[i].memberInfo.npId.handle.data,
					   onlineId, SCE_NP_ONLINEID_MAX_LENGTH) == 0) {
				alreadyInCurrentParty = true;
				break;
			}
		}
		partyVshContextUnlock(&s_partyVshCtx);
		if (alreadyInCurrentParty)
			return 0;
	}

	if (onlineId[0] == '\0')
		return 0;

	ensurePartyNiTemplatesInitialized();
	PartyNewIndicatorState notice = s_partyNiTemplates[2];
	notice.msgArgs[0] = onlineId;
	paf::string launchUri(NP_PARTY_PRESENCE_URI_PREFIX);
	launchUri += onlineId;
	notice.execArg = launchUri;
	return postPartyNewIndicator(&notice);
}

/* 0x813856CC */
static void onPartyPushNotification(const void *pushContext,
				    const NpPushNotificationSender *sender,
				    const char *titleId, const char *dataType,
				    const char *payload, SceSize payloadSize,
				    void *userArg)
{
	(void)userArg;
	if (pushContext == NULL || sender == NULL || dataType == NULL || titleId == NULL)
		return;
	if (sce_paf_strcasecmp(titleId, NP_PARTY_TITLE_ID) != 0)
		return;

	if (sce_paf_strcasecmp(dataType, NP_PARTY_PUSH_EVENT_JOIN_FRIEND) != 0) {
		if (sce_paf_strcasecmp(dataType, NP_PARTY_PUSH_EVENT_REMOVE_FRIEND) == 0) {
			s_partyNiState.lastEventKey.titleId = "";
			s_partyNiState.lastEventKey.itemId = "";
			s_partyNiState.lastEventKey.rowid = 0;
		}
		return;
	}

	if (payload != NULL && payloadSize > 0) {
		char parsedSessionId[NP_PARTY_ROOM_SESSION_ID_SIZE];
		sce_paf_memset(parsedSessionId, 0, sizeof(parsedSessionId));

		paf::string body(payload, payloadSize);
		size_t keyPos = body.find("\"sessionId\"");
		size_t colonPos = (keyPos != paf::string::npos) ? body.find(":", keyPos) : paf::string::npos;
		size_t quoteStart = (colonPos != paf::string::npos) ? body.find("\"", colonPos) : paf::string::npos;
		size_t quoteEnd = (quoteStart != paf::string::npos) ? body.find("\"", quoteStart + 1) : paf::string::npos;
		if (quoteStart != paf::string::npos && quoteEnd != paf::string::npos && quoteEnd > quoteStart + 1) {
			paf::string extracted = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
			sce_paf_strncpy(parsedSessionId, extracted.c_str(), 45);
		}

		char activeSessionId[NP_PARTY_ROOM_SESSION_ID_SIZE];
		sce_paf_memset(activeSessionId, 0, sizeof(activeSessionId));
		if (partyVshContextLock(&s_partyVshCtx) < 0)
			return;
		sce_paf_memcpy(activeSessionId, s_partyVshCtx.roomSessionId, sizeof(activeSessionId));
		if (partyVshContextUnlock(&s_partyVshCtx) < 0)
			return;
		if (sce_paf_memcmp(activeSessionId, parsedSessionId, sizeof(activeSessionId)) == 0)
			return;
	}

	notifyFriendJoinedParty(sender->onlineId);
}

/* Custom allocator for sce::Json in PartyGetFriendsInPartyJob (vtable 0x8153E048) */
class PartyJsonAllocator : public sce::Json::MemAllocator {
public:
	virtual void *allocate(size_t size, void *userData)
	{
		(void)userData;
		return sce_paf_malloc(size);
	}

	virtual void deallocate(void *ptr, void *userData)
	{
		(void)userData;
		sce_paf_free(ptr);
	}

	virtual void notifyError(int error, size_t size, void *userData)
	{
		(void)error;
		(void)size;
		(void)userData;
	}
};

struct PartyJsonNullDefaults {
	sce::Json::Value booleanValue;
	sce::Json::Value integerValue;
	sce::Json::Value uintegerValue;
	sce::Json::Value realValue;
	sce::Json::Value stringValue;
	sce::Json::Value nullValue;
};

/* Null-access fallback for sce::Json lookups (0x8144AAC0) */
static const sce::Json::Value &partyJsonNullAccessCallback(
	sce::Json::ValueType accessType, const sce::Json::Value *parent, void *context)
{
	(void)parent;
	const PartyJsonNullDefaults *defaults =
		static_cast<const PartyJsonNullDefaults *>(context);
	switch (accessType) {
	case sce::Json::kValueTypeBoolean:
		return defaults->booleanValue;
	case sce::Json::kValueTypeInteger:
		return defaults->integerValue;
	case sce::Json::kValueTypeUInteger:
		return defaults->uintegerValue;
	case sce::Json::kValueTypeReal:
		return defaults->realValue;
	case sce::Json::kValueTypeString:
		return defaults->stringValue;
	default:
		return defaults->nullValue;
	}
}

/* Job: fetch friends currently in a party via NP WebAPI (0x81384E12..0x81387F7D) */
class PartyGetFriendsInPartyJob : public paf::job::JobItem {
public:
	/* 0x81384E12 */
	PartyGetFriendsInPartyJob(int webApiLibCtx)
		: paf::job::JobItem("SceNpPartyGetFriendsInParty", NULL),
		  m_webApiLibCtx(webApiLibCtx),
		  m_requestId(-1),
		  m_pageSize(0),
		  m_accumulatedCount(0),
		  m_totalResults(0),
		  m_friendsInPartyCount(0),
		  m_singleFriendOnlineId()
	{
		char titleIdWithSuffix[32];
		sce_paf_snprintf(titleIdWithSuffix, sizeof(titleIdWithSuffix), "%s" "_00",
				 NP_PARTY_TITLE_ID);
		sce_paf_snprintf(
			m_apiPath, sizeof(m_apiPath),
			"/v" "1/users/me/friends/sessions?relatedNpTitleId=%s&limit=%d&fields=friends&availablePlatforms=PS4,PSVITA",
			titleIdWithSuffix, NP_PARTY_FRIENDS_QUERY_LIMIT);
	}

	/* 0x81387A2C / 0x81387A66 */
	virtual ~PartyGetFriendsInPartyJob()
	{
	}

	/* 0x81387F56 */
	virtual void Cancel()
	{
		if (m_requestId >= 0) {
			sceNpWebApiAbortRequest(m_requestId);
			m_requestId = -1;
		}
	}

	/* 0x81387AA6 */
	virtual int Run()
	{
		m_pageSize = 0;
		m_accumulatedCount = 0;
		m_totalResults = 0;
		m_friendsInPartyCount = 0;

		int ret = ExecuteRequest();
		if (ret == 1 || ret < 0)
			return ret;
		if (m_accumulatedCount == 0 && m_friendsInPartyCount == 0)
			return 0;

		ensurePartyNiTemplatesInitialized();
		if (m_friendsInPartyCount == 1 && !m_singleFriendOnlineId.empty()) {
			PartyNewIndicatorState notice = s_partyNiTemplates[0];
			notice.msgArgs[0] = m_singleFriendOnlineId;
			paf::string launchUri(NP_PARTY_PRESENCE_URI_PREFIX);
			launchUri.append(m_singleFriendOnlineId.c_str());
			notice.execArg = launchUri;
			postPartyNewIndicator(&notice);
		} else if (m_friendsInPartyCount > 1) {
			int displayCount = m_friendsInPartyCount;
			if (m_accumulatedCount < m_totalResults)
				displayCount = (m_totalResults - m_accumulatedCount) + m_friendsInPartyCount;

			char countText[256];
			sce_paf_snprintf(countText, sizeof(countText), "%d", displayCount);

			PartyNewIndicatorState notice = s_partyNiTemplates[1];
			notice.msgArgs[0] = countText;
			postPartyNewIndicator(&notice);
		}
		return 0;
	}

	virtual void Finish(int result)
	{
		(void)result;
	}

private:
	/* 0x813875A6 */
	int ParseResponseJson(const char *json, SceSize jsonSize)
	{
		PartyJsonAllocator allocator;
		sce::Json::Initializer initializer;
		sce::Json::InitParameter initParam(&allocator, NULL, 1024);
		initializer.initialize(&initParam);

		PartyJsonNullDefaults nullDefaults;
		nullDefaults.booleanValue.set(false);
		nullDefaults.integerValue.set(static_cast<int64_t>(-99));
		nullDefaults.uintegerValue.set(static_cast<uint64_t>(99));
		nullDefaults.realValue.set(-0.99);
		nullDefaults.stringValue.set(sce::Json::String("invalid string"));
		nullDefaults.nullValue.set(sce::Json::kValueTypeNull);

		sce::Json::Value root;
		sce::Json::Array sessions;
		sce::Json::Array friends;

		root.clear();
		root.setNullAccessCallBack(partyJsonNullAccessCallback, &nullDefaults);
		int ret = sce::Json::Parser::parse(root, json, jsonSize);
		if (ret != 0)
			return ret;

		(void)root["start"].getInteger();
		int pageSize = static_cast<int>(root["size"].getInteger());
		int totalResults = static_cast<int>(root["totalResults"].getInteger());
		if (m_totalResults == 0)
			m_totalResults = totalResults;
		m_pageSize = pageSize;
		m_accumulatedCount += pageSize;

		sessions = root["sessions"].getArray();
		int sessionIndex = 0;
		for (sce::Json::Array::iterator it = sessions.begin();
		     it != sessions.end(); ++it, ++sessionIndex) {
			friends = (*it)["friends"].getArray();
			int friendsInSession = static_cast<int>(friends.size());
			m_friendsInPartyCount += friendsInSession;
			if (m_totalResults == 1 && friendsInSession == 1 && sessionIndex == 0) {
				sce::Json::Array::iterator friendIt = friends.begin();
				if (friendIt->getType() == sce::Json::kValueTypeObject) {
					const char *onlineId =
						(*friendIt)["onlineId"].getString().c_str();
					SceSize onlineIdLen =
						(onlineId != NULL) ? sce_paf_strlen(onlineId) : 0;
					m_singleFriendOnlineId.assign(onlineId, onlineIdLen);
				}
			}
		}

		initializer.terminate();
		return 0;
	}

	/* 0x813878DC */
	int ExecuteRequest()
	{
		m_requestId = -1;
		int ret = SceNpWebApi_0D910413(m_webApiLibCtx, NP_PARTY_WEBAPI_SERVICE_GROUP,
					       m_apiPath, NP_PARTY_WEBAPI_METHOD_GET,
					       NULL, 0, &m_requestId);
		if (ret < 0)
			return ret;

		NpWebApiResponseInfo response;
		sce_paf_memset(&response, 0, sizeof(response));
		ret = sceNpWebApiSendRequest2(m_requestId, NULL, 0, &response);
		if (ret >= 0 && response.httpStatus == 200) {
			SceSize responseCapacity = NP_PARTY_WEBAPI_DEFAULT_BUF_SIZE;
			char contentLengthText[256];
			sce_paf_memset(contentLengthText, 0, sizeof(contentLengthText));
			ret = sceNpWebApiGetHttpResponseHeaderValue(
				m_requestId, "Content-Length",
				contentLengthText, sizeof(contentLengthText));
			if (ret == 0) {
				int parsedLength = sce_paf_atoi(contentLengthText);
				if (parsedLength > 0)
					responseCapacity = static_cast<SceSize>(parsedLength);
			} else if (ret == NP_PARTY_WEBAPI_ERR_HEADER_NOT_FOUND) {
				ret = 0;
			}

			char *buffer = new char[responseCapacity];
			if (buffer != NULL) {
				sce_paf_memset(buffer, 0, responseCapacity);
				SceSize totalRead = 0;
				while (totalRead < responseCapacity) {
					int readRet = sceNpWebApiReadData(
						m_requestId, buffer + totalRead,
						responseCapacity - totalRead);
					ret = readRet;
					if (readRet == 0)
						break;
					if (readRet < 0)
						break;
					totalRead += static_cast<SceSize>(readRet);
				}
				if (ret >= 0)
					ret = ParseResponseJson(buffer, totalRead);
				delete[] buffer;
			}
		}

		if (m_requestId >= 0) {
			sceNpWebApiDeleteRequest(m_requestId);
			m_requestId = -1;
		}
		return ret;
	}

	int m_webApiLibCtx;
	SceInt64 m_requestId;
	char m_apiPath[192];
	int m_pageSize;
	int m_accumulatedCount;
	int m_totalResults;
	int m_friendsInPartyCount;
	paf::string m_singleFriendOnlineId;
};

/* Job: leave current session via NP WebAPI DELETE when party app stops (0x8138750E..0x813875A5) */
class PartyLeaveSessionJob : public paf::job::JobItem {
public:
	PartyLeaveSessionJob(const char *sessionId, const char *selfOnlineId, int webApiLibCtx)
		: paf::job::JobItem("SceNpPartyLeaveSession", NULL),
		  m_webApiLibCtx(webApiLibCtx)
	{
		sce_paf_memcpy(m_sessionId, sessionId, sizeof(m_sessionId));
		sce_paf_memset(m_apiPath, 0, sizeof(m_apiPath));
		sce_paf_snprintf(m_apiPath, sizeof(m_apiPath), "/v" "1/sessions/%s/members/%s",
				 m_sessionId, selfOnlineId);
	}

	/* 0x8138750E / 0x8138752A */
	virtual ~PartyLeaveSessionJob()
	{
	}

	/* 0x8138754A */
	virtual int Run()
	{
		SceInt64 requestId = -1;
		int ret = SceNpWebApi_0D910413(m_webApiLibCtx, NP_PARTY_WEBAPI_SERVICE_GROUP,
					       m_apiPath, NP_PARTY_WEBAPI_METHOD_DELETE,
					       NULL, 0, &requestId);
		if (ret >= 0) {
			sceNpWebApiSendRequest(requestId, NULL, 0);
			sceNpWebApiDeleteRequest(requestId);
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		(void)result;
	}

private:
	char m_sessionId[NP_PARTY_ROOM_SESSION_ID_SIZE];
	char m_apiPath[192];
	int m_webApiLibCtx;
};

/* 0x81387F80 */
static void onImpsMyOnlineStatusChanged(int onlineStatus, void *userArg)
{
	(void)userArg;
	if (onlineStatus != NP_PARTY_IMPS_STATUS_ONLINE || s_partyNiJobQueue == NULL)
		return;

	int webApiLibCtx = 0;
	shellNpGetWebApiLibCtx(&webApiLibCtx);
	if (webApiLibCtx < 0)
		return;

	PartyGetFriendsInPartyJob *job = new PartyGetFriendsInPartyJob(webApiLibCtx);
	if (job == NULL)
		return;

	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	s_partyNiJobQueue->Enqueue(jobRef);
}

/* 0x81385A6E */
static int initPartyNewIndicatorSystem(void)
{
	ensurePartyNiTemplatesInitialized();

	NewEventAccessor *accessor = new NewEventAccessor();
	if (accessor != NULL)
		SceLsdb_919C0934(accessor);
	s_partyNewEventAccessor = accessor;
	if (accessor == NULL)
		return -1;

	int ret = SceLsdb_4C137FCC(accessor);
	if (ret != 0) {
		SceLsdb_937405B5(accessor);
		delete accessor;
		s_partyNewEventAccessor = NULL;
		return ret;
	}

	s_partyNiState.pending = false;
	if (sceKernelCreateLwMutex(&s_partyNiState.mutex, "SceNpPartyVSHNewIndicator",
				   0, 0, NULL) != 0)
		return -1;
	if (sceKernelCreateLwCond(&s_partyNiState.cond, "SceNpPartyVSHNewIndicator",
				  0, &s_partyNiState.mutex, NULL) != 0)
		return -1;

	SceUID threadId = sceKernelCreateThread(
		"SceNpPartyVSHNewIndicator",
		partyNewIndicatorThreadEntry, 191, 0x2000u, 0, 0, NULL);
	if (threadId < 0)
		return threadId;

	s_partyNiThreadId = threadId;
	ret = sceKernelStartThread(threadId, 0, NULL);
	if (ret < 0)
		return ret;

	shellNpPushRegisterService(NP_PARTY_PUSH_SERVICE_NAME, 1,
				   reinterpret_cast<void *>(onPartyPushNotification),
			    NULL, &s_partyPushHandle);

	paf::job::JobQueue::Option queueOption;
	queueOption.priority = 0x10000114;
	queueOption.stackSize = 0x4000;
	queueOption.numThreads = 1;
	queueOption.threadOption = NULL;

	s_partyNiJobQueue = new paf::job::JobQueue("SceNpPartyVshInitializeNI", &queueOption);
	if (s_partyNiJobQueue == NULL)
		return -1;

	NpImpsSvcListenerCallbacks callbacks;
	sce_paf_memset(&callbacks, 0, sizeof(callbacks));
	callbacks.onMyOnlineStatus = onImpsMyOnlineStatusChanged;
	ret = npImpsSvcRegisterListener(&callbacks, &s_partyImpsListenerId);
	if (ret != 0)
		s_partyImpsListenerId = 0;
	return ret;
}

/* 0x81386478 */
static int showPartyVoiceNotice(SceUInt8 voiceEnabled)
{
	paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
	if (imposePlugin == NULL)
		return NP_PARTY_VSH_ERROR_INTERNAL;

	ImposePluginInterface *imposeIface =
		static_cast<ImposePluginInterface *>(imposePlugin->GetInterface(1));
	if (imposeIface == NULL)
		return NP_PARTY_VSH_ERROR_INTERNAL;

	paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
	if (indicatorPlugin == NULL)
		return NP_PARTY_VSH_ERROR_INTERNAL;

	SceUInt32 msgHash = (voiceEnabled != 0)
		? static_cast<SceUInt32>(NP_PARTY_MSG_VOICE_ENABLED_HASH)
		: static_cast<SceUInt32>(NP_PARTY_MSG_VOICE_DISABLED_HASH);
	paf::IDParam msgId(msgHash);
	const wchar_t *rawText = indicatorPlugin->GetString(msgId);
	paf::wstring noticeText(rawText != NULL ? rawText : L"");

	ImposeNotice notice;
	notice.name = NP_PARTY_VOICE_NOTICE_NAME;
	notice.message = noticeText.c_str();

	imposeIface->removeNotice(NP_PARTY_VOICE_NOTICE_NAME);
	if (imposeIface->showNotice(&notice) == 0)
		return NP_PARTY_VSH_ERROR_INTERNAL;
	return 0;
}

/* 0x81385C1E */
int npPartyVshInit(void)
{
	if (s_partyVshCtx.initialized != 0)
		return NP_PARTY_VSH_ERROR_ALREADY_INITIALIZED;

	partyVshContextReset(&s_partyVshCtx);
	int ret = npPartyIpcServerInit();
	if (ret < 0)
		return ret;

	ret = partyVshContextCreateMutex(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	initPartyNewIndicatorSystem();
	s_partyVshCtx.initialized = 1;
	return ret;
}

/* 0x81385C72 (declared in np_imps_svc.h) */
int npPartyGetMemberGamePresence(const SceNpId *npId,
				 NpImpsGamePresenceWithTitle *outPresence)
{
	if (s_partyVshCtx.initialized == 0)
		return NP_PARTY_VSH_ERROR_ALREADY_INITIALIZED;
	if (outPresence == NULL)
		return NP_PARTY_VSH_ERROR_INVALID_ARGUMENT;

	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	ret = NP_PARTY_VSH_ERROR_NOT_FOUND;
	for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
		if (sceNpCmpNpId(&s_partyVshCtx.room.members[i].memberInfo.npId, npId) == 0) {
			sce_paf_memcpy(outPresence,
				       &s_partyVshCtx.room.members[i].memberInfo.gamePresence,
				       sizeof(SceNpBasicGamePresence));
			ret = 0;
			break;
		}
	}

	partyVshContextUnlock(&s_partyVshCtx);
	return ret;
}

/* 0x81385D2A */
int npPartyVshGetObserverMembers(NpPartyObserverMemberList *outList)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	if (s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_NOT_IN_PARTY) {
		ret = NP_PARTY_OBSERVER_ERROR_NOT_IN_PARTY;
	} else {
		SceUInt8 outCount = 0;
		SceNpPartyRoomMemberId selfMemberId = s_partyVshCtx.room.selfInfo.memberInfo.memberId;
		for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
			const SceNpPartyMemberInfo &member = s_partyVshCtx.room.members[i].memberInfo;
			if (member.memberId != selfMemberId && outCount < NP_PARTY_OBSERVER_MEMBER_MAX) {
				NpPartyObserverMember &dst = outList->members[outCount];
				dst.npId = member.npId;
				sce_paf_memcpy(dst.gamePresenceData, member.gamePresence.title,
					       sizeof(dst.gamePresenceData));
				dst.presenceKind = 3;
				++outCount;
			}
		}
		outList->memberCount = outCount;
	}

	partyVshContextUnlock(&s_partyVshCtx);
	return ret;
}

/* 0x81385E00 */
int npPartyVshOnGameInit(const SceNpPartyInitParam *initParam, SceUID gameAppId)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.gameInitParam = *initParam;
	s_partyVshCtx.gameAppId = gameAppId;
	s_partyVshCtx.gameClientState = NP_PARTY_GAME_STATE_INITIALIZED;
	sce_paf_memset(&s_partyVshCtx.gameHandlers, 0, sizeof(s_partyVshCtx.gameHandlers));

	npPartyIpcSendAppGameInit(&s_partyVshCtx.gameInitParam, gameAppId);
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81385E54 */
int npPartyVshOnGameRegisterHandlers(const NpPartyGameHandlerFlags *handlers)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.gameClientState = NP_PARTY_GAME_STATE_HANDLERS_REGISTERED;
	s_partyVshCtx.gameHandlers = *handlers;
	npPartyIpcSendAppGameHandlers(&s_partyVshCtx.gameHandlers);

	if (s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PARTY ||
	    s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PRIVATE_PARTY) {
		SceNpPartyMemberList memberList;
		buildGameMemberListLocked(&memberList);
		npPartyIpcSendGameJoined(&memberList);

		SceKernelSysClock now;
		now.quad = 0;
		sceKernelGetProcessTime(&now);
		if (s_partyVshCtx.gameHandlers.voiceEvents != 0) {
			for (SceUInt8 i = 0; i < s_partyVshCtx.voiceStateCount; ++i) {
				if ((now.quad - s_partyVshCtx.voiceTalkingTimestamps[i].quad) > SCE_NP_PARTY_ROOM_TALKING_LIMIT &&
				    s_partyVshCtx.voiceStates[i].memberVoiceState == SCE_NP_PARTY_MEMBER_VOICE_STATE_TALKING) {
					s_partyVshCtx.voiceStates[i].memberVoiceState =
						SCE_NP_PARTY_MEMBER_VOICE_STATE_CONNECTED;
				}
				npPartyIpcSendGameMemberVoice(&s_partyVshCtx.voiceStates[i]);
			}
		}
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81385FD2 */
int npPartyVshGetMembers(SceNpPartyMemberList *outMembers)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	buildGameMemberListLocked(outMembers);
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386154 */
int npPartyVshGetMemberInfo(const SceNpId *npId, SceNpPartyMemberInfo *outInfo)
{
	bool found = false;
	if (partyVshContextLock(&s_partyVshCtx) >= 0) {
		for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
			if (sceNpCmpNpId(&s_partyVshCtx.room.members[i].memberInfo.npId, npId) == 0) {
				buildGameMemberInfo(&s_partyVshCtx.room.members[i], outInfo);
				found = true;
				break;
			}
		}
		partyVshContextUnlock(&s_partyVshCtx);
	}
	return found ? 0 : NP_PARTY_GAME_ERROR_MEMBER_NOT_FOUND;
}

/* 0x813861E0 */
int npPartyVshGetMemberSessionInfo(SceNpPartyRoomMemberId memberId,
				   SceNpPartyMemberGameSessionUpdateInfo *outInfo)
{
	bool found = false;
	if (partyVshContextLock(&s_partyVshCtx) >= 0) {
		if (s_partyVshCtx.gameHandlers.gameSessionEvents != 1 ||
		    s_partyVshCtx.gameHandlers.gameSessionSystemReady != 1) {
			partyVshContextUnlock(&s_partyVshCtx);
			return NP_PARTY_GAME_ERROR_SESSION_NOT_READY;
		}

		for (SceUInt8 i = 0; i < s_partyVshCtx.sessionCount; ++i) {
			const NpPartyMemberSessionSlot &slot = s_partyVshCtx.sessions[i];
			if (slot.memberId != memberId)
				continue;

			found = true;
			sce_paf_memset(outInfo, 0, sizeof(*outInfo));
			outInfo->memberId = memberId;
			if (sce_paf_memcmp(&s_partyVshCtx.gameHandlers.gameCommId, &slot.commId,
					   sizeof(SceNpCommunicationId)) != 0) {
				outInfo->type = SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_NOT_JOINABLE;
			} else {
				outInfo->type = slot.updateType;
				outInfo->sessionId = slot.sessionId;
			}
			break;
		}
		partyVshContextUnlock(&s_partyVshCtx);
	}
	return found ? 0 : NP_PARTY_GAME_ERROR_MEMBER_NOT_FOUND;
}

/* 0x813862E6 */
int npPartyVshGetPartyId(SceNpPartyId *outId)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	ret = sceMd5Digest(s_partyVshCtx.room.roomInfo, NP_PARTY_ROOM_SESSION_ID_SIZE, outId->id);
	if (ret >= 0)
		return partyVshContextUnlock(&s_partyVshCtx);

	partyVshContextUnlock(&s_partyVshCtx);
	return ret;
}

/* 0x81386334 */
int npPartyVshGetGameSessionReadyState(SceNpPartyGameSessionUpdateReady *outReady)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	int status = 0;
	if (s_partyVshCtx.gameHandlers.gameSessionEvents == 0) {
		status = NP_PARTY_GAME_ERROR_SESSION_NOT_READY;
	} else if (s_partyVshCtx.gameHandlers.gameSessionReadyValid == 0) {
		status = NP_PARTY_GAME_ERROR_READY_STATE_UNAVAILABLE;
	} else {
		outReady->result = s_partyVshCtx.gameHandlers.gameSessionReadyResult;
	}

	ret = partyVshContextUnlock(&s_partyVshCtx);
	return (ret >= 0) ? status : ret;
}

/* 0x8138639C */
int npPartyVshGetMemberVoiceInfo(SceNpPartyRoomMemberId memberId,
				 SceNpPartyMemberVoiceState *outVoiceState)
{
	bool found = false;
	if (partyVshContextLock(&s_partyVshCtx) >= 0) {
		SceKernelSysClock now;
		now.quad = 0;
		sceKernelGetProcessTime(&now);
		for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
			if (s_partyVshCtx.room.members[i].memberInfo.memberId != memberId)
				continue;

			SceNpPartyMemberVoiceState state = s_partyVshCtx.voiceStates[i].memberVoiceState;
			if ((now.quad - s_partyVshCtx.voiceTalkingTimestamps[i].quad) > SCE_NP_PARTY_ROOM_TALKING_LIMIT &&
			    state == SCE_NP_PARTY_MEMBER_VOICE_STATE_TALKING) {
				state = SCE_NP_PARTY_MEMBER_VOICE_STATE_CONNECTED;
				s_partyVshCtx.voiceStates[i].memberVoiceState = state;
			}
			*outVoiceState = state;
			found = true;
			break;
		}
		partyVshContextUnlock(&s_partyVshCtx);
	}
	return found ? 0 : NP_PARTY_GAME_ERROR_MEMBER_NOT_FOUND;
}

/* 0x8138668A */
int npPartyVshReEnableVoice(void)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	switch (s_partyVshCtx.voiceSuspendState) {
	case NP_PARTY_VOICE_SUSPEND_PENDING:
		s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_ACTIVE;
		break;
	case NP_PARTY_VOICE_SUSPENDED:
		if (s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PARTY ||
		    s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PRIVATE_PARTY) {
			npPartyIpcSendAppReEnableVoice();
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_RESUMED;
			showPartyVoiceNotice(1);
		} else {
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_RESUME_PENDING;
		}
		break;
	default:
		break;
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386708 */
int npPartyVshOnGameTerm(void)
{
	npPartyVshReEnableVoice();

	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.gameClientState = NP_PARTY_GAME_STATE_UNINITIALIZED;
	sce_paf_memset(&s_partyVshCtx.gameInitParam, 0, sizeof(s_partyVshCtx.gameInitParam));
	sce_paf_memset(&s_partyVshCtx.gameHandlers, 0, sizeof(s_partyVshCtx.gameHandlers));
	s_partyVshCtx.gameAppId = -1;
	npPartyIpcSendAppGameTerm();
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386752 */
int npPartyVshGetState(SceNpPartyState *outState)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	*outState = s_partyVshCtx.partyState;
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x8138677A */
int npPartyVshDisableVoice(void)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	switch (s_partyVshCtx.voiceSuspendState) {
	case NP_PARTY_VOICE_ACTIVE:
	case NP_PARTY_VOICE_RESUMED:
		if (s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PARTY ||
		    s_partyVshCtx.partyState == SCE_NP_PARTY_STATE_IN_PRIVATE_PARTY) {
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_SUSPENDED;
			npPartyIpcSendAppDisableVoice();
			showPartyVoiceNotice(0);
		} else {
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_SUSPEND_PENDING;
		}
		break;
	case NP_PARTY_VOICE_RESUME_PENDING:
		s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_SUSPENDED;
		break;
	default:
		break;
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x813867F0 */
int npPartyVshOnAppInit(void)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.partyState = SCE_NP_PARTY_STATE_NOT_IN_PARTY;
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386814 */
int npPartyVshOnAppRegisterHandler(void)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	if (s_partyVshCtx.gameClientState == NP_PARTY_GAME_STATE_INITIALIZED) {
		npPartyIpcSendAppGameInit(&s_partyVshCtx.gameInitParam, s_partyVshCtx.gameAppId);
	} else if (s_partyVshCtx.gameClientState == NP_PARTY_GAME_STATE_HANDLERS_REGISTERED) {
		npPartyIpcSendAppGameInit(&s_partyVshCtx.gameInitParam, s_partyVshCtx.gameAppId);
		npPartyIpcSendAppGameHandlers(&s_partyVshCtx.gameHandlers);
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386870 */
int npPartyVshOnAppJoinedParty(const NpPartyAppRoomSnapshot *snapshot,
			       const char *roomSessionId)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.room = *snapshot;
	sce_paf_memcpy(s_partyVshCtx.roomSessionId, roomSessionId, sizeof(s_partyVshCtx.roomSessionId));
	sce_paf_memset(s_partyVshCtx.voiceStates, 0, sizeof(s_partyVshCtx.voiceStates));
	s_partyVshCtx.voiceStateCount = 0;
	sce_paf_memset(&s_partyVshCtx.sessions[0], 0, sizeof(s_partyVshCtx.sessions[0]));

	SceUInt32 memberNum = s_partyVshCtx.room.memberNum;
	SceNpPartyRoomMemberId selfMemberId = snapshot->selfInfo.memberInfo.memberId;
	for (SceUInt32 i = 0; i < memberNum; ++i) {
		SceNpPartyRoomMemberId memberId = s_partyVshCtx.room.members[i].memberInfo.memberId;
		s_partyVshCtx.voiceStates[i].memberId = memberId;
		s_partyVshCtx.voiceStates[i].memberVoiceState = (memberId == selfMemberId)
			? SCE_NP_PARTY_MEMBER_VOICE_STATE_CONNECTED
			: SCE_NP_PARTY_MEMBER_VOICE_STATE_DISCONNECTED;

		s_partyVshCtx.sessions[i].memberId = memberId;
		s_partyVshCtx.sessions[i].updateType = SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_NOT_JOINABLE;
	}
	s_partyVshCtx.voiceStateCount = static_cast<SceUInt8>(memberNum);
	s_partyVshCtx.sessionCount = static_cast<SceUInt8>(memberNum);

	if ((s_partyVshCtx.room.roomState & NP_PARTY_ROOM_PRIVATE_BIT) != 0)
		s_partyVshCtx.partyState = SCE_NP_PARTY_STATE_IN_PRIVATE_PARTY;
	else
		s_partyVshCtx.partyState = SCE_NP_PARTY_STATE_IN_PARTY;

	SceNpPartyMemberList memberList;
	buildGameMemberListLocked(&memberList);
	npPartyIpcSendGameJoined(&memberList);
	npPartyIpcNotifyObserversChanged();

	if (s_partyVshCtx.voiceSuspendState == NP_PARTY_VOICE_SUSPENDED)
		npPartyIpcSendAppDisableVoice();

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386A2A */
int npPartyVshOnAppMemberLeft(const NpPartyAppRoomSnapshot *snapshot,
			      const NpPartyAppMemberRecord *member)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.room = *snapshot;
	SceNpPartyRoomMemberId targetId = member->memberInfo.memberId;

	for (SceUInt8 i = 0; i < s_partyVshCtx.voiceStateCount; ++i) {
		if (s_partyVshCtx.voiceStates[i].memberId == targetId) {
			sce_paf_memset(&s_partyVshCtx.voiceStates[i], 0,
				       sizeof(s_partyVshCtx.voiceStates[i]));
			if (i + 1 < s_partyVshCtx.voiceStateCount) {
				sce_paf_memmove(&s_partyVshCtx.voiceStates[i],
						&s_partyVshCtx.voiceStates[i + 1],
						sizeof(SceNpPartyMemberVoiceInfo) *
						(s_partyVshCtx.voiceStateCount - 1 - i));
			}
		}
	}
	s_partyVshCtx.voiceStateCount = static_cast<SceUInt8>(s_partyVshCtx.room.memberNum);

	for (SceUInt8 i = 0; i < s_partyVshCtx.sessionCount; ++i) {
		if (s_partyVshCtx.sessions[i].memberId == targetId) {
			sce_paf_memset(&s_partyVshCtx.sessions[i], 0,
				       sizeof(s_partyVshCtx.sessions[i]));
			if (i + 1 < s_partyVshCtx.sessionCount) {
				sce_paf_memmove(&s_partyVshCtx.sessions[i],
						&s_partyVshCtx.sessions[i + 1],
						sizeof(NpPartyMemberSessionSlot) *
						(s_partyVshCtx.sessionCount - 1 - i));
			}
		}
	}
	s_partyVshCtx.sessionCount = static_cast<SceUInt8>(s_partyVshCtx.room.memberNum);

	SceNpPartyMemberInfo gameMember;
	buildGameMemberInfo(member, &gameMember);
	npPartyIpcSendGameMemberLeft(&gameMember);
	npPartyIpcNotifyObserversChanged();
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386B5C */
int npPartyVshOnAppMemberJoined(const NpPartyAppRoomSnapshot *snapshot,
				const NpPartyAppMemberRecord *member)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.room = *snapshot;
	SceUInt8 voiceIndex = s_partyVshCtx.voiceStateCount;
	SceUInt8 sessionIndex = s_partyVshCtx.sessionCount;
	s_partyVshCtx.voiceStates[voiceIndex].memberId = member->memberInfo.memberId;
	s_partyVshCtx.voiceStates[voiceIndex].memberVoiceState =
		SCE_NP_PARTY_MEMBER_VOICE_STATE_DISCONNECTED;
	s_partyVshCtx.voiceStateCount = static_cast<SceUInt8>(voiceIndex + 1);

	s_partyVshCtx.sessions[sessionIndex].memberId = member->memberInfo.memberId;
	s_partyVshCtx.sessions[sessionIndex].updateType =
		SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_NOT_JOINABLE;
	s_partyVshCtx.sessionCount = static_cast<SceUInt8>(sessionIndex + 1);

	SceNpPartyMemberInfo gameMember;
	buildGameMemberInfo(member, &gameMember);
	npPartyIpcSendGameMemberJoined(&gameMember);
	npPartyIpcNotifyObserversChanged();
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386C04 */
int npPartyVshOnAppMemberPresenceChanged(const NpPartyAppRoomSnapshot *snapshot,
					 const NpPartyAppMemberRecord *member)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.room = *snapshot;
	SceUInt32 sdkVersion = s_partyVshCtx.gameInitParam.sdkVersion;
	if (sdkVersion >= NP_PARTY_SDK_VERSION_PRESENCE_MIN &&
	    sdkVersion < NP_PARTY_SDK_VERSION_PRESENCE_DEP) {
		SceNpPartyMemberInfo gameMember;
		buildGameMemberInfo(member, &gameMember);
		npPartyIpcSendGameMemberPresence(&gameMember);
	}
	npPartyIpcNotifyObserversChanged();

	if (member->memberInfo.memberId == s_partyVshCtx.room.selfInfo.memberInfo.memberId) {
		if (s_partyVshCtx.voiceSuspendState == NP_PARTY_VOICE_RESUME_PENDING) {
			npPartyIpcSendAppReEnableVoice();
			showPartyVoiceNotice(1);
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_RESUMED;
		} else if (s_partyVshCtx.voiceSuspendState == NP_PARTY_VOICE_SUSPEND_PENDING) {
			npPartyIpcSendAppDisableVoice();
			showPartyVoiceNotice(0);
			s_partyVshCtx.voiceSuspendState = NP_PARTY_VOICE_SUSPENDED;
		}
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386CDA */
int npPartyVshOnAppMemberVoiceChanged(const SceNpPartyMemberVoiceInfo *voiceInfo)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
		if (s_partyVshCtx.room.members[i].memberInfo.memberId == voiceInfo->memberId) {
			s_partyVshCtx.voiceStates[i] = *voiceInfo;
			SceKernelSysClock now;
			now.quad = 0;
			sceKernelGetProcessTime(&now);
			s_partyVshCtx.voiceTalkingTimestamps[i] = now;
		}
	}
	s_partyVshCtx.voiceStateCount = static_cast<SceUInt8>(s_partyVshCtx.room.memberNum);

	if (s_partyVshCtx.gameHandlers.voiceEvents != 0)
		npPartyIpcSendGameMemberVoice(voiceInfo);

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386DA4 */
int npPartyVshOnAppMemberSessCommIdUpdate(SceNpPartyRoomMemberId memberId,
					  const SceNpCommunicationId *commId)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	for (SceUInt8 i = 0; i < s_partyVshCtx.sessionCount; ++i) {
		NpPartyMemberSessionSlot &slot = s_partyVshCtx.sessions[i];
		if (slot.memberId != memberId)
			continue;
		if (sce_paf_memcmp(&slot.commId, commId, sizeof(SceNpCommunicationId)) == 0)
			continue;

		slot.commId = *commId;
		if (sce_paf_memcmp(&slot.commId, &s_partyVshCtx.gameHandlers.gameCommId,
				   sizeof(SceNpCommunicationId)) == 0 &&
		    slot.updateType == SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_JOINABLE) {
			SceNpPartyMemberGameSessionUpdateInfo updateInfo;
			sce_paf_memset(&updateInfo, 0, sizeof(updateInfo));
			updateInfo.type = slot.updateType;
			updateInfo.memberId = memberId;
			updateInfo.sessionId = slot.sessionId;
			npPartyIpcSendGameMemberSession(&updateInfo);
		}
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81386EF8 */
int npPartyVshOnAppMemberGameSessionUpdate(const NpPartyAppGameSessionPayload *payload)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	SceNpPartyRoomMemberId targetId = payload->sessionInfo.memberId;
	for (SceUInt8 i = 0; i < s_partyVshCtx.sessionCount; ++i) {
		NpPartyMemberSessionSlot &slot = s_partyVshCtx.sessions[i];
		if (slot.memberId != targetId)
			continue;

		if (slot.updateType != payload->sessionInfo.type ||
		    sce_paf_memcmp(&slot.sessionId, &payload->sessionInfo.sessionId,
				   sizeof(SceNpSessionId)) != 0) {
			slot.sessionId = payload->sessionInfo.sessionId;
			slot.updateType = payload->sessionInfo.type;

			if (s_partyVshCtx.gameHandlers.gameSessionSystemReady != 0 &&
			    targetId != s_partyVshCtx.room.selfInfo.memberInfo.memberId &&
			    sce_paf_memcmp(&s_partyVshCtx.gameHandlers.gameCommId, &slot.commId,
					   sizeof(SceNpCommunicationId)) == 0) {
				npPartyIpcSendGameMemberSession(&payload->sessionInfo);
			}
		}
		break;
	}

	for (SceUInt32 i = 0; i < s_partyVshCtx.room.memberNum; ++i) {
		if (s_partyVshCtx.room.members[i].memberInfo.memberId == targetId) {
			sce_paf_memcpy(s_partyVshCtx.room.members[i].memberInfo.gamePresence.title,
				       payload->gamePresenceData,
				       sizeof(payload->gamePresenceData));
			npPartyIpcNotifyObserversChanged();
			break;
		}
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81387090 */
int npPartyVshOnAppGameSessionUpdateReady(const SceNpPartyGameSessionUpdateReady *ready,
					  const SceNpCommunicationId *gameCommId)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	s_partyVshCtx.gameHandlers.gameSessionReadyResult = ready->result;
	s_partyVshCtx.gameHandlers.gameSessionReadyValid = 1;
	if (ready->result != 0) {
		s_partyVshCtx.gameHandlers.gameSessionSystemReady = 0;
		sce_paf_memset(&s_partyVshCtx.gameHandlers.gameCommId, 0,
			       sizeof(s_partyVshCtx.gameHandlers.gameCommId));
	} else {
		s_partyVshCtx.gameHandlers.gameCommId = *gameCommId;
		s_partyVshCtx.gameHandlers.gameSessionSystemReady = 1;
	}

	SceNpPartyMemberGameSessionUpdateInfo readyMembers[SCE_NP_PARTY_MEMBER_NUM_MAX];
	sce_paf_memset(readyMembers, 0, sizeof(readyMembers));
	SceUInt8 readyCount = 0;

	if (s_partyVshCtx.gameHandlers.gameSessionSystemReady != 0) {
		SceNpPartyRoomMemberId selfMemberId =
			s_partyVshCtx.room.selfInfo.memberInfo.memberId;
		for (SceUInt8 i = 0; i < s_partyVshCtx.sessionCount; ++i) {
			const NpPartyMemberSessionSlot &slot = s_partyVshCtx.sessions[i];
			if (slot.memberId == selfMemberId)
				continue;

			SceNpPartyMemberGameSessionUpdateInfo &out = readyMembers[readyCount];
			out.memberId = slot.memberId;
			if (sce_paf_memcmp(&slot.commId, &s_partyVshCtx.gameHandlers.gameCommId,
					   sizeof(SceNpCommunicationId)) != 0) {
				out.type = SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_NOT_JOINABLE;
			} else {
				out.type = slot.updateType;
				out.sessionId = slot.sessionId;
			}
			++readyCount;
		}
	}

	npPartyIpcSendGameSessionReady(ready, readyMembers, readyCount);
	return partyVshContextUnlock(&s_partyVshCtx);
}

/* 0x81387222 */
int npPartyVshOnAppLeftParty(const SceNpPartyRoomLeftReason *reason, SceUInt8 appStopped)
{
	int ret = partyVshContextLock(&s_partyVshCtx);
	if (ret < 0)
		return ret;

	if (appStopped != 0 && s_partyVshCtx.room.memberNum != 0) {
		paf::job::JobQueue *bgQueue = shellGetNpBackgroundJobQueue();
		int webApiLibCtx = 0;
		if (bgQueue != NULL && shellNpGetWebApiLibCtx(&webApiLibCtx) >= 0) {
			PartyLeaveSessionJob *leaveJob = new PartyLeaveSessionJob(
				s_partyVshCtx.roomSessionId,
				s_partyVshCtx.room.selfInfo.memberInfo.npId.handle.data,
				webApiLibCtx);
			if (leaveJob != NULL) {
				paf::common::SharedPtr<paf::job::JobItem> jobRef(leaveJob);
				bgQueue->Enqueue(jobRef);
			}
		}
	}

	SceUInt32 previousMemberNum = s_partyVshCtx.room.memberNum;
	sce_paf_memset(&s_partyVshCtx.room, 0, sizeof(s_partyVshCtx.room));
	sce_paf_memset(s_partyVshCtx.voiceStates, 0,
		       sizeof(s_partyVshCtx.voiceStates) + sizeof(s_partyVshCtx.voiceStateCount) +
		       sizeof(s_partyVshCtx.voiceAlignment));
	sce_paf_memset(s_partyVshCtx.sessions, 0,
		       sizeof(s_partyVshCtx.sessions) + sizeof(s_partyVshCtx.sessionCount) +
		       sizeof(s_partyVshCtx.sessionAlignment));
	s_partyVshCtx.partyState = SCE_NP_PARTY_STATE_NOT_IN_PARTY;

	if (previousMemberNum != 0) {
		npPartyIpcSendGameLeft(reason);
		npPartyIpcNotifyObserversChanged();
	}

	return partyVshContextUnlock(&s_partyVshCtx);
}
