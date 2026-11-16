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
