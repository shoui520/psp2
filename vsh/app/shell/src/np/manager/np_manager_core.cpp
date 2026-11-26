/*
 * np_manager_core.cpp: SceNpManager account/session state machine, callback
 * dispatcher, request pool, ticket cache, registry synchronization, and
 * myprofile.dat persistence (0x81326690..0x81328E67, 0x813297F0..0x81329E1F).
 */
#include "np_manager_core.h"

#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <rtc/rtc.h>
#include <np/np_auth.h>
#include <np_common/np_common.h>
#include <regmgr/regmgr.h>
#include "../auth/np_auth_core.h"

extern "C" {
extern int SceNetInternal_9DBD2F60(SceUID threadId, int enable, int reserved);
extern int SceNpCommon_652FC5E0(const char *environment);
extern int SceNpCommon_DA04BA55(const void *settings);
extern int SceNpCommon_F585C86C(void);
extern int sceAppMgrGetUserDirPath(int partitionId, char *path, SceSize size);
extern int sceAppMgrUpdateSaveDataParam(const char *path);
extern int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *outVal);
extern int sceRegMgrSetKeyIntForUser(const char *category, const char *name, int value);
extern int sceRegMgrGetKeyStrForUser(const char *category, const char *name,
				     char *buf, SceSize size);
extern int sceRegMgrSetKeyStrForUser(const char *category, const char *name,
				     const char *buf, SceSize size);
extern int sceRegMgrGetKeyBinForUser(const char *category, const char *name,
				     void *buf, SceSize size);
extern int sceRegMgrSetKeyBinForUser(const char *category, const char *name,
				     const void *buf, SceSize size);
extern int sceRegMgrGetKeysForUser(const char *category, RegMgrKeyInfo *keys, int count);
extern int sceRegMgrSetKeysForUser(const char *category, const RegMgrKeyInfo *keys, int count);
extern int sceKernelSignalCondTo(SceUID condId, SceUID threadId);
extern int vshRtcSetCurrentTick(const SceRtcTick *tick);
extern int vshRtcSetCurrentSecureTick(const SceRtcTick *tick);
extern int vshRtcSetCurrentNetworkTick(const SceRtcTick *tick);
extern int sceNpDrmCheckDrmReset(const void *data, SceSize size, SceUInt64 accountId,
				 int *outStatus);
extern SceSize sce_paf_strlen(const char *s);
extern void *sce_paf_memset(void *dst, int ch, SceSize len);
}

/* External shell subsystem hooks called during sign-in and credential writes */
extern int shellIdDatWrite(const NpIdDatUpdateInfo *info, SceUInt32 fieldMask,
			   SceUInt8 forceWrite, int unused);			/* 0x81361F44 */
extern void shellNotifyProfileSaved(void);					/* 0x813C542E */
extern int npCommunityEnsureUser(const SceNpOnlineId *onlineId);		/* 0x8132A098 */
extern int npCommunitySetUserEnv(const SceNpOnlineId *onlineId,
				 const char *envName, const char *countryCode);	/* 0x8132A0B6 */
extern int npCommunityRemoveUser(const SceNpOnlineId *onlineId);		/* 0x8132A370 */
extern int npProfileFetchOwnProfile(int profileReqHandle, void *outProfile,
				    int *outStatus, int *outFlags,
				    SceUInt64 *outTimestamp, void *outStamp,
				    const void *ticketBuf, SceSize ticketSize,
				    int mode);					/* 0x8132E250 */

#define NP_MGR_MAX_CALLBACK_SLOTS	8
#define NP_MGR_MAX_REQUEST_ID		15
#define NP_MGR_CALLBACK_MSG_QUEUE	16
#define NP_MGR_CALLBACK_PRIORITY	0x10000100
#define NP_MGR_CALLBACK_STACK_SIZE	4096U
#define NP_MGR_TICKET_MARGIN_USEC	300000000ULL
#define NP_MGR_UNIX_EPOCH_TICKS		62135596800000000ULL
#define NP_MGR_LOCALE_COUNT		20U
#define NP_MGR_FATAL_ERROR_COUNT	26U

#define NP_MGR_IO_ERR_NOT_FOUND		-2147418110	/* 0x80010002 */
#define NP_MGR_COMMUNITY_ERR_EXISTS	-2141911275	/* 0x80550715 */

/* Registry key types for RegMgrKeyInfo */
#define REGMGR_KEY_TYPE_INT		0
#define REGMGR_KEY_TYPE_STR		1
#define REGMGR_KEY_TYPE_BIN		2

/* 12-byte callback entry (0x815C5E80 / 0x815C5EE0, 8 slots each) */
struct NpManagerCallbackSlot {
	int slotId;
	NpAccountEventCallback func;
	void *userArg;
};

/* 12-byte message pipe payload for SceNpManagerCallback thread */
struct NpManagerCallbackMsg {
	int kind;			/* 0 = sign-in state change, 1 = account event */
	int signInState;
	int accountEvent;
};

/* 16-byte active request handle node */
struct NpManagerRequestNode {
	NpManagerRequestNode *next;
	int reqId;
	SceUID waitThreadId;
	int aborted;
};

/* 608-byte (0x260) primary NP account & session state (0x815C5C20) */
struct NpAccountSession {
	SceNpOnlineId onlineId;			/* +0x000 (20 bytes) */
	SceUID mutexId;				/* +0x014 */
	SceUID condId;				/* +0x018 */
	int forcedError;			/* +0x01C */
	int lastSignInResult;			/* +0x020 */
	int sysUpdateReqHandle;			/* +0x024 */
	int authReqId;				/* +0x028 */
	void *ticketBuf;			/* +0x02C */
	SceSize ticketSize;			/* +0x030 */
	SceUInt8 contentRatingFlags[4];		/* +0x034 */
	SceUInt8 regionInfo[4];			/* +0x038 */
	SceUInt16 birthYear;			/* +0x03C */
	SceUInt8 birthMonth;			/* +0x03E */
	SceUInt8 birthDay;			/* +0x03F */
	SceUInt64 ticketValidDurationUsec;	/* +0x040 */
	SceUInt64 accountId;			/* +0x048 */
	char countryCodeStr[8];			/* +0x050 */
	char profileCountryStr[8];		/* +0x058 */
	int profileReqHandle;			/* +0x060 */
	SceUInt32 padBeforeProfile;		/* +0x064 */
	NpAccountProfileCache profile;		/* +0x068 (456 bytes) */
	int profileStatus;			/* +0x230 */
	int profileFlags;			/* +0x234 */
	SceUInt64 profileTimestamp;		/* +0x238 */
	SceUInt8 profileStamp[16];		/* +0x240 */
	SceRtcTick ticketAcquiredTick;		/* +0x250 */
	int clockUpdateReqId;			/* +0x258 */
	SceUInt32 tailPad;			/* +0x25C */
};

/* 20 locale codes indexed by ticket language byte (0x8151D5EC) */
static const char *const s_npLocaleCodes[NP_MGR_LOCALE_COUNT] = {
	"ja", "en", "fr", "es", "de",
	"it", "nl", "pt", "ru", "ko",
	"ch", "zh", "fi", "sv", "da",
	"no", "pl", "pt", "en", "tr"
};

/* 26 fatal auth error codes that disable enable_np (0x8151D660) */
static const int s_fatalSignInErrors[NP_MGR_FATAL_ERROR_COUNT] = {
	-2141912064,	/* 0x80550400 */
	-2141912311,	/* 0x80550309 */
	-2141911800,	/* 0x80550508 */
	-2141912025,	/* 0x80550427 */
	-2141912016,	/* 0x80550430 */
	-2141912015,	/* 0x80550431 */
	-2141911985,	/* 0x8055044F */
	-2141912031,	/* 0x80550421 */
	-2141912000,	/* 0x80550440 */
	-2141911999,	/* 0x80550441 */
	-2141911998,	/* 0x80550442 */
	-2141911997,	/* 0x80550443 */
	-2141911996,	/* 0x80550444 */
	-2141911995,	/* 0x80550445 */
	-2141911994,	/* 0x80550446 */
	-2141911993,	/* 0x80550447 */
	-2141911992,	/* 0x80550448 */
	-2141911991,	/* 0x80550449 */
	-2141911990,	/* 0x8055044A */
	-2141911989,	/* 0x8055044B */
	-2141911988,	/* 0x8055044C */
	-2141911987,	/* 0x8055044D */
	-2141911986,	/* 0x8055044E */
	-2141911968,	/* 0x80550460 */
	-2141911967,	/* 0x80550461 */
	-2141912013	/* 0x80550433 */
};

/* Global manager state (0x815467B4..0x81546808, 0x815C5BB0..0x815C6088) */
static int s_npMgrInitialized = 0;

/* Callback subsystem state */
static SceUID s_cbMsgPipeId = -1;
static SceUID s_cbThreadId = -1;
static int s_cbInitialized = 0;
static int s_cbStopRequested = 0;
static int s_cbLastSignInState = 1;
static SceKernelLwMutexWork *s_cbMutex = NULL;
static NpManagerCallbackSlot s_accountEventCallbacks[NP_MGR_MAX_CALLBACK_SLOTS];
static NpManagerCallbackSlot s_signInStateCallbacks[NP_MGR_MAX_CALLBACK_SLOTS];

/* Request handle list state */
static NpManagerRequestNode *s_reqListHead = NULL;
static int s_reqLastId = 0;
static SceKernelLwMutexWork *s_reqMutex = NULL;

/* Cached myprofile.dat state */
static SceUInt8 s_myProfileLoaded = 0;
static NpCachedMyProfileFile s_cachedMyProfile;

/* Sign-in / ticket state */
static int s_enableNpFlag = 0;
static SceUID s_activeSignInThreadId = -1;
static int s_authTicketResult = 0;
static SceUID s_authTicketSemaId = 0;

/* In-memory credentials & account session */
static char s_cachedLoginId[68];
static char s_cachedPassword[36];
static SceUInt64 s_cachedAccountId = 0;
static NpAccountSession s_accountSession;

