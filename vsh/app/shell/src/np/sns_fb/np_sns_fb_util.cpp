/*
 * np_sns_fb_util.cpp: SceNpSnsFbUtil lifecycle, registry configuration,
 * thread-safe request registry, FQL/proxy.authorize HTTP clients, account check,
 * auth/bind, stream-publish wrapper, and Facebook error mapping (0x81338ADC..0x8133C993).
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libmd5.h>
#include <rtc.h>
#include <string.h>
#include "np_sns_fb_internal.h"

#define NP_SNS_FB_FQL_QUERY_BUF_MAX		0x281U
#define NP_SNS_FB_FQL_URL_BUF_MAX		0x321U
#define NP_SNS_FB_LONG_URL_BUF_MAX		0x1223U
#define NP_SNS_FB_NP_ERR_NOT_SIGNED_IN_A	-2141911798	/* 0x8055050A */
#define NP_SNS_FB_NP_ERR_NOT_SIGNED_IN_B	-2141911797	/* 0x8055050B */

/* Supported Facebook locale strings indexed by system language (0x81546850) */
static const char *const s_fbLocaleTable[] = {
	"ja_JP", "en_US", "fr_FR", "es_ES", "de_DE",
	"it_IT", "nl_NL", "pt_PT", "ru_RU", "ko_KR",
	"zh_HK", "zh_CN", "fi_FI", "sv_SE", "da_DK",
	"nb_NO", "pl_PL", "pt_BR", "en_GB", "tr_TR",
	NULL
};

/* Active request tracking node (56 bytes = 0x38) */
struct NpSnsFbUtilReqNode {
	SceInt32 refCount;					/* +0x00 */
	SceInt32 markedForDelete;				/* +0x04 */
	SceInt32 aborted;					/* +0x08 */
	SceInt32 canceledBySystem;				/* +0x0C */
	SceInt32 inUse;						/* +0x10 */
	SceInt32 reqId;						/* +0x14 */
	NpSnsFbStreamRequest *streamReq;			/* +0x18 */
	SceInt32 npAccountReq;					/* +0x1C */
	NpSnsFbLibRequest *authLibReq;				/* +0x20 */
	SceInt32 httpConnId;					/* +0x24 */
	SceInt32 httpReqId;					/* +0x28 */
	NpSnsFbLwMutex *reqMutex;				/* +0x2C */
	NpSnsFbUtilReqNode *next;				/* +0x30 */
	NpSnsFbUtilReqNode *prev;				/* +0x34 */
};

/* Module global state (0x81546848..0x815468B0, 0x815C6304..0x815C633C) */
static SceUInt8 s_utilInitialized = 0;
static SceInt32 s_utilHttpTemplateId = 0;
static SceInt32 s_fqlHttpTemplateId = 0;
static SceInt32 s_reqListInitialized = 0;
static NpSnsFbLwMutex *s_reqListMutex = NULL;
static NpSnsFbUtilReqNode s_reqListSentinel;
static SceInt32 s_authHttpTemplateId = 0;
static SceInt32 s_streamHttpTemplateId = 0;

/* Forward declarations */
static int npSnsFbFqlInit(SceInt32 httpTemplateId);
static int npSnsFbFqlTerm(void);
static int npSnsFbFqlGetPermissionsInfo(SceInt32 reqId, const char *sessionKey,
					const char *sessionSecret, SceInt32 langIndex,
					NpSnsFbPermissionMap *permMap, char *errMsgBuf,
					SceSize errMsgBufSize, SceInt32 *outFbError);
static int npSnsFbFqlCheckPermissions(SceInt32 reqId, const char *accessToken, const char *uid,
				      NpSnsFbPermissionMap *permMap, char *errMsgBuf,
				      SceSize errMsgBufSize, SceInt32 *outFbError);
static int npSnsFbProxyAuthorize(SceInt32 reqId, const char *sessionKey,
				 const char *sessionSecret, const char *proxiedAppId,
				 const char *remoteAppId, const char *permissions,
				 char *outAccessToken, SceUInt64 *outExpires, char *errMsgBuf,
				 SceSize errMsgBufSize, SceInt32 *outFbError);
static int npSnsFbFqlAbortHttpRequest(SceInt32 httpReqId);

static int npSnsFbRegGetAccount(char *outAccount);
static int npSnsFbRegSetAccount(const char *account);
static int npSnsFbRegGetSession(char *outSessionKey, char *outSessionSecret, char *outUid);
static int npSnsFbRegSetSession(const char *sessionKey, const char *sessionSecret,
				const char *uid);
static int npSnsFbRegGetShareFlags(SceInt32 *outShareTrophy, SceInt32 *outSharePurchase,
				   SceInt32 *outShareRating, SceInt32 *outGameIntegrations);
static int npSnsFbRegSetShareFlags(const SceInt32 *shareTrophy, const SceInt32 *sharePurchase,
				   const SceInt32 *shareRating,
				   const SceInt32 *gameIntegrations);
static int npSnsFbRegResetSession(void);
static int npSnsFbRegResetShareFlags(void);
static int npSnsFbRegResetAll(void);

static int npSnsFbReqListInit(void);
static int npSnsFbReqListTerm(void);
static void npSnsFbReqRelease(NpSnsFbUtilReqNode *node);
static int npSnsFbReqAcquire(SceInt32 reqId, NpSnsFbUtilReqNode **outNode);
static int npSnsFbReqCreate(void);
static int npSnsFbReqDestroy(SceInt32 reqId);
static int npSnsFbReqAttachStream(SceInt32 reqId, NpSnsFbStreamRequest *streamReq);
static int npSnsFbReqDetachStream(SceInt32 reqId);
static int npSnsFbReqAttachAccount(SceInt32 reqId, SceInt32 npAccountReq);
static int npSnsFbReqDetachAccount(SceInt32 reqId);
static int npSnsFbReqAttachAuthLib(SceInt32 reqId, NpSnsFbLibRequest *authLibReq);
static int npSnsFbReqDetachAuthLib(SceInt32 reqId);
static int npSnsFbReqAttachHttp(SceInt32 reqId, SceInt32 httpConnId, SceInt32 httpReqId);
static int npSnsFbReqDetachHttp(SceInt32 reqId);
static int npSnsFbReqAbortInternal(SceInt32 reqId, SceInt32 bySystem);
static int npSnsFbReqCheckAbort(SceInt32 reqId);
static int npSnsFbReqOverrideAbortResult(int curResult, SceInt32 reqId);

static int npSnsFbCheckChatRestriction(SceInt32 reqId, SceInt32 *outAllowed);
static int npSnsFbAbortAccountRequest(SceInt32 npAccountReq);
static int npSnsFbMapFacebookError(SceInt32 fbError);
static int npSnsFbMapTransportError(int err);

static int npSnsFbAuthClientInit(SceInt32 httpTemplateId);
static int npSnsFbAuthClientTerm(void);
static int npSnsFbAuthLogin(SceInt32 reqId, const char *email, const char *password,
			    NpSnsFbSession *outSession, SceInt32 *outFbError);
static int npSnsFbAuthCheckSession(SceInt32 reqId, const NpSnsFbSession *session);
static int npSnsFbAuthAbort(NpSnsFbLibRequest *authLibReq);

static int npSnsFbStreamClientInit(SceInt32 httpTemplateId);
static int npSnsFbStreamClientTerm(void);
static int npSnsFbStreamPublish(SceInt32 reqId, const NpSnsFbSession *session,
				const char *message, const char *attachment,
				const char *actionLinks, char *errMsgBuf, SceSize errMsgBufSize,
				SceInt32 *outFbError);
static int npSnsFbStreamAbort(NpSnsFbStreamRequest *streamReq);

/* Helper: reset inUse flag on a request handle before dispatching another sub-step */
static int npSnsFbClearRequestInUse(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret >= 0 && node->inUse != 0)
		node->inUse = 0;
	if (node != NULL)
		npSnsFbReqRelease(node);
	return ret;
}

/* Helper: tokenize comma/whitespace-separated permission list into a NpSnsFbPermissionMap */
static int npSnsFbParsePermissionList(const char *permList, NpSnsFbPermissionMap *permMap,
				      int maxPerms)
{
	int count = 0;
	SceSize pos = 0;

	while (permList[pos] != '\0') {
		while ((sceClibLookCtypeTable(permList[pos]) & 8) != 0 || permList[pos] == ',')
			++pos;
		if (permList[pos] == '\0')
			break;

		SceSize end = pos;
		while (permList[end] != '\0' && (sceClibLookCtypeTable(permList[end]) & 8) == 0 &&
		       permList[end] != ',') {
			++end;
		}

		SceSize tokenLen = end - pos;
		char permName[65];
		permName[0] = '\0';
		sceClibStrncat(permName, permList + pos, tokenLen);
		permName[tokenLen] = '\0';

		NpSnsFbPermissionValue *entry = permMap->findOrInsert(permName);
		entry->header[0] = '\0';
		entry->summary[0] = '\0';
		entry->granted = 0;

		pos = end;
		++count;
		if (maxPerms > 0 && count >= maxPerms)
			break;
	}
	return count;
}

