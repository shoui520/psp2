/*
 * np_oauth_vsh.cpp: NpOAuthVsh OAuth 2.0 client service for SceShell
 * (initialization, NPSSO acquisition, authorization code grant, and access token caching).
 */
#include "np_oauth_internal.h"
#include <libkernel/libkernel.h>
#include <libhttp/methods.h>

static NpOAuthContext g_npOAuthContext;

static const char *const kNpOAuthVshClientId = "c397a7a8-2b14-402c-b71c-ff9b5dcd5994";
static const char *const kNpOAuthVshProdSecret = "fY9JfNg7pijD74oE";
static const char *const kNpOAuthVshDevSecret = "NiO7r2Zruu7qikHs";
static const char *const kNpOAuthDefaultRedirectUri = "orbis://games";
static const char *const kNpOAuthVshRedirectUri = "psvita://local/vsh";
static const char *const kNpOAuthVshFullScope =
	"openid,kamaji:social_get_graph,kamaji:get_account_hash,user:account.identityMapper";

static int npOAuthFindAndAcquireRequest(NpOAuthRequest *request, NpOAuthRequest **outRequest)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	int ret = 0;

	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	NpOAuthRequest *cur = ctx->requestListHead;
	while (cur != NULL) {
		if (cur->requestId == request->requestId && cur == request) {
			ret = npOAuthRequestAcquire(request, outRequest);
			sceKernelUnlockLwMutex(ctx->mutex, 1);
			return ret;
		}
		cur = cur->next;
	}
	sceKernelUnlockLwMutex(ctx->mutex, 1);
	return NP_OAUTH_ERR_INVALID_REQUEST_ID;
}

static void npOAuthClearNpssoCacheLocked(void)
{
	NpOAuthContext *ctx = &g_npOAuthContext;

	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	sceClibMemset(&ctx->hasNpssoToken, 0, sizeof(ctx->hasNpssoToken) + sizeof(ctx->npssoToken));
	ctx->hasNpssoToken = 0;
	sceClibMemset(&ctx->issuerId, 0, sizeof(ctx->issuerId));
	sceKernelUnlockLwMutex(ctx->mutex, 1);
}

static void npOAuthAppendFormPart(char *dest, int *remaining, const char *part)
{
	SceSize copyLimit = (*remaining > 0) ? (SceSize)(*remaining) : 0U;
	sceClibStrncat(dest, part, copyLimit);
	*remaining -= (int)sceClibStrnlen(part, 0x7FFFFFFFU);
}

static int npOAuthAppendFormPair(char *dest, int *remaining, const char *key, const char *value,
				 bool addTrailingAmp)
{
	npOAuthAppendFormPart(dest, remaining, key);
	npOAuthAppendFormPart(dest, remaining, "=");
	npOAuthAppendFormPart(dest, remaining, value);
	if (addTrailingAmp)
		npOAuthAppendFormPart(dest, remaining, "&");
	return 0;
}

