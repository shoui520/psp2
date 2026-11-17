/*
 * SceNpWebApi: the exports. Each finds its library context or request by the IDs it was given and
 * hands over to the library's own functions.
 */

#include "np_webapi_internal.h"

extern "C" {

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
	const SceNpWebApiContentParameter *pContentParameter, int a6, SceInt64 *pRequestId)
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
	return npWebApiSend(requestId, 0, pData, dataSize, 0, pHttpStatus);
}

int sceNpWebApiSendMultipartRequest(SceInt64 requestId, int partIndex, const void *pData, SceSize dataSize)
{
	if (partIndex > 0 && pData != NULL && dataSize != 0)
		return npWebApiSend(requestId, partIndex, pData, dataSize, 0, NULL);
	return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
}

int sceNpWebApiSendRequest2(SceInt64 requestId, const void *pData, SceSize dataSize,
	SceNpWebApiResponseInformationOption *pRespInfoOption)
{
	return npWebApiSend(requestId, 0, pData, dataSize, 1, pRespInfoOption);
}

int sceNpWebApiSendMultipartRequest2(SceInt64 requestId, int partIndex, const void *pData, SceSize dataSize,
	SceNpWebApiResponseInformationOption *pRespInfoOption)
{
	if (partIndex > 0 && pData != NULL && dataSize != 0)
		return npWebApiSend(requestId, partIndex, pData, dataSize, 1, pRespInfoOption);
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