/* Forward declarations */
static int npMgrCallbackInit(int queueDepth, int priority, SceSize stackSize);
static void npMgrCallbackTerm(void);
static int npMgrCallbackNotifySignInState(int state);
static int npMgrCallbackNotifyAccountEvent(int eventId);
static int npMgrCallbackThreadEntry(SceSize args, void *argp);
static int npMgrRequestPoolInit(void);
static int npMgrRequestPoolTerm(void);
static NpManagerRequestNode *npMgrRequestFind(int reqId);
static int npMgrRequestAlloc(void);
static void npMgrRequestFree(NpManagerRequestNode *node);
static int npMgrRequestAbort(NpAccountSession *session, int reqId);
static int npMgrSaveMyProfile(const NpAccountSession *session);
static int npMgrLoadMyProfile(void);
static int npMgrCopyMyProfile(void *outBuf);
static int npMgrCopyValidTicket(NpAccountSession *session, void *outBuf, SceSize *ioSize);
static bool npMgrExtractValidMiniTicket(NpAccountSession *session, void *outData,
					SceSize *ioDataSize);
static int npMgrGetAutoClockSetting(void);
static int npMgrLoadEnableNpSetting(void);
static int npMgrGetEnableNp(void);
static int npMgrSetEnableNp(int enabled);
static int npMgrCheckEnableNp(void);
static int npMgrWriteRegistryCredsRaw(const char *loginId, const char *password,
				      const SceUInt64 *accountId);
static int npMgrUpdateRegistryCredsVerified(SceUInt64 accountId, const char *loginId,
					    const char *password);
static int npMgrClearRegistryCreds(void);
static int npMgrLocaleCodeFromLang(unsigned int langIndex, char *outLocale);
static int npMgrSyncRegionRegistry(const NpAccountSession *session);
static void npMgrClearCachedSession(NpAccountSession *session);
static SceUID npMgrGetActiveSignInThread(void);
static int npMgrAcquireSessionEx(NpAccountSession *session, int reqId, void *outTicket,
				 SceSize *ioSize, int flags, const void *loginId,
				 const void *password);
static int npMgrAcquireSession(NpAccountSession *session, int reqId, void *outTicket,
			       SceSize *ioSize, int flags);
static int npMgrGetCachedNpIdInternal(NpAccountSession *session, SceNpId *outNpId);
static int npMgrGetRatingAndRegion(NpAccountSession *session, SceUInt8 *outRating,
				   SceUInt8 *outRegion);
static int npMgrSignOutInternal(NpAccountSession *session, int resultCode);
static int npMgrUnlinkAccountInternal(NpAccountSession *session);
static int npMgrUpdateProfileCacheInternal(NpAccountSession *session, const void *languages,
					   const void *panelId, const SceUInt32 *panelColor,
					   const void *avatarUrl, const void *aboutMe);
static int npMgrAbortAuthTicket(NpAccountSession *session);
static int npMgrFetchAuthTicket(NpAccountSession *session, const void *overrideLogin,
				const void *overridePassword, int flags);
static int npMgrRunClockUpdate(NpAccountSession *session);
static int npMgrAuthTicketCallback(int authReqId, int resultOrSize, void *userArg);
static int npMgrCheckSystemUpdate(NpAccountSession *session);
static int npMgrAbortSystemUpdate(NpAccountSession *session);
static int npMgrAbortProfileFetch(NpAccountSession *session);
static int npMgrFetchOwnProfile(NpAccountSession *session);

/* 0x81326690 */
unsigned int npManagerCoreIsInitialized(void)
{
	return (s_npMgrInitialized > 0) ? 1U : 0U;
}

/* 0x813266A2 */
int npManagerGetCachedCredentials(void *outLoginId, void *outPassword, SceUInt64 *outAccountId)
{
	if (outLoginId != NULL)
		sceClibMemcpy_safe(outLoginId, s_cachedLoginId, sizeof(s_cachedLoginId));
	if (outPassword != NULL)
		sceClibMemcpy_safe(outPassword, s_cachedPassword, sizeof(s_cachedPassword));
	if (outAccountId != NULL)
		*outAccountId = s_cachedAccountId;
	return 0;
}

/* 0x813266DE */
int npManagerCoreInit(void)
{
	if (s_npMgrInitialized > 0)
		return NP_MGR_ERR_ALREADY_INIT;

	sceClibMemset(&s_accountSession, 0, sizeof(s_accountSession));
	int ret = npMgrLoadEnableNpSetting();
	if (ret >= 0) {
		SceUID mutexId = sceKernelCreateMutex("SceNpManager", 2U, 0, NULL);
		ret = mutexId;
		if (mutexId >= 0) {
			s_accountSession.mutexId = mutexId;
			ret = sceKernelCreateCond("SceNpManager", 0, mutexId, NULL);
			if (ret >= 0) {
				s_accountSession.condId = ret;
				ret = npMgrRequestPoolInit();
			}
			if (ret >= 0) {
				ret = npMgrCallbackInit(NP_MGR_CALLBACK_MSG_QUEUE,
							NP_MGR_CALLBACK_PRIORITY,
							NP_MGR_CALLBACK_STACK_SIZE);
			}
		}
	}

	if (ret < 0) {
		npMgrCallbackTerm();
		npMgrRequestPoolTerm();
		if (s_accountSession.condId > 0) {
			sceKernelDeleteCond(s_accountSession.condId);
			s_accountSession.condId = 0;
		}
		if (s_accountSession.mutexId > 0) {
			sceKernelDeleteMutex(s_accountSession.mutexId);
			s_accountSession.mutexId = 0;
		}
		return ret;
	}

	sceClibMemset(s_cachedLoginId, 0, sizeof(s_cachedLoginId));
	sceClibMemset(s_cachedPassword, 0, sizeof(s_cachedPassword));
	s_cachedAccountId = 0;

	sceRegMgrGetKeyStrForUser("/CONFIG/NP", "login_id", s_cachedLoginId, 65);
	ret = sceRegMgrGetKeyStrForUser("/CONFIG/NP", "password", s_cachedPassword, 33);
	if (ret >= 0) {
		ret = sceRegMgrGetKeyBinForUser("/CONFIG/NP", "account_id",
						&s_cachedAccountId, sizeof(s_cachedAccountId));
	}
	if (ret >= 0) {
		char envName[20];
		ret = sceRegMgrGetKeyStrForUser("/CONFIG/NP", "env", envName, 17);
		if (ret >= 0) {
			SceNpCommon_652FC5E0(envName);
			NpCommonSettings settings;
			ret = npCommonServiceGetSettings(&settings);
			if (ret >= 0) {
				SceNpCommon_DA04BA55(&settings);
				npMgrLoadMyProfile();
				s_npMgrInitialized = 1;
				return ret;
			}
		}
	}
	return ret;
}

/* 0x813268BC */
int npAccountCreateRequest(void)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrRequestAlloc();
}

/* 0x813268DC */
int npAccountDestroyRequest(int reqHandle)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	NpManagerRequestNode *node = npMgrRequestFind(reqHandle);
	if (node != NULL)
		npMgrRequestFree(node);
	return 0;
}

/* 0x81326906 */
int npAccountGetTicket(int reqHandle, void *ticketBuf, SceSize *ioSize)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrAcquireSession(&s_accountSession, reqHandle, ticketBuf, ioSize, 0);
}

/* 0x8132693C */
int npAccountGetTicketForService(int reqHandle, void *ticketBuf, SceSize *ioSize, int flags)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrAcquireSession(&s_accountSession, reqHandle, ticketBuf, ioSize, flags);
}

/* 0x81326976 */
int npAccountGetTicketCached(int reqHandle, void *ticketBuf, SceSize *ioSize)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrAcquireSession(&s_accountSession, reqHandle, ticketBuf, ioSize,
				   NP_MGR_ACQUIRE_FORCE_REFRESH);
}

/* 0x813269AC */
int npAccountGetTicketForce(int reqHandle, void *ticketBuf, SceSize *ioSize)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrAcquireSession(&s_accountSession, reqHandle, ticketBuf, ioSize,
				   NP_MGR_ACQUIRE_NET_INTERNAL);
}

/* 0x813269E2 */
int npAccountAbortRequest(int reqHandle)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrRequestAbort(&s_accountSession, reqHandle);
}

/* 0x81326A0C */
int npAccountGetNpId(int reqHandle, SceNpId *outNpId)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (outNpId == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	if (s_accountSession.ticketBuf == NULL || s_accountSession.ticketSize == 0 ||
	    s_accountSession.profile.npId.handle.data[0] == '\0') {
		int ret = npMgrAcquireSession(&s_accountSession, reqHandle, NULL, NULL, 0);
		if (ret < 0)
			return ret;
	}

	sceClibMemcpy_safe(outNpId, &s_accountSession.profile.npId, sizeof(SceNpId));
	return 0;
}

/* 0x81326A92 */
int npAccountGetNpIdForce(int reqHandle, SceNpId *outNpId)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (outNpId == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	if (s_accountSession.ticketBuf == NULL || s_accountSession.ticketSize == 0 ||
	    s_accountSession.profile.npId.handle.data[0] == '\0') {
		int ret = npMgrAcquireSession(&s_accountSession, reqHandle, NULL, NULL,
					      NP_MGR_ACQUIRE_NET_INTERNAL);
		if (ret < 0)
			return ret;
	}

	sceClibMemcpy_safe(outNpId, &s_accountSession.profile.npId, sizeof(SceNpId));
	return 0;
}

/* 0x81326B18 */
int npAccountGetProfileAndMeta(int reqHandle, void *profileBuf, void *metaBuf)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (profileBuf == NULL && metaBuf == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	if (s_accountSession.ticketBuf == NULL || s_accountSession.ticketSize == 0 ||
	    s_accountSession.profile.npId.handle.data[0] == '\0') {
		int ret = npMgrAcquireSession(&s_accountSession, reqHandle, NULL, NULL, 0);
		if (ret < 0)
			return ret;
	}

	if (profileBuf != NULL) {
		NpAccountProfileWithStatus *outProfile =
			static_cast<NpAccountProfileWithStatus *>(profileBuf);
		sceClibMemcpy_safe(&outProfile->profile, &s_accountSession.profile,
				   sizeof(NpAccountProfileCache));
		outProfile->extraStatus = static_cast<SceUInt32>(s_accountSession.profileTimestamp);
	}
	if (metaBuf != NULL) {
		NpAccountMetaInfo *outMeta = static_cast<NpAccountMetaInfo *>(metaBuf);
		outMeta->accountId = s_accountSession.accountId;
		sceClibMemcpy_safe(outMeta->regionInfo, s_accountSession.regionInfo,
				   sizeof(outMeta->regionInfo));
		sceClibMemcpy_safe(outMeta->contentRatingFlags, s_accountSession.contentRatingFlags,
				   sizeof(outMeta->contentRatingFlags));
	}
	return 0;
}