static int npOAuthAppendEscapedFormPair(char *dest, int *remaining, char *escapeBuf,
					const char *key, const char *rawValue, bool addTrailingAmp)
{
	sceClibMemset(escapeBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
	int ret = sceHttpUriEscape(escapeBuf, NULL, NP_OAUTH_BODY_BUF_SIZE, rawValue);
	if (ret < 0)
		return ret;
	npOAuthAppendFormPair(dest, remaining, key, escapeBuf, addTrailingAmp);
	return 0;
}

NpOAuthContext *npOAuthGetContext(void)
{
	return &g_npOAuthContext;
}

int npOAuthInit(void)
{
	NpOAuthAllocator *allocator = SceNpCommonPs4_53484B79();
	if (allocator == NULL)
		return NP_OAUTH_ERR_INTERNAL;

	int ret = npCslaInit(allocator);
	if (ret < 0)
		return ret;

	char fwVersion[8];
	sceClibMemset(fwVersion, 0, sizeof(fwVersion));
	SceNpCommon_0624AE44(fwVersion);

	char userAgent[32];
	sceClibMemset(userAgent, 0, sizeof(userAgent));
	sceClibSnprintf(userAgent, sizeof(userAgent), "NpOAuthVsh/%s", fwVersion);

	int httpTemplateId = sceHttpCreateTemplate(userAgent, SCE_HTTP_VERSION_1_1, SCE_FALSE);
	if (httpTemplateId < 0)
		return httpTemplateId;

	NpOAuthContext *ctx = &g_npOAuthContext;
	ctx->allocator = allocator;
	ctx->httpTemplateId = httpTemplateId;
	ctx->clientId = kNpOAuthVshClientId;

	char npEnv[20];
	sceNpCommonGetNpEnviroment(npEnv);
	if (sceClibStrncmp(npEnv, "np", 16U) != 0 &&
	    sceClibStrncmp(npEnv, "sp-int", 16U) != 0 &&
	    sceClibStrncmp(npEnv, "prod-qa", 16U) != 0 &&
	    sceClibStrncmp(npEnv, "mgmt", 16U) != 0) {
		ctx->clientSecret = kNpOAuthVshDevSecret;
	} else {
		ctx->clientSecret = kNpOAuthVshProdSecret;
	}

	int enableRet = sceHttpsEnableOptionPrivate(ctx->httpTemplateId, NP_OAUTH_HTTPS_ENABLE_OPTS);
	ret = enableRet | sceHttpsDisableOptionPrivate(ctx->httpTemplateId,
						       NP_OAUTH_HTTPS_DISABLE_OPTS);
	if (ret < 0)
		return ret;

	SceKernelLwMutexWork *mutex = (SceKernelLwMutexWork *)ctx->allocator->allocate(
		sizeof(SceKernelLwMutexWork), ctx->allocator->context);
	ctx->mutex = mutex;
	if (mutex == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	ret = sceKernelCreateLwMutex(mutex, "SceNpOAuthVshMutex", SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	if (ret >= 0) {
		int netCbId = npAccountRegisterNetCallback(npOAuthOnSignOutCallback, NULL);
		ret = netCbId;
		if (netCbId >= 0) {
			ctx->netCallbackId = netCbId;
			int stateCbId = npAccountRegisterCallback(npOAuthOnStateChangeCallback, NULL);
			ret = stateCbId;
			if (stateCbId >= 0) {
				ctx->accountCallbackId = stateCbId;
				return 0;
			}
			npAccountUnregisterNetCallback(ctx->netCallbackId);
		}
		if (ctx->mutex != NULL)
			sceKernelDeleteLwMutex(ctx->mutex);
	}

	if (ctx->mutex != NULL) {
		ctx->allocator->deallocate(ctx->mutex, ctx->allocator->context);
		ctx->mutex = NULL;
	}
	return ret;
}

int npOAuthCreateRequest(int *outReqHandle)
{
	NpOAuthRequest *created = NULL;
	int ret = npOAuthRequestCreate(&created);
	if (ret >= 0) {
		NpOAuthContext *ctx = &g_npOAuthContext;
		sceKernelLockLwMutex(ctx->mutex, 1, NULL);
		SceUInt32 reqId = ctx->nextRequestId + 1U;
		ctx->nextRequestId = reqId;
		if (reqId > NP_OAUTH_MAX_REQUEST_ID) {
			reqId = 1U;
			ctx->nextRequestId = 1U;
		}
		created->requestId = reqId;
		if (ctx->requestListHead != NULL)
			created->next = ctx->requestListHead;
		ctx->requestListHead = created;
		sceKernelUnlockLwMutex(ctx->mutex, 1);
		*outReqHandle = (int)created;
	}
	return ret;
}

int npOAuthDestroyRequest(int reqHandle)
{
	NpOAuthRequest *request = (NpOAuthRequest *)reqHandle;
	NpOAuthContext *ctx = &g_npOAuthContext;

	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	NpOAuthRequest *prev = NULL;
	NpOAuthRequest *cur = ctx->requestListHead;
	while (cur != NULL) {
		if (cur->requestId == request->requestId && cur == request)
			break;
		prev = cur;
		cur = cur->next;
	}
	if (cur != NULL) {
		if (prev != NULL)
			prev->next = cur->next;
		else
			ctx->requestListHead = cur->next;
	}
	sceKernelUnlockLwMutex(ctx->mutex, 1);

	return npOAuthRequestDestroy(request);
}

int npOAuthAbortRequest(int reqHandle)
{
	return npOAuthRequestAbort((NpOAuthRequest *)reqHandle);
}

static int npOAuthEnsureAccountTicket(NpOAuthRequest *request)
{
	if (!npAccountIsSignedIn())
		return NP_OAUTH_ERR_SIGNED_OUT;

	NpOAuthRequest *acquired = NULL;
	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret < 0)
		return ret;

	int ticketReqId = 0;
	SceSize ticketSize = 0;
	int created = npAccountCreateRequest();
	ret = created;
	if (created >= 0) {
		ticketReqId = created;
		ret = npOAuthRequestSetTicketReqId(request, ticketReqId);
		if (ret >= 0)
			ret = npAccountGetTicket(ticketReqId, NULL, &ticketSize);
	}
	npOAuthRequestClearTicketReqId(request);
	if (ticketReqId != 0)
		npAccountDestroyRequest(ticketReqId);
	npOAuthRequestRelease(request);
	return ret;
}

static int npOAuthFetchConsoleToken(NpOAuthRequest *request, char *outConsoleToken)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpCslaCreateRequestOption cslaOpt;
	cslaOpt.allocator = ctx->allocator;

	NpOAuthRequest *acquired = NULL;
	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret >= 0) {
		int cslaReqId = 0;
		int created = npCslaCreateRequest(&cslaOpt);
		ret = created;
		if (created >= 0) {
			cslaReqId = created;
			ret = npOAuthRequestSetCslaReqId(request, cslaReqId);
			if (ret >= 0)
				ret = npCslaGetConsoleToken(cslaReqId, outConsoleToken, 0);
		}
		npOAuthRequestClearCslaReqId(request);
		if (cslaReqId != 0)
			npCslaDestroyRequest(cslaReqId);
		npOAuthRequestRelease(request);
	}
	npOAuthHandleFatalError(ret);
	return ret;
}

