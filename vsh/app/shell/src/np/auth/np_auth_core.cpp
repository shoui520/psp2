/*
 * np_auth_core.cpp: internal SceNpAuth HTTPS ticket acquisition engine
 * (0x8132475C..0x81325EC3).
 */
#include "np_auth_core.h"

#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libssl.h>
#include <np_common/np_common.h>
#include <vshbridge/vshbridge.h>

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

extern "C" {
extern SceUInt32 SceLibKernel_9F793F84(void);
extern int SceNetInternal_9DBD2F60(SceUID threadId, int enable, int reserved);
extern int SceNpCommon_14BC2B99(char *dst, SceSize dstSize, const char *src,
				SceSize srcLength, SceSize *outLength);
extern void sceNpCommonGetSystemSwVersion(char *outVersion);
extern int sceRegMgrGetKeyStr(const char *category, const char *name,
				     char *buf, SceSize size);
extern int sceRegMgrGetKeyBin(const char *category, const char *name,
				     void *buf, SceSize size);
extern int sceHttpsEnableOptionPrivate(int tmplId, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int tmplId, SceUInt32 sslFlags);
extern int vshSblAimgrGetConsoleId(void *consoleId);
extern int vshSblSsDecryptWithPortability(int keyType, const void *iv,
					  const void *inBuf, void *outBuf);
extern int vshSblSsCreatePassPhrase(const void *inData, void *outPhrase);
}

#define NP_AUTH_MAX_REQUEST_ID		16
#define NP_AUTH_HTTP_TIMEOUT_USEC	20000000U
#define NP_AUTH_SSL_ENABLE_FLAGS	101U
#define NP_AUTH_SSL_DISABLE_FLAGS	24U
#define NP_AUTH_URL_BUF_SIZE		128U
#define NP_AUTH_BODY_BUF_SIZE		3584U
#define NP_AUTH_WORK_BUF_SIZE		(NP_AUTH_URL_BUF_SIZE + NP_AUTH_BODY_BUF_SIZE)
#define NP_AUTH_SDK_LEGACY_URI_ESCAPE	0x01600000U
#define NP_AUTH_PASSPHRASE_BUF_LEN	57U
#define NP_AUTH_SERVER_ERR_BASE		0x80550400U
#define NP_AUTH_SERVER_ERR_GENERIC	0x80550480U

enum NpAuthRequestStatus {
	NP_AUTH_STATUS_INIT	= 0,
	NP_AUTH_STATUS_QUEUED	= 1,
	NP_AUTH_STATUS_RUNNING	= 2,
	NP_AUTH_STATUS_ABORTED	= 3,
	NP_AUTH_STATUS_DONE	= 4
};

enum NpAuthFormParamKind {
	NP_AUTH_PARAM_TYPE		= 0,
	NP_AUTH_PARAM_FIRST		= 1,
	NP_AUTH_PARAM_MINITICKET	= 2,
	NP_AUTH_PARAM_SERVICE_ID	= 3,
	NP_AUTH_PARAM_LOGIN_ID		= 4,
	NP_AUTH_PARAM_PASSWORD		= 5,
	NP_AUTH_PARAM_ENTITLEMENT_ID	= 6,
	NP_AUTH_PARAM_CONSUMED_COUNT	= 7,
	NP_AUTH_PARAM_COOKIE		= 8,
	NP_AUTH_PARAM_CONSOLE_ID	= 9,
	NP_AUTH_PARAM_SERVICE_ENTITY	= 10,
	NP_AUTH_PARAM_COUNT		= 11
};

struct NpAuthFormKeyDesc {
	SceSize keyLen;
	const char *keyPrefix;
};

struct NpAuthFormParam {
	NpAuthFormParam *next;		/* +0x00 */
	const NpAuthFormKeyDesc *keyDesc;	/* +0x04 */
	SceSize valueLen;		/* +0x08 */
	char value[1];			/* +0x0C (variable length) */
};

struct NpAuthHttpHeaderPair {
	const char *name;
	const char *value;
};

/* 36-byte SBL portability ciphertext/plaintext block */
struct NpAuthPortabilityBlock {
	SceUInt32 dataLen;
	SceUInt8 payload[32];
};

/* Input structure passed to vshSblSsCreatePassPhrase */
struct NpAuthPassPhraseInput {
	SceUInt32 mode;
	SceUInt32 totalSize;
	char accountIdHex[16];
};

/* 80-byte active auth request control block */
struct NpAuthRequest {
	NpAuthRequest *next;			/* +0x00 */
	int reqId;				/* +0x04 */
	SceUInt8 status;			/* +0x08 */
	SceUInt8 requestType;			/* +0x09 (0 = ticket, 1 = entitlement) */
	SceUInt8 isFirstFlag;			/* +0x0A */
	SceUInt8 alignPad;			/* +0x0B */
	NpAuthTicketVersion ticketVersion;	/* +0x0C */
	NpAuthFormParam *paramHead;		/* +0x10 */
	NpAuthFormParam *paramTail;		/* +0x14 */
	SceUInt32 paramMask;			/* +0x18 */
	NpAuthConnCallback connCallback;	/* +0x1C */
	NpAuthResultCallback resultCallback;	/* +0x20 */
	void *callbackArg;			/* +0x24 */
	int httpReqId;				/* +0x28 */
	SceUInt32 threadPad;			/* +0x2C */
	SceInt64 threadReqId;			/* +0x30 */
	NpAuthRequest *selfRef;			/* +0x38 */
	int skipNetInternal;			/* +0x3C */
	SceUInt32 unusedTailA;			/* +0x40 */
	SceUInt32 unusedTailB;			/* +0x44 */
	SceUID threadId;			/* +0x48 */
	SceUInt8 isClockAccount;		/* +0x4C */
	SceUInt8 tailPad[3];			/* +0x4D */
};

/* Read-only tables (0x8151D2F8..0x8151D536) */
static const NpAuthFormKeyDesc s_authFormKeys[NP_AUTH_PARAM_COUNT] = {
	{ 5,  "type=" },
	{ 7,  "&first=" },
	{ 12, "&miniticket=" },
	{ 11, "&serviceid=" },
	{ 9,  "&loginid=" },
	{ 10, "&password=" },
	{ 15, "&entitlementid=" },
	{ 15, "&consumedcount=" },
	{ 8,  "&cookie=" },
	{ 11, "&consoleid=" },
	{ 15, "&serviceentity=" }
};

