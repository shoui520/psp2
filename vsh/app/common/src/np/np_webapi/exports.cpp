/*
 * SceNpWebApi: the exports. Each finds its library context or request by the IDs it was given and
 * hands over to the library's own functions.
 */

#include "np_webapi_internal.h"

int g_initState;		/* 0x81010000 */
int g_initCount;		/* 0x81010004 */
int g_defaultLibCtxId;		/* 0x81010008 */

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
			if (g_initState == 1)
				g_initState = g_initCount != 0;
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
			if (g_initState == 1)
				g_initState = g_initCount != 0;
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

} /* extern "C" */