static int npOAuthBuildUrl(char *outUrl, SceSize outUrlSize, const char *scheme, const char *path,
			   const char *query)
{
	char host[128];
	char npEnv[20];
	sceClibMemset(host, 0, sizeof(host));
	sceClibMemset(npEnv, 0, 17U);

	int ret = sceNpCommonGetNpEnviroment(npEnv);
	if (ret >= 0) {
		int written = sceClibSnprintf(host, sizeof(host) - 1U, "%s%s%s", "auth.api.", npEnv,
					      ".ac.playstation.net");
		ret = (written >= 0) ? written : NP_OAUTH_ERR_INTERNAL;
	}
	if (ret < 0)
		return ret;

	SceSize curLen = sceClibStrnlen(outUrl, 0x7FFFFFFFU);
	SceSize schemeLen = sceClibStrnlen(scheme, 0x7FFFFFFFU);
	if (curLen + schemeLen >= outUrlSize)
		return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	sceClibStrncat(outUrl, scheme, schemeLen);

	curLen = sceClibStrnlen(outUrl, 0x7FFFFFFFU);
	SceSize hostLen = sceClibStrnlen(host, 0x7FFFFFFFU);
	if (curLen + hostLen >= outUrlSize)
		return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	sceClibStrncat(outUrl, host, hostLen);

	curLen = sceClibStrnlen(outUrl, 0x7FFFFFFFU);
	SceSize pathLen = sceClibStrnlen(path, 0x7FFFFFFFU);
	if (curLen + pathLen >= outUrlSize)
		return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	sceClibStrncat(outUrl, path, pathLen);

	if (query != NULL) {
		curLen = sceClibStrnlen(outUrl, 0x7FFFFFFFU);
		SceSize sepLen = sceClibStrnlen("?", 0x7FFFFFFFU);
		if (curLen + sepLen >= outUrlSize)
			return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
		sceClibStrncat(outUrl, "?", sepLen);

		curLen = sceClibStrnlen(outUrl, 0x7FFFFFFFU);
		SceSize queryLen = sceClibStrnlen(query, 0x7FFFFFFFU);
		if (curLen + queryLen >= outUrlSize)
			return NP_OAUTH_ERR_BUFFER_TOO_SMALL;
		sceClibStrncat(outUrl, query, queryLen);
	}
	return ret;
}

/* 0x813A794A */
static int npOAuthBuildPasswordTokenBody(NpOAuthRequest *request, const char *clientId,
					 const char *clientSecret, const char *username,
					 const char *password, char *outBody, int outBodySize)
{
	NpOAuthAllocator *allocator = g_npOAuthContext.allocator;
	if (request == NULL || username == NULL || password == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	char *escapeBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE, allocator->context);
	if (escapeBuf == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(escapeBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
	int remaining = outBodySize - 1;
	npOAuthAppendFormPair(outBody, &remaining, "grant_type", "password", true);

	int ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "client_id",
					       clientId, true);
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "client_secret",
						   clientSecret, true);
	}
	if (ret >= 0) {
		npOAuthAppendFormPair(outBody, &remaining, "scope", "openid", true);
		ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "username",
						   username, true);
	}
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "password",
						   password, false);
		if (ret >= 0 && remaining <= 0)
			ret = NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	}

	allocator->deallocate(escapeBuf, allocator->context);
	return ret;
}

static int npOAuthCheckNotAborted(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	NpOAuthRequest *acquired = NULL;
	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret >= 0) {
		sceKernelLockLwMutex(acquired->mutex, 1, NULL);
		if (acquired->aborted != 0)
			ret = NP_OAUTH_ERR_ABORTED;
		sceKernelUnlockLwMutex(acquired->mutex, 1);
	}
	if (acquired != NULL)
		npOAuthRequestRelease(request);
	return ret;
}