/* 0x81326BDE */
int npAccountUpdateCredentials(int reqHandle, const void *loginId, const void *password)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (loginId == NULL || password == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	int ret = npMgrAcquireSessionEx(&s_accountSession, reqHandle, NULL, NULL, 0,
					loginId, password);
	if (ret >= 0) {
		sceClibMemcpy_safe(s_cachedLoginId, loginId, sizeof(s_cachedLoginId));
		sceClibMemcpy_safe(s_cachedPassword, password, sizeof(s_cachedPassword));
		s_cachedAccountId = s_accountSession.accountId;
	}
	return ret;
}

/* 0x81326C90 */
void npAccountNotifyBootSignIn(void)
{
	int enableNp = 0;
	if (sceRegMgrGetKeyIntForUser("/CONFIG/NP", "enable_np", &enableNp) >= 0 &&
	    enableNp == 1) {
		npMgrCallbackNotifySignInState(0);
	}
}

/* 0x81326CDC */
int npAccountClearSession(void)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrSignOutInternal(&s_accountSession, 0);
}

/* 0x81326D06 */
int npAccountClearSessionWithResult(int resultCode)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrSignOutInternal(&s_accountSession, resultCode);
}

/* 0x81326D30 */
int npAccountIsSignedIn(void)
{
	return npMgrGetEnableNp();
}

/* 0x81326D38 */
int npAccountSetCredentials(SceUInt64 accountId, const void *loginId, const void *password)
{
	return npMgrUpdateRegistryCredsVerified(accountId,
						static_cast<const char *>(loginId),
						static_cast<const char *>(password));
}

/* 0x81326D40 */
int npAccountGetOnlineId(void *outOnlineId)
{
	if (s_accountSession.onlineId.data[0] == '\0')
		return NP_MGR_ERR_NO_ONLINE_ID;
	sceClibMemcpy_safe(outOnlineId, &s_accountSession.onlineId, sizeof(SceNpOnlineId));
	return 0;
}

/* 0x81326D6C */
int npAccountClearTicketCache(void)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrUnlinkAccountInternal(&s_accountSession);
}

/* 0x81326D94 */
int npAccountGetCachedAccountInfo(void *outInfo)
{
	return npMgrCopyMyProfile(outInfo);
}

/* 0x81326D9C */
int npAccountHasCachedTicket(void)
{
	return npMgrCopyValidTicket(&s_accountSession, NULL, NULL);
}

/* 0x81326DB0 */
int npAccountCopyCachedTicket(void *outBuf, SceSize *ioSize)
{
	if (outBuf == NULL || ioSize == NULL)
		return NP_MGR_ERR_INVALID_ARG;
	if (*ioSize < s_accountSession.ticketSize)
		return NP_MGR_ERR_BUFFER_TOO_SMALL;
	if (npMgrCopyValidTicket(&s_accountSession, outBuf, ioSize) == 1)
		return 0;
	return NP_MGR_ERR_NO_TICKET;
}

/* 0x81326DFA */
int npAccountCheckTicketValid(void *outTicket, SceSize *outSize)
{
	if (npMgrExtractValidMiniTicket(&s_accountSession, outTicket, outSize))
		return 0;
	return NP_MGR_ERR_NO_TICKET;
}

/* 0x81326E1E */
int npAccountGetRegion(SceNpCountryCode *countryCode, int *languageCode)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (countryCode == NULL || languageCode == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	SceUInt8 region[4];
	int ret = npMgrGetRatingAndRegion(&s_accountSession, NULL, region);
	if (ret < 0)
		return ret;

	countryCode->data[0] = static_cast<char>(region[0]);
	countryCode->data[1] = static_cast<char>(region[1]);
	countryCode->term = '\0';
	*languageCode = region[3];
	return 0;
}

/* 0x81326E96 */
int npAccountGetRegionAndLocale(SceNpCountryCode *countryCode, char *localeStr)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (countryCode == NULL || localeStr == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	SceUInt8 region[4];
	int ret = npMgrGetRatingAndRegion(&s_accountSession, NULL, region);
	if (ret < 0)
		return ret;

	countryCode->data[0] = static_cast<char>(region[0]);
	countryCode->data[1] = static_cast<char>(region[1]);
	countryCode->term = '\0';
	return npMgrLocaleCodeFromLang(region[3], localeStr);
}

/* 0x81326F12 */
int npAccountGetContentRating(int *isRestricted, int *age)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (isRestricted == NULL || age == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	SceUInt8 rating[4];
	int ret = npMgrGetRatingAndRegion(&s_accountSession, rating, NULL);
	if (ret < 0)
		return ret;

	*isRestricted = ((rating[2] & 0x40U) != 0) ? 1 : 0;
	*age = static_cast<int>(rating[0] >> 1);
	return 0;
}

/* 0x81326F8A */
int npAccountGetChatRestriction(int *isRestricted)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (isRestricted == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	SceUInt8 rating[4];
	int ret = npMgrGetRatingAndRegion(&s_accountSession, rating, NULL);
	if (ret < 0)
		return ret;

	*isRestricted = ((rating[2] & 0x80U) != 0) ? 1 : 0;
	return 0;
}

/* 0x81326FF4 */
int npAccountReadRegistryCredentials(void *loginId, void *password, SceUInt64 *accountId)
{
	char loginBuf[68];
	char passwordBuf[36];
	SceUInt64 accountBuf = 0;

	if (loginId != NULL)
		sceClibMemset(loginId, 0, sizeof(loginBuf));
	if (password != NULL)
		sceClibMemset(password, 0, sizeof(passwordBuf));

	RegMgrKeyInfo keys[3];
	sceClibMemset(keys, 0, sizeof(keys));

	keys[0].size = sizeof(RegMgrKeyInfo);
	keys[0].name = "login_id";
	keys[0].type = REGMGR_KEY_TYPE_STR;
	keys[0].valueSize = 65;
	keys[0].data = loginBuf;

	keys[1].size = sizeof(RegMgrKeyInfo);
	keys[1].name = "password";
	keys[1].type = REGMGR_KEY_TYPE_STR;
	keys[1].valueSize = 33;
	keys[1].data = passwordBuf;

	keys[2].size = sizeof(RegMgrKeyInfo);
	keys[2].name = "account_id";
	keys[2].type = REGMGR_KEY_TYPE_BIN;
	keys[2].valueSize = sizeof(accountBuf);
	keys[2].data = &accountBuf;

	int ret = sceRegMgrGetKeysForUser("/CONFIG/NP", keys, 3);
	if (ret >= 0) {
		if (loginId != NULL)
			sceClibMemcpy_safe(loginId, loginBuf, sizeof(loginBuf));
		if (password != NULL)
			sceClibMemcpy_safe(password, passwordBuf, sizeof(passwordBuf));
		if (accountId != NULL)
			sceClibMemcpy_safe(accountId, &accountBuf, sizeof(accountBuf));
	}
	return ret;
}

/* 0x813270D8 */
int npAccountWriteRegistryCredentials(const void *loginId, const void *password,
				      const SceUInt64 *accountId)
{
	NpIdDatUpdateInfo idDat;
	idDat.accountId = 0;
	idDat.sysVersion = 0;
	idDat.hasDigest = 0;

	int ret = npMgrWriteRegistryCredsRaw(static_cast<const char *>(loginId),
					     static_cast<const char *>(password),
					     accountId);
	if (ret < 0)
		return ret;

	if (loginId != NULL)
		sceClibMemcpy_safe(s_cachedLoginId, loginId, sizeof(s_cachedLoginId));
	if (password != NULL)
		sceClibMemcpy_safe(s_cachedPassword, password, sizeof(s_cachedPassword));
	if (accountId != NULL)
		s_cachedAccountId = *accountId;

	sceAppMgrUpdateSaveDataParam("ux0:");
	if (accountId != NULL) {
		idDat.accountId = *accountId;
		shellIdDatWrite(&idDat, 4U, 0, 0);
	}
	return 0;
}

/* 0x81327226 */
int npAccountWriteRegistryCredentialsEx(const void *loginId, const void *password,
					const SceUInt64 *accountId, const char *onlineId)
{
	NpIdDatUpdateInfo idDat;
	idDat.accountId = 0;
	idDat.sysVersion = 0;
	idDat.hasDigest = 0;

	int ret = npMgrWriteRegistryCredsRaw(static_cast<const char *>(loginId),
					     static_cast<const char *>(password),
					     accountId);
	if (ret < 0)
		return ret;

	if (loginId != NULL)
		sceClibMemcpy_safe(s_cachedLoginId, loginId, sizeof(s_cachedLoginId));
	if (password != NULL)
		sceClibMemcpy_safe(s_cachedPassword, password, sizeof(s_cachedPassword));
	if (accountId != NULL)
		s_cachedAccountId = *accountId;

	sceAppMgrUpdateSaveDataParam("ux0:");

	SceUInt32 fieldMask = 0;
	if (accountId != NULL) {
		fieldMask = 4U;
		idDat.accountId = *accountId;
	}
	if (onlineId != NULL) {
		fieldMask |= 8U;
		idDat.onlineId.assign(onlineId, sce_paf_strlen(onlineId));
	}
	if (fieldMask != 0)
		shellIdDatWrite(&idDat, fieldMask, 0, 0);
	return 0;
}

/* 0x813273AA */
int npAccountGetTicketVersion(void)
{
	return s_accountSession.lastSignInResult;
}

/* 0x813273B6 */
int npAccountGetCachedNpId(SceNpId *outNpId)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	return npMgrGetCachedNpIdInternal(&s_accountSession, outNpId);
}

/* 0x813273E0 */
int npAccountGetMemoryCredentials(void *loginId, void *password, SceUInt64 *accountId)
{
	if (s_npMgrInitialized <= 0)
		return NP_MGR_ERR_NOT_INIT;
	if (loginId != NULL)
		sceClibMemcpy_safe(loginId, s_cachedLoginId, sizeof(s_cachedLoginId));
	if (password != NULL)
		sceClibMemcpy_safe(password, s_cachedPassword, sizeof(s_cachedPassword));
	if (accountId != NULL)
		*accountId = s_cachedAccountId;
	return 0;
}

/* 0x81327434 */
int npAccountResetState(void)
{
	npMgrClearCachedSession(&s_accountSession);
	return 0;
}

/* 0x81327444 */
int npAccountUpdateCachedProfile(const void *languages, const void *panelId,
				 const SceUInt32 *panelColor, const void *avatarUrl,
				 const void *aboutMe)
{
	return npMgrUpdateProfileCacheInternal(&s_accountSession, languages, panelId,
					       panelColor, avatarUrl, aboutMe);
}

