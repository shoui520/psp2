/*
 * SceNpActivityNet's exports: the library's heap, its contexts and the requests run on them,
 * and the utility's cache of the host and trophy icon URLs (no binary names this file; the
 * function names are ours, see np_activity.h).
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "np_activity.h"
#include "common/activity_memory.h"
#include "common/activity_lock_manager.h"
#include "common/activity_simple_lock.h"
#include "context_manager.h"
#include "component/context/component_context.h"
#include "component/utility/component_utility.h"

/* the list's head: a context's fields up to its links */
#define CONTEXT_LIST_HEAD_SIZE	offsetof(ComponentContext, m_state)

static AbstractContextManager *s_manager = NULL;
static ComponentContext *s_contextList = NULL;	/* the head of the library's contexts */

/* the action URL of a user's status: pspr:show?...&jid=<jid>&... */
#define ACTION_URL_PREFIX	"pspr:show?"
#define ACTION_URL_JID		"jid="
#define ACTION_URL_SIZE		0x401
#define JID_SIZE		64

static inline void term(void)
{
	if (s_manager != NULL) {
		delete s_manager;
		s_manager = NULL;
		if (s_contextList != NULL) {
			activityFree(s_contextList);
			s_contextList = NULL;
		}
		ActivityLockManager::Finalize();
		activityMemoryTerm();
	}
}

/* the context with the handle, with the lock held */
static inline ComponentContext *findContextLocked(int context)
{
	ComponentContext *ctx;

	if (context == 0)
		return NULL;
	for (ctx = s_contextList->m_next; ctx != NULL; ctx = ctx->m_next) {
		if (ctx == (ComponentContext *)context)
			return ctx;
	}
	return NULL;
}

static inline ComponentContext *findContext(int context)
{
	if (context == 0)
		return NULL;

	ActivitySimpleLock lock(ActivityLockManager::GetInstance()->GetMutex());

	return findContextLocked(context);
}