static int npOAuthSendHttpRequest(NpOAuthRequest *request, NpOAuthRequest *acquired, int method,
				  const char *url, const void *postBody, SceSize postBodyLen,
				  int headerCount, const char **headerNames,
				  const char **headerValues, SceBool keepAlive, int *outHttpReqId,
				  int *outConnId, int *outStatusCode, SceUInt64 *outContentLength,
				  int *outIsChunked)
{
	if (request == NULL || acquired == NULL || url == NULL || outStatusCode == NULL ||
	    outContentLength == NULL || outIsChunked == NULL) {
		return NP_OAUTH_ERR_COMMON_INVALID_ARG;
	}

	int connId = sceHttpCreateConnectionWithURL(g_npOAuthContext.httpTemplateId, url, keepAlive);
	if (connId <= 0)
		return connId;

	*outConnId = connId;
	int httpReqId = sceHttpCreateRequestWithURL(connId, method, url, (SceUInt64)postBodyLen);
	if (httpReqId <= 0)
		return httpReqId;

	*outHttpReqId = httpReqId;
	int ret = npOAuthRequestSetHttpReqId(request, httpReqId);
	if (ret < 0)
		return ret;

	ret = npOAuthCheckNotAborted(request);
	if (ret < 0)
		return ret;

	ret = sceHttpSetAutoRedirect(*outHttpReqId, SCE_FALSE);
	if (ret < 0)
		return ret;

	for (int i = 0; i < headerCount; ++i) {
		if (headerNames[i] != NULL && headerValues[i] != NULL) {
			ret = sceHttpAddRequestHeader(*outHttpReqId, headerNames[i], headerValues[i],
						      SCE_HTTP_HEADER_OVERWRITE);
			if (ret < 0)
				return ret;
		}
	}

	if (postBody != NULL) {
		ret = sceHttpAddRequestHeader(*outHttpReqId, "Content-Type",
					      "application/x-www-form-urlencoded",
					      SCE_HTTP_HEADER_OVERWRITE);
		if (ret < 0)
			return ret;
	}

	ret = sceHttpSendRequest(*outHttpReqId, postBody, postBodyLen);
	if (ret != NP_OAUTH_HTTP_ERR_PARSE_NOT_FOUND && ret < 0) {
		if (ret == NP_OAUTH_HTTP_ERR_SSL) {
			int sslErrNum = 0;
			unsigned int sslDetail = 0;
			sceHttpsGetSslError(*outHttpReqId, &sslErrNum, &sslDetail);
		}
		return ret;
	}

	ret = sceHttpGetStatusCode(*outHttpReqId, outStatusCode);
	if (ret < 0)
		return ret;

	ret = sceHttpGetResponseContentLength(*outHttpReqId, outContentLength);
	if (ret == 0) {
		*outIsChunked = 0;
		return 0;
	}
	if (ret == NP_OAUTH_HTTP_ERR_NO_CONTENT_LENGTH || ret == NP_OAUTH_HTTP_ERR_CHUNK_ENC) {
		*outIsChunked = 1;
		return 0;
	}
	*outIsChunked = 1;
	return ret;
}