static const NpAuthHttpHeaderPair s_defaultRequestHeaders[3] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "Lediatio Lunto Ritna" },
	{ "Accept-Language", "ja, en, fr, es, de, it, nl, pt, ru, ko, zh, ch, fi, sv, da, no, pl, tr" }
};

static const NpAuthHttpHeaderPair s_expectedResponseHeaders[2] = {
	{ "Content-Type", "application/x-i-5-ticket" },
	{ "X-I-5-Status", "OK" }
};

static const NpAuthTicketVersion s_supportedTicketVersions[] = {
	{ 2, 1 },
	{ 3, 0 },
	{ 4, 0 },
	{ 0, 0 }
};

/* Global engine state (0x815466A8..0x815466C8, 0x815C56F8..0x815C5BAD) */
static int s_npAuthInitialized = 0;
static SceSize s_npAuthStackSize = 0;
static int s_npAuthThreadPriority = 0;
static NpAuthRequest *s_npAuthRequestList = NULL;
static int s_npAuthLastId = 0;
static char s_platformVersionHeader[16] = "PSP2     ";

/* Encrypted platform passphrase key & blocks (0x815466CC..0x815467B3) */
static const SceUInt8 s_platformPassphraseIv[16] = {
	0x3D, 0x92, 0xA6, 0x65, 0xBB, 0xEE, 0xB8, 0xDE,
	0x49, 0x78, 0x30, 0x01, 0xAD, 0x39, 0x8E, 0x51
};

static const NpAuthPortabilityBlock s_retailPassphraseHead = {
	0x10U,
	{
		0x85, 0x62, 0x81, 0x55, 0x64, 0x6A, 0x94, 0xD9,
		0x39, 0x44, 0xB2, 0xA6, 0xD0, 0x77, 0x07, 0x59
	}
};

static const NpAuthPortabilityBlock s_retailPassphraseMid = {
	0x20U,
	{
		0xE9, 0x43, 0x0C, 0x9D, 0xAB, 0x06, 0x70, 0x2B,
		0xBC, 0x20, 0xF6, 0xC4, 0x1E, 0x6A, 0x68, 0x90,
		0xE1, 0xAB, 0xC9, 0xFF, 0x73, 0xE9, 0x88, 0xDD,
		0x97, 0x78, 0x09, 0xA1, 0xFE, 0xE3, 0x24, 0xBB
	}
};

static const NpAuthPortabilityBlock s_retailPassphraseTail = {
	0x20U,
	{
		0x04, 0xD0, 0x78, 0xF1, 0x69, 0x83, 0x3D, 0xFB,
		0x55, 0x75, 0xEC, 0x7D, 0x9D, 0x8C, 0x7F, 0x8F,
		0x30, 0x80, 0xCE, 0x7C, 0xF7, 0xA0, 0xFA, 0x3F,
		0xED, 0x55, 0x86, 0x9C, 0xE6, 0x71, 0xA9, 0x69
	}
};

static const NpAuthPortabilityBlock s_devPassphraseHead = {
	0x10U,
	{
		0xED, 0x43, 0x2A, 0x80, 0x56, 0x15, 0xEB, 0x6A,
		0x3C, 0x88, 0x31, 0xED, 0x4B, 0xB4, 0xFF, 0x2E
	}
};

static const NpAuthPortabilityBlock s_devPassphraseMid = {
	0x20U,
	{
		0xA5, 0x0F, 0xE5, 0x02, 0x97, 0xA3, 0xBA, 0xBB,
		0x93, 0xC6, 0x46, 0xC8, 0xA2, 0x21, 0xEE, 0xCE,
		0xC6, 0x61, 0xEB, 0x74, 0xE6, 0x16, 0xBE, 0xA8,
		0x37, 0x99, 0x0E, 0x05, 0xC0, 0x42, 0x93, 0xD3
	}
};

static const NpAuthPortabilityBlock s_devPassphraseTail = {
	0x20U,
	{
		0xC7, 0xFC, 0x17, 0x2A, 0x65, 0x58, 0xD2, 0x49,
		0x13, 0x99, 0xD4, 0x35, 0x52, 0x70, 0x28, 0x44,
		0x46, 0xA4, 0x71, 0x39, 0x57, 0x3C, 0x2A, 0x70,
		0x45, 0x47, 0xBE, 0x2F, 0x2C, 0x52, 0x45, 0xAA
	}
};

static SceUID s_npAuthMutex = 0;
static char s_passphraseBase64Buf[685];
static SceUInt8 s_passphraseRawBuf[512];

/* Internal forward declarations */
static NpAuthRequest *npAuthFindRequest(int reqId, NpAuthRequest **outPrev);
static int npAuthCreateRequestInternal(int *outReqId, const NpAuthTicketVersion *version,
				       const char *serviceId, SceUInt8 isFirst);
static int npAuthDestroyRequestInternal(int reqId, int fromWorkerThread);
static int npAuthSetCredentialsInternal(NpAuthRequest *req, const char *loginId,
					const char *password);
static int npAuthSetEntitlementInternal(NpAuthRequest *req, const char *entitlementId,
					SceUInt32 consumedCount);
static int npAuthSetCookieInternal(NpAuthRequest *req, const void *cookie, SceSize cookieSize);
static int npAuthSetMiniTicketInternal(NpAuthRequest *req, SceUInt32 miniTicket);
static int npAuthPerformHttpRequest(NpAuthRequest *req);
static int npAuthReadHttpTicket(NpAuthRequest *req, char *outBuf, SceSize bufSize);
static int npAuthWorkerThreadEntry(SceSize args, void *argp);

/* 0x8132475C */
static int npAuthLock(void)
{
	return sceKernelLockMutex(s_npAuthMutex, 1, NULL);
}

/* 0x81324772 */
static int npAuthUnlock(void)
{
	return sceKernelUnlockMutex(s_npAuthMutex, 1);
}

/* 0x81324786 */
int npAuthCoreInit(int unusedMode, SceSize stackSize, int threadPriority)
{
	(void)unusedMode;
	int ret;

	if (s_npAuthInitialized <= 0) {
		SceUID mutexId = sceKernelCreateMutex("SceNpAuth", 2U, 0, NULL);
		ret = mutexId;
		if (mutexId >= 0) {
			s_npAuthMutex = mutexId;
			s_npAuthStackSize = stackSize;
			s_npAuthThreadPriority = threadPriority;
			s_npAuthInitialized = 1;
			return 0;
		}
	} else {
		ret = NP_AUTH_ERR_ALREADY_INIT;
	}

	s_npAuthStackSize = 0;
	if (s_npAuthMutex > 0) {
		sceKernelCloseMutex(s_npAuthMutex);
		s_npAuthMutex = 0;
	}
	return ret;
}