/* --- SceNpManagerCallback subsystem (0x81327468..0x81327A57) --- */

/* 0x81327468 */
static int npMgrCallbackInit(int queueDepth, int priority, SceSize stackSize)
{
	if (s_cbInitialized > 0)
		return NP_MGR_ERR_ALREADY_INIT;

	SceUID pipeId = sceKernelCreateMsgPipe("SceNpManagerCallback", 64, 12,
					       static_cast<SceSize>(12 * queueDepth), NULL);
	int ret = pipeId;
	if (pipeId >= 0) {
		s_cbMsgPipeId = pipeId;
		s_cbMutex = static_cast<SceKernelLwMutexWork *>(
			sceNpCommonMallocNpServerName(sizeof(SceKernelLwMutexWork)));
		if (s_cbMutex != NULL) {
			ret = sceKernelCreateLwMutex(s_cbMutex, "SceNpManagerCallback",
						     0x2002U, 0, NULL);
			if (ret >= 0) {
				SceUID tid = sceKernelCreateThread("SceNpManagerCallback",
								   npMgrCallbackThreadEntry,
								   priority, stackSize,
								   0, 0, NULL);
				ret = tid;
				if (tid >= 0) {
					s_cbThreadId = tid;
					ret = sceKernelStartThread(tid, 0, NULL);
					if (ret >= 0) {
						sceClibMemset(s_signInStateCallbacks, 0,
							      sizeof(s_signInStateCallbacks));
						s_cbLastSignInState = 1;
						s_cbInitialized = 1;
						s_cbStopRequested = 0;
						return 0;
					}
				}
			} else {
				sceNpCommonFreeNpServerName(s_cbMutex);
				s_cbMutex = NULL;
			}
		} else {
			ret = NP_MGR_ERR_NO_MEMORY;
		}
	}

	if (s_cbThreadId != -1) {
		sceKernelDeleteThread(s_cbThreadId);
		s_cbThreadId = -1;
	}
	if (s_cbMutex != NULL) {
		sceKernelDeleteLwMutex(s_cbMutex);
		sceNpCommonFreeNpServerName(s_cbMutex);
		s_cbMutex = NULL;
	}
	if (s_cbMsgPipeId != -1) {
		sceKernelDeleteMsgPipe(s_cbMsgPipeId);
		s_cbMsgPipeId = -1;
	}
	return ret;
}

/* 0x813275E2 */
static void npMgrCallbackTerm(void)
{
	if (s_cbInitialized == 0)
		return;

	SceUInt32 sendCount = 0;
	SceUInt32 recvCount = 0;
	s_cbStopRequested = 1;
	sceKernelCancelMsgPipe(s_cbMsgPipeId, &sendCount, &recvCount);
	sceKernelWaitThreadEnd(s_cbThreadId, NULL, NULL);

	if (s_cbThreadId != -1) {
		sceKernelDeleteThread(s_cbThreadId);
		s_cbThreadId = -1;
	}
	if (s_cbMutex != NULL) {
		sceKernelDeleteLwMutex(s_cbMutex);
		sceNpCommonFreeNpServerName(s_cbMutex);
		s_cbMutex = NULL;
	}
	if (s_cbMsgPipeId != -1) {
		sceKernelDeleteMsgPipe(s_cbMsgPipeId);
		s_cbMsgPipeId = -1;
	}
	s_cbLastSignInState = 1;
	s_cbInitialized = 0;
}

/* 0x813276B8 */
static int npMgrCallbackNotifySignInState(int state)
{
	NpManagerCallbackMsg msg;
	SceSize sentSize = 0;
	msg.kind = 0;
	msg.signInState = state;
	msg.accountEvent = 0;
	return sceKernelSendMsgPipe(s_cbMsgPipeId, &msg, sizeof(msg), 1U, &sentSize, NULL);
}

/* 0x813276FA */
static int npMgrCallbackNotifyAccountEvent(int eventId)
{
	NpManagerCallbackMsg msg;
	SceSize sentSize = 0;
	msg.kind = 1;
	msg.signInState = 0;
	msg.accountEvent = eventId;
	return sceKernelSendMsgPipe(s_cbMsgPipeId, &msg, sizeof(msg), 1U, &sentSize, NULL);
}

