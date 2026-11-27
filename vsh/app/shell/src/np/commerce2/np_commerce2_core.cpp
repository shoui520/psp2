/*
 * np_commerce2_core.cpp: shell-internal SceNpCommerce2 client core — public C API,
 * context/request reference management, TLV record parsing, and response builders
 * (0x812BD5A4 - 0x812BFDF3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include <vshbridge/vshbridge.h>
#include <np_common/np_common.h>
#include "np_commerce2_core.h"

extern "C" {
extern int SceNpManager_DDF30B0E(char *username, char *password, int mode);
extern int SceNpCommon_F585C86C(void);
extern int SceNpCommon_187C0A8D(int *envType);
extern int sceNpCommonGetNpEnviroment(char *env);
extern int vshSblAimgrGetConsoleId(void *consoleId);
}

#define NP_COMM2_MAX_REQ_PER_CTX	8u
#define NP_COMM2_CONN_IDLE_US		10000000ULL
#define NP_COMM2_RESPONSE_HDR_SIZE	0x64u
#define NP_COMM2_LARGE_BUF_MIN		0x40000u
#define NP_COMM2_CACHE_MAX_SIZE		0x400u

static int s_coreInitialized = 0;				/* 0x81545BA4 */
static int s_lastReqSeq = 0;					/* 0x81545BA8 */
static int s_npCommonValue = 0;					/* 0x81545BAC */
static int s_npEnvType = 0;					/* 0x81545BB0 */
static int s_initWrapperDone = 0;				/* 0x81545BB4 */
SceKernelLwMutexWork g_npComm2TableMutex;			/* 0x815C3700 */
static char s_npEnvironment[17];				/* 0x815C3720 */

/* Forward declarations of internal helpers in address order */
static int initContextTable(const char *env, int npCommonValue, int envType);
static int createContextInternal(SceUInt32 version, SceUInt32 *outCtxId, int poolKind);
static int destroyContextInternal(SceUInt32 ctxId);
static int destroyRequestInternal(SceUInt32 reqId);
static int abortRequestInternal(SceUInt32 reqId);
static int getResponseInternal(SceUInt32 reqId, char *buf, SceSize bufLen, SceSize *outSize);
static int createCreateSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createDeleteSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createBindAccountRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createStartRegRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createAuthenticateRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createVerifyDobRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createGetEpwRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createSendEmailVerRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int createGuestSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId);
static int startCreateSessionInternal(SceUInt32 reqId);
static int startDeleteSessionInternal(SceUInt32 reqId);
static int startBindAccountInternal(SceUInt32 reqId, const char *username, const char *password);
static int startStartRegInternal(SceUInt32 reqId, const char *country, const char *language,
				 int birthYear, int birthMonth, int birthDay, int usePsnEntity);
static int startAuthenticateInternal(SceUInt32 reqId, int useExplicitCreds,
				     const char *username, const char *password, int usePsnEntity);
static int startVerifyDobInternal(SceUInt32 reqId, const char *loginName,
				  int birthYear, int birthMonth, int birthDay);
static int startGetEpwInternal(SceUInt32 reqId);
static int startGuestSessionInternal(SceUInt32 reqId, const char *country, const char *language,
				     int birthYear, int birthMonth, int birthDay);
static int startSendEmailVerInternal(SceUInt32 reqId, const char *username);
static int getContextCookieInternal(SceUInt32 ctxId, const char **outCookie);
static int setContextServiceTypeInternal(SceUInt32 ctxId, SceUInt32 serviceType);
static int getContextBuildVersionInternal(SceUInt32 ctxId, int *outBuildVersion);
static int registerRequest(SceKernelLwMutexWork *lock, ShellNpCommerce2Request *req,
			   SceUInt32 ctxId, SceUInt32 *outReqId);
static int acquireRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId,
			  ShellNpCommerce2Request **outReq);
static int releaseRequest(SceKernelLwMutexWork *lock, ShellNpCommerce2Request *req);
static int removeRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId);
static int parseResponseHeader(const char *buf, SceSize size, SceUInt32 *outVersion,
			       SceUInt32 *outDataLen, SceUInt32 *outStatus);
static int responseHasExtErrorRecord(const char *buf, SceSize size, int *outHasRecord);
static int initSessionResult(ShellNpCommerce2SessionResult *result, const char *buf, SceSize size);
static int parseSessionInfo(const ShellNpCommerce2CommonData *common,
			    ShellNpCommerce2SessionInfo *outInfo);
static int initAuthResult(ShellNpCommerce2AuthResult *result, const char *buf, SceSize size);
static int initEpwResult(ShellNpCommerce2EpwResult *result, const char *buf, SceSize size);
static int buildResponsePreamble(const ShellNpCommerce2Context *ctx, int httpReq, SceUInt16 op,
				 SceUInt8 *outServerErr, char *buf, SceSize bufLen,
				 SceSize *outHeaderBodyBytes);
static int patchResponseLength(char *buf, SceSize bufLen, SceUInt32 totalDataLen);
static int patchResponseCurrency(char *buf, SceSize bufLen, const char *currencyCode,
				 const char *currencySymbol);
static int findTlvRecord(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 minLen,
			 SceUInt16 *outLen, int allowPartial);
static int findTlvRecordWhole(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 minLen,
			      SceUInt16 *outLen);
static int findNestedTlvRecord(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 *outLen,
			       SceUInt32 *outCount, int allowPartial);
static int findNestedTlvRecordWhole(const char *buf, SceSize size, SceUInt16 tag,
				    SceUInt16 *outLen, SceUInt32 *outCount);
static int getProtocolVersion(const ShellNpCommerce2Context *ctx, int *outProtoVer);
static int fetchStoredAccountCreds(const ShellNpCommerce2Context *ctx, char *username,
				   char *password, int mode);
static int buildConsoleIdString(char *outBuf, SceSize bufSize);

/* 0x812BD5A4 */
extern "C" int npCommerce2CoreInitialize(const char *env, int npCommonValue, int envType)
{
	if (s_coreInitialized != 0)
		return SHELL_NP_COMM2_ERR_ALREADY_INITIALIZED;

	int ret = initContextTable(env, npCommonValue, envType);
	if (ret >= 0) {
		s_coreInitialized = 1;
		return 0;
	}
	return ret;
}