/* Helper: MD5-hex a signing base string + secret */
static int npSnsFbComputeMd5Hex(const char *baseStr, const char *secret, char *outHex)
{
	SceMd5Context md5Ctx;
	int ret = sceMd5BlockInit(&md5Ctx);
	if (ret != 0)
		return ret;

	sceMd5BlockUpdate(&md5Ctx, baseStr, sceClibStrnlen(baseStr, 0x7FFFFFFFU));
	sceMd5BlockUpdate(&md5Ctx, secret, sceClibStrnlen(secret, 0x7FFFFFFFU));

	SceUInt8 digest[16];
	sceMd5BlockResult(&md5Ctx, digest);

	char *cursor = outHex;
	for (int idx = 0; idx < 16; ++idx) {
		sceClibSnprintf(cursor, 33U, "%02x", static_cast<unsigned int>(digest[idx]));
		cursor += 2;
	}
	outHex[32] = '\0';
	return 0;
}

/* ============================================================================
 * 1. Top-Level SceNpSnsFbUtil API (0x81338ADC..0x81339F7F)
 * ============================================================================ */

/* 0x81338ADC */
static int npSnsFbUtilInitCore(void)
{
	if (s_utilInitialized != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	int tmplId = sceHttpCreateTemplate(NP_SNS_FB_USER_AGENT, 2, 1);
	if (tmplId < 0)
		return tmplId;

	sceHttpSetAcceptEncodingGZIPEnabled(tmplId, SCE_FALSE);
	int ret = npSnsFbStreamClientInit(tmplId);
	if (ret >= 0) {
		s_utilHttpTemplateId = tmplId;
		ret = npSnsFbReqListInit();
		if (ret >= 0) {
			ret = npSnsFbAuthClientInit(tmplId);
			if (ret >= 0) {
				ret = npSnsFbFqlInit(tmplId);
				if (ret >= 0) {
					s_utilInitialized = 1;
					return 0;
				}
				npSnsFbAuthClientTerm();
			}
			npSnsFbReqListTerm();
		}
		npSnsFbStreamClientTerm();
	}

	if (tmplId > 0)
		sceHttpDeleteTemplate(tmplId);
	return ret;
}

/* 0x81338B7E */
int npSnsFbUtilInit(void)
{
	if (s_utilInitialized != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	npSnsFbMemInitCallbacks(npCommunityAlloc, npCommunityFree);
	return npSnsFbUtilInitCore();
}

/* 0x81338BB2 */
int npSnsFbUtilTerm(void)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	npSnsFbFqlTerm();
	npSnsFbAuthClientTerm();
	npSnsFbReqListTerm();
	npSnsFbStreamClientTerm();
	sceHttpDeleteTemplate(s_utilHttpTemplateId);
	s_utilInitialized = 0;
	npSnsFbMemTerm();
	return 0;
}

/* 0x81338BFC */
int npSnsFbUtilIsInitialized(void)
{
	return static_cast<int>(s_utilInitialized);
}

/* 0x81338C08 */
int npSnsFbUtilCreateRequest(void)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	return npSnsFbReqCreate();
}

/* 0x81338C26 */
int npSnsFbUtilDestroyRequest(SceInt32 reqId)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	return npSnsFbReqDestroy(reqId);
}

int npSnsFbCreateContext(void)
{
	return npSnsFbUtilCreateRequest();
}

int npSnsFbDestroyContext(int reqId)
{
	return npSnsFbUtilDestroyRequest(reqId);
}

int npSnsFbIsEnabled(int *outEnabled)
{
	return npSnsFbUtilIsShareTrophyEnabled(outEnabled);
}

/* 0x81338C44 */
int npSnsFbUtilAbortRequest(SceInt32 reqId)
{
	return npSnsFbReqAbortInternal(reqId, 0);
}

/* 0x81338C4C */
int npSnsFbUtilAbortRequestBySystem(SceInt32 reqId)
{
	return npSnsFbReqAbortInternal(reqId, 1);
}

/* 0x81338C54 */
int npSnsFbUtilCheckAccount(SceInt32 reqId, SceInt32 *outSignedIn, SceInt32 *outChatAllowed)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outSignedIn == NULL || outChatAllowed == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	*outSignedIn = 0;
	*outChatAllowed = 0;

	int signedInRet = npAccountIsSignedIn();
	int ret = 0;
	if (signedInRet == 1) {
		ret = 0;
	} else if (signedInRet == 0) {
		ret = NP_SNS_FB_ERR_NOT_SIGNED_IN;
	} else {
		ret = signedInRet;
	}

	if (ret < 0) {
		*outSignedIn = 0;
		return ret;
	}

	*outSignedIn = 1;
	ret = npSnsFbCheckChatRestriction(reqId, outChatAllowed);
	if (ret < 0 &&
	    (ret == NP_SNS_FB_NP_ERR_NOT_SIGNED_IN_A || ret == NP_SNS_FB_NP_ERR_NOT_SIGNED_IN_B)) {
		*outSignedIn = 0;
	}
	return ret;
}

/* 0x81338CD8 */
int npSnsFbUtilAuthAndBind(SceInt32 reqId, const char *email, const char *password,
			   SceInt32 *outFbError)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (email == NULL || password == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));

	if (email[0] != '\0') {
		int ret = npSnsFbAuthLogin(reqId, email, password, &session, outFbError);
		if (ret < 0)
			return ret;
		ret = npSnsFbRegSetAccount(email);
		if (ret < 0)
			return ret;
		ret = npSnsFbRegSetSession(session.sessionKey, session.sessionSecret, session.uid);
		if (ret < 0)
			return ret;
		return 0;
	}

	int ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;
	return npSnsFbAuthCheckSession(reqId, &session);
}

/* 0x81338DA6 */
int npSnsFbUtilPostFeed(SceInt32 reqId, const char *message, const char *attachment,
			const char *actionLinks, char *errMsgBuf, SceSize errMsgBufSize,
			SceInt32 *outFbError)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	SceInt32 signedIn = 0;
	SceInt32 chatAllowed = 0;
	int ret = npSnsFbUtilCheckAccount(reqId, &signedIn, &chatAllowed);
	if (ret < 0)
		return ret;
	if (signedIn == 0)
		return NP_SNS_FB_ERR_NOT_SIGNED_IN;
	if (chatAllowed == 0)
		return NP_SNS_FB_ERR_CHAT_RESTRICTION;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));
	ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	ret = npSnsFbClearRequestInUse(reqId);
	if (ret < 0)
		return ret;

	ret = npSnsFbStreamPublish(reqId, &session, message, attachment, actionLinks,
				   errMsgBuf, errMsgBufSize, outFbError);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81338EAA */
int npSnsFbUtilGetPermissionsInfo(SceInt32 reqId, SceInt32 langIndex, const char *permList,
				  NpSnsFbPermissionsInfoResult *outResult, char *errMsgBuf,
				  SceSize errMsgBufSize, SceInt32 *outFbError)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));
	int ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	NpSnsFbPermissionMap permMap;
	int parsedCount = npSnsFbParsePermissionList(permList, &permMap, NP_SNS_FB_MAX_PERMISSIONS);
	if (parsedCount == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	ret = npSnsFbFqlGetPermissionsInfo(reqId, session.sessionKey, session.sessionSecret,
					   langIndex, &permMap, errMsgBuf, errMsgBufSize,
					   outFbError);
	if (ret < 0)
		return ret;

	outResult->count = 0;
	int outIdx = 0;
	for (NpSnsFbPermissionNode *cur = permMap.first();
	     cur != NULL && outIdx < NP_SNS_FB_MAX_PERMISSIONS;
	     cur = cur->next, ++outIdx) {
		NpSnsFbPermissionValue *dstEntry = &outResult->entries[outIdx];
		sceClibMemset(dstEntry->header, 0, sizeof(dstEntry->header));
		sceClibMemset(dstEntry->summary, 0, sizeof(dstEntry->summary));
		sceClibStrncpy(dstEntry->header, cur->value.header, 0xFFU);
		sceClibStrncpy(dstEntry->summary, cur->value.summary, 0xFFU);
		dstEntry->granted = cur->value.granted;
		++outResult->count;
	}
	return 0;
}