static int npOAuthRequestNpssoToken(NpOAuthRequest *request, const char *consoleToken)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthAllocator *allocator = ctx->allocator;

	NpOAuthRequest *acquired = NULL;
	int httpReqId = 0;
	int connId = 0;
	int statusCode = 0;
	int isChunked = 0;
	SceUInt64 contentLength = 0;

	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret >= 0) {
		char *urlBuf = (char *)allocator->allocate(NP_OAUTH_URL_BUF_SIZE,
							   allocator->context);
		if (urlBuf != NULL) {
			sceClibMemset(urlBuf, 0, NP_OAUTH_URL_BUF_SIZE);
			ret = npOAuthBuildUrl(urlBuf, NP_OAUTH_URL_BUF_SIZE, "https://",
					      "/2.0/oauth/token", NULL);
			if (ret >= 0) {
				char *bodyBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE,
									    allocator->context);
				if (bodyBuf != NULL) {
					sceClibMemset(bodyBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
					SceUInt64 accountId = 0;
					char loginId[72];
					char password[36];
					ret = npManagerGetCachedCredentials(loginId, password,
									    &accountId);
					if (ret >= 0) {
						ret = npOAuthBuildPasswordTokenBody(
							acquired, ctx->clientId, ctx->clientSecret,
							loginId, password, bodyBuf,
							(int)NP_OAUTH_BODY_BUF_SIZE);
					}
					if (ret >= 0) {
						SceSize bodyLen = sceClibStrnlen(bodyBuf,
										 0x7FFFFFFFU);
						const char *headerName = "X-NP-CONSOLE-TOKEN";
						const char *headerValue = consoleToken;
						ret = npOAuthSendHttpRequest(
							request, acquired, SCE_HTTP_METHOD_POST,
							urlBuf, bodyBuf, bodyLen, 1, &headerName,
							&headerValue, SCE_FALSE, &httpReqId,
							&connId, &statusCode, &contentLength,
							&isChunked);
					}
					if (ret >= 0) {
						NpOAuthNpssoResponse npssoResp;
						sceClibMemset(&npssoResp, 0, sizeof(npssoResp));
						ret = npOAuthReadNpssoResponse(
							acquired, statusCode, isChunked,
							contentLength, &npssoResp);
						if (ret >= 0) {
							sceKernelLockLwMutex(ctx->mutex, 1, NULL);
							sceClibMemcpy_safe(ctx->npssoToken,
								      npssoResp.npssoToken,
								      sizeof(ctx->npssoToken));
							ctx->hasNpssoToken = 1;
							sceKernelUnlockLwMutex(ctx->mutex, 1);

							sceKernelLockLwMutex(ctx->mutex, 1, NULL);
							sceClibMemcpy_safe(&ctx->issuerId,
								      &npssoResp.issuerId,
								      sizeof(ctx->issuerId));
							sceKernelUnlockLwMutex(ctx->mutex, 1);
						}
					}
					allocator->deallocate(bodyBuf, allocator->context);
				} else {
					ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
				}
			}
			allocator->deallocate(urlBuf, allocator->context);
		} else {
			ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
		}
	}

	npOAuthRequestClearHttpReqId(request);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (connId > 0)
		sceHttpDeleteConnection(connId);
	if (acquired != NULL) {
		npOAuthRequestRelease(request);
		acquired = NULL;
	}
	if (npOAuthCheckNotAborted(request) == NP_OAUTH_ERR_ABORTED)
		return NP_OAUTH_ERR_ABORTED;
	return ret;
}

/* 0x813A82F6 */
static int npOAuthBuildAuthorizeQuery(const char *clientId, const char *redirectUri,
				      const char *scope, char *outQuery, int outQuerySize)
{
	NpOAuthAllocator *allocator = g_npOAuthContext.allocator;
	if (redirectUri == NULL || scope == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	char *escapeBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE, allocator->context);
	if (escapeBuf == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	int remaining = outQuerySize - 1;
	int ret = npOAuthAppendEscapedFormPair(outQuery, &remaining, escapeBuf, "client_id",
					       clientId, true);
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outQuery, &remaining, escapeBuf, "redirect_uri",
						   redirectUri, true);
	}
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outQuery, &remaining, escapeBuf, "scope", scope,
						   true);
	}
	if (ret >= 0) {
		npOAuthAppendFormPair(outQuery, &remaining, "response_type", "code", true);
		npOAuthAppendFormPair(outQuery, &remaining, "prompt", "none", false);
		if (remaining <= 0)
			ret = NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	}

	allocator->deallocate(escapeBuf, allocator->context);
	return ret;
}