static __attribute__((noinline)) int destroyContext(int context)
{
	ComponentContext *ctx;
	ComponentContext *next;
	int ret;

	if (s_manager == NULL || s_contextList == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (context == 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	for (ctx = s_contextList->m_next; ctx != NULL; ctx = next) {
		next = ctx->m_next;
		if (ctx == (ComponentContext *)context) {
			if (ctx->m_prev != NULL)
				ctx->m_prev->m_next = ctx->m_next;
			if (ctx->m_next != NULL)
				ctx->m_next->m_prev = ctx->m_prev;

			ret = ((ComponentContext *)context)->Finalize();
			s_manager->DestroyComponent((ComponentContext *)context);
			return ret;
		}
	}
	return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
}

int SceNpActivityNet_2351D75A(void)
{
	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	term();
	return 0;
}

static inline int init(void)
{
	int ret;

	s_manager = new ContextManager;
	s_contextList = (ComponentContext *)activityMalloc(CONTEXT_LIST_HEAD_SIZE);
	if (s_manager == NULL || s_contextList == NULL) {
		term();
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(s_contextList, 0, CONTEXT_LIST_HEAD_SIZE);

	ret = ActivityLockManager::Initialize();
	if (ACTIVITY_FAILED(ret))
		term();
	else
		ComponentUtility::ClearCache();
	return ret;
}

int SceNpActivityNet_348268A0(ActivityMallocFunction mallocFunc, ActivityFreeFunction freeFunc)
{
	if (s_manager != NULL)
		return NP_ACTIVITY_ERROR_ALREADY_INITIALIZED;
	if (mallocFunc == NULL || freeFunc == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	activityMemoryInitWithFunctions(mallocFunc, freeFunc);
	return init();
}

int SceNpActivityNet_5C07C7E5(SceSize size, void *memblock)
{
	if (s_manager != NULL)
		return NP_ACTIVITY_ERROR_ALREADY_INITIALIZED;
	activityMemoryInit(size, memblock);
	return init();
}

int SceNpActivityNet_26B9D4A3(int *initialized)
{
	if (initialized == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	*initialized = s_manager != NULL;
	return 0;
}

int SceNpActivityNet_F8049E41(int *context)
{
	ComponentContext *ctx;
	ComponentContext *last;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;

	ActivitySimpleLock lock(ActivityLockManager::GetInstance()->GetMutex());

	if (s_manager == NULL || s_contextList == NULL) {
		ret = NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	} else {
		ctx = s_manager->CreateContext();
		if (ctx == NULL) {
			ret = NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
		} else {
			ctx->SetManager(s_manager);
			ret = ctx->Initialize();
			if (!ACTIVITY_FAILED(ret)) {
				*context = (int)ctx;
				last = s_contextList;
				while (last->m_next != NULL)
					last = last->m_next;
				last->m_next = ctx;
				ctx->m_next = NULL;
				ctx->m_prev = last;
			}
		}
	}
	if (!ACTIVITY_FAILED(ret)) {
		ctx = findContextLocked(*context);
		ret = NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
		if (ctx != NULL)
			ret = ctx->AllocStaticResourceBuffer();
	}
	return ret;
}

int SceNpActivityNet_72346DA7(int context)
{
	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;

	ActivitySimpleLock lock(ActivityLockManager::GetInstance()->GetMutex());

	return destroyContext(context);
}

int SceNpActivityNet_35B49CA4(int context)
{
	ComponentContext *ctx;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;

	ActivitySimpleLock lock(ActivityLockManager::GetInstance()->GetMutex());

	ctx = findContextLocked(context);
	ret = NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (ctx != NULL)
		ret = ctx->ExecuteAbort();
	return ret;
}

/* the requests: each runs on its context without the library's lock */
#define CONTEXT_REQUEST(call)						\
	ComponentContext *ctx;						\
	int ret;							\
									\
	if (s_manager == NULL)						\
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;		\
	ctx = findContext(context);					\
	ret = NP_ACTIVITY_ERROR_INVALID_ARGUMENT;			\
	if (ctx != NULL)						\
		ret = ctx->call;					\
	return ret

int SceNpActivityNet_BE4CC2D1(int context, int mode)
{
	CONTEXT_REQUEST(OpenConnection(mode));
}

int SceNpActivityNet_F3586055(int context)
{
	CONTEXT_REQUEST(CloseConnection());
}

int SceNpActivityNet_48B5F8B3(int context, const SceNpId *npId, int *setting)
{
	CONTEXT_REQUEST(GetAccountInformation(npId, setting));
}

int SceNpActivityNet_42CFC0DE(int context, const SceNpId *npId, const int *setting)
{
	CONTEXT_REQUEST(SetAccountInformation(npId, setting));
}

int SceNpActivityNet_C55B9299(int context, const SceNpId *npId, const SceUInt64 *since,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	NpActivityUpdates *updates)
{
	ComponentContext *ctx;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (updates == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ctx = findContext(context);
	ret = NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (ctx != NULL)
		ret = ctx->CheckStatusUpdate(npId, since, titleIds, count, updates);
	return ret;
}

int SceNpActivityNet_99C86ADA(int context, const SceNpId *npId, const NpActivityRange *range,
	NpActivityStatus *statuses, int max, int *count)
{
	CONTEXT_REQUEST(GetFriendStatusTimeline(npId, range, statuses, max, count));
}

int SceNpActivityNet_40475264(int context, const SceNpId *npId,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
	const NpActivityRange *range, NpActivityStatusList *lists)
{
	CONTEXT_REQUEST(GetFriendStatusTimelineByTitle(npId, titleIds, count, range, lists));
}

int SceNpActivityNet_2000C248(int context, const SceNpId *npId, const NpActivityOwner *owners,
	SceSize count, int *max, NpActivityStatusList *lists)
{
	CONTEXT_REQUEST(GetStatusTimelineByAccount(npId, owners, count, max, lists));
}

int SceNpActivityNet_363CB63A(int context, const SceNpId *npId, const NpActivityOwner *owners,
	SceSize count, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max,
	NpActivityStatusList *lists)
{
	CONTEXT_REQUEST(GetStatusTimelineByAccountAndTitle(npId, owners, count, titleIds, max,
		lists));
}

int SceNpActivityNet_AD0B8FDD(int context, const SceNpId *npId, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, NpActivityStatusDetail *details, SceSize max,
	int *numDetails)
{
	CONTEXT_REQUEST(GetStatusInfo(npId, keys, count, locale, details, max, numDetails));
}

int SceNpActivityNet_7E58E9B9(int context, const SceNpId *npId, const NpActivityStatusKey *keys,
	SceSize count, const int *locale, void *work, NpActivityDetailState *state,
	NpActivityStatusDetailCallback callback, void *arg)
{
	CONTEXT_REQUEST(GetStatusInfoWithCallback(npId, keys, count, locale, work, state,
		callback, arg));
}

int SceNpActivityNet_7E3116FB(int context, const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityDetailParam *param, NpActivityStatusInfo *info,
	NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
	SceNpId *voters, int maxVoters, SceUInt32 *numVoters)
{
	ComponentContext *ctx;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (isVoted == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	*isVoted = 0;
	ctx = findContext(context);
	ret = NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (ctx != NULL)
		ret = ctx->GetStatusDetail(npId, key, param, info, comments, maxComments,
			numComments, isVoted, voters, maxVoters, numVoters);
	return ret;
}

int SceNpActivityNet_5A0524AD(int context, const SceNpId *npId, const NpActivityStatusInfo *infos,
	SceSize count, NpActivityPostResult *results)
{
	CONTEXT_REQUEST(AddStatusList(npId, infos, count, results));
}

int SceNpActivityNet_DA59D2CF(int context, const SceNpId *npId, int statusId)
{
	CONTEXT_REQUEST(RemoveStatus(npId, statusId));
}

int SceNpActivityNet_747FBB53(int context, const SceNpId *npId, const NpActivityStatusKey *key,
	const NpActivityCommentPost *comment, NpActivityCommentRequest *request)
{
	CONTEXT_REQUEST(AddComment(npId, key, comment, request));
}

int SceNpActivityNet_92CF4044(int context, const SceNpId *npId, const NpActivityCommentKey *key,
	NpActivityRequest *request)
{
	CONTEXT_REQUEST(RemoveComment(npId, key, request));
}

int SceNpActivityNet_75F6CD31(int context, const SceNpId *npId, const NpActivityStatusKey *key,
	NpActivityRequest *request)
{
	CONTEXT_REQUEST(AddVoteGoodJob(npId, key, request));
}

int SceNpActivityNet_690808DA(int context, const SceNpId *npId, const NpActivityStatusKey *key,
	NpActivityRequest *request)
{
	CONTEXT_REQUEST(RemoveVoteGoodJob(npId, key, request));
}

int SceNpActivityNet_319CB4F4(const SceNpId *npId, char *onlineId, SceSize size)
{
	ComponentUtility *utility;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	utility = s_manager->CreateUtility();
	if (utility == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	ret = utility->ConvertNpIdToJid(npId, onlineId, size);
	s_manager->DestroyComponent(utility);
	return ret;
}

int SceNpActivityNet_C35984F3(const char *onlineId, SceNpId *npId)
{
	ComponentUtility *utility;
	int ret;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	utility = s_manager->CreateUtility();
	if (utility == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	ret = utility->ConvertJidToNpId(onlineId, npId);
	s_manager->DestroyComponent(utility);
	return ret;
}

int SceNpActivityNet_447AC73E(int error)
{
	switch (error) {
	case (int)0x80558906:
	case (int)0x8055890C:
	case (int)0x80558921:
		return 1;
	case (int)0x80558A01:
	case (int)0x80558A03:
	case (int)0x80558A05:
	case (int)0x80558A0A:
	case (int)0x80558A11:
		return 1;
	default:
		if (error == (int)0x8055050A)
			return NP_ACTIVITY_ERROR_KIND_DISCARD;
		return NP_ACTIVITY_ERROR_KIND_RETRY;
	}
}

int SceNpActivityNet_C27E99D3(const char *actionUrl, char *onlineId, SceSize size)
{
	char jid[JID_SIZE + 1];
	SceSize prefixLength;
	SceSize keyLength;
	const char *p;
	char *dst;
	char c;
	SceSize length;

	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (actionUrl == NULL || onlineId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (size == 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	sceClibMemset(jid, 0, sizeof(jid));
	prefixLength = sceClibStrnlen(ACTION_URL_PREFIX, 0x20);
	keyLength = sceClibStrnlen(ACTION_URL_JID, 0x20);
	if (sceClibStrncmp(actionUrl, ACTION_URL_PREFIX, prefixLength) != 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (sceClibStrnlen(actionUrl, ACTION_URL_SIZE) <= prefixLength)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	/* the jid= parameter */
	p = actionUrl + prefixLength;
	if (p == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	while (sceClibStrncmp(p, ACTION_URL_JID, keyLength) != 0) {
		c = *p;
		while (c != '&' && c != '\0')
			c = *++p;
		if (c == '&')
			p++;
		if (p == NULL)
			return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}

	p += keyLength;
	c = *p;
	dst = jid;
	for (length = 0; c != '&' && c != '\0' && length < JID_SIZE; length++) {
		*dst++ = c;
		c = *++p;
	}
	if (length == 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (size < sceClibStrnlen(jid, sizeof(jid)) + 1)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	sceClibStrncpy(onlineId, jid, size);
	return 0;
}

int SceNpActivityNet_FE7F0822(const NpActivityStatusInfo *info, char *onlineId, SceSize size)
{
	if (s_manager == NULL)
		return NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (info == NULL || onlineId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (info->type != NP_ACTIVITY_STATUS_TYPE_USER)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	return SceNpActivityNet_C27E99D3(info->actionUrl, onlineId, size);
}

char ComponentUtility::s_host[UTILITY_HOST_SIZE];
int ComponentUtility::s_hostValid;
SceUInt64 ComponentUtility::s_hostTime;
char ComponentUtility::s_trophyIconBaseUrl[UTILITY_URL_SIZE];
char ComponentUtility::s_trophyIconBronze[UTILITY_URL_SIZE];
char ComponentUtility::s_trophyIconSilver[UTILITY_URL_SIZE];
char ComponentUtility::s_trophyIconGold[UTILITY_URL_SIZE];
char ComponentUtility::s_trophyIconPlatinum[UTILITY_URL_SIZE];
char ComponentUtility::s_trophyIconHidden[UTILITY_URL_SIZE];

void ComponentUtility::ClearCache()
{
	sceClibMemset(s_host, 0, sizeof(s_host));
	s_hostValid = 0;
	s_hostTime = 0;
	sceClibMemset(s_trophyIconBaseUrl, 0, sizeof(s_trophyIconBaseUrl));
	sceClibMemset(s_trophyIconBronze, 0, sizeof(s_trophyIconBronze));
	sceClibMemset(s_trophyIconSilver, 0, sizeof(s_trophyIconSilver));
	sceClibMemset(s_trophyIconGold, 0, sizeof(s_trophyIconGold));
	sceClibMemset(s_trophyIconHidden, 0, sizeof(s_trophyIconHidden));
	sceClibMemset(s_trophyIconPlatinum, 0, sizeof(s_trophyIconPlatinum));
}

int ComponentUtility::IsFunctionServer()
{
	return 0;
}

int ComponentUtility::IsHostExpired()
{
	SceUInt64 now;

	if (s_hostValid == 0 || sceClibStrnlen(s_host, sizeof(s_host)) == 0)
		return 1;
	now = 0;
	SceNpCommon_55F7FCF7(&now);
	if (now == 0 || s_hostTime > now || s_hostTime + UTILITY_HOST_LIFETIME <= now)
		return 1;
	return 0;
}

int ComponentUtility::SetHost(const char *host, SceSize length)
{
	if (host == NULL || length == 0)
		return NP_ACTIVITY_ERROR_INVALID_PARAMETER;
	sceClibSnprintf(s_host, sizeof(s_host), "%s", host);
	SceNpCommon_55F7FCF7(&s_hostTime);
	s_hostValid = 1;
	return 0;
}

int ComponentUtility::SetTrophyIconUrls(const char *base, const char *bronze,
	const char *silver, const char *gold, const char *hidden, const char *platinum)
{
	if (base == NULL || bronze == NULL || silver == NULL || gold == NULL || hidden == NULL ||
	    platinum == NULL)
		return NP_ACTIVITY_ERROR_INVALID_PARAMETER;
	sceClibSnprintf(s_trophyIconBaseUrl, sizeof(s_trophyIconBaseUrl), "%s", base);
	sceClibSnprintf(s_trophyIconBronze, sizeof(s_trophyIconBronze), "%s", bronze);
	sceClibSnprintf(s_trophyIconSilver, sizeof(s_trophyIconSilver), "%s", silver);
	sceClibSnprintf(s_trophyIconGold, sizeof(s_trophyIconGold), "%s", gold);
	sceClibSnprintf(s_trophyIconHidden, sizeof(s_trophyIconHidden), "%s", hidden);
	sceClibSnprintf(s_trophyIconPlatinum, sizeof(s_trophyIconPlatinum), "%s", platinum);
	return 0;
}