/* 0x813393AA */
int npSnsFbUtilCheckPermissions(SceInt32 reqId, const char *accessToken, const char *permList,
				NpSnsFbCheckPermissionsResult *outResult, char *errMsgBuf,
				SceSize errMsgBufSize, SceInt32 *outFbError)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));
	int ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	NpSnsFbPermissionMap permMap;
	int parsedCount = npSnsFbParsePermissionList(permList, &permMap, 0);
	if (parsedCount == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	ret = npSnsFbFqlCheckPermissions(reqId, accessToken, session.uid, &permMap, errMsgBuf,
					 errMsgBufSize, outFbError);
	if (ret < 0)
		return ret;

	outResult->status = 0;
	sceClibMemset(outResult->grantedPermissions, 0, sizeof(outResult->grantedPermissions));
	sceClibMemset(outResult->ungrantedPermissions, 0, sizeof(outResult->ungrantedPermissions));

	int firstGranted = 1;
	int firstUngranted = 1;
	for (NpSnsFbPermissionNode *cur = permMap.first(); cur != NULL; cur = cur->next) {
		if (cur->value.granted != 0) {
			if (firstGranted == 0)
				sceClibStrncat(outResult->grantedPermissions, ",", 1U);
			sceClibStrncat(outResult->grantedPermissions, cur->permissionName, 0x41U);
			firstGranted = 0;
		} else {
			if (firstUngranted == 0)
				sceClibStrncat(outResult->ungrantedPermissions, ",", 1U);
			sceClibStrncat(outResult->ungrantedPermissions, cur->permissionName, 0x41U);
			firstUngranted = 0;
		}
	}
	return 0;
}

/* 0x813398E0 */
int npSnsFbUtilGetAccessToken(SceInt32 reqId, const char *proxiedAppId, const char *remoteAppId,
			      const char *permissions, NpSnsFbAccessTokenOutput *outToken,
			      char *errMsgBuf, SceSize errMsgBufSize, SceInt32 *outFbError)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));
	int ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	ret = npSnsFbProxyAuthorize(reqId, session.sessionKey, session.sessionSecret,
				    proxiedAppId, remoteAppId, permissions, outToken->accessToken,
				    &outToken->expiration, errMsgBuf, errMsgBufSize, outFbError);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133999A */
int npSnsFbUtilGetAccessTokenWithPermCheck(SceInt32 reqId, const char *proxiedAppId,
					   const char *remoteAppId, const char *permissions,
					   NpSnsFbAccessTokenOutput *outToken, char *errMsgBuf,
					   SceSize errMsgBufSize, SceInt32 *outFbError)
{
	NpSnsFbCheckPermissionsResult checkResult;
	sceClibMemset(&checkResult, 0, sizeof(checkResult));

	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	SceInt32 signedIn = 0;
	SceInt32 chatAllowed = 0;
	SceInt32 gameIntegrations = 0;
	int ret = npSnsFbUtilCheckAccount(reqId, &signedIn, &chatAllowed);
	if (ret < 0)
		return ret;
	if (signedIn == 0)
		return NP_SNS_FB_ERR_NOT_SIGNED_IN;
	if (chatAllowed == 0)
		return NP_SNS_FB_ERR_CHAT_RESTRICTION;

	if (!npSnsFbUtilIsAccountBound())
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	ret = npSnsFbRegGetShareFlags(NULL, NULL, NULL, &gameIntegrations);
	if (ret < 0)
		return ret;
	if (gameIntegrations == 0)
		return NP_SNS_FB_ERR_CONFIG_DISABLED;

	NpSnsFbSession session;
	sceClibMemset(&session, 0, sizeof(session));
	ret = npSnsFbRegGetSession(session.sessionKey, session.sessionSecret, session.uid);
	if (ret < 0)
		return ret;
	if (session.sessionKey[0] == '\0')
		return NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;

	ret = npSnsFbClearRequestInUse(reqId);
	if (ret < 0)
		return ret;

	ret = npSnsFbUtilGetAccessToken(reqId, proxiedAppId, remoteAppId, NULL, outToken,
					errMsgBuf, errMsgBufSize, outFbError);
	if (ret < 0)
		return ret;

	if (permissions == NULL || permissions[0] == '\0')
		return 0;

	ret = npSnsFbClearRequestInUse(reqId);
	if (ret < 0)
		return ret;

	ret = npSnsFbUtilCheckPermissions(reqId, outToken->accessToken, permissions, &checkResult,
					  errMsgBuf, errMsgBufSize, outFbError);
	if (ret < 0)
		return ret;

	if (sceClibStrnlen(checkResult.ungrantedPermissions,
			   sizeof(checkResult.ungrantedPermissions)) != 0) {
		return NP_SNS_FB_ERR_ACCESS_NOT_ALLOWED;
	}
	return 0;
}

/* 0x81339BFA */
int npSnsFbUtilGetAccountName(char *outAccount, SceInt32 bufSize)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outAccount == NULL || bufSize <= 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	char accountBuf[NP_SNS_FB_ACCOUNT_MAX];
	sceClibMemset(accountBuf, 0, sizeof(accountBuf));
	int ret = npSnsFbRegGetAccount(accountBuf);
	if (ret < 0)
		return ret;

	sceClibStrncpy(outAccount, accountBuf, static_cast<SceSize>(bufSize));
	outAccount[bufSize - 1] = '\0';
	return 0;
}

/* 0x81339C70 */
int npSnsFbUtilGetSessionShort(NpSnsFbSessionShort *outSession)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outSession == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	sceClibMemset(outSession, 0, sizeof(NpSnsFbSessionShort));
	int ret = npSnsFbRegGetSession(outSession->sessionKey, outSession->sessionSecret,
				       outSession->uid);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339CB6 */
int npSnsFbUtilGetShareConfig(SceInt32 *outShareTrophy, SceInt32 *outSharePurchase,
			      SceInt32 *outShareRating, SceInt32 *outGameIntegrations)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	int ret = npSnsFbRegGetShareFlags(outShareTrophy, outSharePurchase, outShareRating,
					  outGameIntegrations);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339CDA */
int npSnsFbUtilSetShareConfig(const SceInt32 *shareTrophy, const SceInt32 *sharePurchase,
			      const SceInt32 *shareRating, const SceInt32 *gameIntegrations)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	int ret = npSnsFbRegSetShareFlags(shareTrophy, sharePurchase, shareRating,
					  gameIntegrations);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339CFE */
int npSnsFbUtilClearAllConfig(void)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	int ret = npSnsFbRegResetAll();
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339D22 */
int npSnsFbUtilClearSessionConfig(void)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	int ret = npSnsFbRegResetSession();
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339D46 */
int npSnsFbUtilClearShareConfig(void)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	int ret = npSnsFbRegResetShareFlags();
	return (ret >= 0) ? 0 : ret;
}

/* 0x81339D6A */
int npSnsFbUtilIsShareTrophyEnabled(SceInt32 *outEnabled)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outEnabled == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	char accountBuf[NP_SNS_FB_ACCOUNT_MAX];
	sceClibMemset(accountBuf, 0, sizeof(accountBuf));
	if (npSnsFbRegGetAccount(accountBuf) < 0 || accountBuf[0] == '\0') {
		*outEnabled = 0;
		return 0;
	}

	SceInt32 flagVal = 0;
	int ret = npSnsFbRegGetShareFlags(&flagVal, NULL, NULL, NULL);
	if (ret < 0) {
		*outEnabled = 0;
		return ret;
	}
	*outEnabled = (flagVal != 0) ? 1 : 0;
	return 0;
}

/* 0x81339DFE */
int npSnsFbUtilIsSharePurchaseEnabled(SceInt32 *outEnabled)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outEnabled == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	char accountBuf[NP_SNS_FB_ACCOUNT_MAX];
	sceClibMemset(accountBuf, 0, sizeof(accountBuf));
	if (npSnsFbRegGetAccount(accountBuf) < 0 || accountBuf[0] == '\0') {
		*outEnabled = 0;
		return 0;
	}

	SceInt32 flagVal = 0;
	int ret = npSnsFbRegGetShareFlags(NULL, &flagVal, NULL, NULL);
	if (ret < 0) {
		*outEnabled = 0;
		return ret;
	}
	*outEnabled = (flagVal != 0) ? 1 : 0;
	return 0;
}