static int npOAuthRequestAuthCode(NpOAuthRequest *request, const char *consoleToken,
				  const char *clientId, const char *redirectUri, const char *scope,
				  SceNpAuthorizationCode *outCode)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthAllocator *allocator = ctx->allocator;

	char npssoCopy[NP_OAUTH_NPSSO_TOKEN_SIZE];
	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	sceClibMemcpy_safe(npssoCopy, ctx->npssoToken, sizeof(npssoCopy));
	sceKernelUnlockLwMutex(ctx->mutex, 1);

	char cookieHeader[NP_OAUTH_NPSSO_TOKEN_SIZE];
	sceClibMemcpy_safe(cookieHeader, npssoCopy, sizeof(cookieHeader));

	NpOAuthRequest *acquired = NULL;
	int httpReqId = 0;
	int connId = 0;
	int statusCode = 0;
	int isChunked = 0;
	SceUInt64 contentLength = 0;

	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret >= 0) {
		char *urlBuf = (char *)allocator->allocate(NP_OAUTH_URL_BUF_SIZE,
							   allocator->context);
		if (urlBuf != NULL) {
			sceClibMemset(urlBuf, 0, NP_OAUTH_URL_BUF_SIZE);
			char *queryBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE,
								     allocator->context);
			if (queryBuf != NULL) {
				sceClibMemset(queryBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
				ret = npOAuthBuildAuthorizeQuery(clientId, redirectUri, scope,
								 queryBuf,
								 (int)NP_OAUTH_BODY_BUF_SIZE);
				if (ret >= 0) {
					ret = npOAuthBuildUrl(urlBuf, NP_OAUTH_URL_BUF_SIZE,
							      "https://", "/2.0/oauth/authorize",
							      queryBuf);
				}
				allocator->deallocate(queryBuf, allocator->context);
			} else {
				ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
			}

			if (ret >= 0) {
				const char *headerNames[2];
				const char *headerValues[2];
				headerNames[0] = "Cookie";
				headerValues[0] = cookieHeader;
				headerNames[1] = "X-NP-CONSOLE-TOKEN";
				headerValues[1] = consoleToken;
				ret = npOAuthSendHttpRequest(request, acquired, SCE_HTTP_METHOD_GET,
							     urlBuf, NULL, 0, 2, headerNames,
							     headerValues, SCE_FALSE, &httpReqId,
							     &connId, &statusCode, &contentLength,
							     &isChunked);
			}

			if (ret >= 0) {
				NpOAuthAuthCodeResponse codeResp;
				sceClibMemset(&codeResp, 0, sizeof(codeResp));
				ret = npOAuthReadAuthCodeResponse(acquired, statusCode, isChunked,
								  contentLength, &codeResp);
				if (ret >= 0)
					*outCode = codeResp.authCode;
			}
			allocator->deallocate(urlBuf, allocator->context);
		} else {
			ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
		}
	}

	npOAuthRequestClearHttpReqId(request);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (connId > 0)
		sceHttpDeleteConnection(connId);
	if (acquired != NULL) {
		npOAuthRequestRelease(request);
		acquired = NULL;
	}
	if (npOAuthCheckNotAborted(request) == NP_OAUTH_ERR_ABORTED)
		return NP_OAUTH_ERR_ABORTED;
	return ret;
}

/* Shared inline body of npOAuthGetAuthorizationCode (0x813A89E4) and
 * npOAuthGetAuthorizationCodeWithRedirect (0x813A96E6). */
static inline int npOAuthExecuteAuthCodeGrant(NpOAuthRequest *request, const char *clientId,
					      const char *redirectUri, const char *scope,
					      SceNpAuthorizationCode *outCode, int *outIssuerId)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	char consoleToken[NP_OAUTH_CONSOLE_TOKEN_BUF_SIZE];
	sceClibMemset(consoleToken, 0, sizeof(consoleToken));

	if (request == NULL || clientId == NULL || outCode == NULL || outIssuerId == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	NpOAuthRequest *acquired = NULL;
	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret < 0)
		return ret;

	{
		NpOAuthScopedLwMutexLock lock(request);
		if (!npOAuthRequestIsInUse(request)) {
			npOAuthRequestMarkInUse(request);
			ret = 0;
		} else {
			ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
		}
	}

	if (ret >= 0)
		ret = npOAuthEnsureAccountTicket(request);
	if (ret >= 0)
		ret = npOAuthFetchConsoleToken(request, consoleToken);
	if (ret >= 0) {
		sceKernelLockLwMutex(ctx->mutex, 1, NULL);
		SceUInt8 hasNpsso = ctx->hasNpssoToken;
		sceKernelUnlockLwMutex(ctx->mutex, 1);

		if (hasNpsso == 0)
			ret = npOAuthRequestNpssoToken(request, consoleToken);
	}
	if (ret >= 0) {
		ret = npOAuthRequestAuthCode(request, consoleToken, clientId, redirectUri, scope,
					     outCode);
		if (ret == NP_OAUTH_SERVER_ERR_INVALID_NPSSO) {
			npOAuthClearNpssoCacheLocked();
			ret = npOAuthRequestNpssoToken(request, consoleToken);
			if (ret >= 0) {
				ret = npOAuthRequestAuthCode(request, consoleToken, clientId,
							     redirectUri, scope, outCode);
			}
		}
		if (ret >= 0) {
			sceKernelLockLwMutex(ctx->mutex, 1, NULL);
			*outIssuerId = ctx->issuerId;
			sceKernelUnlockLwMutex(ctx->mutex, 1);
			if (!npAccountIsSignedIn())
				ret = NP_OAUTH_ERR_SIGNED_OUT;
		}
	}

	npOAuthRequestRelease(request);
	return ret;
}