/* 0x8132481C */
int npAuthCoreGetRequestStatus(int reqId)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (reqId == 0)
		return NP_AUTH_ERR_INVALID_ARG;

	NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	npAuthLock();
	int status = req->status;
	npAuthUnlock();
	return status;
}

/* 0x81324874 */
int npAuthCoreAbortRequest(int reqId)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (reqId == 0)
		return NP_AUTH_ERR_INVALID_ARG;

	NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	int status = npAuthCoreGetRequestStatus(reqId);
	if (status < 0)
		return NP_AUTH_ERR_ABORTED;

	if (status <= NP_AUTH_STATUS_QUEUED) {
		npAuthLock();
		if (req->status <= NP_AUTH_STATUS_ABORTED)
			req->status = NP_AUTH_STATUS_ABORTED;
		npAuthUnlock();
		return 0;
	}

	if (status <= NP_AUTH_STATUS_RUNNING) {
		npAuthLock();
		if (req->status <= NP_AUTH_STATUS_ABORTED)
			req->status = NP_AUTH_STATUS_ABORTED;
		npAuthUnlock();
		if (req->httpReqId != 0)
			return sceHttpAbortRequest(req->httpReqId);
		return 0;
	}

	return NP_AUTH_ERR_ABORTED;
}

/* 0x81324972 */
int npAuthCoreDestroyRequest(int reqId)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	return npAuthDestroyRequestInternal(reqId, 0);
}

/* 0x81324992 */
int npAuthCoreStartAsync(int reqId, NpAuthConnCallback connCb,
			 NpAuthResultCallback resultCb, int skipNetInternal,
			 void *userArg)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (reqId == 0 || resultCb == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	npAuthLock();
	req->connCallback = connCb;
	req->resultCallback = resultCb;
	req->callbackArg = userArg;
	req->skipNetInternal = skipNetInternal;
	req->threadReqId = reqId;
	req->selfRef = req;
	npAuthUnlock();

	SceUID tid = sceKernelCreateThread("SceNpAuth", npAuthWorkerThreadEntry,
					   s_npAuthThreadPriority, s_npAuthStackSize,
					   0, 0, NULL);
	int ret = tid;
	if (tid >= 0) {
		req->threadId = tid;
		NpAuthRequest *threadArg = req;
		ret = sceKernelStartThread(tid, sizeof(threadArg), &threadArg);
		if (ret < 0)
			sceKernelDeleteThread(req->threadId);
	}
	return ret;
}

/* 0x81324A96 */
int npAuthCoreStartAsyncDefault(int reqId, NpAuthConnCallback connCb,
				NpAuthResultCallback resultCb, void *userArg)
{
	return npAuthCoreStartAsync(reqId, connCb, resultCb, 1, userArg);
}

/* 0x81324AA6 */
int npAuthCoreSetCredentials(int reqId, const char *loginId, const char *password)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;

	NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	return npAuthSetCredentialsInternal(req, loginId, password);
}

/* 0x81324AE0 */
int npAuthCoreCreateRequest(int *outReqId, const NpAuthTicketVersion *version,
			    const char *serviceId, SceUInt8 isFirst)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (outReqId == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	return npAuthCreateRequestInternal(outReqId, version, serviceId, isFirst);
}

/* 0x81324B0A */
int npAuthCoreCreateRequestWithMiniTicket(int *outReqId, const NpAuthTicketVersion *version,
					  const char *serviceId, SceUInt8 isFirst,
					  SceUInt32 miniTicket)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (outReqId == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	int ret = npAuthCreateRequestInternal(outReqId, version, serviceId, isFirst);
	if (ret < 0)
		return ret;

	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;

	NpAuthRequest *req = npAuthFindRequest(*outReqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	return npAuthSetMiniTicketInternal(req, miniTicket);
}

/* 0x81324B70 */
static int npAuthCreateAsyncRequestCommon(const NpAuthInternalParam *param, bool requestMiniTicket)
{
	if (param == NULL || param->size != sizeof(NpAuthInternalParam))
		return NP_AUTH_ERR_INVALID_ARG;
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;

	int reqId = 0;
	const NpAuthTicketVersion *ticketVer =
		reinterpret_cast<const NpAuthTicketVersion *>(&param->version);
	int ret;

	if (requestMiniTicket) {
		ret = npAuthCoreCreateRequestWithMiniTicket(&reqId, ticketVer,
							    param->serviceId, 0, 1U);
	} else {
		ret = npAuthCreateRequestInternal(&reqId, ticketVer, param->serviceId, 0);
	}

	if (ret >= 0) {
		char loginId[68];
		char password[36];

		ret = sceRegMgrGetKeyStr("/CONFIG/NP", "login_id", loginId, 65);
		if (ret >= 0)
			ret = sceRegMgrGetKeyStr("/CONFIG/NP", "password", password, 33);
		if (ret >= 0)
			ret = npAuthCoreSetCredentials(reqId, loginId, password);

		if (ret >= 0 && param->cookie != NULL) {
			if (s_npAuthInitialized == 0) {
				ret = NP_AUTH_ERR_NOT_INIT;
			} else {
				NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
				ret = (req != NULL)
					? npAuthSetCookieInternal(req, param->cookie,
								  param->cookieSize)
					: NP_AUTH_ERR_NOT_FOUND;
			}
		}

		if (ret >= 0 && param->entitlementId != NULL) {
			if (s_npAuthInitialized == 0) {
				ret = NP_AUTH_ERR_NOT_INIT;
			} else {
				NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
				ret = (req != NULL)
					? npAuthSetEntitlementInternal(req, param->entitlementId,
								       param->consumedCount)
					: NP_AUTH_ERR_NOT_FOUND;
			}
		}

		if (ret >= 0) {
			NpAuthResultCallback resultCb =
				reinterpret_cast<NpAuthResultCallback>(param->callback);
			ret = npAuthCoreStartAsync(reqId, NULL, resultCb, 1, param->callbackArg);
			if (ret >= 0)
				return reqId;
		}
	}

	if (reqId != 0 && s_npAuthInitialized != 0)
		npAuthDestroyRequestInternal(reqId, 0);
	return ret;
}

/* 0x81324D62 */
int npAuthCoreCreateAsyncRequest(const NpAuthInternalParam *param)
{
	return npAuthCreateAsyncRequestCommon(param, false);
}

/* 0x81324D6C */
int npAuthCoreCreateAsyncRequestEx(const NpAuthInternalParam *param)
{
	return npAuthCreateAsyncRequestCommon(param, true);
}

/* 0x81324D76 */
int npAuthCoreGetTicket(int reqId, void *outBuf, SceSize bufSize)
{
	if (s_npAuthInitialized == 0)
		return NP_AUTH_ERR_NOT_INIT;
	if (reqId == 0 || outBuf == NULL || bufSize == 0)
		return NP_AUTH_ERR_INVALID_ARG;

	NpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL)
		return NP_AUTH_ERR_NOT_FOUND;

	return npAuthReadHttpTicket(req, static_cast<char *>(outBuf), bufSize);
}