/* 0x8132773E */
int npAccountRegisterNetCallback(NpAccountEventCallback callback, void *userArg)
{
	if (s_cbInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	sceKernelLockLwMutex(s_cbMutex, 1, NULL);
	int foundIdx = NP_MGR_MAX_CALLBACK_SLOTS;
	for (int idx = 0; idx < NP_MGR_MAX_CALLBACK_SLOTS; ++idx) {
		if (s_signInStateCallbacks[idx].func == NULL) {
			foundIdx = idx;
			break;
		}
	}

	int ret = NP_MGR_ERR_NO_MEMORY;
	if (foundIdx != NP_MGR_MAX_CALLBACK_SLOTS) {
		s_signInStateCallbacks[foundIdx].slotId = foundIdx;
		s_signInStateCallbacks[foundIdx].func = callback;
		s_signInStateCallbacks[foundIdx].userArg = userArg;
		ret = foundIdx;
	}
	sceKernelUnlockLwMutex(s_cbMutex, 1);
	return ret;
}

/* 0x813277B8 */
int npAccountUnregisterNetCallback(int callbackId)
{
	if (s_cbInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	sceKernelLockLwMutex(s_cbMutex, 1, NULL);
	int ret = NP_MGR_ERR_NOT_FOUND;
	if (s_signInStateCallbacks[callbackId].func != NULL) {
		s_signInStateCallbacks[callbackId].slotId = 0;
		s_signInStateCallbacks[callbackId].func = NULL;
		s_signInStateCallbacks[callbackId].userArg = NULL;
		ret = 0;
	}
	sceKernelUnlockLwMutex(s_cbMutex, 1);
	return ret;
}

/* 0x8132781C */
int npAccountRegisterCallback(int (*cb)(int eventId, IPMI::Session *session),
			      IPMI::Session *session)
{
	if (s_cbInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	sceKernelLockLwMutex(s_cbMutex, 1, NULL);
	int foundIdx = NP_MGR_MAX_CALLBACK_SLOTS;
	for (int idx = 0; idx < NP_MGR_MAX_CALLBACK_SLOTS; ++idx) {
		if (s_accountEventCallbacks[idx].func == NULL) {
			foundIdx = idx;
			break;
		}
	}

	int ret = NP_MGR_ERR_NO_MEMORY;
	if (foundIdx != NP_MGR_MAX_CALLBACK_SLOTS) {
		s_accountEventCallbacks[foundIdx].slotId = foundIdx;
		s_accountEventCallbacks[foundIdx].func =
			reinterpret_cast<NpAccountEventCallback>(cb);
		s_accountEventCallbacks[foundIdx].userArg = session;
		ret = foundIdx;
	}
	sceKernelUnlockLwMutex(s_cbMutex, 1);
	return ret;
}

/* 0x81327896 */
int npAccountUnregisterCallback(int callbackId)
{
	if (s_cbInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	sceKernelLockLwMutex(s_cbMutex, 1, NULL);
	int ret = NP_MGR_ERR_NOT_FOUND;
	if (s_accountEventCallbacks[callbackId].func != NULL) {
		s_accountEventCallbacks[callbackId].slotId = 0;
		s_accountEventCallbacks[callbackId].func = NULL;
		s_accountEventCallbacks[callbackId].userArg = NULL;
		ret = 0;
	}
	sceKernelUnlockLwMutex(s_cbMutex, 1);
	return ret;
}

/* 0x813278FA */
static int npMgrCallbackThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	while (s_cbStopRequested <= 0) {
		NpManagerCallbackMsg msg;
		SceSize recvSize = 0;
		sceKernelReceiveMsgPipe(s_cbMsgPipeId, &msg, sizeof(msg), 1U, &recvSize, NULL);
		if (s_cbStopRequested > 0)
			break;

		if (msg.kind == 0) {
			if (msg.signInState == s_cbLastSignInState)
				continue;
			s_cbLastSignInState = msg.signInState;
			sceKernelLockLwMutex(s_cbMutex, 1, NULL);
			for (int idx = 0; idx < NP_MGR_MAX_CALLBACK_SLOTS; ++idx) {
				if (s_signInStateCallbacks[idx].func != NULL) {
					s_signInStateCallbacks[idx].func(
						msg.signInState,
						s_signInStateCallbacks[idx].userArg);
				}
			}
			sceKernelUnlockLwMutex(s_cbMutex, 1);
		} else if (msg.kind == 1) {
			sceKernelLockLwMutex(s_cbMutex, 1, NULL);
			for (int idx = 0; idx < NP_MGR_MAX_CALLBACK_SLOTS; ++idx) {
				if (s_accountEventCallbacks[idx].func != NULL) {
					s_accountEventCallbacks[idx].func(
						msg.accountEvent,
						s_accountEventCallbacks[idx].userArg);
				}
			}
			sceKernelUnlockLwMutex(s_cbMutex, 1);
		}
	}

	sceKernelExitThread(0);
	return 0;
}

/* --- SceNpManagerRequest handle pool (0x81327A58..0x81327D21) --- */

/* 0x81327A58 */
static int npMgrRequestPoolInit(void)
{
	s_reqListHead = NULL;
	s_reqMutex = static_cast<SceKernelLwMutexWork *>(
		sceNpCommonMallocNpServerName(sizeof(SceKernelLwMutexWork)));
	if (s_reqMutex == NULL)
		return NP_MGR_ERR_NO_MEMORY;

	int ret = sceKernelCreateLwMutex(s_reqMutex, "SceNpManagerRequest", 0x2002U, 0, NULL);
	if (ret >= 0)
		return 0;

	sceNpCommonFreeNpServerName(s_reqMutex);
	s_reqMutex = NULL;
	return ret;
}

/* 0x81327AC0 */
static int npMgrRequestPoolTerm(void)
{
	sceKernelLockLwMutex(s_reqMutex, 1, NULL);
	NpManagerRequestNode *cur = s_reqListHead;
	while (cur != NULL) {
		NpManagerRequestNode *next = cur->next;
		sceNpCommonFreeNpServerName(cur);
		cur = next;
	}
	s_reqListHead = NULL;
	sceKernelUnlockLwMutex(s_reqMutex, 1);

	if (s_reqMutex != NULL) {
		sceKernelDeleteLwMutex(s_reqMutex);
		sceNpCommonFreeNpServerName(s_reqMutex);
		s_reqMutex = NULL;
	}
	return 0;
}

/* 0x81327B24 */
static NpManagerRequestNode *npMgrRequestFind(int reqId)
{
	sceKernelLockLwMutex(s_reqMutex, 1, NULL);
	NpManagerRequestNode *cur = s_reqListHead;
	while (cur != NULL) {
		if (cur->reqId == reqId)
			break;
		cur = cur->next;
	}
	sceKernelUnlockLwMutex(s_reqMutex, 1);
	return cur;
}

/* 0x81327B66 */
static int npMgrRequestAlloc(void)
{
	sceKernelLockLwMutex(s_reqMutex, 1, NULL);
	int candidateId = (s_reqLastId != NP_MGR_MAX_REQUEST_ID) ? (s_reqLastId + 1) : 1;
	if (candidateId == s_reqLastId) {
		sceKernelUnlockLwMutex(s_reqMutex, 1);
		return NP_MGR_ERR_NOT_FOUND;
	}

	while (npMgrRequestFind(candidateId) != NULL) {
		candidateId = (candidateId != NP_MGR_MAX_REQUEST_ID) ? (candidateId + 1) : 1;
		if (candidateId == s_reqLastId) {
			sceKernelUnlockLwMutex(s_reqMutex, 1);
			return NP_MGR_ERR_NOT_FOUND;
		}
	}

	s_reqLastId = candidateId;
	NpManagerRequestNode *node = static_cast<NpManagerRequestNode *>(
		sceNpCommonMallocNpServerName(sizeof(NpManagerRequestNode)));
	if (node == NULL) {
		sceKernelUnlockLwMutex(s_reqMutex, 1);
		return NP_MGR_ERR_NO_MEMORY;
	}

	sce_paf_memset(node, 0, sizeof(NpManagerRequestNode));
	node->reqId = candidateId;
	node->next = s_reqListHead;
	s_reqListHead = node;
	sceKernelUnlockLwMutex(s_reqMutex, 1);
	return candidateId;
}

/* 0x81327C20 */
static void npMgrRequestFree(NpManagerRequestNode *node)
{
	sceKernelLockLwMutex(s_reqMutex, 1, NULL);
	NpManagerRequestNode *cur = s_reqListHead;
	NpManagerRequestNode *prev = NULL;
	while (cur != NULL) {
		if (cur == node) {
			if (prev != NULL)
				prev->next = cur->next;
			else
				s_reqListHead = cur->next;
			cur->next = NULL;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	sceKernelUnlockLwMutex(s_reqMutex, 1);
	sceNpCommonFreeNpServerName(node);
}

/* 0x81327C7E */
static int npMgrRequestAbort(NpAccountSession *session, int reqId)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	SceUID activeTid = npMgrGetActiveSignInThread();
	NpManagerRequestNode *node = npMgrRequestFind(reqId);

	if (node != NULL && activeTid >= 0) {
		node->aborted = 1;
		if (node->waitThreadId == activeTid) {
			npMgrAbortSystemUpdate(session);
			npMgrAbortAuthTicket(session);
			npMgrAbortProfileFetch(session);
			if (session->clockUpdateReqId > 0)
				npSignInDialogAbort(session->clockUpdateReqId);
		} else {
			sceKernelSignalCondTo(session->condId, node->waitThreadId);
		}
	}
	sceKernelUnlockMutex(session->mutexId, 1);
	return 0;
}

/* --- myprofile.dat & ticket cache helpers (0x81327D22..0x813281EB) --- */

/* 0x81327D22 */
static int npMgrSaveMyProfile(const NpAccountSession *session)
{
	if (npManagerCoreIsInitialized() == 0)
		return NP_MGR_ERR_NOT_INIT;

	shellNotifyProfileSaved();

	char pathBuf[64];
	int ret = sceAppMgrGetUserDirPath(1, pathBuf, sizeof(pathBuf));
	if (ret < 0)
		return ret;

	SceSize baseLen = sceClibStrnlen(pathBuf, 0x7FFFFFFFU);
	sceClibStrncat(pathBuf, "/np", sizeof(pathBuf) - 1 - baseLen);

	SceIoStat statBuf;
	ret = sceIoGetstat(pathBuf, &statBuf);
	if (ret == NP_MGR_IO_ERR_NOT_FOUND) {
		ret = sceIoMkdir(pathBuf, 6);
		if (ret < 0)
			return ret;
	} else if (ret < 0) {
		return ret;
	}

	sceClibMemset(pathBuf, 0, sizeof(pathBuf));
	ret = sceAppMgrGetUserDirPath(1, pathBuf, sizeof(pathBuf));
	if (ret < 0)
		return ret;

	baseLen = sceClibStrnlen(pathBuf, 0x7FFFFFFFU);
	sceClibStrncat(pathBuf, "/np/myprofile.dat", sizeof(pathBuf) - 1 - baseLen);

	SceUID fd = sceIoOpen(pathBuf, 1538, 6);
	if (fd < 0)
		return fd;

	NpCachedMyProfileFile fileData;
	sceClibMemset(&fileData, 0, sizeof(fileData));
	sceClibMemcpy_safe(&fileData.npId, &session->profile.npId, sizeof(fileData.npId));
	sceClibMemcpy_safe(&fileData.onlineId, &session->onlineId, sizeof(fileData.onlineId));
	sceClibMemcpy_safe(fileData.aboutMe, session->profile.aboutMe, sizeof(fileData.aboutMe));
	sceClibMemcpy_safe(fileData.avatarUrl, session->profile.avatarUrl,
			   sizeof(fileData.avatarUrl));

	SceSSize written = sceIoWrite(fd, &fileData, sizeof(fileData));
	if (written < 0) {
		ret = static_cast<int>(written);
	} else if (written == static_cast<SceSSize>(sizeof(fileData))) {
		s_cachedMyProfile = fileData;
		s_myProfileLoaded = 1;
		ret = 0;
	} else {
		ret = NP_MGR_ERR_INTERNAL;
	}

	sceIoClose(fd);
	return ret;
}

/* 0x81327EF4 */
static int npMgrLoadMyProfile(void)
{
	char pathBuf[64];
	int ret = sceAppMgrGetUserDirPath(1, pathBuf, sizeof(pathBuf));
	if (ret < 0)
		return ret;

	SceSize baseLen = sceClibStrnlen(pathBuf, 0x7FFFFFFFU);
	sceClibStrncat(pathBuf, "/np/myprofile.dat", sizeof(pathBuf) - 1 - baseLen);

	SceUID fd = sceIoOpen(pathBuf, 1, 260);
	if (fd < 0)
		return fd;

	SceSSize readBytes = sceIoRead(fd, &s_cachedMyProfile, sizeof(s_cachedMyProfile));
	if (readBytes < 0) {
		ret = static_cast<int>(readBytes);
	} else if (readBytes == static_cast<SceSSize>(sizeof(s_cachedMyProfile))) {
		s_myProfileLoaded = 1;
		ret = 0;
	} else {
		ret = NP_MGR_ERR_INTERNAL;
	}

	sceIoClose(fd);
	return ret;
}

/* 0x81327F9E */
static int npMgrCopyMyProfile(void *outBuf)
{
	if (s_myProfileLoaded == 0)
		return NP_MGR_ERR_NO_CACHED_PROFILE;
	sceClibMemcpy_safe(outBuf, &s_cachedMyProfile, sizeof(s_cachedMyProfile));
	return 0;
}

/* 0x81328010 */
static int npMgrCopyValidTicket(NpAccountSession *session, void *outBuf, SceSize *ioSize)
{
	int testMode = SceNpCommon_F585C86C();
	if (session->ticketSize == 0)
		return 0;
	if (testMode != 1 && session->profile.npId.handle.data[0] == '\0')
		return 0;

	SceRtcTick nowTick;
	if (sceRtcGetCurrentNetworkTick(&nowTick) < 0)
		return 0;

	testMode = SceNpCommon_F585C86C();
	if (session->ticketBuf == NULL || session->ticketSize == 0)
		return 0;
	if (testMode != 1 && session->profile.npId.handle.data[0] == '\0')
		return 0;

	if (session->ticketAcquiredTick.tick == 0)
		return 0;
	if (nowTick.tick - session->ticketAcquiredTick.tick >=
	    session->ticketValidDurationUsec - NP_MGR_TICKET_MARGIN_USEC)
		return 0;

	if (outBuf != NULL)
		sceClibMemcpy_safe(outBuf, session->ticketBuf, session->ticketSize);
	if (ioSize != NULL)
		*ioSize = session->ticketSize;
	return 1;
}

/* 0x81328104 */
static bool npMgrExtractValidMiniTicket(NpAccountSession *session, void *outData,
					SceSize *ioDataSize)
{
	int testMode = SceNpCommon_F585C86C();
	if (session->ticketSize == 0)
		return false;
	if (testMode != 1 && session->profile.npId.handle.data[0] == '\0')
		return false;

	SceRtcTick nowTick;
	if (sceRtcGetCurrentNetworkTick(&nowTick) < 0)
		return false;

	testMode = SceNpCommon_F585C86C();
	if (session->ticketBuf == NULL || session->ticketSize == 0)
		return false;
	if (testMode != 1 && session->profile.npId.handle.data[0] == '\0')
		return false;

	if (session->ticketAcquiredTick.tick == 0)
		return false;
	if (nowTick.tick - session->ticketAcquiredTick.tick >=
	    session->ticketValidDurationUsec - NP_MGR_TICKET_MARGIN_USEC)
		return false;

	int ret = SceNpCommon_85503FA6(static_cast<const unsigned char *>(session->ticketBuf),
				       session->ticketSize, outData, ioDataSize);
	return ret >= 0;
}

/* --- Registry helpers (0x813281EC..0x81328747) --- */

/* 0x813281EC */
static int npMgrGetAutoClockSetting(void)
{
	int autoSet = 0;
	int ret = sceRegMgrGetKeyIntForUser("/CONFIG/DATE", "set_automatically", &autoSet);
	return (ret >= 0) ? autoSet : ret;
}

/* 0x8132822C */
static int npMgrLoadEnableNpSetting(void)
{
	int enabled = 0;
	int ret = sceRegMgrGetKeyIntForUser("/CONFIG/NP", "enable_np", &enabled);
	s_enableNpFlag = (ret >= 0) ? enabled : 0;
	return ret;
}

/* 0x81328282 */
static int npMgrGetEnableNp(void)
{
	return s_enableNpFlag;
}

/* 0x8132828E */
static int npMgrSetEnableNp(int enabled)
{
	if (s_enableNpFlag == enabled)
		return 0;

	int ret = sceRegMgrSetKeyIntForUser("/CONFIG/NP", "enable_np", enabled);
	if (ret < 0)
		return ret;

	s_enableNpFlag = enabled;
	if (enabled == 1) {
		npMgrCallbackNotifySignInState(0);
		return ret;
	}
	if (enabled == 0) {
		npMgrCallbackNotifySignInState(1);
		return ret;
	}
	return NP_MGR_ERR_INVALID_TYPE;
}

/* 0x813282F4 */
static int npMgrCheckEnableNp(void)
{
	return (s_enableNpFlag == 0) ? NP_MGR_ERR_SIGNED_OUT : 0;
}

/* 0x8132830C */
static int npMgrWriteRegistryCredsRaw(const char *loginId, const char *password,
				      const SceUInt64 *accountId)
{
	int ret;
	if (loginId != NULL) {
		ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "login_id", loginId, 65);
		if (ret < 0)
			return ret;
	}
	if (password != NULL) {
		ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "password", password, 31);
		if (ret < 0)
			return ret;
	}
	if (accountId != NULL) {
		ret = sceRegMgrSetKeyBinForUser("/CONFIG/NP", "account_id",
						accountId, sizeof(*accountId));
		if (ret < 0)
			return ret;
		npCommonServiceUpdateAccountId(*accountId);
	}
	npMgrCallbackNotifyAccountEvent(1);
	return 0;
}

/* 0x81328380 */
static int npMgrUpdateRegistryCredsVerified(SceUInt64 accountId, const char *loginId,
					    const char *password)
{
	SceUInt64 storedAccountId = 0;
	int ret = sceRegMgrGetKeyBinForUser("/CONFIG/NP", "account_id",
					    &storedAccountId, sizeof(storedAccountId));
	if (ret < 0)
		return ret;
	if (accountId != storedAccountId)
		return NP_MGR_ERR_CREDENTIAL_MISMATCH;

	if (loginId != NULL) {
		ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "login_id", loginId, 65);
		if (ret < 0)
			return ret;
	}
	if (password != NULL) {
		ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "password", password, 31);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x8132842C */
static int npMgrClearRegistryCreds(void)
{
	SceUInt64 zeroAccountId = 0;
	int ret = sceRegMgrSetKeyBinForUser("/CONFIG/NP", "account_id",
					    &zeroAccountId, sizeof(zeroAccountId));
	if (ret < 0)
		return ret;

	npCommonServiceUpdateAccountId(zeroAccountId);

	char emptyLogin[68];
	sceClibMemset(emptyLogin, 0, sizeof(emptyLogin));
	ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "login_id", emptyLogin, 65);
	if (ret < 0)
		return ret;

	char emptyPassword[36];
	sceClibMemset(emptyPassword, 0, sizeof(emptyPassword));
	ret = sceRegMgrSetKeyStrForUser("/CONFIG/NP", "password", emptyPassword, 31);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813284CE */
static int npMgrLocaleCodeFromLang(unsigned int langIndex, char *outLocale)
{
	unsigned int clamped = (langIndex < NP_MGR_LOCALE_COUNT) ? langIndex : 1U;
	sceClibStrncpy(outLocale, s_npLocaleCodes[clamped], 6U);
	return 0;
}

/* 0x813284F6 */
static int npMgrSyncRegionRegistry(const NpAccountSession *session)
{
	char storedCountry[4];
	char storedLang[8];
	int storedYob = 0;
	int storedMob = 0;
	int storedDob = 0;

	sceClibMemset(storedCountry, 0, 3U);
	sceClibMemset(storedLang, 0, 6U);

	RegMgrKeyInfo readKeys[5];
	sceClibMemset(readKeys, 0, sizeof(readKeys));

	readKeys[0].size = sizeof(RegMgrKeyInfo);
	readKeys[0].name = "country";
	readKeys[0].type = REGMGR_KEY_TYPE_STR;
	readKeys[0].valueSize = 3;
	readKeys[0].data = storedCountry;

	readKeys[1].size = sizeof(RegMgrKeyInfo);
	readKeys[1].name = "lang";
	readKeys[1].type = REGMGR_KEY_TYPE_STR;
	readKeys[1].valueSize = 6;
	readKeys[1].data = storedLang;

	readKeys[2].size = sizeof(RegMgrKeyInfo);
	readKeys[2].name = "yob";
	readKeys[2].type = REGMGR_KEY_TYPE_INT;
	readKeys[2].valueSize = 4;

	readKeys[3].size = sizeof(RegMgrKeyInfo);
	readKeys[3].name = "mob";
	readKeys[3].type = REGMGR_KEY_TYPE_INT;
	readKeys[3].valueSize = 4;

	readKeys[4].size = sizeof(RegMgrKeyInfo);
	readKeys[4].name = "dob";
	readKeys[4].type = REGMGR_KEY_TYPE_INT;
	readKeys[4].valueSize = 4;

	int ret = sceRegMgrGetKeysForUser("/CONFIG/NP", readKeys, 5);
	if (ret >= 0)
		ret = sceRegMgrGetKeyIntForUser("/CONFIG/NP", "yob", &storedYob);
	if (ret >= 0)
		ret = sceRegMgrGetKeyIntForUser("/CONFIG/NP", "mob", &storedMob);
	if (ret >= 0)
		ret = sceRegMgrGetKeyIntForUser("/CONFIG/NP", "dob", &storedDob);
	if (ret < 0)
		return ret;

	unsigned int langIdx = session->regionInfo[3];
	if (langIdx >= NP_MGR_LOCALE_COUNT)
		langIdx = 1U;

	const char *targetLang = s_npLocaleCodes[langIdx];
	const char *targetCountry = reinterpret_cast<const char *>(session->regionInfo);

	if (sceClibStrncmp(storedCountry, targetCountry, 2U) == 0 &&
	    sceClibStrncmp(storedLang, targetLang, 5U) == 0 &&
	    storedYob == static_cast<int>(session->birthYear) &&
	    storedMob == static_cast<int>(session->birthMonth) &&
	    storedDob == static_cast<int>(session->birthDay)) {
		return 0;
	}

	RegMgrKeyInfo writeKeys[5];
	sceClibMemset(writeKeys, 0, sizeof(writeKeys));

	writeKeys[0].size = sizeof(RegMgrKeyInfo);
	writeKeys[0].name = "country";
	writeKeys[0].type = REGMGR_KEY_TYPE_STR;
	writeKeys[0].valueSize = 3;
	writeKeys[0].data = targetCountry;

	writeKeys[1].size = sizeof(RegMgrKeyInfo);
	writeKeys[1].name = "lang";
	writeKeys[1].type = REGMGR_KEY_TYPE_STR;
	writeKeys[1].valueSize = 6;
	writeKeys[1].data = targetLang;

	writeKeys[2].size = sizeof(RegMgrKeyInfo);
	writeKeys[2].name = "yob";
	writeKeys[2].type = REGMGR_KEY_TYPE_INT;
	writeKeys[2].valueSize = 4;
	writeKeys[2].value = static_cast<int>(session->birthYear);

	writeKeys[3].size = sizeof(RegMgrKeyInfo);
	writeKeys[3].name = "mob";
	writeKeys[3].type = REGMGR_KEY_TYPE_INT;
	writeKeys[3].valueSize = 4;
	writeKeys[3].value = static_cast<int>(session->birthMonth);

	writeKeys[4].size = sizeof(RegMgrKeyInfo);
	writeKeys[4].name = "dob";
	writeKeys[4].type = REGMGR_KEY_TYPE_INT;
	writeKeys[4].valueSize = 4;
	writeKeys[4].value = static_cast<int>(session->birthDay);

	return sceRegMgrSetKeysForUser("/CONFIG/NP", writeKeys, 5);
}

/* --- Core sign-in & session state machine (0x81328748..0x81328E67) --- */

/* 0x81328748 */
static void npMgrClearCachedSession(NpAccountSession *session)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	session->ticketSize = 0;
	if (session->ticketBuf != NULL) {
		sceNpCommonFreeNpServerName(session->ticketBuf);
		session->ticketBuf = NULL;
	}
	sceClibMemset(&session->profile, 0, sizeof(session->profile));
	sceKernelUnlockMutex(session->mutexId, 1);
}

/* 0x8132877E */
static SceUID npMgrGetActiveSignInThread(void)
{
	return s_activeSignInThreadId;
}

/* 0x8132878A */
static int npMgrAcquireSessionEx(NpAccountSession *session, int reqId, void *outTicket,
				 SceSize *ioSize, int flags, const void *loginId,
				 const void *password)
{
	bool useNetInternal = ((flags & NP_MGR_ACQUIRE_NET_INTERNAL) != 0);
	if (useNetInternal)
		SceNetInternal_9DBD2F60(sceKernelGetThreadId(), 1, 0);

	bool hasOverrideCreds = (loginId != NULL && password != NULL);

	sceKernelLockMutex(session->mutexId, 1, NULL);
	NpManagerRequestNode *reqNode = npMgrRequestFind(reqId);
	if (reqNode == NULL) {
		sceKernelUnlockMutex(session->mutexId, 1);
		return NP_MGR_ERR_NOT_FOUND;
	}

	reqNode->waitThreadId = sceKernelGetThreadId();
	while (s_activeSignInThreadId >= 0) {
		sceKernelWaitCond(session->condId, NULL);
		if (reqNode->aborted != 0)
			break;
	}

	int ret = 0;
	bool shouldPersistCreds = false;
	bool executedSignIn = false;

	if (reqNode->aborted != 0) {
		ret = NP_MGR_ERR_ABORTED;
		sceKernelUnlockMutex(session->mutexId, 1);
	} else {
		bool reuseCachedTicket = false;
		bool enableCheckFailed = false;
		if (!hasOverrideCreds) {
			if ((flags & NP_MGR_ACQUIRE_SKIP_ENABLE_CHK) == 0) {
				ret = npMgrCheckEnableNp();
				if (ret < 0) {
					enableCheckFailed = true;
					sceKernelUnlockMutex(session->mutexId, 1);
				}
			}
			if (!enableCheckFailed) {
				SceRtcTick nowTick;
				if (sceRtcGetCurrentNetworkTick(&nowTick) >= 0 &&
				    (flags & NP_MGR_ACQUIRE_FORCE_REFRESH) == 0 &&
				    session->ticketBuf != NULL &&
				    session->ticketSize != 0 &&
				    session->ticketAcquiredTick.tick != 0 &&
				    nowTick.tick - session->ticketAcquiredTick.tick <
					session->ticketValidDurationUsec - NP_MGR_TICKET_MARGIN_USEC) {
					reuseCachedTicket = true;
				}
			}
		}

		if (enableCheckFailed) {
			/* ret already holds NP_MGR_ERR_SIGNED_OUT and mutex is unlocked */
		} else if (reuseCachedTicket) {
			ret = 0;
			sceKernelUnlockMutex(session->mutexId, 1);
		} else {
			executedSignIn = true;
			s_activeSignInThreadId = sceKernelGetThreadId();
			sceKernelUnlockMutex(session->mutexId, 1);

			ret = session->forcedError;
			if (ret >= 0) {
				if (reqNode->aborted != 0) {
					ret = NP_MGR_ERR_ABORTED;
				} else {
					ret = npMgrCheckSystemUpdate(session);
				}
			}

			if (ret >= 0) {
				for (int attempt = 0; attempt < 2; ++attempt) {
					ret = npMgrFetchAuthTicket(session, loginId,
								   password, flags);
					if (attempt == 1 ||
					    (ret != NP_MGR_ERR_CLOCK_SKEW_AUTH &&
					     ret != NP_MGR_ERR_CLOCK_SKEW_SSL)) {
						break;
					}
					ret = npMgrRunClockUpdate(session);
					if (ret < 0)
						break;
				}

				if (ret < 0) {
					if (session->ticketBuf != NULL) {
						sceNpCommonFreeNpServerName(session->ticketBuf);
						session->ticketBuf = NULL;
					}
					session->ticketSize = 0;
				}
			}

			if (ret >= 0)
				ret = session->forcedError;
			if (ret >= 0 && reqNode->aborted != 0)
				ret = NP_MGR_ERR_ABORTED;

			if (ret >= 0 &&
			    (flags & NP_MGR_ACQUIRE_SKIP_PROFILE) == 0 &&
			    SceNpCommon_F585C86C() != 1) {
				ret = npMgrFetchOwnProfile(session);
				if (ret < 0) {
					if (session->ticketBuf != NULL) {
						sceNpCommonFreeNpServerName(session->ticketBuf);
						session->ticketBuf = NULL;
					}
					session->ticketSize = 0;
				} else {
					npMgrCallbackNotifyAccountEvent(0);
					ret = session->forcedError;
					if (ret >= 0 && reqNode->aborted != 0)
						ret = NP_MGR_ERR_ABORTED;
					if (ret >= 0)
						npMgrSaveMyProfile(session);
				}
			}

			if (ret >= 0) {
				if (hasOverrideCreds)
					shouldPersistCreds = true;
				SceRtcTick acquiredTick;
				ret = sceRtcGetCurrentNetworkTick(&acquiredTick);
				if (ret >= 0) {
					session->ticketAcquiredTick = acquiredTick;
					ret = npMgrSyncRegionRegistry(session);
				}
			}
		}
	}

	(void)executedSignIn;

	sceKernelLockMutex(session->mutexId, 1, NULL);
	if (s_activeSignInThreadId == sceKernelGetThreadId())
		s_activeSignInThreadId = -1;
	if (session->forcedError < 0)
		ret = session->forcedError;

	SceKernelMutexInfo mutexInfo;
	mutexInfo.size = sizeof(mutexInfo);
	sceKernelGetMutexInfo(session->mutexId, &mutexInfo);
	if (mutexInfo.numWaitThreads == 0)
		session->forcedError = 0;

	reqNode->waitThreadId = 0;
	reqNode->aborted = 0;

	if (ret >= 0) {
		if (shouldPersistCreds) {
			npMgrWriteRegistryCredsRaw(static_cast<const char *>(loginId),
						   static_cast<const char *>(password),
						   NULL);
			npMgrSetEnableNp(1);
		}
		if (ioSize != NULL) {
			SceSize currentSize = session->ticketSize;
			if (outTicket != NULL) {
				if (*ioSize < currentSize) {
					ret = NP_MGR_ERR_BUFFER_TOO_SMALL;
				} else {
					sceClibMemcpy_safe(outTicket, session->ticketBuf,
							   currentSize);
					currentSize = session->ticketSize;
				}
			}
			*ioSize = currentSize;
		}
	} else {
		for (SceSize idx = 0; idx < NP_MGR_FATAL_ERROR_COUNT; ++idx) {
			if (ret == s_fatalSignInErrors[idx]) {
				npMgrSetEnableNp(0);
				break;
			}
		}
		npMgrClearCachedSession(session);
	}

	session->lastSignInResult = ret;
	sceKernelSignalCond(session->condId);
	sceKernelUnlockMutex(session->mutexId, 1);

	if (useNetInternal)
		SceNetInternal_9DBD2F60(sceKernelGetThreadId(), 0, 0);
	return ret;
}

/* 0x81328B94 */
static int npMgrAcquireSession(NpAccountSession *session, int reqId, void *outTicket,
			       SceSize *ioSize, int flags)
{
	return npMgrAcquireSessionEx(session, reqId, outTicket, ioSize, flags, NULL, NULL);
}

/* 0x81328BC4 */
static int npMgrGetCachedNpIdInternal(NpAccountSession *session, SceNpId *outNpId)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	if (session->ticketBuf != NULL && session->ticketSize != 0 &&
	    session->profile.npId.handle.data[0] != '\0') {
		sceClibMemcpy_safe(outNpId, &session->profile.npId, sizeof(SceNpId));
		sceKernelUnlockMutex(session->mutexId, 1);
		return 0;
	}
	sceKernelUnlockMutex(session->mutexId, 1);
	return NP_MGR_ERR_NO_TICKET;
}

