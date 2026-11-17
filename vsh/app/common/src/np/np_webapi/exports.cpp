/*
 * SceNpWebApi: the exports. Each finds its library context or request by the IDs it was given and
 * hands over to the library's own functions.
 */

#include "np_webapi_internal.h"

extern "C" {

int sceNpWebApiSetNpTitleId(const SceNpTitleId *pTitleId, const SceNpTitleSecret *pTitleSecret)
{
	int result;
	int pid;
	char param[4];

	if (pTitleId == NULL || pTitleSecret == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	sceClibMemset(param, 0, 4);
	pid = sceKernelGetProcessId();
	if (pid < 0)
		return pid;
	sceAppMgrAppParamGetString(pid, 8, param, 4);
	if (sceClibStrncmp("gda", param, sceClibStrnlen("gda", 0xFFFFFFFF)) == 0)
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	if (npWebApiIsShellProcess())
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	result = SceNpCommonPs4_4E897A97(pTitleId, pTitleSecret);
	if (result >= 0 && sceAppMgrIsDevelopmentMode() != 0 && SceNpCommonPs4_64C85FD5() == 0)
		result = npWebApiIpcSetNpTitleId(SceNpCommonPs4_A0599F24(), pTitleId);
	return result;
}

int SceNpWebApi_4699EF17(const void *a1, const void *a2)
{
	if (a1 != NULL && a2 != NULL)
		return SceNpCommonPs4_9AFE6E37(a1, a2);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiGetNpTitleId(SceNpTitleId *pTitleId, SceNpTitleSecret *pTitleSecret)
{
	if (pTitleId != NULL && pTitleSecret != NULL)
		return SceNpCommonPs4_6AC5EF70(pTitleId, pTitleSecret);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiUtilityParseNpId(const char *pNpIdStr, SceNpId *pNpId)
{
	char str[64];
	char name[20];
	char dot[8];
	char slash[8];
	char tail[8];
	SceNpId id;
	int result;
	int hasSlash;
	SceSize start;
	SceSize i;
	SceSize len;
	const char *p;

	if (pNpIdStr == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	sceClibMemset(str, 0, sizeof(str));
	result = SceNpCommonPs4_D082556B(pNpIdStr, sceClibStrnlen(pNpIdStr, 0xFFFFFFFF), str, sizeof(str));
	if (result < 0)
		return result;
	hasSlash = 0;
	sceClibMemset(name, 0, sizeof(name));
	sceClibMemset(dot, 0, sizeof(dot));
	sceClibMemset(slash, 0, sizeof(slash));
	sceClibMemset(tail, 0, sizeof(tail));
	start = 0;
	i = 0;
	p = str;
	while (i <= sceClibStrnlen(str, 0xFFFFFFFF)) {
		len = i - start;
		if (*p == '@') {
			sceClibMemcpy_safe(name, &str[start], len <= sizeof(name) ? len : sizeof(name));
			start = i + 1;
		} else if (*p == '.') {
			sceClibMemcpy_safe(dot, &str[start], len <= sizeof(dot) ? len : sizeof(dot));
			start = i + 1;
		} else if (*p == '/') {
			if (slash != NULL)
				sceClibMemcpy_safe(slash, &str[start], len <= sizeof(slash) ? len : sizeof(slash));
			hasSlash = 1;
			start = i + 1;
		} else if (*p == 0) {
			if (hasSlash) {
				if (tail != NULL)
					sceClibMemcpy_safe(tail, &str[start], len <= sizeof(tail) ? len : sizeof(tail));
			} else if (slash != NULL) {
				sceClibMemcpy_safe(slash, &str[start], len <= sizeof(slash) ? len : sizeof(slash));
			}
		}
		p++;
		i++;
	}
	sceClibMemset(&id, 0, sizeof(id));
	sceClibMemcpy_safe(&id, name, sceClibStrnlen(name, 0xFFFFFFFF));
	sceClibSnprintf((char *)id.opt, sizeof(id.opt), "%s%s%s", dot, slash, tail);
	if (pNpId != NULL) {
		id.reserved[0] = 1;
		sceClibMemcpy_safe(pNpId, &id, sizeof(id));
	}
	return result;
}

int SceNpWebApi_C42F1E67(int libCtxId)
{
	NpWebApiLibContext *libCtx;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx == NULL)
		return NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	npWebApiLibContextSetAborted(libCtx);
	npWebApiLibContextRelease(libCtx);
	return 0;
}

int SceNpWebApi_B280EA3D(int libCtxId)
{
	NpWebApiLibContext *libCtx;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx == NULL)
		return NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (npWebApiLibContextHasOtherUsers(libCtx))
		return NP_WEBAPI_ERROR_ABORT_NOT_ALLOWED;
	npWebApiLibContextClearAborted(libCtx);
	npWebApiLibContextRelease(libCtx);
	return 0;
}

int SceNpWebApi_07913970(int libCtxId, const char *pServerName)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		result = npWebApiResolveUserContext(libCtxId, 0);
		if (result >= 0) {
			userCtx = npWebApiLibContextFindUserContext(libCtx, result);
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
			if (userCtx != NULL)
				result = npWebApiConnectionPoolDeleteConnection(
					(NpWebApiConnectionPool *)npWebApiUserContextRequests(userCtx), pServerName);
			if (userCtx != NULL)
				npWebApiUserContextRelease(userCtx);
		}
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}

int SceNpWebApi_CB51F139(SceInt64 requestId, SceULong64 *pContentLength)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned int libCtxId;
	int userCtxId;
	int result;

	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	if (pContentLength == NULL || requestId == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			if (request != NULL)
				result = npWebApiRequestGetContentLength(request, pContentLength);
			if (request != NULL)
				npWebApiRequestRelease(request);
		} else {
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		}
		if (userCtx != NULL)
			npWebApiUserContextRelease(userCtx);
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}

int SceNpWebApi_4D7CC67C(int a1, void *a2)
{
	if (a2 != NULL)
		return SceNpCommonPs4_D0F55105(a1, a2);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiInitialize(SceSize poolSize)
{
	char param[4];
	int result;
	int id;

	sceClibMemset(param, 0, 4);
	result = sceKernelGetProcessId();
	if (result < 0)
		return result;
	sceAppMgrAppParamGetString(result, 8, param, 4);
	if (sceClibStrncmp("gda", param, sceClibStrnlen("gda", 0xFFFFFFFF)) == 0)
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	if (npWebApiIsShellProcess())
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	if (g_initState == 0)
		g_initState = 1;
	if (g_defaultLibCtxId == 0) {
		result = npWebApiNewLibContext(poolSize, NULL, NULL, 0);
		if (result >= 0) {
			g_defaultLibCtxId = result;
			id = result;
			result = npWebApiCreateUserContextObject(id, NULL, 0);
			if (result >= 0) {
				g_initCount++;
				return 0;
			}
			if (id != 0) {
				npWebApiDeleteLibContext(id);
				if (id == g_defaultLibCtxId)
					g_defaultLibCtxId = 0;
			}
		}
	} else {
		result = NP_WEBAPI_ERROR_ALREADY_INITIALIZED;
	}
	if (g_initCount == 0 && g_initState != 0)
		g_initState = 0;
	return result;
}

int SceNpWebApi_A11FBA1A(SceSize poolSize, void *pool, const char *name)
{
	int result;
	int id;

	if (poolSize == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	if (npWebApiIsShellProcess())
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	if (g_initState == 0)
		g_initState = 1;
	result = npWebApiNewLibContext(poolSize, pool, name, 1);
	if (result >= 0) {
		id = result;
		result = npWebApiCreateUserContextObject(id, NULL, 0);
		if (result >= 0) {
			g_initCount++;
			return id;
		}
		if (id != 0) {
			npWebApiDeleteLibContext(id);
			if (id == g_defaultLibCtxId)
				g_defaultLibCtxId = 0;
		}
	}
	if (g_initCount == 0 && g_initState != 0)
		g_initState = 0;
	return result;
}

int SceNpWebApi_E8FBA9B8(SceSize poolSize, void *pool)
{
	int result;
	int id;

	if (poolSize == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	if (!npWebApiIsShellProcess())
		return NP_WEBAPI_ERROR_NOT_ALLOWED_IN_SHELL;
	if (g_initState == 0)
		g_initState = 1;
	result = npWebApiNewLibContext(poolSize, pool, "SceShell", 2);
	if (result >= 0) {
		id = result;
		result = npWebApiCreateUserContextObject(id, NULL, 0);
		if (result >= 0) {
			g_initCount++;
			return id;
		}
		if (id != 0) {
			npWebApiDeleteLibContext(id);
			if (id == g_defaultLibCtxId)
				g_defaultLibCtxId = 0;
		}
	}
	if (g_initCount == 0 && g_initState != 0)
		g_initState = 0;
	return result;
}

int sceNpWebApiTerminate(void)
{
	int result;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	result = npWebApiStopLibContext(g_defaultLibCtxId);
	if (result >= 0) {
		result = npWebApiDeleteLibContext(g_defaultLibCtxId);
		if (result >= 0) {
			g_initCount--;
			g_defaultLibCtxId = 0;
			if (g_initState == 1 && g_initCount == 0)
				g_initState = 0;
		}
	}
	return result;
}

int SceNpWebApi_66CEC1A6(int libCtxId)
{
	int result;

	if (libCtxId == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	result = npWebApiStopLibContext(libCtxId);
	if (result >= 0) {
		result = npWebApiDeleteLibContext(libCtxId);
		if (result >= 0) {
			g_initCount--;
			if (g_initState == 1 && g_initCount == 0)
				g_initState = 0;
		}
	}
	return result;
}

}

int npWebApiCreateRequestObject(unsigned int userCtxId, const char *apiGroup, const char *path, unsigned int method,
	const SceNpWebApiContentParameter *content, const void *extraData, SceInt64 *pRequestId, unsigned char multipart)
{
	if (apiGroup == NULL || path == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	if (content != NULL && content->contentLength != 0 && content->pContentType == NULL)
		return NP_WEBAPI_ERROR_INVALID_CONTENT_PARAMETER;
	if (npWebApiGetSdkVersion() < NP_WEBAPI_SDK_3_50 || !(method > 3))
		return npWebApiCreateRequestInternal(userCtxId, apiGroup, path, method, content, extraData, pRequestId,
			multipart);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

extern "C" {

int sceNpWebApiCreateRequest(const char *pApiGroup, const char *pPath, SceNpWebApiHttpMethod method,
	const SceNpWebApiContentParameter *pContentParameter, SceInt64 *pRequestId)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 1);
	if (userCtxId >= 0)
		return npWebApiCreateRequestObject(userCtxId, pApiGroup, pPath, method, pContentParameter, 0, pRequestId, 0);
	return userCtxId;
}

int SceNpWebApi_0D910413(int libCtxId, const char *pApiGroup, const char *pPath, SceNpWebApiHttpMethod method,
	const SceNpWebApiContentParameter *pContentParameter, const void *a6, SceInt64 *pRequestId)
{
	int userCtxId;

	if (libCtxId == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	userCtxId = npWebApiResolveUserContext(libCtxId, 1);
	if (userCtxId >= 0)
		return npWebApiCreateRequestObject(userCtxId, pApiGroup, pPath, method, pContentParameter, a6, pRequestId, 0);
	return userCtxId;
}

int sceNpWebApiCreateMultipartRequest(const char *pApiGroup, const char *pPath, SceNpWebApiHttpMethod method,
	SceInt64 *pRequestId)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 1);
	if (userCtxId >= 0)
		return npWebApiCreateRequestObject(userCtxId, pApiGroup, pPath, method, NULL, 0, pRequestId, 1);
	return userCtxId;
}

int SceNpWebApi_93BBA612(int libCtxId, const char *pApiGroup, const char *pPath, SceNpWebApiHttpMethod method,
	SceInt64 *pRequestId)
{
	int userCtxId;

	if (libCtxId == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	userCtxId = npWebApiResolveUserContext(libCtxId, 1);
	if (userCtxId >= 0)
		return npWebApiCreateRequestObject(userCtxId, pApiGroup, pPath, method, NULL, 0, pRequestId, 1);
	return userCtxId;
}

int sceNpWebApiDeleteRequest(SceInt64 requestId)
{
	return npWebApiDeleteRequest(requestId);
}

int sceNpWebApiSendRequest(SceInt64 requestId, const void *pData, SceSize dataSize, int *pHttpStatus)
{
	return npWebApiRequestSend(requestId, 0, pData, dataSize, 0, (NpWebApiResponseBuffer *)pHttpStatus);
}

int sceNpWebApiSendMultipartRequest(SceInt64 requestId, int partIndex, const void *pData, SceSize dataSize)
{
	if (partIndex > 0 && pData != NULL && dataSize != 0)
		return npWebApiRequestSend(requestId, partIndex, pData, dataSize, 0, NULL);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiSendRequest2(SceInt64 requestId, const void *pData, SceSize dataSize,
	SceNpWebApiResponseInformationOption *pRespInfoOption)
{
	return npWebApiRequestSend(requestId, 0, pData, dataSize, 1, (NpWebApiResponseBuffer *)pRespInfoOption);
}

int sceNpWebApiSendMultipartRequest2(SceInt64 requestId, int partIndex, const void *pData, SceSize dataSize,
	SceNpWebApiResponseInformationOption *pRespInfoOption)
{
	if (partIndex > 0 && pData != NULL && dataSize != 0)
		return npWebApiRequestSend(requestId, partIndex, pData, dataSize, 1, (NpWebApiResponseBuffer *)pRespInfoOption);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiGetHttpStatusCode(SceInt64 requestId, int *pStatus)
{
	if (pStatus != NULL)
		return npWebApiGetHttpStatusCode(requestId, pStatus);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiReadData(SceInt64 requestId, void *pData, SceSize size)
{
	if (pData != NULL && size != 0)
		return npWebApiReadData(requestId, pData, size);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiGetErrorCode(void)
{
	return npWebApiGetErrorCode();
}

int sceNpWebApiSetMultipartContentType(SceInt64 requestId, const char *pTypeName, const char *pBoundary)
{
	if (pBoundary != NULL)
		return npWebApiSetMultipartContentType(requestId, pTypeName, pBoundary);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiAddMultipartPart(SceInt64 requestId, const SceNpWebApiMultipartPartParameter *pParam, int *pIndex)
{
	if (pIndex != NULL)
		return npWebApiAddMultipartPart(requestId, pParam, pIndex);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiAddHttpRequestHeader(SceInt64 requestId, const char *pFieldName, const char *pValue)
{
	if (pFieldName != NULL && pValue != NULL)
		return npWebApiAddHttpRequestHeader(requestId, pFieldName, pValue);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiGetHttpResponseHeaderValueLength(SceInt64 requestId, const char *pFieldName, SceSize *pValueLength)
{
	if (pFieldName != NULL && pValueLength != NULL)
		return npWebApiGetHttpResponseHeader(requestId, pFieldName, NULL, 0, pValueLength);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiGetHttpResponseHeaderValue(SceInt64 requestId, const char *pFieldName, char *pValue, SceSize valueSize)
{
	if (pFieldName != NULL && pValue != NULL && valueSize != 0)
		return npWebApiGetHttpResponseHeader(requestId, pFieldName, pValue, valueSize, NULL);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiAbortRequest(SceInt64 requestId)
{
	return npWebApiAbortRequest(requestId);
}

int sceNpWebApiCreatePushEventFilter(const SceNpWebApiPushEventDataType *pDataType, SceSize dataTypeNum)
{
	if (g_defaultLibCtxId != 0) {
		if (pDataType != NULL && dataTypeNum != 0)
			return npWebApiCreatePushEventFilter(g_defaultLibCtxId, pDataType, dataTypeNum);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_19062C5A(int libCtxId, const SceNpWebApiPushEventDataType *pDataType, SceSize dataTypeNum)
{
	if (pDataType != NULL && dataTypeNum != 0)
		return npWebApiCreatePushEventFilter(libCtxId, pDataType, dataTypeNum);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiDeletePushEventFilter(int filterId)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiDeletePushEventFilter(g_defaultLibCtxId, filterId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_6CDD45B6(int libCtxId, int filterId)
{
	return npWebApiDeletePushEventFilter(libCtxId, filterId);
}

int sceNpWebApiRegisterPushEventCallback(int filterId, SceNpWebApiPushEventCallback cbFunc, void *pUserArg)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0) {
		if (cbFunc != NULL)
			return npWebApiRegisterPushEventCallback(userCtxId, filterId, cbFunc, pUserArg);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return userCtxId;
}

int SceNpWebApi_83E94928(int libCtxId, int filterId, SceNpWebApiPushEventCallback cbFunc, void *pUserArg)
{
	int userCtxId;

	userCtxId = npWebApiResolveUserContext(libCtxId, 0);
	if (userCtxId >= 0) {
		if (cbFunc != NULL)
			return npWebApiRegisterPushEventCallback(userCtxId, filterId, cbFunc, pUserArg);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return userCtxId;
}

int sceNpWebApiUnregisterPushEventCallback(int callbackId)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0)
		return npWebApiUnregisterPushEventCallback(userCtxId, callbackId);
	return userCtxId;
}

int SceNpWebApi_FDCBD57D(int libCtxId, int callbackId)
{
	int userCtxId;

	userCtxId = npWebApiResolveUserContext(libCtxId, 0);
	if (userCtxId >= 0)
		return npWebApiUnregisterPushEventCallback(userCtxId, callbackId);
	return userCtxId;
}

int sceNpWebApiCreateHandle(void)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiCreateHandle(g_defaultLibCtxId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_BED99F66(int libCtxId)
{
	return npWebApiCreateHandle(libCtxId);
}

int sceNpWebApiDeleteHandle(int handleId)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiDeleteHandle(g_defaultLibCtxId, handleId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_1D427258(int libCtxId, int handleId)
{
	return npWebApiDeleteHandle(libCtxId, handleId);
}

int sceNpWebApiCreateServicePushEventFilter(int handleId, const char *pNpServiceName, SceNpServiceLabel npServiceLabel,
	const SceNpWebApiPushEventDataType *pDataType, SceSize dataTypeNum)
{
	int libCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	libCtxId = g_defaultLibCtxId;
	if (pNpServiceName == NULL || pDataType == NULL || dataTypeNum == 0)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	if (npWebApiGetSdkVersion() >= NP_WEBAPI_SDK_3_50 && npServiceLabel == (SceNpServiceLabel)-1)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	return npWebApiCreateServicePushEventFilter(libCtxId, handleId, pNpServiceName, npServiceLabel, pDataType, dataTypeNum);
}

int SceNpWebApi_159396F0(int libCtxId, int handleId, const char *pNpServiceName, SceNpServiceLabel npServiceLabel,
	const SceNpWebApiPushEventDataType *pDataType, SceSize dataTypeNum)
{
	if (pDataType != NULL && dataTypeNum != 0)
		return npWebApiCreateServicePushEventFilter(libCtxId, handleId, pNpServiceName, npServiceLabel, pDataType, dataTypeNum);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiDeleteServicePushEventFilter(int filterId)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiDeleteServicePushEventFilter(g_defaultLibCtxId, filterId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_C359A88A(int libCtxId, int filterId)
{
	return npWebApiDeleteServicePushEventFilter(libCtxId, filterId);
}

int sceNpWebApiRegisterServicePushEventCallback(int filterId, SceNpWebApiServicePushEventCallback cbFunc, void *pUserArg)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0) {
		if (cbFunc != NULL)
			return npWebApiRegisterServicePushEventCallback(userCtxId, filterId, cbFunc, NULL, pUserArg);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return userCtxId;
}

int SceNpWebApi_1BDB76C0(int libCtxId, int filterId, SceNpWebApiServicePushEventCallback cbFunc, void *pUserArg)
{
	int userCtxId;

	userCtxId = npWebApiResolveUserContext(libCtxId, 0);
	if (userCtxId >= 0) {
		if (cbFunc != NULL)
			return npWebApiRegisterServicePushEventCallback(userCtxId, filterId, NULL, cbFunc, pUserArg);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return userCtxId;
}

int sceNpWebApiUnregisterServicePushEventCallback(int callbackId)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0)
		return npWebApiUnregisterServicePushEventCallback(userCtxId, callbackId);
	return userCtxId;
}

int SceNpWebApi_C18E6DAA(int libCtxId, int callbackId)
{
	int userCtxId;

	userCtxId = npWebApiResolveUserContext(libCtxId, 0);
	if (userCtxId >= 0)
		return npWebApiUnregisterServicePushEventCallback(userCtxId, callbackId);
	return userCtxId;
}

int sceNpWebApiCreateExtdPushEventFilter(int handleId, const char *pNpServiceName, SceNpServiceLabel npServiceLabel,
	const SceNpWebApiExtdPushEventFilterParameter *pFilterParam, SceSize filterParamNum)
{
	if (g_defaultLibCtxId != 0) {
		if ((pNpServiceName == NULL || npServiceLabel != (SceNpServiceLabel)-1) && pFilterParam != NULL && filterParamNum != 0)
			return npWebApiCreateExtdPushEventFilter(g_defaultLibCtxId, handleId, pNpServiceName, npServiceLabel, pFilterParam,
				filterParamNum);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int sceNpWebApiDeleteExtdPushEventFilter(int filterId)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiDeleteExtdPushEventFilter(g_defaultLibCtxId, filterId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int sceNpWebApiRegisterExtdPushEventCallback(int filterId, SceNpWebApiExtdPushEventCallback cbFunc, void *pUserArg)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0) {
		if (cbFunc != NULL)
			return npWebApiRegisterExtdPushEventCallback(userCtxId, filterId, cbFunc, pUserArg);
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return userCtxId;
}

int sceNpWebApiUnregisterExtdPushEventCallback(int callbackId)
{
	int userCtxId;

	if (g_defaultLibCtxId == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	userCtxId = npWebApiResolveUserContext(g_defaultLibCtxId, 0);
	if (userCtxId >= 0)
		return npWebApiUnregisterExtdPushEventCallback(userCtxId, callbackId);
	return userCtxId;
}

int sceNpWebApiAbortHandle(int handleId)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiAbortHandle(g_defaultLibCtxId, handleId);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

int SceNpWebApi_1140FA82(int libCtxId, int handleId)
{
	return npWebApiAbortHandle(libCtxId, handleId);
}

int sceNpWebApiCheckCallback(void)
{
	return npWebApiCheckCallback();
}

int SceNpWebApi_6E6AFA39(int libCtxId, SceNpWebApiMemoryPoolStats *pCurrentStat)
{
	return npWebApiGetMemoryPoolStats(libCtxId, pCurrentStat);
}

int sceNpWebApiGetMemoryPoolStats(SceNpWebApiMemoryPoolStats *pCurrentStat)
{
	if (g_defaultLibCtxId != 0)
		return npWebApiGetMemoryPoolStats(g_defaultLibCtxId, pCurrentStat);
	return NP_WEBAPI_ERROR_NOT_INITIALIZED;
}

} /* extern "C" */