/* 0x813A89E4 */
int npOAuthGetAuthorizationCode(int reqHandle, const NpOAuthAuthCodeRequest *req,
				SceNpAuthorizationCode *outCode, int *outIssuerId)
{
	if (req == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;
	return npOAuthExecuteAuthCodeGrant((NpOAuthRequest *)reqHandle, req->clientId.id,
					   kNpOAuthDefaultRedirectUri, req->scope, outCode,
					   outIssuerId);
}

static int npOAuthBuildAuthCodeTokenBody(NpOAuthRequest *request, const char *authCode,
					 char *outBody, int outBodySize)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthAllocator *allocator = ctx->allocator;
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	char *escapeBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE, allocator->context);
	if (escapeBuf == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(escapeBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
	int remaining = outBodySize - 1;
	npOAuthAppendFormPair(outBody, &remaining, "grant_type", "authorization_code", true);

	int ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "client_id",
					       ctx->clientId, true);
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "client_secret",
						   ctx->clientSecret, true);
	}
	if (ret >= 0) {
		ret = npOAuthAppendEscapedFormPair(outBody, &remaining, escapeBuf, "redirect_uri",
						   kNpOAuthVshRedirectUri, true);
	}
	if (ret >= 0) {
		npOAuthAppendFormPair(outBody, &remaining, "code", authCode, false);
		if (remaining <= 0)
			ret = NP_OAUTH_ERR_BUFFER_TOO_SMALL;
	}

	allocator->deallocate(escapeBuf, allocator->context);
	return ret;
}

static bool npOAuthCopyValidCachedTokenLocked(void *outToken)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	SceRtcTick nowTick;
	bool needRefresh = (sceRtcGetCurrentNetworkTick(&nowTick) < 0);

	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	SceUInt64 expireTick = ctx->cachedAccessToken.expireTick.tick;
	if (expireTick != 0 && expireTick >= nowTick.tick &&
	    (expireTick - nowTick.tick) >= NP_OAUTH_ACCESS_TOKEN_MIN_REMAINING_US) {
		if (outToken != NULL)
			sceClibMemcpy_safe(outToken, &ctx->cachedAccessToken,
				      sizeof(ctx->cachedAccessToken));
	} else {
		needRefresh = true;
	}
	sceKernelUnlockLwMutex(ctx->mutex, 1);
	return !needRefresh;
}

static int npOAuthExchangeCodeForAccessToken(NpOAuthRequest *request, const char *authCode,
					     void *outToken)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthAllocator *allocator = ctx->allocator;

	NpOAuthRequest *acquired = NULL;
	int httpReqId = 0;
	int connId = 0;
	int statusCode = 0;
	int isChunked = 0;
	SceUInt64 contentLength = 0;

	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret >= 0) {
		char *urlBuf = (char *)allocator->allocate(NP_OAUTH_URL_BUF_SIZE,
							   allocator->context);
		if (urlBuf != NULL) {
			sceClibMemset(urlBuf, 0, NP_OAUTH_URL_BUF_SIZE);
			ret = npOAuthBuildUrl(urlBuf, NP_OAUTH_URL_BUF_SIZE, "https://",
					      "/2.0/oauth/token", NULL);
			if (ret >= 0) {
				char *bodyBuf = (char *)allocator->allocate(NP_OAUTH_BODY_BUF_SIZE,
									    allocator->context);
				if (bodyBuf != NULL) {
					sceClibMemset(bodyBuf, 0, NP_OAUTH_BODY_BUF_SIZE);
					ret = npOAuthBuildAuthCodeTokenBody(
						acquired, authCode, bodyBuf,
						(int)NP_OAUTH_BODY_BUF_SIZE);
					if (ret >= 0) {
						SceSize bodyLen = sceClibStrnlen(bodyBuf,
										 0x7FFFFFFFU);
						ret = npOAuthSendHttpRequest(
							request, acquired, SCE_HTTP_METHOD_POST,
							urlBuf, bodyBuf, bodyLen, 0, NULL, NULL,
							SCE_FALSE, &httpReqId, &connId, &statusCode,
							&contentLength, &isChunked);
					}
					if (ret >= 0) {
						NpOAuthAccessTokenResponse tokenResp;
						sceClibMemset(&tokenResp, 0, sizeof(tokenResp));
						ret = npOAuthParseAccessTokenResponse(
							acquired, statusCode, isChunked,
							contentLength, &tokenResp);
						if (ret >= 0) {
							sceKernelLockLwMutex(ctx->mutex, 1, NULL);
							sceClibMemcpy_safe(&ctx->cachedAccessToken,
								      &tokenResp.token,
								      sizeof(ctx->cachedAccessToken));
							sceKernelUnlockLwMutex(ctx->mutex, 1);

							sceClibMemcpy_safe(outToken, &tokenResp.token,
								      sizeof(tokenResp.token));
						}
					}
					allocator->deallocate(bodyBuf, allocator->context);
				} else {
					ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
				}
			}
			allocator->deallocate(urlBuf, allocator->context);
		} else {
			ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
		}
	}

	npOAuthRequestClearHttpReqId(request);
	if (httpReqId > 0)
		sceHttpDeleteRequest(httpReqId);
	if (connId > 0)
		sceHttpDeleteConnection(connId);
	if (acquired != NULL) {
		npOAuthRequestRelease(request);
		acquired = NULL;
	}
	if (npOAuthCheckNotAborted(request) == NP_OAUTH_ERR_ABORTED)
		ret = NP_OAUTH_ERR_ABORTED;
	return ret;
}