/* 0x81328C0E */
static int npMgrGetRatingAndRegion(NpAccountSession *session, SceUInt8 *outRating,
				   SceUInt8 *outRegion)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	int testMode = SceNpCommon_F585C86C();
	bool missingSession = (session->ticketBuf == NULL || session->ticketSize == 0 ||
			       (testMode != 1 && session->profile.npId.handle.data[0] == '\0'));
	if (missingSession) {
		sceKernelUnlockMutex(session->mutexId, 1);
		return NP_MGR_ERR_NO_TICKET;
	}

	if (outRating != NULL)
		sceClibMemcpy_safe(outRating, session->contentRatingFlags, 4U);
	if (outRegion != NULL)
		sceClibMemcpy_safe(outRegion, session->regionInfo, 4U);

	sceKernelUnlockMutex(session->mutexId, 1);
	return 0;
}

/* 0x81328CA0 */
static int npMgrSignOutInternal(NpAccountSession *session, int resultCode)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	if (s_activeSignInThreadId <= 0) {
		npMgrClearCachedSession(session);
	} else {
		session->forcedError = NP_MGR_ERR_SIGNED_OUT;
		npMgrAbortSystemUpdate(session);
		npMgrAbortAuthTicket(session);
		npMgrAbortProfileFetch(session);
	}
	session->lastSignInResult = resultCode;
	int ret = npMgrSetEnableNp(0);
	sceKernelUnlockMutex(session->mutexId, 1);
	return ret;
}