/* 0x81339E92 */
int npSnsFbUtilIsShareRatingEnabled(SceInt32 *outEnabled)
{
	if (s_utilInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (outEnabled == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	char accountBuf[NP_SNS_FB_ACCOUNT_MAX];
	sceClibMemset(accountBuf, 0, sizeof(accountBuf));
	if (npSnsFbRegGetAccount(accountBuf) < 0 || accountBuf[0] == '\0') {
		*outEnabled = 0;
		return 0;
	}

	SceInt32 flagVal = 0;
	int ret = npSnsFbRegGetShareFlags(NULL, NULL, &flagVal, NULL);
	if (ret < 0) {
		*outEnabled = 0;
		return ret;
	}
	*outEnabled = (flagVal != 0) ? 1 : 0;
	return 0;
}

/* 0x81339F26 */
SceBool npSnsFbUtilIsAccountBound(void)
{
	if (s_utilInitialized == 0)
		return SCE_FALSE;

	char accountBuf[NP_SNS_FB_ACCOUNT_MAX];
	sceClibMemset(accountBuf, 0, sizeof(accountBuf));
	return (npSnsFbRegGetAccount(accountBuf) >= 0 && accountBuf[0] != '\0')
		       ? SCE_TRUE
		       : SCE_FALSE;
}

/* ============================================================================
 * 2. FQL & proxy.authorize HTTP Operations (0x81339F80..0x8133AF4B)
 * ============================================================================ */

/* 0x81339F80 */
static int npSnsFbFqlInit(SceInt32 httpTemplateId)
{
	if (s_fqlHttpTemplateId != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;
	s_fqlHttpTemplateId = httpTemplateId;
	return 0;
}

/* 0x81339F9E */
static int npSnsFbFqlTerm(void)
{
	if (s_fqlHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	s_fqlHttpTemplateId = 0;
	return 0;
}

/* 0x81339FBC */
static int npSnsFbFqlGetPermissionsInfo(SceInt32 reqId, const char *sessionKey,
					const char *sessionSecret, SceInt32 langIndex,
					NpSnsFbPermissionMap *permMap, char *errMsgBuf,
					SceSize errMsgBufSize, SceInt32 *outFbError)
{
	SceInt32 localeCount = 0;
	while (s_fbLocaleTable[localeCount] != NULL)
		++localeCount;
	if (langIndex < 0 || langIndex >= localeCount)
		langIndex = 1;
	const char *localeStr = s_fbLocaleTable[langIndex];

	NpSnsFbUtilReqNode *reqNode = NULL;
	SceInt32 httpConnId = 0;
	SceInt32 httpReqId = 0;
	int ret = 0;

	if (s_fqlHttpTemplateId == 0) {
		ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
	} else if (reqId == 0 || sessionKey == NULL || sessionSecret == NULL || localeStr == NULL) {
		ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
	} else {
		ret = npSnsFbReqAcquire(reqId, &reqNode);
		if (ret >= 0) {
			if (reqNode->inUse != 0)
				return NP_SNS_FB_ERR_INVALID_ARGUMENT;
			reqNode->inUse = 1;

			char rawQuery[NP_SNS_FB_FQL_QUERY_BUF_MAX];
			char encQuery[NP_SNS_FB_FQL_QUERY_BUF_MAX];
			char workUrl[NP_SNS_FB_FQL_URL_BUF_MAX];
			char sigHex[36];
			sceClibMemset(rawQuery, 0, sizeof(rawQuery));
			sceClibMemset(encQuery, 0, sizeof(encQuery));
			sceClibMemset(workUrl, 0, sizeof(workUrl));
			sceClibMemset(sigHex, 0, sizeof(sigHex));

			sceClibStrncat(rawQuery,
				       "query=SELECT permission_name,header,summary"
				       " FROM permissions_info WHERE permission_name IN (",
				       sizeof(rawQuery) - 1U);
			sceClibStrncat(encQuery,
				       "query=SELECT+permission_name%2Cheader%2Csummary"
				       "+FROM+permissions_info+WHERE+permission_name+IN+%28",
				       sizeof(encQuery) - 1U);

			int isFirst = 1;
			for (NpSnsFbPermissionNode *cur = permMap->first(); cur != NULL;
			     cur = cur->next) {
				if (cur->value.granted != 0)
					continue;
				if (isFirst == 0) {
					sceClibStrncat(rawQuery, ",",
						       sizeof(rawQuery) - 1U -
							       sceClibStrnlen(rawQuery, sizeof(rawQuery)));
					sceClibStrncat(encQuery, "%2C",
						       sizeof(encQuery) - 1U -
							       sceClibStrnlen(encQuery, sizeof(encQuery)));
				}
				sceClibStrncat(rawQuery, "\"",
					       sizeof(rawQuery) - 1U -
						       sceClibStrnlen(rawQuery, sizeof(rawQuery)));
				sceClibStrncat(rawQuery, cur->permissionName,
					       sizeof(rawQuery) - 1U -
						       sceClibStrnlen(rawQuery, sizeof(rawQuery)));
				sceClibStrncat(rawQuery, "\"",
					       sizeof(rawQuery) - 1U -
						       sceClibStrnlen(rawQuery, sizeof(rawQuery)));

				sceClibStrncat(encQuery, "%22",
					       sizeof(encQuery) - 1U -
						       sceClibStrnlen(encQuery, sizeof(encQuery)));
				sceClibStrncat(encQuery, cur->permissionName,
					       sizeof(encQuery) - 1U -
						       sceClibStrnlen(encQuery, sizeof(encQuery)));
				sceClibStrncat(encQuery, "%22",
					       sizeof(encQuery) - 1U -
						       sceClibStrnlen(encQuery, sizeof(encQuery)));
				isFirst = 0;
			}

			sceClibStrncat(rawQuery, ")",
				       sizeof(rawQuery) - 1U -
					       sceClibStrnlen(rawQuery, sizeof(rawQuery)));
			sceClibStrncat(encQuery, "%29",
				       sizeof(encQuery) - 1U -
					       sceClibStrnlen(encQuery, sizeof(encQuery)));

			sceClibSnprintf(workUrl, sizeof(workUrl),
					"api_key=%slocale=%smethod=facebook.fql.query%s"
					"session_key=%sv=1.0",
					NP_SNS_FB_API_KEY, localeStr, rawQuery, sessionKey);

			ret = npSnsFbComputeMd5Hex(workUrl, sessionSecret, sigHex);
			if (ret == 0) {
				sceClibMemset(workUrl, 0, sizeof(workUrl));
				sceClibSnprintf(workUrl, sizeof(workUrl),
						"https://api.facebook.com/restserver.php"
						"?api_key=%s&locale=%s&method=facebook.fql.query"
						"&%s&session_key=%s&v=1.0&sig=%s",
						NP_SNS_FB_API_KEY, localeStr, encQuery,
						sessionKey, sigHex);

				ret = sceHttpCreateConnectionWithURL(s_fqlHttpTemplateId, workUrl,
								     0);
				if (ret > 0) {
					httpConnId = ret;
					ret = sceHttpCreateRequestWithURL(httpConnId, 0, workUrl,
									  0);
					if (ret > 0) {
						httpReqId = ret;
						ret = npSnsFbReqAttachHttp(reqId, httpConnId,
									   httpReqId);
						if (ret >= 0)
							ret = npSnsFbReqCheckAbort(reqId);
						if (ret >= 0)
							ret = sceHttpSendRequest(httpReqId, NULL,
										 0);
						SceInt32 statusCode = 0;
						if (ret >= 0)
							ret = sceHttpGetStatusCode(httpReqId,
										   &statusCode);
						if (ret >= 0 && statusCode == 200) {
							char localMsg[64];
							NpSnsFbErrorInfo errDetail;
							sceClibMemset(localMsg, 0,
								      sizeof(localMsg));
							sceClibMemset(&errDetail, 0,
								      sizeof(errDetail));
							errDetail.errorMsgBuf = localMsg;
							errDetail.errorMsgBufSize =
								sizeof(localMsg);

							int parseRet =
								npSnsFbLibParsePermissionsInfoResponse(
									httpConnId, httpReqId,
									permMap, &errDetail);
							ret = npSnsFbMapTransportError(parseRet);
							if (ret < 0 && errDetail.errorCode != 0) {
								if (outFbError != NULL)
									*outFbError =
										errDetail.errorCode;
								sceClibStrncpy(errMsgBuf, localMsg,
									       errMsgBufSize);
								ret = npSnsFbMapFacebookError(
									errDetail.errorCode);
							}
						}
					}
				}
			}
		}
	}

	npSnsFbReqDetachHttp(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (httpConnId > 0)
		sceHttpDeleteConnection(httpConnId);
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133A676 */
static int npSnsFbFqlCheckPermissions(SceInt32 reqId, const char *accessToken, const char *uid,
				      NpSnsFbPermissionMap *permMap, char *errMsgBuf,
				      SceSize errMsgBufSize, SceInt32 *outFbError)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	SceInt32 httpConnId = 0;
	SceInt32 httpReqId = 0;
	int ret = 0;

	char *urlBuf = static_cast<char *>(sceNpCommonMallocNpServerName(NP_SNS_FB_LONG_URL_BUF_MAX));
	if (urlBuf == NULL) {
		ret = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(urlBuf, 0, NP_SNS_FB_LONG_URL_BUF_MAX);
		if (s_fqlHttpTemplateId == 0) {
			ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
		} else if (reqId == 0 || accessToken == NULL || uid == NULL) {
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		} else {
			ret = npSnsFbReqAcquire(reqId, &reqNode);
			if (ret >= 0) {
				if (reqNode->inUse != 0)
					return NP_SNS_FB_ERR_INVALID_ARGUMENT;
				reqNode->inUse = 1;

				sceClibSnprintf(urlBuf, NP_SNS_FB_LONG_URL_BUF_MAX,
						"https://api.facebook.com/method/fql.query"
						"?access_token=%s&query=SELECT+",
						accessToken);

				int isFirst = 1;
				for (NpSnsFbPermissionNode *cur = permMap->first(); cur != NULL;
				     cur = cur->next) {
					if (cur->value.granted != 0)
						continue;
					if (isFirst == 0) {
						sceClibStrncat(
							urlBuf, "%2C",
							NP_SNS_FB_LONG_URL_BUF_MAX - 1U -
								sceClibStrnlen(
									urlBuf,
									NP_SNS_FB_LONG_URL_BUF_MAX));
					}
					sceClibStrncat(
						urlBuf, cur->permissionName,
						NP_SNS_FB_LONG_URL_BUF_MAX - 1U -
							sceClibStrnlen(urlBuf,
								       NP_SNS_FB_LONG_URL_BUF_MAX));
					isFirst = 0;
				}

				sceClibStrncat(urlBuf, "+FROM+permissions+WHERE+uid+%3D+",
					       NP_SNS_FB_LONG_URL_BUF_MAX - 1U -
						       sceClibStrnlen(urlBuf,
								      NP_SNS_FB_LONG_URL_BUF_MAX));
				sceClibStrncat(urlBuf, uid,
					       NP_SNS_FB_LONG_URL_BUF_MAX - 1U -
						       sceClibStrnlen(urlBuf,
								      NP_SNS_FB_LONG_URL_BUF_MAX));

				ret = sceHttpCreateConnectionWithURL(s_fqlHttpTemplateId, urlBuf,
								     0);
				if (ret > 0) {
					httpConnId = ret;
					ret = sceHttpCreateRequestWithURL(httpConnId, 0, urlBuf, 0);
					if (ret > 0) {
						httpReqId = ret;
						ret = npSnsFbReqAttachHttp(reqId, httpConnId,
									   httpReqId);
						if (ret >= 0)
							ret = npSnsFbReqCheckAbort(reqId);
						if (ret >= 0)
							ret = sceHttpSendRequest(httpReqId, NULL,
										 0);
						SceInt32 statusCode = 0;
						if (ret >= 0)
							ret = sceHttpGetStatusCode(httpReqId,
										   &statusCode);
						if (ret >= 0 && statusCode == 200) {
							char localMsg[64];
							NpSnsFbErrorInfo errDetail;
							sceClibMemset(localMsg, 0,
								      sizeof(localMsg));
							sceClibMemset(&errDetail, 0,
								      sizeof(errDetail));
							errDetail.errorMsgBuf = localMsg;
							errDetail.errorMsgBufSize =
								sizeof(localMsg);

							int parseRet =
								npSnsFbLibParseCheckPermissionsResponse(
									httpConnId, httpReqId,
									permMap, &errDetail);
							ret = npSnsFbMapTransportError(parseRet);
							if (ret < 0 && errDetail.errorCode != 0) {
								if (outFbError != NULL)
									*outFbError =
										errDetail.errorCode;
								sceClibStrncpy(errMsgBuf, localMsg,
									       errMsgBufSize);
								ret = npSnsFbMapFacebookError(
									errDetail.errorCode);
							}
						}
					}
				}
			}
		}
	}

	if (urlBuf != NULL)
		sceNpCommonFreeNpServerName(urlBuf);
	npSnsFbReqDetachHttp(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (httpConnId > 0)
		sceHttpDeleteConnection(httpConnId);
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133A926 */
static int npSnsFbProxyAuthorize(SceInt32 reqId, const char *sessionKey,
				 const char *sessionSecret, const char *proxiedAppId,
				 const char *remoteAppId, const char *permissions,
				 char *outAccessToken, SceUInt64 *outExpires, char *errMsgBuf,
				 SceSize errMsgBufSize, SceInt32 *outFbError)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	SceInt32 httpConnId = 0;
	SceInt32 httpReqId = 0;
	int ret = 0;

	char *urlBuf = static_cast<char *>(sceNpCommonMallocNpServerName(NP_SNS_FB_LONG_URL_BUF_MAX));
	if (urlBuf == NULL) {
		ret = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	} else {
		char sigHex[36];
		sceClibMemset(urlBuf, 0, NP_SNS_FB_LONG_URL_BUF_MAX);
		sceClibMemset(sigHex, 0, sizeof(sigHex));

		if (s_fqlHttpTemplateId == 0) {
			ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
		} else if (reqId == 0 || sessionKey == NULL || sessionSecret == NULL ||
			   proxiedAppId == NULL || remoteAppId == NULL || outExpires == NULL) {
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		} else {
			ret = npSnsFbReqAcquire(reqId, &reqNode);
			if (ret >= 0) {
				if (reqNode->inUse != 0)
					return NP_SNS_FB_ERR_INVALID_ARGUMENT;
				reqNode->inUse = 1;

				int hasPerms = (permissions != NULL && permissions[0] != '\0');
				if (hasPerms) {
					sceClibSnprintf(
						urlBuf, NP_SNS_FB_LONG_URL_BUF_MAX,
						"api_key=%smethod=facebook.proxy.authorize"
						"permissions=%sproxied_app_id=%sremote_app_id=%s"
						"session_key=%sv=1.0",
						NP_SNS_FB_API_KEY, permissions, proxiedAppId,
						remoteAppId, sessionKey);
				} else {
					sceClibSnprintf(
						urlBuf, NP_SNS_FB_LONG_URL_BUF_MAX,
						"api_key=%smethod=facebook.proxy.authorize"
						"proxied_app_id=%sremote_app_id=%s"
						"session_key=%sv=1.0",
						NP_SNS_FB_API_KEY, proxiedAppId, remoteAppId,
						sessionKey);
				}

				ret = npSnsFbComputeMd5Hex(urlBuf, sessionSecret, sigHex);
				if (ret == 0) {
					sceClibMemset(urlBuf, 0, NP_SNS_FB_LONG_URL_BUF_MAX);
					if (hasPerms) {
						sceClibSnprintf(
							urlBuf, NP_SNS_FB_LONG_URL_BUF_MAX,
							"https://api.facebook.com/restserver.php"
							"?api_key=%s&method=facebook.proxy.authorize"
							"&permissions=%s&proxied_app_id=%s"
							"&remote_app_id=%s&session_key=%s"
							"&v=1.0&sig=%s",
							NP_SNS_FB_API_KEY, permissions,
							proxiedAppId, remoteAppId, sessionKey,
							sigHex);
					} else {
						sceClibSnprintf(
							urlBuf, NP_SNS_FB_LONG_URL_BUF_MAX,
							"https://api.facebook.com/restserver.php"
							"?api_key=%s&method=facebook.proxy.authorize"
							"&proxied_app_id=%s&remote_app_id=%s"
							"&session_key=%s&v=1.0&sig=%s",
							NP_SNS_FB_API_KEY, proxiedAppId,
							remoteAppId, sessionKey, sigHex);
					}

					ret = sceHttpCreateConnectionWithURL(s_fqlHttpTemplateId,
									     urlBuf, 0);
					if (ret > 0) {
						httpConnId = ret;
						ret = sceHttpCreateRequestWithURL(httpConnId, 0,
										  urlBuf, 0);
						if (ret > 0) {
							httpReqId = ret;
							ret = npSnsFbReqAttachHttp(reqId,
										   httpConnId,
										   httpReqId);
							if (ret >= 0)
								ret = npSnsFbReqCheckAbort(reqId);
							if (ret >= 0)
								ret = sceHttpSendRequest(httpReqId,
											 NULL, 0);
							SceInt32 statusCode = 0;
							if (ret >= 0)
								ret = sceHttpGetStatusCode(
									httpReqId, &statusCode);
							if (ret >= 0 && statusCode == 200) {
								char localMsg[64];
								NpSnsFbErrorInfo errDetail;
								sceClibMemset(localMsg, 0,
									      sizeof(localMsg));
								sceClibMemset(&errDetail, 0,
									      sizeof(errDetail));
								errDetail.errorMsgBuf = localMsg;
								errDetail.errorMsgBufSize =
									sizeof(localMsg);

								SceUInt64 absExpires = 0;
								int parseRet =
									npSnsFbLibParseAccessTokenResponse(
										httpConnId,
										httpReqId,
										outAccessToken,
										&absExpires,
										&errDetail);
								ret = npSnsFbMapTransportError(
									parseRet);
								if (ret >= 0) {
									SceRtcTick netTick;
									SceDateTime curDate;
									SceUInt64 nowEpoch = 0;
									ret = sceRtcGetCurrentNetworkTick(
										&netTick);
									if (ret >= 0)
										ret = sceRtcSetTick(
											&curDate,
											&netTick);
									if (ret >= 0)
										ret = sceRtcGetTime64_t(
											&curDate,
											&nowEpoch);
									if (ret >= 0)
										*outExpires =
											absExpires -
											nowEpoch;
								} else if (errDetail.errorCode !=
									   0) {
									if (outFbError != NULL)
										*outFbError =
											errDetail.errorCode;
									sceClibStrncpy(
										errMsgBuf, localMsg,
										errMsgBufSize);
									ret = npSnsFbMapFacebookError(
										errDetail.errorCode);
								}
							}
						}
					}
				}
			}
		}
	}

	if (urlBuf != NULL)
		sceNpCommonFreeNpServerName(urlBuf);
	npSnsFbReqDetachHttp(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (httpConnId > 0)
		sceHttpDeleteConnection(httpConnId);
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133AF1E */
static int npSnsFbFqlAbortHttpRequest(SceInt32 httpReqId)
{
	if (s_fqlHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (httpReqId == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return sceHttpAbortRequest(httpReqId);
}

/* ============================================================================
 * 3. Registry Access Helpers (/CONFIG/NP2/FACEBOOK) (0x8133AF4C..0x8133B51B)
 * ============================================================================ */

/* 0x8133AF4C */
static int npSnsFbRegGetAccount(char *outAccount)
{
	if (outAccount == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	int ret = sceRegMgrGetKeyStr(NP_SNS_FB_REG_CATEGORY, "account", outAccount,
					    NP_SNS_FB_ACCOUNT_MAX);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133AF7C */
static int npSnsFbRegSetAccount(const char *account)
{
	if (account == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	int ret = sceRegMgrSetKeyStr(NP_SNS_FB_REG_CATEGORY, "account", account,
					    NP_SNS_FB_ACCOUNT_MAX);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133AFAC */
static int npSnsFbRegGetSession(char *outSessionKey, char *outSessionSecret, char *outUid)
{
	if (outSessionKey == NULL || outSessionSecret == NULL || outUid == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int ret = sceRegMgrGetKeyStr(NP_SNS_FB_REG_CATEGORY, "session_key", outSessionKey,
					    NP_SNS_FB_SESSION_KEY_MAX);
	if (ret < 0)
		return ret;
	ret = sceRegMgrGetKeyStr(NP_SNS_FB_REG_CATEGORY, "session_secret", outSessionSecret,
					NP_SNS_FB_SESSION_SECRET_MAX);
	if (ret < 0)
		return ret;
	ret = sceRegMgrGetKeyStr(NP_SNS_FB_REG_CATEGORY, "uid", outUid, NP_SNS_FB_UID_MAX);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133B01A */
static int npSnsFbRegSetSession(const char *sessionKey, const char *sessionSecret, const char *uid)
{
	if (sessionKey == NULL || sessionSecret == NULL || uid == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int ret = sceRegMgrSetKeyStr(NP_SNS_FB_REG_CATEGORY, "session_key", sessionKey,
					    NP_SNS_FB_SESSION_KEY_MAX);
	if (ret < 0)
		return ret;
	ret = sceRegMgrSetKeyStr(NP_SNS_FB_REG_CATEGORY, "session_secret", sessionSecret,
					NP_SNS_FB_SESSION_SECRET_MAX);
	if (ret < 0)
		return ret;
	ret = sceRegMgrSetKeyStr(NP_SNS_FB_REG_CATEGORY, "uid", uid, NP_SNS_FB_UID_MAX);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133B088 */
static int npSnsFbRegGetShareFlags(SceInt32 *outShareTrophy, SceInt32 *outSharePurchase,
				   SceInt32 *outShareRating, SceInt32 *outGameIntegrations)
{
	if (outShareTrophy == NULL && outSharePurchase == NULL && outShareRating == NULL &&
	    outGameIntegrations == NULL) {
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	}

	SceInt32 regVal = 0;
	if (outShareTrophy != NULL) {
		int ret = sceRegMgrGetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_trophy",
						    &regVal);
		if (ret < 0)
			return ret;
		*outShareTrophy = (regVal != 0) ? 1 : 0;
	}
	if (outSharePurchase != NULL) {
		int ret = sceRegMgrGetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_purchase",
						    &regVal);
		if (ret < 0)
			return ret;
		*outSharePurchase = (regVal != 0) ? 1 : 0;
	}
	if (outShareRating != NULL) {
		int ret = sceRegMgrGetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_rating",
						    &regVal);
		if (ret < 0)
			return ret;
		*outShareRating = (regVal != 0) ? 1 : 0;
	}
	if (outGameIntegrations != NULL) {
		int ret = sceRegMgrGetKeyInt(NP_SNS_FB_REG_CATEGORY, "game_integrations",
						    &regVal);
		if (ret < 0)
			return ret;
		*outGameIntegrations = (regVal != 0) ? 1 : 0;
	}
	return 0;
}

/* 0x8133B174 */
static int npSnsFbRegSetShareFlags(const SceInt32 *shareTrophy, const SceInt32 *sharePurchase,
				   const SceInt32 *shareRating,
				   const SceInt32 *gameIntegrations)
{
	if (shareTrophy != NULL) {
		int ret = sceRegMgrSetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_trophy",
						    *shareTrophy);
		if (ret < 0)
			return ret;
	}
	if (sharePurchase != NULL) {
		int ret = sceRegMgrSetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_purchase",
						    *sharePurchase);
		if (ret < 0)
			return ret;
	}
	if (shareRating != NULL) {
		int ret = sceRegMgrSetKeyInt(NP_SNS_FB_REG_CATEGORY, "share_rating",
						    *shareRating);
		if (ret < 0)
			return ret;
	}
	if (gameIntegrations != NULL) {
		int ret = sceRegMgrSetKeyInt(NP_SNS_FB_REG_CATEGORY, "game_integrations",
						    *gameIntegrations);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int npSnsFbRegResetKey(const char *keyName)
{
	char fullPath[256];
	sceClibMemset(fullPath, 0, sizeof(fullPath));
	sceClibSnprintf(fullPath, sizeof(fullPath), "%s/%s", NP_SNS_FB_REG_CATEGORY, keyName);
	int ret = sceRegMgrResetRegistryLv(fullPath, 32);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8133B1F4 */
static int npSnsFbRegResetSession(void)
{
	int ret = npSnsFbRegResetKey("session_key");
	if (ret < 0)
		return ret;
	ret = npSnsFbRegResetKey("session_secret");
	if (ret < 0)
		return ret;
	ret = npSnsFbRegResetKey("uid");
	if (ret < 0)
		return ret;
	return npSnsFbRegResetKey("access_token");
}

/* 0x8133B346 */
static int npSnsFbRegResetShareFlags(void)
{
	int ret = npSnsFbRegResetKey("share_trophy");
	if (ret < 0)
		return ret;
	ret = npSnsFbRegResetKey("share_purchase");
	if (ret < 0)
		return ret;
	ret = npSnsFbRegResetKey("share_rating");
	if (ret < 0)
		return ret;
	return npSnsFbRegResetKey("game_integrations");
}

/* 0x8133B498 */
static int npSnsFbRegResetAll(void)
{
	int ret = npSnsFbRegResetKey("account");
	if (ret < 0)
		return ret;
	ret = npSnsFbRegResetSession();
	if (ret < 0)
		return ret;
	return npSnsFbRegResetShareFlags();
}

/* ============================================================================
 * 4. Thread-Safe Request Registry (0x8133B51C..0x8133C1F7)
 * ============================================================================ */

/* 0x8133B51C */
static int npSnsFbReqListInit(void)
{
	if (s_reqListInitialized != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	sceClibMemset(&s_reqListSentinel, 0, sizeof(s_reqListSentinel));
	s_reqListSentinel.reqId = NP_SNS_FB_REQ_ID_BASE;
	s_reqListMutex = new NpSnsFbLwMutex("SceNpSnsFbUtilReqList");
	if (s_reqListMutex == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	s_reqListInitialized = 1;
	return 0;
}

/* 0x8133B58E */
static void npSnsFbReqRelease(NpSnsFbUtilReqNode *node)
{
	if (node == NULL)
		return;

	NpSnsFbScopedLock listGuard(s_reqListMutex);
	if (node->refCount != 0)
		--node->refCount;

	if (node->markedForDelete == 1 && node->refCount == 0) {
		if (node->reqMutex != NULL) {
			{
				NpSnsFbScopedLock reqGuard(node->reqMutex);
				node->streamReq = NULL;
				node->npAccountReq = 0;
				node->authLibReq = NULL;
				node->httpConnId = 0;
				node->httpReqId = 0;
			}
			delete node->reqMutex;
			node->reqMutex = NULL;
		}
		npSnsFbMemFree(node);
	}
}

/* 0x8133B62C */
static int npSnsFbReqAcquire(SceInt32 reqId, NpSnsFbUtilReqNode **outNode)
{
	if (outNode == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	*outNode = NULL;
	if (reqId == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	NpSnsFbScopedLock listGuard(s_reqListMutex);
	for (NpSnsFbUtilReqNode *cur = s_reqListSentinel.next; cur != NULL; cur = cur->next) {
		if (cur->reqId == reqId) {
			++cur->refCount;
			*outNode = cur;
			return 0;
		}
		if (cur->reqId > reqId)
			break;
	}
	return NP_SNS_FB_ERR_INVALID_ARGUMENT;
}

/* 0x8133B6D2 */
static int npSnsFbReqDestroy(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	npSnsFbUtilAbortRequest(reqId);
	{
		NpSnsFbScopedLock listGuard(s_reqListMutex);
		if (node->prev != NULL)
			node->prev->next = node->next;
		if (node->next != NULL)
			node->next->prev = node->prev;
		node->markedForDelete = 1;
	}
	npSnsFbReqRelease(node);
	return 0;
}

/* 0x8133B7DE */
static int npSnsFbReqListTerm(void)
{
	if (s_reqListInitialized == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	NpSnsFbUtilReqNode *cur = s_reqListSentinel.next;
	while (cur != NULL) {
		NpSnsFbUtilReqNode *nextNode = cur->next;
		npSnsFbReqDestroy(cur->reqId);
		cur = nextNode;
	}

	if (s_reqListMutex != NULL) {
		delete s_reqListMutex;
		s_reqListMutex = NULL;
	}
	s_reqListInitialized = 0;
	return 0;
}

/* 0x8133B840 */
static int npSnsFbReqCreate(void)
{
	NpSnsFbUtilReqNode *node =
		static_cast<NpSnsFbUtilReqNode *>(npSnsFbMemAlloc(sizeof(NpSnsFbUtilReqNode)));
	if (node == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	sceClibMemset(node, 0, sizeof(NpSnsFbUtilReqNode));
	node->reqMutex = new NpSnsFbLwMutex("SceNpSnsFbUtilReq");
	if (node->reqMutex == NULL) {
		npSnsFbMemFree(node);
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;
	}

	int resultId = NP_SNS_FB_ERR_EXCEEDS_MAX;
	{
		NpSnsFbScopedLock listGuard(s_reqListMutex);
		NpSnsFbUtilReqNode *insertAfter = &s_reqListSentinel;
		NpSnsFbUtilReqNode *cur = s_reqListSentinel.next;
		SceInt32 prevId = s_reqListSentinel.reqId;

		while (cur != NULL && cur->reqId == prevId + 1) {
			insertAfter = cur;
			prevId = cur->reqId;
			cur = cur->next;
		}

		if (prevId < NP_SNS_FB_REQ_ID_MAX) {
			resultId = prevId + 1;
			node->reqId = resultId;
			node->prev = insertAfter;
			node->next = insertAfter->next;
			if (insertAfter->next != NULL)
				insertAfter->next->prev = node;
			insertAfter->next = node;
		}
	}

	if (resultId < 0) {
		delete node->reqMutex;
		node->reqMutex = NULL;
		npSnsFbMemFree(node);
		return resultId;
	}
	return node->reqId;
}

/* 0x8133B96A */
static int npSnsFbReqAttachStream(SceInt32 reqId, NpSnsFbStreamRequest *streamReq)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		if (node->streamReq != NULL)
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		else
			node->streamReq = streamReq;
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133BA44 */
static int npSnsFbReqDetachStream(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		node->streamReq = NULL;
	}
	npSnsFbReqRelease(node);
	return 0;
}

/* 0x8133BB0A */
static int npSnsFbReqAttachAccount(SceInt32 reqId, SceInt32 npAccountReq)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		if (node->npAccountReq != 0)
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		else
			node->npAccountReq = npAccountReq;
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133BBE4 */
static int npSnsFbReqDetachAccount(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		node->npAccountReq = 0;
	}
	npSnsFbReqRelease(node);
	return 0;
}

/* 0x8133BCAA */
static int npSnsFbReqAttachAuthLib(SceInt32 reqId, NpSnsFbLibRequest *authLibReq)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		if (node->authLibReq != NULL)
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		else
			node->authLibReq = authLibReq;
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133BD84 */
static int npSnsFbReqDetachAuthLib(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		node->authLibReq = NULL;
	}
	npSnsFbReqRelease(node);
	return 0;
}

/* 0x8133BE4A */
static int npSnsFbReqAttachHttp(SceInt32 reqId, SceInt32 httpConnId, SceInt32 httpReqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		if (node->httpReqId != 0) {
			ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
		} else {
			node->httpConnId = httpConnId;
			node->httpReqId = httpReqId;
		}
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133BF2A */
static int npSnsFbReqDetachHttp(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		node->httpConnId = 0;
		node->httpReqId = 0;
	}
	npSnsFbReqRelease(node);
	return 0;
}

/* 0x8133BFF2, 0x8133C0EA, 0x8133C0F6 */
static int npSnsFbReqAbortInternal(SceInt32 reqId, SceInt32 bySystem)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		node->aborted = 1;
		if (bySystem != 0)
			node->canceledBySystem = 1;
		if (node->streamReq != NULL)
			ret = npSnsFbStreamAbort(node->streamReq);
		if (node->npAccountReq != 0)
			ret = npSnsFbAbortAccountRequest(node->npAccountReq);
		if (node->authLibReq != NULL)
			ret = npSnsFbAuthAbort(node->authLibReq);
		if (node->httpReqId != 0)
			ret = npSnsFbFqlAbortHttpRequest(node->httpReqId);
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133C102 */
static int npSnsFbReqCheckAbort(SceInt32 reqId)
{
	NpSnsFbUtilReqNode *node = NULL;
	int ret = npSnsFbReqAcquire(reqId, &node);
	if (ret < 0)
		return ret;

	{
		NpSnsFbScopedLock reqGuard(node->reqMutex);
		if (node->canceledBySystem != 0)
			ret = NP_SNS_FB_ERR_CANCELED_BY_SYSTEM;
		else if (node->aborted != 0)
			ret = NP_SNS_FB_ERR_ABORTED;
	}
	npSnsFbReqRelease(node);
	return ret;
}

/* 0x8133C1CE */
static int npSnsFbReqOverrideAbortResult(int curResult, SceInt32 reqId)
{
	int abortRet = npSnsFbReqCheckAbort(reqId);
	if (abortRet == NP_SNS_FB_ERR_ABORTED || abortRet == NP_SNS_FB_ERR_CANCELED_BY_SYSTEM)
		return abortRet;
	return curResult;
}

/* ============================================================================
 * 5. Account Restriction Check, Error Mapping, Auth & Stream Wrappers (0x8133C1F8..0x8133C993)
 * ============================================================================ */

struct NpSnsFbAccountMeta {
	SceUInt64 accountId;					/* +0x00 */
	SceUInt8 regionBytes[4];				/* +0x08 */
	SceUInt8 contentRatingFlags[4];				/* +0x0C (byte 2 bit 0x80 = chat restrict) */
};

/* 0x8133C1F8 */
static int npSnsFbCheckChatRestriction(SceInt32 reqId, SceInt32 *outAllowed)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	SceInt32 accountReq = 0;
	int ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;

	if (reqId != 0 && outAllowed != NULL) {
		ret = npSnsFbReqAcquire(reqId, &reqNode);
		if (ret >= 0) {
			if (reqNode->inUse != 0)
				return NP_SNS_FB_ERR_INVALID_ARGUMENT;
			reqNode->inUse = 1;

			ret = npAccountCreateRequest();
			if (ret >= 0) {
				accountReq = ret;
				ret = npSnsFbReqAttachAccount(reqId, accountReq);
				if (ret >= 0)
					ret = npSnsFbReqCheckAbort(reqId);
				if (ret >= 0) {
					SceUInt8 profileBuf[464];
					NpSnsFbAccountMeta meta;
					sceClibMemset(profileBuf, 0, sizeof(profileBuf));
					sceClibMemset(&meta, 0, sizeof(meta));
					ret = npAccountGetProfileAndMeta(accountReq, profileBuf,
									 &meta);
					if (ret >= 0)
						*outAllowed =
							((meta.contentRatingFlags[2] & 0x80U) == 0)
								? 1
								: 0;
				}
			}
		}
	}

	npSnsFbReqDetachAccount(reqId);
	if (accountReq > 0)
		npAccountDestroyRequest(accountReq);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133C2FC */
static int npSnsFbAbortAccountRequest(SceInt32 npAccountReq)
{
	if (npAccountReq == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npAccountAbortRequest(npAccountReq);
}

/* 0x8133C314 */
static int npSnsFbMapFacebookError(SceInt32 fbError)
{
	if (fbError == 0)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;
	if (fbError == 2 || fbError == 455)
		return NP_SNS_FB_ERR_API_ERROR;
	if (fbError == 102)
		return NP_SNS_FB_ERR_SESSION_INVALID;

	switch (fbError) {
	case 400:
	case 401:
		return NP_SNS_FB_ERR_INVALID_CREDENTIALS;
	case 402:
	case 403:
		return NP_SNS_FB_ERR_AUTH_CLIENT_ERROR;
	case 404:
		return NP_SNS_FB_ERR_ACCOUNT_LOCKED;
	case 405:
		return NP_SNS_FB_ERR_ACCOUNT_CHECKPOINT;
	case 406:
		return NP_SNS_FB_ERR_ACCOUNT_DISABLED;
	case 407:
		return NP_SNS_FB_ERR_PASSWORD_CHANGE_REQUIRED;
	default:
		if (fbError >= 400 && fbError <= 449)
			return NP_SNS_FB_ERR_AUTH_CLIENT_ERROR;
		return NP_SNS_FB_ERR_API_ERROR;
	}
}

/* 0x8133C3DE */
static int npSnsFbMapTransportError(int err)
{
	if (err == 0)
		return 0;
	if (err > 0) {
		switch (err) {
		case NP_SNS_FB_POSIX_ENOMEM:
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		case NP_SNS_FB_POSIX_EINVAL:
			return NP_SNS_FB_ERR_INVALID_ARGUMENT;
		case NP_SNS_FB_POSIX_ECANCELED:
			return NP_SNS_FB_ERR_ABORTED;
		default:
			return NP_SNS_FB_ERR_INTERNAL_ERROR;
		}
	}
	if (((static_cast<SceUInt32>(err) >> 16) & 0xFFFU) == 0x85U)
		return NP_SNS_FB_ERR_XML_PARSE;
	return err;
}

/* 0x8133C434 */
static int npSnsFbAuthClientInit(SceInt32 httpTemplateId)
{
	if (s_authHttpTemplateId != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;
	s_authHttpTemplateId = httpTemplateId;
	return 0;
}

/* 0x8133C452 */
static int npSnsFbAuthClientTerm(void)
{
	if (s_authHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	s_authHttpTemplateId = 0;
	return 0;
}

/* 0x8133C470 */
static int npSnsFbAuthLogin(SceInt32 reqId, const char *email, const char *password,
			    NpSnsFbSession *outSession, SceInt32 *outFbError)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	NpSnsFbLibRequest *libReq = NULL;
	SceInt32 fbError = 0;

	NpSnsFbLibConfig libCfg;
	sceClibMemset(&libCfg, 0, sizeof(libCfg));
	libCfg.httpTemplateId = s_authHttpTemplateId;
	libCfg.apiKey = NP_SNS_FB_API_KEY;
	libCfg.appSecret = NP_SNS_FB_APP_SECRET;
	libCfg.allocFunc = npSnsFbMemAlloc;
	libCfg.freeFunc = npSnsFbMemFree;
	libCfg.workBufSize = 0x2000U;

	int ret = 0;
	if (s_authHttpTemplateId == 0) {
		ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
	} else if (reqId == 0 || outSession == NULL) {
		ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
	} else {
		ret = npSnsFbReqAcquire(reqId, &reqNode);
		if (ret >= 0) {
			if (reqNode->inUse != 0)
				return NP_SNS_FB_ERR_INVALID_ARGUMENT;
			reqNode->inUse = 1;

			ret = npSnsFbMapTransportError(npSnsFbLibCreateRequest(&libCfg, &libReq));
			if (ret >= 0)
				ret = npSnsFbReqAttachAuthLib(reqId, libReq);
			if (ret >= 0)
				ret = npSnsFbReqCheckAbort(reqId);
			if (ret >= 0) {
				int loginRet = npSnsFbLibAuthLogin(&libCfg, outSession, libReq,
								   email, password, &fbError);
				if (outFbError != NULL)
					*outFbError = fbError;
				ret = npSnsFbMapTransportError(loginRet);
				if (ret == -1)
					ret = npSnsFbMapFacebookError(fbError);
			}
		}
	}

	npSnsFbReqDetachAuthLib(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (libReq != NULL)
		npSnsFbMapTransportError(npSnsFbLibDestroyRequest(&libCfg, libReq));
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133C5B6 */
static int npSnsFbAuthCheckSession(SceInt32 reqId, const NpSnsFbSession *session)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	NpSnsFbLibRequest *libReq = NULL;
	SceInt32 fbError = 0;

	NpSnsFbLibConfig libCfg;
	sceClibMemset(&libCfg, 0, sizeof(libCfg));
	libCfg.httpTemplateId = s_authHttpTemplateId;
	libCfg.apiKey = NP_SNS_FB_API_KEY;
	libCfg.appSecret = NP_SNS_FB_APP_SECRET;
	libCfg.allocFunc = npSnsFbMemAlloc;
	libCfg.freeFunc = npSnsFbMemFree;
	libCfg.workBufSize = 0x2000U;

	int ret = 0;
	if (s_authHttpTemplateId == 0) {
		ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
	} else if (reqId == 0 || session == NULL) {
		ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
	} else {
		ret = npSnsFbReqAcquire(reqId, &reqNode);
		if (ret >= 0) {
			if (reqNode->inUse != 0)
				return NP_SNS_FB_ERR_INVALID_ARGUMENT;
			reqNode->inUse = 1;

			ret = npSnsFbMapTransportError(npSnsFbLibCreateRequest(&libCfg, &libReq));
			if (ret >= 0)
				ret = npSnsFbReqAttachAuthLib(reqId, libReq);
			if (ret >= 0)
				ret = npSnsFbReqCheckAbort(reqId);
			if (ret >= 0) {
				char uidBuf[NP_SNS_FB_UID_MAX];
				sceClibMemset(uidBuf, 0, sizeof(uidBuf));
				int userRet = npSnsFbLibGetLoggedInUser(&libCfg, session, libReq,
									uidBuf, &fbError);
				ret = npSnsFbMapTransportError(userRet);
				if (ret >= 0)
					uidBuf[20] = '\0';
				else if (ret == -1)
					ret = npSnsFbMapFacebookError(fbError);
			}
		}
	}

	npSnsFbReqDetachAuthLib(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (libReq != NULL)
		npSnsFbMapTransportError(npSnsFbLibDestroyRequest(&libCfg, libReq));
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133C6FC */
static int npSnsFbAuthAbort(NpSnsFbLibRequest *authLibReq)
{
	if (s_authHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (authLibReq == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	NpSnsFbLibConfig libCfg;
	sceClibMemset(&libCfg, 0, sizeof(libCfg));
	libCfg.httpTemplateId = s_authHttpTemplateId;
	libCfg.allocFunc = npSnsFbMemAlloc;
	libCfg.freeFunc = npSnsFbMemFree;
	return npSnsFbMapTransportError(npSnsFbLibAbortRequest(&libCfg, authLibReq));
}

/* 0x8133C778 */
static int npSnsFbStreamClientInit(SceInt32 httpTemplateId)
{
	if (s_streamHttpTemplateId != 0)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	NpSnsFbStreamInitParam initParam = {
		httpTemplateId,
		npSnsFbMemAlloc,
		npSnsFbMemFree
	};
	int ret = npSnsFbMapTransportError(npSnsFbStreamInit(&initParam));
	if (ret < 0)
		return ret;
	s_streamHttpTemplateId = httpTemplateId;
	return 0;
}

/* 0x8133C7E4 */
static int npSnsFbStreamClientTerm(void)
{
	if (s_streamHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	npSnsFbMapTransportError(npSnsFbStreamTerm());
	s_streamHttpTemplateId = 0;
	return 0;
}

/* 0x8133C812 */
static int npSnsFbStreamPublish(SceInt32 reqId, const NpSnsFbSession *session,
				const char *message, const char *attachment,
				const char *actionLinks, char *errMsgBuf, SceSize errMsgBufSize,
				SceInt32 *outFbError)
{
	NpSnsFbUtilReqNode *reqNode = NULL;
	NpSnsFbStreamRequest *streamReq = NULL;
	SceInt32 fbError = 0;

	NpSnsFbStreamAppConfig appCfg = {
		NP_SNS_FB_API_KEY,
		NP_SNS_FB_APP_SECRET,
		0x2000U,
		s_streamHttpTemplateId
	};

	int ret = 0;
	if (s_streamHttpTemplateId == 0) {
		ret = NP_SNS_FB_ERR_NOT_INITIALIZED;
	} else if (reqId == 0 || session == NULL) {
		ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
	} else {
		ret = npSnsFbReqAcquire(reqId, &reqNode);
		if (ret >= 0) {
			if (reqNode->inUse != 0)
				return NP_SNS_FB_ERR_INVALID_ARGUMENT;
			reqNode->inUse = 1;

			ret = npSnsFbMapTransportError(npSnsFbStreamCreateRequest(&streamReq));
			if (ret >= 0)
				ret = npSnsFbReqAttachStream(reqId, streamReq);
			if (ret >= 0)
				ret = npSnsFbReqCheckAbort(reqId);
			if (ret >= 0) {
				int pubRet = npSnsFbStreamPublishSimple(streamReq, &appCfg, session,
									message, attachment,
									actionLinks, errMsgBuf,
									errMsgBufSize, &fbError);
				ret = npSnsFbMapTransportError(pubRet);
				if (ret < 0) {
					if (outFbError != NULL)
						*outFbError = fbError;
					if (ret == NP_SNS_FB_ERR_API_ERROR && fbError != 0)
						ret = npSnsFbMapFacebookError(fbError);
				}
			}
		}
	}

	npSnsFbReqDetachStream(reqId);
	if (reqNode != NULL)
		npSnsFbReqRelease(reqNode);
	if (streamReq != NULL)
		npSnsFbMapTransportError(npSnsFbStreamDestroyRequest(streamReq));
	return npSnsFbReqOverrideAbortResult(ret, reqId);
}

/* 0x8133C964 */
static int npSnsFbStreamAbort(NpSnsFbStreamRequest *streamReq)
{
	if (s_streamHttpTemplateId == 0)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (streamReq == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbMapTransportError(npSnsFbStreamAbortRequest(streamReq));
}