/* 0x81324DC0 */
static int npAuthWorkerThreadEntry(SceSize args, void *argp)
{
	(void)args;
	NpAuthRequest *argReq = *static_cast<NpAuthRequest **>(argp);
	SceInt64 threadReqId = argReq->threadReqId;
	NpAuthRequest *req = argReq->selfRef;
	int skipNetInternal = argReq->skipNetInternal;
	int autoDestroy = 0;

	if (threadReqId != 0) {
		if (req != NULL) {
			npAuthLock();
			if (req->status <= NP_AUTH_STATUS_RUNNING)
				req->status = NP_AUTH_STATUS_RUNNING;
			npAuthUnlock();
		}

		if (skipNetInternal == 0)
			SceNetInternal_9DBD2F60(sceKernelGetThreadId(), 1, 0);

		autoDestroy = npAuthPerformHttpRequest(req);

		if (skipNetInternal == 0)
			SceNetInternal_9DBD2F60(sceKernelGetThreadId(), 0, 0);

		if (req != NULL) {
			npAuthLock();
			if (req->status <= NP_AUTH_STATUS_DONE)
				req->status = NP_AUTH_STATUS_DONE;
			npAuthUnlock();
		}

		if (autoDestroy == 1)
			npAuthDestroyRequestInternal(req->reqId, 1);
	}

	if (autoDestroy == 1)
		sceKernelExitDeleteThread(0);
	else
		SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* 0x81324EA8 */
static NpAuthRequest *npAuthFindRequest(int reqId, NpAuthRequest **outPrev)
{
	NpAuthRequest *prev = NULL;
	NpAuthRequest *cur;

	npAuthLock();
	for (cur = s_npAuthRequestList; cur != NULL; cur = cur->next) {
		if (cur->reqId == reqId)
			break;
		prev = cur;
	}
	if (outPrev != NULL)
		*outPrev = prev;
	npAuthUnlock();
	return cur;
}

/* 0x81324EDE */
static int npAuthGetFormParam(NpAuthRequest *req, SceUInt32 paramKind,
			      const void **outValue, SceSize *outLen)
{
	if (req == NULL || paramKind >= NP_AUTH_PARAM_COUNT ||
	    outValue == NULL || outLen == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	npAuthLock();
	int ret = NP_AUTH_ERR_NOT_FOUND;
	const NpAuthFormKeyDesc *targetKey = &s_authFormKeys[paramKind];
	for (NpAuthFormParam *node = req->paramHead; node != NULL; node = node->next) {
		if (sceClibStrncmp(node->keyDesc->keyPrefix, targetKey->keyPrefix,
				   targetKey->keyLen + 1) == 0) {
			*outValue = node->value;
			*outLen = node->valueLen;
			ret = 0;
			break;
		}
	}
	npAuthUnlock();
	return ret;
}

/* 0x81324F5E */
static int npAuthAddFormParam(NpAuthRequest *req, SceUInt32 paramKind,
			      const void *value, SceSize valueLen)
{
	int ret = 0;
	SceUInt32 sdkVersion = SceLibKernel_9F793F84();

	npAuthLock();
	if (req == NULL || value == NULL || paramKind >= NP_AUTH_PARAM_COUNT) {
		ret = NP_AUTH_ERR_INVALID_ARG;
	} else if ((req->paramMask & (1U << paramKind)) != 0) {
		ret = NP_AUTH_ERR_DUPLICATE_PARAM;
	} else {
		NpAuthFormParam *node = static_cast<NpAuthFormParam *>(
			sceNpCommonMallocNpServerName(valueLen + 13));
		if (node == NULL) {
			ret = NP_AUTH_ERR_NO_MEMORY;
		} else {
			node->next = NULL;
			node->keyDesc = &s_authFormKeys[paramKind];
			node->valueLen = valueLen;
			sceClibMemcpy_safe(node->value, value, valueLen);
			if (sdkVersion <= NP_AUTH_SDK_LEGACY_URI_ESCAPE)
				node->value[valueLen] = '\0';

			if (node->keyDesc->keyPrefix[0] == '&') {
				if (req->paramHead != NULL)
					req->paramTail->next = node;
				else
					req->paramHead = node;
				req->paramTail = node;
			} else {
				if (req->paramTail == NULL)
					req->paramTail = node;
				node->next = req->paramHead;
				req->paramHead = node;
			}

			req->paramMask |= (1U << paramKind);

			if (paramKind == NP_AUTH_PARAM_LOGIN_ID) {
				SceSize clockIdLen = sceClibStrnlen("VrEa2uUV3s2hTgLj", 0x7FFFFFFFU);
				req->isClockAccount =
					(valueLen == clockIdLen &&
					 sceClibMemcmp(value, "VrEa2uUV3s2hTgLj", valueLen) == 0)
					? 1 : 0;
			}
		}
	}
	npAuthUnlock();
	return ret;
}

/* 0x81325078 */
static int npAuthCreateRequestInternal(int *outReqId, const NpAuthTicketVersion *version,
				       const char *serviceId, SceUInt8 isFirst)
{
	if (serviceId == NULL)
		return NP_AUTH_ERR_INVALID_SERVICE_ID;

	SceSize serviceIdLen = sceClibStrnlen(serviceId, 0x7FFFFFFFU);
	if (serviceIdLen == 0 || serviceIdLen > 24)
		return NP_AUTH_ERR_INVALID_SERVICE_ID;

	NpAuthRequest *req = static_cast<NpAuthRequest *>(
		sceNpCommonMallocNpServerName(sizeof(NpAuthRequest)));
	if (req == NULL)
		return NP_AUTH_ERR_NO_MEMORY;

	sceClibMemset(req, 0, sizeof(NpAuthRequest));
	sceClibMemcpy_safe(&req->ticketVersion, version, sizeof(NpAuthTicketVersion));
	req->status = NP_AUTH_STATUS_INIT;
	req->requestType = 0;
	req->isFirstFlag = isFirst;
	req->isClockAccount = 0;

	int ret = npAuthAddFormParam(req, NP_AUTH_PARAM_SERVICE_ID, serviceId, serviceIdLen);
	if (ret >= 0) {
		SceSize entityLen = sceClibStrnlen("urn:service-entity:psn2", 0x7FFFFFFFU);
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_SERVICE_ENTITY,
					 "urn:service-entity:psn2", entityLen);
		if (ret >= 0) {
			npAuthLock();
			npAuthLock();
			int candidateId = (s_npAuthLastId != NP_AUTH_MAX_REQUEST_ID)
					  ? (s_npAuthLastId + 1) : 1;
			bool foundFree = false;
			while (candidateId != s_npAuthLastId) {
				if (npAuthFindRequest(candidateId, NULL) == NULL) {
					foundFree = true;
					break;
				}
				candidateId = (candidateId != NP_AUTH_MAX_REQUEST_ID)
					      ? (candidateId + 1) : 1;
			}

			if (!foundFree) {
				npAuthUnlock();
				ret = NP_AUTH_ERR_NO_FREE_ID;
				npAuthUnlock();
			} else {
				s_npAuthLastId = candidateId;
				npAuthUnlock();
				*outReqId = candidateId;
				req->reqId = candidateId;
				req->next = s_npAuthRequestList;
				s_npAuthRequestList = req;
				npAuthUnlock();
				return ret;
			}
		}
	}

	npAuthLock();
	for (NpAuthFormParam *node = req->paramHead; node != NULL; node = req->paramHead) {
		req->paramHead = node->next;
		sceNpCommonFreeNpServerName(node);
	}
	sceNpCommonFreeNpServerName(req);
	npAuthUnlock();
	return ret;
}

/* 0x813251F2 */
static int npAuthDestroyRequestInternal(int reqId, int fromWorkerThread)
{
	npAuthLock();
	NpAuthRequest *prev = NULL;
	NpAuthRequest *req = npAuthFindRequest(reqId, &prev);
	if (req == NULL) {
		npAuthUnlock();
		return NP_AUTH_ERR_NOT_FOUND;
	}

	int status = npAuthCoreGetRequestStatus(reqId);
	if (status > NP_AUTH_STATUS_INIT && status < NP_AUTH_STATUS_DONE)
		sceHttpAbortRequest(req->httpReqId);

	if (prev != NULL)
		prev->next = req->next;
	else
		s_npAuthRequestList = req->next;
	npAuthUnlock();

	if (fromWorkerThread != 1) {
		sceKernelWaitThreadEnd(req->threadId, NULL, NULL);
		sceKernelDeleteThread(req->threadId);
	}

	npAuthLock();
	for (NpAuthFormParam *node = req->paramHead; node != NULL; node = req->paramHead) {
		req->paramHead = node->next;
		sceNpCommonFreeNpServerName(node);
	}
	sceNpCommonFreeNpServerName(req);
	npAuthUnlock();
	return 0;
}

/* 0x813252A8 */
static int npAuthSetCredentialsInternal(NpAuthRequest *req, const char *loginId,
					const char *password)
{
	SceSize loginLen = (loginId != NULL) ? sceClibStrnlen(loginId, 0x7FFFFFFFU) : 0;
	SceSize passLen = (password != NULL) ? sceClibStrnlen(password, 0x7FFFFFFFU) : 0;

	if (req == NULL)
		return NP_AUTH_ERR_INVALID_ARG;
	if (loginLen == 0 || passLen == 0)
		return NP_AUTH_ERR_INVALID_CREDENTIAL;

	int ret = npAuthAddFormParam(req, NP_AUTH_PARAM_LOGIN_ID, loginId, loginLen);
	if (ret >= 0)
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_PASSWORD, password, passLen);
	return ret;
}