int npOAuthGetAccessToken(int reqHandle, void *outToken)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthRequest *request = (NpOAuthRequest *)reqHandle;

	char consoleToken[NP_OAUTH_CONSOLE_TOKEN_BUF_SIZE];
	SceNpAuthorizationCode authCode;
	sceClibMemset(consoleToken, 0, sizeof(consoleToken));
	sceClibMemset(&authCode, 0, sizeof(authCode));

	if (request == NULL || outToken == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	NpOAuthRequest *acquired = NULL;
	int ret = npOAuthFindAndAcquireRequest(request, &acquired);
	if (ret < 0)
		return ret;

	{
		NpOAuthScopedLwMutexLock lock(request);
		if (!npOAuthRequestIsInUse(request)) {
			npOAuthRequestMarkInUse(request);
			ret = 0;
		} else {
			ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
		}
	}

	if (ret >= 0)
		ret = npOAuthEnsureAccountTicket(request);

	if (ret >= 0) {
		if (!npOAuthCopyValidCachedTokenLocked(outToken)) {
			ret = npOAuthFetchConsoleToken(request, consoleToken);
			if (ret >= 0) {
				sceKernelLockLwMutex(ctx->mutex, 1, NULL);
				SceUInt8 hasNpsso = ctx->hasNpssoToken;
				sceKernelUnlockLwMutex(ctx->mutex, 1);

				if (hasNpsso == 0)
					ret = npOAuthRequestNpssoToken(request, consoleToken);
			}
			if (ret >= 0) {
				ret = npOAuthRequestAuthCode(request, consoleToken,
							     kNpOAuthVshClientId,
							     kNpOAuthVshRedirectUri,
							     kNpOAuthVshFullScope, &authCode);
				if (ret == NP_OAUTH_SERVER_ERR_INVALID_NPSSO) {
					npOAuthClearNpssoCacheLocked();
					ret = npOAuthRequestNpssoToken(request, consoleToken);
					if (ret >= 0) {
						ret = npOAuthRequestAuthCode(
							request, consoleToken, kNpOAuthVshClientId,
							kNpOAuthVshRedirectUri, "openid", &authCode);
					}
				}
			}
			if (ret >= 0)
				ret = npOAuthExchangeCodeForAccessToken(request, authCode.code,
									outToken);
		}
		if (ret >= 0 && !npAccountIsSignedIn())
			ret = NP_OAUTH_ERR_SIGNED_OUT;
	}

	npOAuthRequestRelease(request);
	return ret;
}

int npOAuthClearAccessTokenCache(void)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	sceClibMemset(&ctx->cachedAccessToken, 0, sizeof(ctx->cachedAccessToken));
	return sceKernelUnlockLwMutex(ctx->mutex, 1);
}

int npOAuthGetCachedAccessToken(void *outToken, bool *outValid)
{
	NpOAuthContext *ctx = &g_npOAuthContext;
	NpOAuthAccessToken zeroToken;
	sceClibMemset(&zeroToken, 0, sizeof(zeroToken));

	sceKernelLockLwMutex(ctx->mutex, 1, NULL);
	if (sceClibMemcmp(&ctx->cachedAccessToken, &zeroToken, sizeof(zeroToken)) != 0)
		*outValid = npOAuthCopyValidCachedTokenLocked(outToken);
	else
		*outValid = false;
	sceKernelUnlockLwMutex(ctx->mutex, 1);
	return 0;
}

int npOAuthGetAuthorizationCodeWithRedirect(int reqHandle,
					    const NpOAuthAuthCodeWithRedirectRequest *req,
					    SceNpAuthorizationCode *outCode, int *outIssuerId)
{
	if (req == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;
	return npOAuthExecuteAuthCodeGrant((NpOAuthRequest *)reqHandle, req->clientId.id,
					   req->redirectUri, req->scope, outCode, outIssuerId);
}

void npOAuthOnSignOutCallback(int eventType, void *userArg)
{
	(void)userArg;
	if (eventType == 1) {
		npOAuthClearAccessTokenCache();
		npOAuthClearNpssoCacheLocked();
		npCslaClearCache();
	}
}

int npOAuthOnStateChangeCallback(int eventType, IPMI::Session *session)
{
	(void)session;
	if (eventType == 1) {
		npOAuthClearAccessTokenCache();
		npOAuthClearNpssoCacheLocked();
	}
	return 0;
}