/* 0x812BD5D4 */
extern "C" int SceNpCommerce2_AA090F5E(SceUInt32 version, SceUInt32 *outCtxId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return createContextInternal(version, outCtxId, 1);
}

/* 0x812BD5F8 */
extern "C" int sceNpCommerce2DestroyCtx(SceUInt32 ctxId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return destroyContextInternal(ctxId);
}

/* 0x812BD61A */
extern "C" int sceNpCommerce2DestroyReq(SceUInt32 reqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return destroyRequestInternal(reqId);
}

/* 0x812BD63C */
extern "C" int sceNpCommerce2AbortReq(SceUInt32 reqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return abortRequestInternal(reqId);
}

/* 0x812BD65E */
extern "C" int sceNpCommerce2CreateSessionCreateReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createCreateSessionRequest(ctxId, outReqId);
}

/* 0x812BD68A */
extern "C" int sceNpCommerce2CreateSessionStart(SceUInt32 reqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return startCreateSessionInternal(reqId);
}

/* 0x812BD6AC */
extern "C" int SceNpCommerce2_B7387C96(SceUInt32 reqId, void *buf, SceSize bufLen,
				       SceSize *fillSize)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return getResponseInternal(reqId, reinterpret_cast<char *>(buf), bufLen, fillSize);
}

/* 0x812BD6CC */
extern "C" int SceNpCommerce2_C737A929(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createDeleteSessionRequest(ctxId, outReqId);
}

/* 0x812BD6F8 */
extern "C" int SceNpCommerce2_2DC2A0F9(SceUInt32 reqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return startDeleteSessionInternal(reqId);
}

/* 0x812BD71A */
extern "C" int npCommerce2CreateBindAccountReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createBindAccountRequest(ctxId, outReqId);
}

/* 0x812BD746 */
extern "C" int npCommerce2StartBindAccountReq(SceUInt32 reqId, const char *username,
					      const char *password)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (username == NULL || password == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startBindAccountInternal(reqId, username, password);
}

/* 0x812BD772 */
extern "C" int npCommerce2CreateStartRegReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createStartRegRequest(ctxId, outReqId);
}

/* 0x812BD79E */
extern "C" int npCommerce2StartStartRegReq(SceUInt32 reqId, const char *country,
					   const char *language, int birthYear, int birthMonth,
					   int birthDay, int usePsnEntity)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (country == NULL || language == NULL || birthYear == 0 || birthMonth == 0 ||
	    birthDay == 0)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startStartRegInternal(reqId, country, language, birthYear, birthMonth, birthDay,
				     usePsnEntity);
}

/* 0x812BD7E4 */
extern "C" int npCommerce2CreateAuthenticateReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createAuthenticateRequest(ctxId, outReqId);
}

/* 0x812BD810 */
extern "C" int npCommerce2StartAuthenticateReq(SceUInt32 reqId, int useExplicitCreds,
					       const char *username, const char *password,
					       int usePsnEntity)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (useExplicitCreds == 1 && (username == NULL || password == NULL))
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startAuthenticateInternal(reqId, useExplicitCreds, username, password, usePsnEntity);
}

/* 0x812BD84C */
extern "C" int npCommerce2CreateVerifyDobReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createVerifyDobRequest(ctxId, outReqId);
}

/* 0x812BD878 */
extern "C" int npCommerce2StartVerifyDobReq(SceUInt32 reqId, const char *loginName,
					    int birthYear, int birthMonth, int birthDay)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (loginName == NULL || birthYear == 0 || birthMonth == 0 || birthDay == 0)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startVerifyDobInternal(reqId, loginName, birthYear, birthMonth, birthDay);
}

/* 0x812BD8B8 */
extern "C" int SceNpCommerce2_07C16199(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createGetEpwRequest(ctxId, outReqId);
}

/* 0x812BD8E4 */
extern "C" int SceNpCommerce2_D35F548B(SceUInt32 reqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return startGetEpwInternal(reqId);
}

/* 0x812BD906 */
extern "C" int SceNpCommerce2_5953F48D(ShellNpCommerce2EpwResult *result, const void *data,
				       SceSize size)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (result == NULL || data == NULL || size == 0)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return initEpwResult(result, reinterpret_cast<const char *>(data), size);
}

/* 0x812BD934 */
extern "C" int npCommerce2CreateSendEmailVerReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createSendEmailVerRequest(ctxId, outReqId);
}

/* 0x812BD960 */
extern "C" int npCommerce2StartSendEmailVerReq(SceUInt32 reqId, const char *username)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (username == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startSendEmailVerInternal(reqId, username);
}

/* 0x812BD98C */
extern "C" int npCommerce2CreateGuestSessionReq(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outReqId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return createGuestSessionRequest(ctxId, outReqId);
}

/* 0x812BD9B8 */
extern "C" int npCommerce2StartGuestSessionReq(SceUInt32 reqId, const char *country,
					       const char *language, int birthYear,
					       int birthMonth, int birthDay)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (country == NULL || language == NULL || birthYear == 0 || birthMonth == 0 ||
	    birthDay == 0)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return startGuestSessionInternal(reqId, country, language, birthYear, birthMonth, birthDay);
}

/* 0x812BDA02 */
extern "C" int npCommerce2BindThreadCookie(SceUID threadId, const char *cookie)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (cookie == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return npComm2HttpSetThreadCookie(threadId, cookie);
}

/* 0x812BDA2E */
extern "C" int npCommerce2UnbindThreadCookie(void)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return npComm2HttpClearThreadCookie();
}

/* 0x812BDA4A */
extern "C" int npCommerce2GetContextCookie(SceUInt32 ctxId, const char **outCookie)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (outCookie == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;
	return getContextCookieInternal(ctxId, outCookie);
}

/* 0x812BDA76 */
extern "C" int npCommerce2SetContextServiceType(SceUInt32 ctxId, SceUInt32 serviceType)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	if (serviceType == 0 || serviceType >= 9u)
		return SHELL_NP_COMM2_ERR_INVALID_SERVICE_TYPE;
	return setContextServiceTypeInternal(ctxId, serviceType);
}

/* 0x812BDAA4 */
extern "C" int npCommerce2GetContextBuildVersion(SceUInt32 ctxId, int *outBuildVersion)
{
	if (s_coreInitialized == 0)
		return SHELL_NP_COMM2_ERR_NOT_INITIALIZED;
	return getContextBuildVersionInternal(ctxId, outBuildVersion);
}