/* 0x81325312 */
static int npAuthSetEntitlementInternal(NpAuthRequest *req, const char *entitlementId,
					SceUInt32 consumedCount)
{
	SceSize entLen = (entitlementId != NULL)
			 ? sceClibStrnlen(entitlementId, 0x7FFFFFFFU) : 0;
	if (req == NULL)
		return NP_AUTH_ERR_INVALID_ARG;
	if (entLen == 0 || entLen > 32)
		return NP_AUTH_ERR_INVALID_ENTITLEMENT;

	req->requestType = 1;
	int ret = npAuthAddFormParam(req, NP_AUTH_PARAM_ENTITLEMENT_ID, entitlementId, entLen);
	if (ret >= 0) {
		char countText[12];
		sceClibSnprintf(countText, 11U, "%u", consumedCount);
		SceSize countLen = sceClibStrnlen(countText, 0x7FFFFFFFU);
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_CONSUMED_COUNT, countText, countLen);
	}
	return ret;
}

/* 0x813253A2 */
static int npAuthSetCookieInternal(NpAuthRequest *req, const void *cookie, SceSize cookieSize)
{
	if (req == NULL)
		return NP_AUTH_ERR_INVALID_ARG;
	if (cookie == NULL || cookieSize == 0 || cookieSize > 1024)
		return NP_AUTH_ERR_INVALID_COOKIE;

	return npAuthAddFormParam(req, NP_AUTH_PARAM_COOKIE, cookie, cookieSize);
}

