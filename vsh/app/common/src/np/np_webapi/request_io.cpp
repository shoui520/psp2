/*
 * SceNpWebApi: the request exports that reach a request through its ID and ask the HTTP library for
 * something. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiGetHttpStatusCode(SceInt64 requestId, int *pStatusCode)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned int libCtxId;
	int userCtxId;
	int result;
	int statusCode;

	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	statusCode = 0;
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			if (request != NULL) {
				result = sceHttpGetStatusCode(npWebApiRequestGetHttpRequestId(request), &statusCode);
				if (pStatusCode != NULL)
					*pStatusCode = statusCode;
			} else {
				result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			}
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

int npWebApiReadData(SceInt64 requestId, void *data, SceSize size)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned int libCtxId;
	int userCtxId;
	int result;
	SceSize buffered;
	SceSize remaining;
	int ret;

	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	request = NULL;
	userCtx = NULL;
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			if (request != NULL) {
				buffered = npWebApiRequestReadBuffered(request, data, size);
				result = buffered;
				remaining = size - buffered;
				if (remaining != 0) {
					npWebApiLibContextLock(libCtx);
					npWebApiRequestSetKind(request, 5);
					if (npWebApiRequestGetBusy(request) != 0) {
						result = NP_WEBAPI_ERROR_REQUEST_BUSY;
						npWebApiLibContextUnlock(libCtx);
					} else {
						npWebApiLibContextUnlock(libCtx);
						ret = sceHttpReadData(npWebApiRequestGetHttpRequestId(request), (char *)data + result, remaining);
						result = ret;
						if (ret >= 0) {
							result = ret + buffered;
						} else if (ret == NP_WEBAPI_HTTP_ERROR_SSL) {
							result = npWebApiConvertHttpError(npWebApiRequestGetHttpRequestId(request),
								NP_WEBAPI_HTTP_ERROR_SSL);
						}
					}
				}
			} else {
				result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			}
		} else {
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		}
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL) {
		if (request != NULL) {
			npWebApiLibContextLock(libCtx);
			npWebApiRequestSetKind(request, 0);
			if (npWebApiRequestGetBusy(request) != 0)
				result = NP_WEBAPI_ERROR_REQUEST_BUSY;
			npWebApiLibContextUnlock(libCtx);
		}
	}
	if (request != NULL)
		npWebApiRequestRelease(request);
	if (userCtx != NULL)
		npWebApiUserContextRelease(userCtx);
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}

int npWebApiRequestGetResponseHeaderValue(NpWebApiRequest *request, const char *fieldStr, void *buffer, SceSize size,
	SceSize *pLength)
{
	const char *header;
	SceSize headerLength;
	const char *fieldValue;
	SceSize valueLength;
	SceSize copyLength;
	SceSize length;
	int result;

	header = (const char *)request->field220;
	headerLength = request->field224;
	fieldValue = NULL;
	valueLength = 0;
	result = sceHttpParseResponseHeader(header, headerLength, fieldStr, &fieldValue, &valueLength);
	if (result >= 0) {
		length = valueLength;
		if (npWebApiGetSdkVersion() >= NP_WEBAPI_SDK_3_50)
			result = 0;
		if (buffer != NULL) {
			copyLength = size;
			if (valueLength <= size)
				copyLength = valueLength;
			sceClibMemcpy_safe(buffer, fieldValue, copyLength);
			length = valueLength;
		}
		if (pLength != NULL)
			*pLength = length;
	}
	return result;
}

int npWebApiGetHttpResponseHeader(SceInt64 requestId, const char *name, char *value, SceSize size, SceSize *pLength)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned int libCtxId;
	int userCtxId;
	int result;
	int libCtxSaved;

	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			libCtxSaved = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			if (request != NULL)
				libCtxSaved = npWebApiRequestGetResponseHeaderValue(request, name, value, size, pLength);
			result = libCtxSaved;
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

int npWebApiRequestGetContentLength(NpWebApiRequest *request, SceULong64 *pContentLength)
{
	int result;

	result = NP_WEBAPI_ERROR_RESPONSE_NOT_AVAILABLE;
	if (request->httpRequestId != 0)
		return sceHttpGetResponseContentLength(request->httpRequestId, pContentLength);
	return result;
}