/* 0x81328D22 */
static int npMgrUnlinkAccountInternal(NpAccountSession *session)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	if (s_activeSignInThreadId > 0) {
		session->forcedError = NP_MGR_ERR_SIGNED_OUT;
		npMgrAbortSystemUpdate(session);
		npMgrAbortAuthTicket(session);
		npMgrAbortProfileFetch(session);
	}

	int ret = npMgrClearRegistryCreds();
	if (ret >= 0) {
		npMgrClearCachedSession(session);
		ret = npMgrSetEnableNp(0);
	}
	sceKernelUnlockMutex(session->mutexId, 1);
	return ret;
}

/* 0x81328DA8 */
static int npMgrUpdateProfileCacheInternal(NpAccountSession *session, const void *languages,
					   const void *panelId, const SceUInt32 *panelColor,
					   const void *avatarUrl, const void *aboutMe)
{
	sceKernelLockMutex(session->mutexId, 1, NULL);
	if (session->ticketBuf == NULL || session->ticketSize == 0 ||
	    session->profile.npId.handle.data[0] == '\0') {
		sceKernelUnlockMutex(session->mutexId, 1);
		return NP_MGR_ERR_NO_TICKET;
	}

	if (languages != NULL && languages != session->profile.languages)
		sceClibMemcpy_safe(session->profile.languages, languages,
				   sizeof(session->profile.languages));
	if (panelId != NULL && panelId != session->profile.panelId)
		sceClibMemcpy_safe(session->profile.panelId, panelId,
				   sizeof(session->profile.panelId));
	if (panelColor != NULL)
		session->profile.panelColor = *panelColor;
	if (avatarUrl != NULL && avatarUrl != session->profile.avatarUrl)
		sceClibMemcpy_safe(session->profile.avatarUrl, avatarUrl,
				   sizeof(session->profile.avatarUrl));
	if (aboutMe != NULL && aboutMe != session->profile.aboutMe)
		sceClibMemcpy_safe(session->profile.aboutMe, aboutMe,
				   sizeof(session->profile.aboutMe));

	int ret = npMgrSaveMyProfile(session);
	npMgrCallbackNotifyAccountEvent(0);
	sceKernelUnlockMutex(session->mutexId, 1);
	return ret;
}

/* --- Auth ticket fetch, clock retry, sysupdate & profile steps (0x813297F0..0x81329E1F) --- */

/* 0x813297F0 */
static int npMgrAbortAuthTicket(NpAccountSession *session)
{
	return npAuthCoreAbortRequest(session->authReqId);
}