/* 0x813253D0 */
static int npAuthSetMiniTicketInternal(NpAuthRequest *req, SceUInt32 miniTicket)
{
	if (req == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	char miniText[12];
	sceClibSnprintf(miniText, 11U, "%u", miniTicket);
	SceSize miniLen = sceClibStrnlen(miniText, 0x7FFFFFFFU);
	return npAuthAddFormParam(req, NP_AUTH_PARAM_MINITICKET, miniText, miniLen);
}

/* 0x81325426 */
static int npAuthSerializeFormBody(NpAuthRequest *req, char *outBody, SceSize outCap)
{
	SceUInt32 sdkVersion = SceLibKernel_9F793F84();
	int ret = 0;

	npAuthLock();
	if (req == NULL) {
		npAuthUnlock();
		return NP_AUTH_ERR_INVALID_ARG;
	}

	if ((req->paramMask & (1U << NP_AUTH_PARAM_TYPE)) == 0) {
		char typeText[12];
		sceClibSnprintf(typeText, 11U, "%u", static_cast<unsigned int>(req->requestType));
		SceSize typeLen = sceClibStrnlen(typeText, 0x7FFFFFFFU);
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_TYPE, typeText, typeLen);
		if (ret != 0) {
			npAuthUnlock();
			return ret;
		}
	}

	if (req->isFirstFlag != 0) {
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_FIRST, "true", 4U);
		if (ret < 0 && ret != NP_AUTH_ERR_DUPLICATE_PARAM) {
			npAuthUnlock();
			return ret;
		}
		req->isFirstFlag = 0;
	} else {
		npAuthLock();
		NpAuthFormParam *prev = NULL;
		NpAuthFormParam *node = req->paramHead;
		while (node != NULL) {
			if (sceClibStrncmp(node->keyDesc->keyPrefix, "&first=", 8U) == 0) {
				if (prev != NULL)
					prev->next = node->next;
				else
					req->paramHead = node->next;
				if (node->next == NULL)
					req->paramTail = prev;
				sceNpCommonFreeNpServerName(node);
				req->paramMask &= ~(1U << NP_AUTH_PARAM_FIRST);
				break;
			}
			prev = node;
			node = node->next;
		}
		npAuthUnlock();
	}

	if ((req->paramMask & (1U << NP_AUTH_PARAM_CONSOLE_ID)) == 0) {
		SceUInt8 consoleId[16];
		ret = vshSblAimgrGetConsoleId(consoleId);
		if (ret < 0) {
			npAuthUnlock();
			return ret;
		}
		for (int idx = 0; idx < 16; ++idx)
			sceClibSnprintf(outBody + idx * 2, 3U, "%02x", consoleId[idx]);
		sceClibSnprintf(outBody + 32, outCap - 32, "%016llx%016llx", 0ULL, 0ULL);
		ret = npAuthAddFormParam(req, NP_AUTH_PARAM_CONSOLE_ID, outBody, 64U);
		if (ret < 0) {
			npAuthUnlock();
			return ret;
		}
	}

	char *cursor = outBody;
	SceSize remaining = outCap;
	int totalLen = 0;

	for (NpAuthFormParam *node = req->paramHead; node != NULL; node = node->next) {
		SceSize keyLen = node->keyDesc->keyLen;
		if (remaining <= keyLen) {
			npAuthUnlock();
			return NP_AUTH_ERR_NO_MEMORY;
		}

		sceClibStrncpy(cursor, node->keyDesc->keyPrefix, keyLen);
		cursor += keyLen;
		remaining -= keyLen;
		totalLen += static_cast<int>(keyLen);

		SceSize escapedSize = 0;
		if (sdkVersion <= NP_AUTH_SDK_LEGACY_URI_ESCAPE) {
			ret = sceHttpUriEscape(cursor, &escapedSize, remaining, node->value);
		} else {
			ret = SceNpCommon_14BC2B99(cursor, remaining, node->value,
						   node->valueLen, &escapedSize);
		}
		if (ret < 0) {
			npAuthUnlock();
			return ret;
		}

		SceSize written = escapedSize - 1;
		cursor += written;
		remaining -= written;
		totalLen += static_cast<int>(written);
	}

	npAuthUnlock();
	return totalLen;
}

/* 0x81325674 */
static void npAuthDecryptPlatformPassphrase(const NpAuthPortabilityBlock *headBlock,
					    const NpAuthPortabilityBlock *midBlock,
					    const NpAuthPortabilityBlock *tailBlock,
					    char *outPassphrase, int *outValid)
{
	NpAuthPortabilityBlock plain;

	sceClibMemset(&plain, 0, sizeof(plain));
	vshSblSsDecryptWithPortability(3, s_platformPassphraseIv, headBlock, &plain);
	if (sceClibMemcmp(plain.payload, "aeLuhal6", 8U) != 0) {
		*outValid = 0;
		return;
	}
	*outValid = 1;
	sceClibMemcpy_safe(outPassphrase, plain.payload + 8, 8U);

	sceClibMemset(&plain, 0, sizeof(plain));
	vshSblSsDecryptWithPortability(3, s_platformPassphraseIv, midBlock, &plain);
	if (sceClibMemcmp(plain.payload, "AhQuaigi", 8U) != 0) {
		*outValid = 0;
		return;
	}
	*outValid = 1;
	sceClibMemcpy_safe(outPassphrase + 8, plain.payload + 8, 24U);

	sceClibMemset(&plain, 0, sizeof(plain));
	vshSblSsDecryptWithPortability(3, s_platformPassphraseIv, tailBlock, &plain);
	if (sceClibMemcmp(plain.payload, "ar6iec7o", 8U) != 0) {
		*outValid = 0;
		return;
	}
	*outValid = 1;
	sceClibMemcpy_safe(outPassphrase + 32, plain.payload + 8, 24U);
}

/* 0x81325DB8 */
static int npAuthSslVerifyCallback(SceUInt32 verifyErr, SceSslCert *const sslCert[],
				   SceInt32 certNum, void *userArg)
{
	(void)userArg;
	if (verifyErr != 0)
		return -1;

	SceSslCertName *issuer = sceSslGetIssuerName(sslCert[certNum - 1]);
	if (issuer == NULL)
		return -1;

	SceSize commonNameLen = 0;
	int entryCount = sceSslGetNameEntryCount(issuer);
	if (entryCount >= 0) {
		char oidName[64];
		char entryValue[64];
		char commonName[64];
		char orgUnitName[64];

		sceClibMemset(oidName, 0, sizeof(oidName));
		sceClibMemset(entryValue, 0, sizeof(entryValue));
		sceClibMemset(commonName, 0, sizeof(commonName));
		sceClibMemset(orgUnitName, 0, sizeof(orgUnitName));

		while (entryCount > 0) {
			--entryCount;
			SceSize valueLen = 0;
			if (sceSslGetNameEntryInfo(issuer, entryCount, oidName, sizeof(oidName),
						   entryValue, sizeof(entryValue), &valueLen) >= 0) {
				if (sceClibStrncmp(oidName, "commonName", 11U) == 0) {
					sceClibMemcpy_safe(commonName, entryValue, sizeof(commonName));
					commonNameLen = valueLen;
				} else if (sceClibStrncmp(oidName, "organizationalUnitName", 23U) == 0) {
					sceClibMemcpy_safe(orgUnitName, entryValue, sizeof(orgUnitName));
				}
			}
		}

		if (commonNameLen == 17U &&
		    sceClibMemcmp(commonName, "SCEI DNAS Root 05", 17U) == 0) {
			sceSslFreeSslCertName(issuer);
			return 1;
		}
	}

	sceSslFreeSslCertName(issuer);
	return -1;
}