/* 0x812BDAC8 */
static int initContextTable(const char *env, int npCommonValue, int envType)
{
	int ret = npComm2SetEnvironment(env);
	if (ret < 0)
		return ret;
	ret = npComm2SetCommonValue(npCommonValue);
	if (ret < 0)
		return ret;
	ret = npComm2SetEnvironmentType(envType);
	if (ret < 0)
		return ret;

	sceClibMemset(&g_npComm2TableMutex, 0, sizeof(g_npComm2TableMutex));
	ret = sceKernelCreateLwMutex(&g_npComm2TableMutex, "SceNpCommerce2",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812BDB1E */
static int createContextInternal(SceUInt32 version, SceUInt32 *outCtxId, int poolKind)
{
	if (version > 2u)
		return SHELL_NP_COMM2_ERR_UNSUPPORTED_VERSION;
	if (outCtxId == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;

	ShellNpCommerce2Context *ctx =
		reinterpret_cast<ShellNpCommerce2Context *>(npCommerce2Alloc(sizeof(ShellNpCommerce2Context)));
	if (ctx == NULL)
		return SHELL_NP_COMM2_ERR_OUT_OF_MEMORY;

	sceClibMemset(ctx, 0, sizeof(*ctx));
	ctx->version = version;
	ctx->serviceType = (ctx->ctxPoolKind == 1) ? 1u : 3u;

	int ret = npComm2HttpCreateTemplate(&ctx->httpTemplate, &ctx->httpConnection);
	if (ret >= 0)
		ret = npComm2AddContext(&g_npComm2TableMutex, ctx, outCtxId, poolKind);

	if (ret < 0) {
		if (ctx->httpTemplate != 0)
			npComm2HttpDeleteTemplate(ctx->httpTemplate, ctx->httpConnection);
		npCommerce2Free(ctx);
	}
	return ret;
}

/* 0x812BDBBE */
static int destroyContextInternal(SceUInt32 ctxId)
{
	ShellNpCommerce2Context *ctx = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, ctxId, &ctx, 1);
	if (ret >= 0)
		ret = npComm2RemoveContext(&g_npComm2TableMutex, ctxId);
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BDC16 */
static int destroyRequestInternal(SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0) {
			removeRequest(&g_npComm2TableMutex, reqId);
			ret = 0;
		}
	}
	if (req != NULL)
		releaseRequest(&g_npComm2TableMutex, req);
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BDC8A */
static int abortRequestInternal(SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0) {
			npComm2HttpAbortRequest(req->httpRequest);
			req->flags |= SHELL_NP_COMM2_REQ_FLAG_ABORTED;
			ret = 0;
		}
	}
	if (req != NULL)
		releaseRequest(&g_npComm2TableMutex, req);
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BDD08 */
static int getResponseInternal(SceUInt32 reqId, char *buf, SceSize bufLen, SceSize *outSize)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int ret;

	if (buf == NULL || bufLen == 0 || outSize == NULL)
		return SHELL_NP_COMM2_ERR_INVALID_ARG;

	ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0) {
			if (req->op > SHELL_NP_COMM2_OP_DEL_SESS &&
			    req->op <= SHELL_NP_COMM2_OP_GET_PROD &&
			    ctx->ctxPoolKind != 1 && bufLen < NP_COMM2_LARGE_BUF_MIN) {
				ret = SHELL_NP_COMM2_ERR_RESPONSE_BUF_TOO_SMALL;
			} else {
				ret = npComm2HttpCheckStatus(req->httpRequest,
							     &ctx->nativeBuildVersion);
			}

			if (ret >= 0) {
				SceUInt8 serverErr = 0;
				SceSize skipBytes = 0;
				int preambleLen = buildResponsePreamble(ctx, req->httpRequest,
									req->op, &serverErr,
									buf, bufLen, &skipBytes);
				ret = preambleLen;
				if (preambleLen >= 0) {
					int truncated = 0;
					int bodyRead = npComm2HttpReadBody(req->httpRequest, buf,
									   buf + preambleLen,
									   bufLen, &truncated,
									   static_cast<int>(skipBytes));
					ret = bodyRead;
					if (bodyRead >= 0) {
						char *endPtr = buf + preambleLen + bodyRead;
						ret = npComm2HttpUpdateCookie(
							ctx->httpTemplate, ctx->httpConnection,
							req->httpRequest, ctx->cookieHeader,
							sizeof(ctx->cookieHeader));
						if (ret >= 0) {
							bool allowTruncated =
								(req->op == SHELL_NP_COMM2_OP_GET_CAT ||
								 req->op == SHELL_NP_COMM2_OP_GET_DRM ||
								 req->op == SHELL_NP_COMM2_OP_SRCH_PROD ||
								 req->op == SHELL_NP_COMM2_OP_GET_SRVS ||
								 req->op == SHELL_NP_COMM2_OP_GET_SRV_DETAIL ||
								 req->op == SHELL_NP_COMM2_OP_GET_SUBSCR ||
								 req->op == SHELL_NP_COMM2_OP_RECOM_PRODS);
							if (truncated == 1 && !allowTruncated) {
								ret = SHELL_NP_COMM2_ERR_INSUFFICIENT_BUF;
							} else {
								SceSize filled = static_cast<SceSize>(endPtr - buf);
								ret = patchResponseLength(
									buf, bufLen,
									filled - NP_COMM2_RESPONSE_HDR_SIZE);
								if (ret >= 0) {
									*outSize = filled;
									if (serverErr != 0) {
										ret = static_cast<int>(
											SHELL_NP_COMM2_ERR_SERVER_BASE | serverErr);
										if ((req->op == SHELL_NP_COMM2_OP_REG_CC ||
										     req->op == SHELL_NP_COMM2_OP_VERIFY_MASTER_BILL) &&
										    ret == SHELL_NP_COMM2_ERR_SERVER_GENERIC) {
											ret = SHELL_NP_COMM2_ERR_SERVER_CC_REJECTED;
										} else if (req->op == SHELL_NP_COMM2_OP_SET_RES_ADDR &&
											   ret == SHELL_NP_COMM2_ERR_SERVER_GENERIC) {
											ret = SHELL_NP_COMM2_ERR_SERVER_ADDR_REJECTED;
										} else {
											int hasExtErr = 0;
											responseHasExtErrorRecord(buf, filled, &hasExtErr);
											if (hasExtErr == 1)
												ret = SHELL_NP_COMM2_ERR_HAS_EXT_ERROR_RECORD;
										}
									} else {
										ret = 0;
										ShellNpCommerce2SessionInfo sessInfo;
										bool parsedSession = false;
										if (req->op == SHELL_NP_COMM2_OP_AUTHENTICATE) {
											ShellNpCommerce2AuthResult authRes;
											if (initAuthResult(&authRes, buf, filled) >= 0 &&
											    parseSessionInfo(&authRes.common, &sessInfo) >= 0) {
												parsedSession = true;
											}
										} else if (req->op == SHELL_NP_COMM2_OP_CR_SESS ||
											   req->op == SHELL_NP_COMM2_OP_CR_GUEST_SESS) {
											ShellNpCommerce2SessionResult sessRes;
											if (initSessionResult(&sessRes, buf, filled) >= 0 &&
											    parseSessionInfo(&sessRes.common, &sessInfo) >= 0) {
												parsedSession = true;
											}
										}
										if (parsedSession) {
											sceClibStrncpy(ctx->currencyCode,
												       sessInfo.currencyCode, 4);
											ctx->currencyCode[3] = '\0';
											sceClibStrncpy(ctx->currencySymbol,
												       sessInfo.currencySymbol, 4);
											ctx->currencySymbol[3] = '\0';
											patchResponseCurrency(buf, filled,
													      ctx->currencyCode,
													      ctx->currencySymbol);
										}
										if (req->op == SHELL_NP_COMM2_OP_CR_GUEST_SESS ||
										    req->op == SHELL_NP_COMM2_OP_VERIFY_DOB ||
										    req->op == SHELL_NP_COMM2_OP_AUTHENTICATE ||
										    req->op == SHELL_NP_COMM2_OP_START_REG ||
										    req->op == SHELL_NP_COMM2_OP_CR_SESS) {
											if (filled <= NP_COMM2_CACHE_MAX_SIZE) {
												ctx->cachedResponseSize = filled;
												sceClibMemcpy_safe(ctx->cachedResponse, buf, filled);
											} else {
												ctx->cachedResponseSize = 0;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

static inline int createOpRequestInline(SceUInt32 ctxId, SceUInt16 op, SceUInt32 *outReqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, ctxId, &ctx, 1);
	if (ret >= 0) {
		ShellNpCommerce2Request *req =
			reinterpret_cast<ShellNpCommerce2Request *>(npCommerce2Alloc(sizeof(ShellNpCommerce2Request)));
		if (req != NULL) {
			sceClibMemset(req, 0, sizeof(*req));
			req->op = op;
			ret = registerRequest(&g_npComm2TableMutex, req, ctxId, outReqId);
			if (ret < 0)
				npCommerce2Free(req);
		} else {
			ret = SHELL_NP_COMM2_ERR_OUT_OF_MEMORY;
		}
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE00A */
static int createCreateSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_CR_SESS, outReqId);
}

/* 0x812BE1D6 */
static int createDeleteSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_DEL_SESS, outReqId);
}

/* 0x812BE362 */
static int createBindAccountRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_BIND_ACCOUNT, outReqId);
}

/* 0x812BE51A */
static int createStartRegRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_START_REG, outReqId);
}

/* 0x812BE702 */
static int createAuthenticateRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_AUTHENTICATE, outReqId);
}

/* 0x812BE8EC */
static int createVerifyDobRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_VERIFY_DOB, outReqId);
}