/* 0x813297FA */
static int npMgrFetchAuthTicket(NpAccountSession *session, const void *overrideLogin,
				const void *overridePassword, int flags)
{
	NpAuthTicketVersion ticketVer;
	ticketVer.major = 4;
	ticketVer.minor = 0;

	session->authReqId = 0;
	s_authTicketSemaId = 0;

	SceUID semaId = sceKernelCreateSema("SceNpManager", 0, 0, 1, NULL);
	int ret = semaId;
	if (semaId >= 0) {
		s_authTicketSemaId = semaId;

		char loginId[68];
		char password[36];
		SceUInt64 expectedAccountId = 0;

		if (overrideLogin != NULL && overridePassword != NULL) {
			sceClibMemcpy_safe(loginId, overrideLogin, sizeof(loginId));
			sceClibMemcpy_safe(password, overridePassword, sizeof(password));
			ret = npManagerGetCachedCredentials(NULL, NULL, &expectedAccountId);
		} else {
			ret = npManagerGetCachedCredentials(loginId, password, &expectedAccountId);
		}

		if (ret >= 0) {
			ret = npAuthCoreCreateRequestWithMiniTicket(&session->authReqId,
								    &ticketVer,
								    "IV0001-NPXS01001_00",
								    1, 1U);
		}
		if (ret >= 0) {
			ret = npAuthCoreSetCredentials(session->authReqId, loginId, password);
			if (ret == NP_AUTH_ERR_DUPLICATE_PARAM)
				ret = 0;
		}
		if (ret >= 0) {
			s_authTicketResult = 0;
			ret = npAuthCoreStartAsyncDefault(session->authReqId, NULL,
							  npMgrAuthTicketCallback, session);
		}
		if (ret >= 0) {
			sceKernelWaitSema(s_authTicketSemaId, 1, NULL);
			while (npAuthCoreGetRequestStatus(session->authReqId) < 4)
				sceKernelDelayThread(10000U);

			ret = s_authTicketResult;
			if (ret >= 0) {
				SceNpTicketParam paramBuf;
				const unsigned char *ticketBytes =
					static_cast<const unsigned char *>(session->ticketBuf);

				ret = sceNpAuthGetTicketParam(ticketBytes, session->ticketSize,
							      SCE_NP_TICKET_PARAM_SUBJECT_ACCOUNT_ID,
							      &paramBuf);
				if (ret >= 0) {
					SceUInt64 ticketAccountId = 0;
					sceClibMemcpy_safe(&ticketAccountId, paramBuf.data,
							   sizeof(ticketAccountId));
					if (expectedAccountId != 0 && flags == 0 &&
					    expectedAccountId != ticketAccountId) {
						ret = NP_MGR_ERR_ACCOUNT_MISMATCH;
					} else {
						session->accountId = ticketAccountId;
						ret = sceNpAuthGetTicketParam(
							ticketBytes, session->ticketSize, 14,
							&paramBuf);
					}
				}
				if (ret >= 0) {
					int drmStatus = 0;
					sceNpDrmCheckDrmReset(&paramBuf, 64U,
							      session->accountId, &drmStatus);
					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_SUBJECT_ONLINE_ID, &paramBuf);
				}
				if (ret >= 0) {
					sceClibMemcpy_safe(&session->onlineId, &paramBuf,
							   sizeof(SceNpOnlineId));
					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_SUBJECT_REGION, &paramBuf);
				}
				if (ret >= 0) {
					const SceUInt8 *regionBytes = paramBuf.data;
					sceClibMemcpy_safe(session->countryCodeStr, regionBytes, 2U);
					session->countryCodeStr[2] = '\0';
					sceClibMemset(session->regionInfo, 0,
						      sizeof(session->regionInfo));
					sceClibMemcpy_safe(session->regionInfo, regionBytes, 2U);
					session->regionInfo[3] = regionBytes[3];

					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_SUBJECT_STATUS,
						&paramBuf);
				}
				if (ret >= 0) {
					SceUInt32 controlWord = 0;
					sceClibMemcpy_safe(&controlWord, paramBuf.data,
							   sizeof(controlWord));
					sceClibMemset(session->contentRatingFlags, 0,
						      sizeof(session->contentRatingFlags));
					session->contentRatingFlags[0] =
						static_cast<SceUInt8>(((controlWord >> 24) & 0x7FU) << 1);
					session->contentRatingFlags[2] =
						static_cast<SceUInt8>(
							(((controlWord >> 8) & 1U) << 7) |
							(((controlWord >> 9) & 1U) << 6));
					session->contentRatingFlags[3] =
						static_cast<SceUInt8>(controlWord & 0xFFU);

					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_EXPIRE_DATE, &paramBuf);
				}
				SceUInt64 expireMs = 0;
				if (ret >= 0) {
					sceClibMemcpy_safe(&expireMs, paramBuf.data,
							   sizeof(expireMs));
					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_ISSUED_DATE, &paramBuf);
				}
				if (ret >= 0) {
					SceUInt64 issueMs = 0;
					sceClibMemcpy_safe(&issueMs, paramBuf.data,
							   sizeof(issueMs));
					if (expireMs > issueMs) {
						SceUInt64 durationUsec = (expireMs - issueMs) * 1000ULL;
						session->ticketValidDurationUsec =
							(durationUsec < NP_MGR_TICKET_MARGIN_USEC)
							? NP_MGR_TICKET_MARGIN_USEC
							: durationUsec;
					} else {
						session->ticketValidDurationUsec =
							NP_MGR_TICKET_MARGIN_USEC;
					}

					SceRtcTick serverTick;
					serverTick.tick = issueMs * 1000ULL + NP_MGR_UNIX_EPOCH_TICKS;
					ret = vshRtcSetCurrentSecureTick(&serverTick);
					if (ret >= 0)
						ret = vshRtcSetCurrentNetworkTick(&serverTick);
					if (ret >= 0 && npMgrGetAutoClockSetting() != 0)
						ret = vshRtcSetCurrentTick(&serverTick);
				}
				if (ret >= 0) {
					ret = sceNpAuthGetTicketParam(
						ticketBytes, session->ticketSize,
						SCE_NP_TICKET_PARAM_SUBJECT_DOB, &paramBuf);
					if (ret >= 0) {
						session->birthYear = paramBuf.date.year;
						session->birthMonth = paramBuf.date.month;
						session->birthDay = paramBuf.date.day;
					}
				}
			}
		}
	}

	if (s_authTicketSemaId > 0) {
		sceKernelDeleteSema(s_authTicketSemaId);
		s_authTicketSemaId = 0;
	}
	if (session->authReqId > 0) {
		npAuthCoreDestroyRequest(session->authReqId);
		session->authReqId = 0;
	}
	return ret;
}

/* 0x81329BDA */
static int npMgrRunClockUpdate(NpAccountSession *session)
{
	SceUID selfPid = sceKernelGetProcessId();
	int ret = npSignInDialogStartModeB(&session->clockUpdateReqId, 0, selfPid);
	if (ret < 0) {
		session->clockUpdateReqId = 0;
		return ret;
	}

	int status = 0;
	for (;;) {
		status = npSignInDialogGetStatus(session->clockUpdateReqId);
		if (status == 2)
			break;
		sceKernelDelayThread(100000U);
	}

	int clockResult = 0;
	if (session->clockUpdateReqId > 0) {
		npSignInDialogGetResult(session->clockUpdateReqId, &clockResult);
		if (clockResult != 0)
			status = clockResult;
	}
	session->clockUpdateReqId = 0;
	return status;
}

/* 0x81329C58 */
static int npMgrAuthTicketCallback(int authReqId, int resultOrSize, void *userArg)
{
	NpAccountSession *session = static_cast<NpAccountSession *>(userArg);
	int result = resultOrSize;

	if (resultOrSize > 0) {
		if (session->ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(session->ticketBuf);
			session->ticketBuf = NULL;
		}
		session->ticketSize = static_cast<SceSize>(resultOrSize);
		session->ticketBuf = sceNpCommonMallocNpServerName(session->ticketSize);
		if (session->ticketBuf != NULL) {
			result = npAuthCoreGetTicket(authReqId, session->ticketBuf,
						     session->ticketSize);
		} else {
			result = NP_MGR_ERR_NO_MEMORY;
		}
	}

	s_authTicketResult = result;
	sceKernelSignalSema(s_authTicketSemaId, 1);
	return 0;
}

/* 0x81329CB8 */
static int npMgrCheckSystemUpdate(NpAccountSession *session)
{
	int updateRequired = -1;
	int ret = npSysUpdateCreateRequest(&session->sysUpdateReqHandle);
	if (ret >= 0) {
		ret = npSysUpdateCheck(session->sysUpdateReqHandle, 1, &updateRequired, 0, 3);
		if (ret >= 0 && updateRequired == 1)
			ret = NP_MGR_ERR_SYS_UPDATE_REQUIRED;
	}
	if (session->sysUpdateReqHandle != 0) {
		npSysUpdateDestroyRequest(session->sysUpdateReqHandle);
		session->sysUpdateReqHandle = 0;
	}
	return ret;
}

/* 0x81329D22 */
static int npMgrAbortSystemUpdate(NpAccountSession *session)
{
	if (session->sysUpdateReqHandle == 0)
		return 0;
	npSysUpdateAbortRequest(session->sysUpdateReqHandle);
	session->sysUpdateReqHandle = 0;
	return 0;
}

/* 0x81329D34 */
static int npMgrAbortProfileFetch(NpAccountSession *session)
{
	return npProfileAbortRequest(session->profileReqHandle);
}

/* 0x81329D3E */
static int npMgrFetchOwnProfile(NpAccountSession *session)
{
	char envName[20];
	int ret = npCommunityEnsureUser(&session->onlineId);
	if (ret == NP_MGR_COMMUNITY_ERR_EXISTS || ret >= 0) {
		sceNpCommonGetNpEnviroment(envName);
		ret = npCommunitySetUserEnv(&session->onlineId, envName, session->countryCodeStr);
	}
	if (ret >= 0) {
		int reqHandle = npProfileCreateRequest(&session->onlineId, 1, NULL);
		ret = reqHandle;
		if (reqHandle >= 0) {
			session->profileReqHandle = reqHandle;
			ret = npProfileFetchOwnProfile(reqHandle, &session->profile,
						       &session->profileStatus,
						       &session->profileFlags,
						       &session->profileTimestamp,
						       session->profileStamp,
						       session->ticketBuf,
						       session->ticketSize, 2);
		}
	}

	if (ret >= 0) {
		npProfileDestroyRequest(session->profileReqHandle);
		session->profileReqHandle = 0;
		sceClibMemcpy_safe(session->profileCountryStr,
				   session->profile.npId.opt, 2U);
		session->profileCountryStr[2] = '\0';
		sceClibStrncpy(reinterpret_cast<char *>(session->profile.npId.opt + 4),
			       "psp2", 4U);
		return ret;
	}

	npProfileDestroyRequest(session->profileReqHandle);
	session->profileReqHandle = 0;
	npCommunityRemoveUser(&session->onlineId);
	return ret;
}