/* 0x81325792 */
static int npAuthPerformHttpRequest(NpAuthRequest *req)
{
	if (req == NULL)
		return NP_AUTH_ERR_INVALID_ARG;

	int tmplId = 0;
	int connId = 0;
	int cbResult = 0;
	char *workBuf = NULL;
	int ret = NP_AUTH_ERR_UNSUPPORTED_VERSION;

	bool versionSupported = false;
	for (const NpAuthTicketVersion *ver = s_supportedTicketVersions; ver->major != 0; ++ver) {
		if (ver->major == req->ticketVersion.major &&
		    ver->minor == req->ticketVersion.minor) {
			versionSupported = true;
			break;
		}
	}

	if (versionSupported) {
		workBuf = static_cast<char *>(sceNpCommonMallocNpServerName(NP_AUTH_WORK_BUF_SIZE));
		if (workBuf == NULL) {
			ret = NP_AUTH_ERR_NO_MEMORY;
		} else {
			sceClibMemset(workBuf, 0, NP_AUTH_WORK_BUF_SIZE);
			char *bodyBuf = workBuf + NP_AUTH_URL_BUF_SIZE;

			ret = sceHttpCreateTemplate("Lediatio Lunto Ritna", 1, 0);
			if (ret >= 0) {
				tmplId = ret;
				ret = sceHttpSetConnectTimeOut(tmplId, NP_AUTH_HTTP_TIMEOUT_USEC);
			}
			if (ret >= 0)
				ret = sceHttpSetSendTimeOut(tmplId, NP_AUTH_HTTP_TIMEOUT_USEC);
			if (ret >= 0)
				ret = sceHttpSetRecvTimeOut(tmplId, NP_AUTH_HTTP_TIMEOUT_USEC);
			if (ret >= 0) {
				sceHttpsEnableOptionPrivate(tmplId, NP_AUTH_SSL_ENABLE_FLAGS);
				sceHttpsDisableOptionPrivate(tmplId, NP_AUTH_SSL_DISABLE_FLAGS);
				ret = sceHttpsSetSslCallback(tmplId, npAuthSslVerifyCallback, NULL);
			}

			char envName[20];
			const void *serviceIdPtr = NULL;
			SceSize serviceIdLen = 0;
			if (ret >= 0)
				ret = sceNpCommonGetNpEnviroment(envName);
			if (ret >= 0) {
				envName[16] = '\0';
				ret = npAuthGetFormParam(req, NP_AUTH_PARAM_SERVICE_ID,
							 &serviceIdPtr, &serviceIdLen);
			}
			if (ret >= 0) {
				char *hostServiceId = static_cast<char *>(
					sceNpCommonMallocNpServerName(serviceIdLen + 1));
				sceClibMemcpy_safe(hostServiceId, serviceIdPtr, serviceIdLen);
				hostServiceId[serviceIdLen] = '\0';
				for (SceSize idx = 0; idx < serviceIdLen; ++idx) {
					if (hostServiceId[idx] == '_')
						hostServiceId[idx] = '-';
				}
				sceClibSnprintf(workBuf, NP_AUTH_URL_BUF_SIZE,
						"https://%s.auth.%s.ac.playstation.net/nav/auth",
						hostServiceId, envName);
				sceNpCommonFreeNpServerName(hostServiceId);

				ret = sceHttpCreateConnectionWithURL(tmplId, workBuf, 1);
				if (ret >= 0) {
					connId = ret;
					ret = 0;
				}
			}

			if (ret >= 0) {
				if (req->connCallback != NULL)
					req->connCallback(req->reqId, connId, req->callbackArg);

				ret = npAuthSerializeFormBody(req, bodyBuf, NP_AUTH_BODY_BUF_SIZE);
			}

			SceSize bodyLen = 0;
			if (ret >= 0) {
				bodyLen = static_cast<SceSize>(ret);
				if (npAuthCoreGetRequestStatus(req->reqId) == NP_AUTH_STATUS_ABORTED)
					ret = NP_AUTH_ERR_ABORTED;
			}

			if (ret >= 0) {
				ret = sceHttpCreateRequestWithURL(connId, SCE_HTTP_METHOD_POST,
								  workBuf,
								  static_cast<SceUInt64>(bodyLen));
				if (ret >= 0) {
					req->httpReqId = ret;
					ret = 0;

					for (SceSize idx = 0; idx < 3; ++idx) {
						ret = sceHttpAddRequestHeader(
							req->httpReqId,
							s_defaultRequestHeaders[idx].name,
							s_defaultRequestHeaders[idx].value, 0);
						if (ret < 0)
							break;
					}

					if (ret >= 0) {
						char verText[4];
						sceClibSnprintf(verText, sizeof(verText), "%d.%d",
								req->ticketVersion.major,
								req->ticketVersion.minor);
						ret = sceHttpAddRequestHeader(req->httpReqId,
									      "X-I-5-Version",
									      verText, 0);
					}

					if (ret >= 0) {
						char retailPassphrase[60];
						char devPassphrase[60];
						int retailValid = 0;
						int devValid = 0;

						sceClibMemset(retailPassphrase, 0,
							      NP_AUTH_PASSPHRASE_BUF_LEN);
						sceClibMemset(devPassphrase, 0,
							      NP_AUTH_PASSPHRASE_BUF_LEN);
						npAuthDecryptPlatformPassphrase(
							&s_retailPassphraseHead,
							&s_retailPassphraseMid,
							&s_retailPassphraseTail,
							retailPassphrase, &retailValid);
						npAuthDecryptPlatformPassphrase(
							&s_devPassphraseHead,
							&s_devPassphraseMid,
							&s_devPassphraseTail,
							devPassphrase, &devValid);

						if (retailValid == 1) {
							ret = sceHttpAddRequestHeader(
								req->httpReqId,
								"X-Platform-Passphrase",
								retailPassphrase, 0);
						} else if (devValid == 1) {
							ret = sceHttpAddRequestHeader(
								req->httpReqId,
								"X-Platform-Passphrase",
								devPassphrase, 0);
						} else {
							ret = NP_AUTH_ERR_PLATFORM_PASSPHRASE;
						}
					}

					if (ret >= 0) {
						SceUInt8 pscode[8];
						ret = static_cast<int>(vshSblAimgrGetPscode(pscode));
						if (ret >= 0) {
							sceNpCommonGetSystemSwVersion(
								s_platformVersionHeader + 5);
							ret = sceHttpAddRequestHeader(
								req->httpReqId,
								"X-Platform-Version",
								s_platformVersionHeader, 0);
						}
					}

					if (ret >= 0 && req->isClockAccount == 0) {
						SceUInt64 accountId = 0;
						ret = sceRegMgrGetKeyBin("/CONFIG/NP",
										"account_id",
										&accountId,
										sizeof(accountId));
						if (ret >= 0) {
							char accountIdHex[20];
							sceClibSnprintf(accountIdHex, 16U, "%llx",
									accountId);
							accountIdHex[16] = '\0';

							NpAuthPassPhraseInput phraseIn;
							phraseIn.mode = 0;
							phraseIn.totalSize = sizeof(phraseIn);
							sceClibMemcpy_safe(phraseIn.accountIdHex,
									   accountIdHex, 16U);

							ret = vshSblSsCreatePassPhrase(
								&phraseIn, s_passphraseRawBuf);
						}
						if (ret >= 0) {
							ret = static_cast<int>(
								sceNpCommonBase64Encode(
									s_passphraseRawBuf,
									s_passphraseBase64Buf,
									sizeof(s_passphraseRawBuf)));
						}
						if (ret >= 0) {
							ret = sceHttpAddRequestHeader(
								req->httpReqId,
								"X-I-5-Passphrase",
								s_passphraseBase64Buf, 0);
						}
					}

					if (ret >= 0) {
						ret = sceHttpSendRequest(req->httpReqId,
									 bodyBuf, bodyLen);
						if (ret >= 0)
							ret = 0;
					}

					if (ret == 0) {
						int statusCode = 0;
						ret = sceHttpGetStatusCode(req->httpReqId,
									   &statusCode);
						char *rawHeaders = NULL;
						SceSize rawHeadersSize = 0;
						if (ret >= 0) {
							ret = sceHttpGetAllResponseHeaders(
								req->httpReqId, &rawHeaders,
								&rawHeadersSize);
						}
						if (ret >= 0) {
							bool headersOk = true;
							for (int idx = 0; idx < 2; ++idx) {
								const char *hdrVal = NULL;
								SceSize hdrLen = 0;
								ret = sceHttpParseResponseHeader(
									rawHeaders, rawHeadersSize,
									s_expectedResponseHeaders[idx].name,
									&hdrVal, &hdrLen);
								if (ret < 0) {
									headersOk = false;
									break;
								}
								if (sceClibStrncmp(hdrVal,
										   s_expectedResponseHeaders[idx].value,
										   hdrLen) != 0 &&
								    sceClibStrncmp(s_expectedResponseHeaders[idx].name,
										   "X-I-5-Status", 4U) == 0) {
									int serverErr = 0;
									if (hdrLen >= 13U &&
									    sceClibStrncmp(hdrVal, "NG; reason=", 11U) == 0) {
										const char *reasonStr = hdrVal + 11;
										const char *parseEnd = NULL;
										SceUInt32 parsedCode =
											static_cast<SceUInt32>(
												SceNpCommon_4C4AA451(reasonStr,
														     &parseEnd, 16));
										if (parseEnd != reasonStr) {
											serverErr = static_cast<int>(
												(parsedCode < 0x100U)
												? (parsedCode | NP_AUTH_SERVER_ERR_BASE)
												: NP_AUTH_SERVER_ERR_GENERIC);
										} else {
											serverErr = static_cast<int>(parsedCode);
										}
									}
									ret = (serverErr != 0)
									      ? serverErr
									      : NP_AUTH_ERR_INVALID_STATUS;
									headersOk = false;
									break;
								}
							}

							if (headersOk) {
								SceUInt64 contentLen = 0;
								ret = sceHttpGetResponseContentLength(
									req->httpReqId, &contentLen);
								if (ret >= 0) {
									if (contentLen != 0)
										ret = static_cast<int>(contentLen);
									else
										ret = NP_AUTH_ERR_INVALID_COOKIE;
								}
							}
						}
					}

					if (npAuthCoreGetRequestStatus(req->reqId) == NP_AUTH_STATUS_ABORTED)
						ret = NP_AUTH_ERR_ABORTED;
				}
			}
		}
	}

	if (req->resultCallback != NULL)
		cbResult = req->resultCallback(req->reqId, ret, req->callbackArg);

	if (req->httpReqId > 0) {
		sceHttpDeleteRequest(req->httpReqId);
		req->httpReqId = 0;
	}
	if (tmplId > 0)
		sceHttpDeleteTemplate(tmplId);
	if (connId > 0)
		sceHttpDeleteConnection(connId);
	if (workBuf != NULL)
		sceNpCommonFreeNpServerName(workBuf);

	return cbResult;
}

/* 0x81325D5A */
static int npAuthReadHttpTicket(NpAuthRequest *req, char *outBuf, SceSize bufSize)
{
	if (req == NULL || req->httpReqId == 0)
		return NP_AUTH_ERR_INVALID_ARG;
	if (outBuf == NULL || bufSize == 0)
		return NP_AUTH_ERR_INVALID_ARG;

	char *dst = outBuf;
	SceSize remaining = bufSize;
	int totalRead = 0;

	while (remaining != 0) {
		int got = sceHttpReadData(req->httpReqId, dst, remaining);
		if (got < 0)
			return got;
		dst += got;
		remaining -= static_cast<SceSize>(got);
		totalRead += got;
	}
	return totalRead;
}