/* 0x812BEAB4 */
static int createGetEpwRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_GET_EPW, outReqId);
}

/* 0x812BEC38 */
static int createSendEmailVerRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_SEND_EMAIL_VER, outReqId);
}

/* 0x812BEE1E */
static int createGuestSessionRequest(SceUInt32 ctxId, SceUInt32 *outReqId)
{
	return createOpRequestInline(ctxId, SHELL_NP_COMM2_OP_CR_GUEST_SESS, outReqId);
}

static void refreshIdleConnection(ShellNpCommerce2Context *ctx)
{
	SceUInt64 now = 0;
	if (npComm2GetSystemTime(&now) >= 0 &&
	    (now - ctx->lastRequestTime) >= NP_COMM2_CONN_IDLE_US) {
		npComm2HttpResetConnection(ctx->httpTemplate, &ctx->httpConnection);
	}
}

/* 0x812BE0A6 */
static int startCreateSessionInternal(SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char username[71];
	char password[36];
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = fetchStoredAccountCreds(ctx, username, password, 0);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendCredsAction(ctx->serviceType, ctx->httpTemplate,
						     &ctx->httpConnection, req->op,
						     &req->httpRequest, protoVer,
						     username, password, consoleId);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE272 */
static int startDeleteSessionInternal(SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendEmptyAction(ctx->serviceType, ctx->httpTemplate,
						     &ctx->httpConnection, req->op,
						     &req->httpRequest);
			if (ret >= 0) {
				npComm2GetSystemTime(&ctx->lastRequestTime);
				ctx->cachedResponseSize = 0;
			}
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE3FE */
static int startBindAccountInternal(SceUInt32 reqId, const char *username, const char *password)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendCredsAction(ctx->serviceType, ctx->httpTemplate,
						     &ctx->httpConnection, req->op,
						     &req->httpRequest, protoVer,
						     username, password, consoleId);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE5B6 */
static int startStartRegInternal(SceUInt32 reqId, const char *country, const char *language,
				 int birthYear, int birthMonth, int birthDay, int usePsnEntity)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendStartRegAction(ctx->serviceType, ctx->httpTemplate,
							&ctx->httpConnection, req->op,
							&req->httpRequest, protoVer,
							country, language, birthYear, birthMonth,
							birthDay, consoleId, usePsnEntity);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE79E */
static int startAuthenticateInternal(SceUInt32 reqId, int useExplicitCreds,
				     const char *username, const char *password, int usePsnEntity)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char storedUsername[71];
	char storedPassword[36];
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0 && useExplicitCreds == 0) {
			ret = fetchStoredAccountCreds(ctx, storedUsername, storedPassword, 0);
			if (ret >= 0) {
				username = storedUsername;
				password = storedPassword;
			}
		}
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendAuthAction(ctx->serviceType, ctx->httpTemplate,
						    &ctx->httpConnection, req->op,
						    &req->httpRequest, protoVer,
						    username, password, consoleId, usePsnEntity);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BE988 */
static int startVerifyDobInternal(SceUInt32 reqId, const char *loginName,
				  int birthYear, int birthMonth, int birthDay)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendVerifyDobAction(ctx->serviceType, ctx->httpTemplate,
							 &ctx->httpConnection, req->op,
							 &req->httpRequest, protoVer,
							 loginName, birthYear, birthMonth,
							 birthDay, consoleId);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BEB50 */
static int startGetEpwInternal(SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendEmptyAction(ctx->serviceType, ctx->httpTemplate,
						     &ctx->httpConnection, req->op,
						     &req->httpRequest);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BECD4 */
static int startGuestSessionInternal(SceUInt32 reqId, const char *country, const char *language,
				     int birthYear, int birthMonth, int birthDay)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendGuestSessAction(ctx->serviceType, ctx->httpTemplate,
							 &ctx->httpConnection, req->op,
							 &req->httpRequest, protoVer,
							 country, language, birthYear, birthMonth,
							 birthDay, consoleId);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BEEBA */
static int startSendEmailVerInternal(SceUInt32 reqId, const char *username)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *req = NULL;
	int protoVer = 0;
	char consoleId[65];

	int ret = npComm2AcquireContext(&g_npComm2TableMutex, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = acquireRequest(&g_npComm2TableMutex, reqId, &req);
		if (ret >= 0)
			ret = getProtocolVersion(ctx, &protoVer);
		if (ret >= 0)
			ret = buildConsoleIdString(consoleId, sizeof(consoleId));
		if (ret >= 0) {
			refreshIdleConnection(ctx);
			ret = npComm2SendEmailVerAction(ctx->serviceType, ctx->httpTemplate,
							&ctx->httpConnection, req->op,
							&req->httpRequest, protoVer,
							username, consoleId);
			if (ret >= 0)
				npComm2GetSystemTime(&ctx->lastRequestTime);
		}
	}
	if (req != NULL) {
		if ((req->flags & SHELL_NP_COMM2_REQ_FLAG_ABORTED) != 0)
			ret = SHELL_NP_COMM2_ERR_ABORTED;
		releaseRequest(&g_npComm2TableMutex, req);
	}
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BEFE0 */
static int getContextCookieInternal(SceUInt32 ctxId, const char **outCookie)
{
	ShellNpCommerce2Context *ctx = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, ctxId, &ctx, 1);
	if (ret >= 0)
		*outCookie = ctx->cookieHeader;
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BF034 */
static int setContextServiceTypeInternal(SceUInt32 ctxId, SceUInt32 serviceType)
{
	ShellNpCommerce2Context *ctx = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, ctxId, &ctx, 1);
	if (ret >= 0)
		ctx->serviceType = serviceType;
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BF088 */
static int getContextBuildVersionInternal(SceUInt32 ctxId, int *outBuildVersion)
{
	ShellNpCommerce2Context *ctx = NULL;
	int ret = npComm2AcquireContext(&g_npComm2TableMutex, ctxId, &ctx, 1);
	if (ret >= 0)
		*outBuildVersion = ctx->nativeBuildVersion;
	if (ctx != NULL)
		npComm2ReleaseContext(&g_npComm2TableMutex, ctx);
	return ret;
}

/* 0x812BF0E0 */
static int registerRequest(SceKernelLwMutexWork *lock, ShellNpCommerce2Request *req,
			   SceUInt32 ctxId, SceUInt32 *outReqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	int ret = npComm2AcquireContext(lock, ctxId, &ctx, 1);

	if (ret >= 0) {
		unsigned int count = 0;
		for (ShellNpCommerce2Request *cur = ctx->requests; cur != NULL; cur = cur->next)
			count++;

		if (count >= NP_COMM2_MAX_REQ_PER_CTX) {
			ret = SHELL_NP_COMM2_ERR_REQ_MAX;
		} else {
			int startSeq = s_lastReqSeq;
			int candidate = (startSeq == 0xFFFF) ? 1 : (startSeq + 1);
			while (candidate != startSeq) {
				ShellNpCommerce2Context *probeCtx = NULL;
				int probeRet = npComm2AcquireContext(lock, ctxId, &probeCtx, 0);
				if (probeRet >= 0) {
					ShellNpCommerce2Request *cur = probeCtx->requests;
					while (cur != NULL) {
						if (static_cast<SceUInt16>(cur->id) ==
						    static_cast<SceUInt16>(candidate)) {
							break;
						}
						cur = cur->next;
					}
					if (cur == NULL)
						probeRet = SHELL_NP_COMM2_ERR_REQ_NOT_FOUND;
				}
				if (probeCtx != NULL)
					npComm2ReleaseContext(lock, probeCtx);
				if (probeRet < 0)
					break;
				candidate = (candidate == 0xFFFF) ? 1 : (candidate + 1);
			}

			if (candidate == s_lastReqSeq) {
				ret = SHELL_NP_COMM2_ERR_REQ_ID_FULL;
			} else {
				SceUInt32 fullId = (static_cast<SceUInt32>(static_cast<SceUInt16>(ctxId)) << 16) |
						   static_cast<SceUInt16>(candidate);
				req->next = ctx->requests;
				ctx->requests = req;
				s_lastReqSeq = candidate;
				req->id = fullId;
				*outReqId = fullId;
				ret = 0;
			}
		}
	}

	if (ctx != NULL)
		npComm2ReleaseContext(lock, ctx);
	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812BF238 */
static int acquireRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId,
			  ShellNpCommerce2Request **outReq)
{
	ShellNpCommerce2Context *ctx = NULL;
	ShellNpCommerce2Request *found = NULL;
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	int ret = npComm2AcquireContext(lock, reqId >> 16, &ctx, 0);

	if (ret >= 0) {
		ShellNpCommerce2Request *cur = ctx->requests;
		while (cur != NULL) {
			if (static_cast<SceUInt16>(cur->id) == static_cast<SceUInt16>(reqId))
				break;
			cur = cur->next;
		}
		if (cur != NULL)
			found = cur;
		else
			ret = SHELL_NP_COMM2_ERR_REQ_NOT_FOUND;
	}
	if (ctx != NULL)
		npComm2ReleaseContext(lock, ctx);
	if (ret >= 0) {
		found->refCount++;
		*outReq = found;
	}
	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812BF2DA */
static int releaseRequest(SceKernelLwMutexWork *lock, ShellNpCommerce2Request *req)
{
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	req->refCount--;
	if (req->refCount == 0 && (req->flags & SHELL_NP_COMM2_REQ_FLAG_REMOVED) != 0) {
		if (req->httpRequest != 0)
			npComm2HttpDeleteRequest(req->httpRequest);
		req->httpRequest = 0;
		npCommerce2Free(req);
	}
	if (lockRet == 0)
		return sceKernelUnlockLwMutex(lock, 1);
	return lockRet;
}

/* 0x812BF318 */
static int removeRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId)
{
	ShellNpCommerce2Context *ctx = NULL;
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	int ret = npComm2AcquireContext(lock, reqId >> 16, &ctx, 0);

	if (ret >= 0) {
		ShellNpCommerce2Request *cur = ctx->requests;
		ShellNpCommerce2Request *prev = NULL;
		while (cur != NULL) {
			if (cur->id == reqId) {
				if (prev != NULL)
					prev->next = cur->next;
				else
					ctx->requests = cur->next;
				cur->next = NULL;
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		npComm2ReleaseContext(lock, ctx);
		if (cur != NULL) {
			if (cur->httpRequest != 0) {
				npComm2HttpAbortRequest(cur->httpRequest);
				cur->flags |= SHELL_NP_COMM2_REQ_FLAG_ABORTED;
			}
			cur->flags |= SHELL_NP_COMM2_REQ_FLAG_REMOVED;
			if (cur->refCount == 0) {
				if (cur->httpRequest != 0)
					npComm2HttpDeleteRequest(cur->httpRequest);
				cur->httpRequest = 0;
				npCommerce2Free(cur);
			}
			ret = 0;
		} else {
			ret = SHELL_NP_COMM2_ERR_REQ_NOT_FOUND;
		}
	}
	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812BF3E4 */
static int parseResponseHeader(const char *buf, SceSize size, SceUInt32 *outVersion,
			       SceUInt32 *outDataLen, SceUInt32 *outStatus)
{
	if (size < 0x68u)
		return SHELL_NP_COMM2_ERR_INVALID_RESULT_DATA;

	ShellNpCommerce2ResponseHeader hdr;
	char tailWord[4];
	sceClibMemcpy_safe(&hdr, buf, sizeof(hdr));
	sceClibMemcpy_safe(tailWord, buf + sizeof(hdr), sizeof(tailWord));

	if (hdr.length < 4u || size < hdr.length + NP_COMM2_RESPONSE_HDR_SIZE)
		return SHELL_NP_COMM2_ERR_INVALID_RESULT_DATA;

	*outDataLen = hdr.length - 4u;
	if (outVersion != NULL)
		*outVersion = hdr.version;
	if (outStatus != NULL)
		*outStatus = hdr.status;
	return 104;
}

/* 0x812BF476 */
static int responseHasExtErrorRecord(const char *buf, SceSize size, int *outHasRecord)
{
	SceUInt32 version = 0;
	SceUInt32 dataLen = 0;
	SceUInt32 count = 0;
	int offset = parseResponseHeader(buf, size, &version, &dataLen, NULL);
	if (offset < 0)
		return offset;

	int found = findNestedTlvRecordWhole(buf + offset, dataLen, 120u, NULL, &count);
	*outHasRecord = (found >= 0) ? 1 : 0;
	return 0;
}

/* 0x812BF4D0 */
static int initSessionResult(ShellNpCommerce2SessionResult *result, const char *buf, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLen = 0;
	int offset = parseResponseHeader(buf, size, &version, &dataLen, NULL);
	if (offset < 0)
		return offset;

	sceClibMemset(result, 0, sizeof(*result));
	result->common.version = version;
	result->common.bufHead = buf;
	result->common.bufSize = static_cast<SceSize>(offset) + dataLen;
	result->common.data = buf + offset;
	result->common.dataSize = dataLen;

	int ret = findNestedTlvRecordWhole(buf + offset, dataLen, 22u, NULL, &result->tagCount);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812BF55A */
static int parseSessionInfo(const ShellNpCommerce2CommonData *common,
			    ShellNpCommerce2SessionInfo *outInfo)
{
	SceUInt16 recordLen = 0;
	int offset = findTlvRecordWhole(common->data, common->dataSize, 94u, 60u, &recordLen);
	if (offset < 0)
		return offset;

	sceClibMemset(outInfo, 0, sizeof(*outInfo));
	outInfo->common.version = common->version;
	outInfo->common.bufHead = common->bufHead;
	const char *rec = common->data + offset;
	outInfo->common.data = rec;
	outInfo->common.dataSize = recordLen;

	SceUInt64 netAccountId = 0;
	sceClibMemcpy_safe(&netAccountId, rec, 8u);
	outInfo->accountId = sceNetNtohll(netAccountId);
	outInfo->onlineId = rec + 8;
	outInfo->currencyCode = rec + 25;
	outInfo->currencySymbol = rec + 28;

	SceUInt32 netDecimals = 0;
	sceClibMemcpy_safe(&netDecimals, rec + 34, 4u);
	outInfo->decimals = sceNetNtohl(netDecimals);

	SceUInt32 netSymPos = 0;
	sceClibMemcpy_safe(&netSymPos, rec + 38, 4u);
	outInfo->symbolPosition = (sceNetNtohl(netSymPos) != 0) ? 1u : 0u;

	SceUInt32 netSymSpace = 0;
	sceClibMemcpy_safe(&netSymSpace, rec + 42, 4u);
	outInfo->symbolWithSpace = sceNetNtohl(netSymSpace);

	SceUInt64 netTimestamp = 0;
	sceClibMemcpy_safe(&netTimestamp, rec + 46, 8u);
	outInfo->serverTimestamp = sceNetNtohll(netTimestamp);

	SceUInt16 netStoreVer = 0;
	sceClibMemcpy_safe(&netStoreVer, rec + 54, 2u);
	outInfo->storeVersion = sceNetNtohs(netStoreVer);

	const char *subStart = rec + 56;
	outInfo->common.subData = subStart;
	outInfo->localeString = outInfo->common.bufHead + 4;

	SceUInt16 localeLen = 0;
	int localeOff = findTlvRecordWhole(subStart, recordLen - 56u, 93u, 0u, &localeLen);
	if (localeOff >= 0 && localeLen != 0)
		outInfo->localeString = subStart + localeOff;
	return localeOff;
}

/* 0x812BF6A8 */
static int initAuthResult(ShellNpCommerce2AuthResult *result, const char *buf, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLen = 0;
	int offset = parseResponseHeader(buf, size, &version, &dataLen, NULL);
	if (offset < 0)
		return offset;

	sceClibMemset(result, 0, sizeof(*result));
	result->common.version = version;
	result->common.bufHead = buf;
	result->common.bufSize = static_cast<SceSize>(offset) + dataLen;
	const char *payload = buf + offset;
	result->common.data = payload;
	result->common.dataSize = dataLen;

	int ret = findNestedTlvRecordWhole(payload, dataLen, 22u, NULL, &result->sessionCount);
	if (ret < 0 && ret != SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND)
		return ret;

	ret = findNestedTlvRecordWhole(payload, dataLen, 100u, NULL, &result->accountCount);
	if (ret < 0 && ret != SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND)
		return ret;

	ret = findNestedTlvRecordWhole(payload, dataLen, 88u, NULL, &result->entitlementCount);
	if (ret < 0 && ret != SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND)
		return ret;

	result->redirectUrl = result->common.bufHead + 4;
	SceUInt16 urlLen = 0;
	ret = findTlvRecordWhole(payload, dataLen, 92u, 0u, &urlLen);
	if (ret >= 0 && urlLen != 0)
		result->redirectUrl = payload + ret;
	if (ret < 0 && ret != SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND)
		return ret;

	return 0;
}

/* 0x812BF7C6 */
static int initEpwResult(ShellNpCommerce2EpwResult *result, const char *buf, SceSize size)
{
	SceUInt32 version = 0;
	SceUInt32 dataLen = 0;
	int offset = parseResponseHeader(buf, size, &version, &dataLen, NULL);
	if (offset < 0)
		return offset;

	sceClibMemset(result, 0, sizeof(*result));
	result->common.version = version;
	result->common.bufHead = buf;
	result->common.bufSize = static_cast<SceSize>(offset) + dataLen;
	const char *payload = buf + offset;
	result->common.data = payload;
	result->common.dataSize = dataLen;
	result->epw = buf + 4;

	SceUInt16 epwLen = 0;
	int ret = findTlvRecordWhole(payload, dataLen, 118u, 0u, &epwLen);
	if (ret >= 0 && epwLen != 0)
		result->epw = payload + ret;
	return (ret >= 0) ? 0 : ret;
}

/* 0x812BF870 */
static int buildResponsePreamble(const ShellNpCommerce2Context *ctx, int httpReq, SceUInt16 op,
				 SceUInt8 *outServerErr, char *buf, SceSize bufLen,
				 SceSize *outHeaderBodyBytes)
{
	(void)op;
	if (bufLen < 0x68u)
		return SHELL_NP_COMM2_ERR_INSUFFICIENT_BUF;

	ShellNpCommerce2ResponseHeader hdr;
	sceClibMemset(&hdr, 0, sizeof(hdr));
	hdr.version = ctx->version;
	hdr.nativeBuildVersion = static_cast<SceUInt32>(ctx->nativeBuildVersion);
	sceClibStrncpy(hdr.currencyCode, ctx->currencyCode, sizeof(hdr.currencyCode));
	hdr.currencyCode[3] = '\0';
	sceClibStrncpy(hdr.currencySymbol, ctx->currencySymbol, sizeof(hdr.currencySymbol));
	hdr.currencySymbol[3] = '\0';
	sceClibStrncpy(hdr.decimalPoint, ".", sizeof(hdr.decimalPoint));
	hdr.decimalPoint[3] = '\0';
	sceClibStrncpy(hdr.thousandSeparator, ",", sizeof(hdr.thousandSeparator));
	hdr.thousandSeparator[3] = '\0';
	sceClibMemcpy_safe(buf, &hdr, sizeof(hdr));

	SceUInt8 statusPrefix[4];
	int readLen = npComm2HttpReadExact(httpReq, reinterpret_cast<char *>(statusPrefix), 4u);
	if (readLen < 0)
		return readLen;
	if (readLen < 4)
		return SHELL_NP_COMM2_ERR_INVALID_RESPONSE;

	*outServerErr = statusPrefix[1];
	statusPrefix[2] = 0;
	statusPrefix[3] = 0;
	sceClibMemcpy_safe(buf + sizeof(hdr), statusPrefix, sizeof(statusPrefix));
	*outHeaderBodyBytes = 4u;
	return 104;
}

/* 0x812BF98E */
static int patchResponseLength(char *buf, SceSize bufLen, SceUInt32 totalDataLen)
{
	if (bufLen < NP_COMM2_RESPONSE_HDR_SIZE)
		return SHELL_NP_COMM2_ERR_INSUFFICIENT_BUF;

	ShellNpCommerce2ResponseHeader hdr;
	sceClibMemcpy_safe(&hdr, buf, sizeof(hdr));
	hdr.length = totalDataLen;
	sceClibMemcpy_safe(buf, &hdr, sizeof(hdr));
	return 0;
}

/* 0x812BF9F0 */
static int patchResponseCurrency(char *buf, SceSize bufLen, const char *currencyCode,
				 const char *currencySymbol)
{
	if (bufLen < NP_COMM2_RESPONSE_HDR_SIZE)
		return SHELL_NP_COMM2_ERR_INSUFFICIENT_BUF;

	ShellNpCommerce2ResponseHeader hdr;
	sceClibMemcpy_safe(&hdr, buf, sizeof(hdr));
	sceClibStrncpy(hdr.currencyCode, currencyCode, sizeof(hdr.currencyCode));
	hdr.currencyCode[3] = '\0';
	sceClibStrncpy(hdr.currencySymbol, currencySymbol, sizeof(hdr.currencySymbol));
	hdr.currencySymbol[3] = '\0';
	sceClibMemcpy_safe(buf, &hdr, sizeof(hdr));
	return 0;
}

/* 0x812BFA68 */
static int findTlvRecord(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 minLen,
			 SceUInt16 *outLen, int allowPartial)
{
	if (size < 4u)
		return SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND;

	const char *cursor = buf;
	SceUInt16 recordLen = 0;
	for (;;) {
		SceUInt16 rawTag = 0;
		SceUInt16 rawLen = 0;
		sceClibMemcpy_safe(&rawTag, cursor, 2u);
		SceUInt16 curTag = sceNetNtohs(rawTag) & 0x0FFFu;
		sceClibMemcpy_safe(&rawLen, cursor + 2, 2u);
		recordLen = sceNetNtohs(rawLen);
		cursor += 4;
		if (curTag == tag)
			break;
		cursor += recordLen;
		if (size < static_cast<SceSize>(cursor - buf) + 4u)
			return SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND;
	}

	int offset = static_cast<int>(cursor - buf);
	if (size < static_cast<SceSize>(offset) + minLen)
		return SHELL_NP_COMM2_ERR_RECORD_CUT_OFF;
	if (recordLen < minLen)
		return SHELL_NP_COMM2_ERR_RECORD_TOO_SHORT;
	if (size < static_cast<SceSize>(offset) + recordLen && allowPartial == 0)
		return SHELL_NP_COMM2_ERR_RECORD_TRUNCATED;

	if (outLen != NULL)
		*outLen = recordLen;
	return offset;
}

/* 0x812BFB42 */
static int findTlvRecordWhole(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 minLen,
			      SceUInt16 *outLen)
{
	return findTlvRecord(buf, size, tag, minLen, outLen, 0);
}

/* 0x812BFB5A */
static int findNestedTlvRecord(const char *buf, SceSize size, SceUInt16 tag, SceUInt16 *outLen,
			       SceUInt32 *outCount, int allowPartial)
{
	const char *cursor = buf;
	SceSize consumed = 0;

	for (;;) {
		SceUInt16 containerLen = 0;
		int ret = findTlvRecord(cursor, size - consumed, 1u, 4u, &containerLen, allowPartial);
		if (ret < 0)
			return ret;

		const char *inner = cursor + ret;
		SceUInt16 rawTag = 0;
		SceUInt16 rawCount = 0;
		sceClibMemcpy_safe(&rawTag, inner, 2u);
		SceUInt16 innerTag = sceNetNtohs(rawTag) & 0x0FFFu;
		sceClibMemcpy_safe(&rawCount, inner + 2, 2u);
		SceUInt16 innerCount = sceNetNtohs(rawCount);
		if (innerTag == tag) {
			if (outLen != NULL)
				*outLen = static_cast<SceUInt16>(containerLen - 4u);
			if (outCount != NULL)
				*outCount = innerCount;
			return static_cast<int>(inner - buf) + 4;
		}
		cursor = inner + containerLen;
		consumed = static_cast<SceSize>(cursor - buf);
		if (size < consumed)
			return SHELL_NP_COMM2_ERR_RECORD_NOT_FOUND;
	}
}

/* 0x812BFC2A */
static int findNestedTlvRecordWhole(const char *buf, SceSize size, SceUInt16 tag,
				    SceUInt16 *outLen, SceUInt32 *outCount)
{
	return findNestedTlvRecord(buf, size, tag, outLen, outCount, 0);
}

/* 0x812BFC40 */
int npComm2SetEnvironment(const char *env)
{
	if (env == NULL)
		return SHELL_NP_COMM2_ERR_NO_ENVIRONMENT;
	if (sceClibStrnlen(env, 16u) >= 17u)
		return SHELL_NP_COMM2_ERR_ENV_TOO_LONG;
	sceClibStrncpy(s_npEnvironment, env, 17u);
	s_npEnvironment[16] = '\0';
	return 0;
}

/* 0x812BFC82 */
int npComm2GetEnvironment(char *outEnv, SceSize maxLen)
{
	sceClibStrncpy(outEnv, s_npEnvironment, maxLen);
	outEnv[maxLen - 1] = '\0';
	return 0;
}

/* 0x812BFCA0 */
static int getProtocolVersion(const ShellNpCommerce2Context *ctx, int *outProtoVer)
{
	if (ctx->version != 2u)
		return SHELL_NP_COMM2_ERR_INVALID_PROTO_VER;
	*outProtoVer = 25;
	return 0;
}

/* 0x812BFCB8 */
static int fetchStoredAccountCreds(const ShellNpCommerce2Context *ctx, char *username,
				   char *password, int mode)
{
	(void)ctx;
	return SceNpManager_DDF30B0E(username, password, mode);
}

/* 0x812BFCC6 */
static int buildConsoleIdString(char *outBuf, SceSize bufSize)
{
	if (bufSize < 64u)
		return SHELL_NP_COMM2_ERR_CONSOLE_ID_BUF_SMALL;

	SceUInt8 consoleId[16];
	int ret = vshSblAimgrGetConsoleId(consoleId);
	if (ret >= 0) {
		for (int i = 0; i < 16; ++i)
			sceClibSnprintf(outBuf + i * 2, 3u, "%02x", consoleId[i]);
		sceClibSnprintf(outBuf + 32, bufSize - 32u, "%016llx%016llx", 0ULL, 0ULL);
	}
	return ret;
}

/* 0x812BFD58 */
int npComm2SetCommonValue(int value)
{
	s_npCommonValue = value;
	return 0;
}

/* 0x812BFD66 */
int npComm2SetEnvironmentType(int envType)
{
	s_npEnvType = envType;
	return 0;
}

/* 0x812BFD74 */
int npComm2GetSystemTime(SceUInt64 *outTime)
{
	*outTime = sceKernelGetSystemTimeWide();
	return 0;
}

/* 0x812BFD84 */
extern "C" int npCommerce2CoreInit(void)
{
	if (s_initWrapperDone != 0)
		return SHELL_NP_COMM2_ERR_ALREADY_INITIALIZED;

	char env[20];
	int envType = 0;
	int ret = sceNpCommonGetNpEnviroment(env);
	if (ret >= 0) {
		int commonVal = SceNpCommon_F585C86C();
		SceNpCommon_187C0A8D(&envType);
		ret = npCommerce2CoreInitialize(env, commonVal, envType);
		if (ret >= 0) {
			s_initWrapperDone = 1;
			return 0;
		}
	}
	return ret;
}
