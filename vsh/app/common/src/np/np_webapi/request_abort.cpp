/*
 * SceNpWebApi: stopping a request by its ID. The request waits on a connection, on the push
 * service, on an object of its own or on its HTTP request, and each is stopped its own way.
 */

#include "np_webapi_internal.h"

int npWebApiAbortRequest(SceInt64 requestId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned char aborted;
	int libCtxId;
	int userCtxId;
	int result;

	result = 0;
	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			if (request != NULL) {
				npWebApiLibContextLock(libCtx);
				if (npWebApiRequestGetBusy(request) == 0) {
					npWebApiRequestSetBusy(request, 1);
					switch (npWebApiRequestGetKind(request)) {
					case 1:
						aborted = 0;
						npWebApiLibContextGetAborted(libCtx, &aborted);
						result = npWebApiConnectionPoolAbortRequest(
							(NpWebApiConnectionPool *)npWebApiUserContextGetObject44(userCtx), request,
							aborted);
						break;
					case 2:
						npWebApiPushObjectAbortRequest(
							(NpWebApiPushObject *)npWebApiLibContextGetPushEventObject(libCtx), request);
						break;
					case 3:
						result = npWebApiDestroyObject((void *)npWebApiRequestGetField232(request));
						break;
					case 4:
					case 5:
						result = sceHttpAbortRequest(npWebApiRequestGetHttpRequestId(request));
						break;
					default:
						break;
					}
				}
				npWebApiLibContextUnlock(libCtx);
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

int npWebApiDeleteHttpRequest(int httpRequestId)
{
	if (httpRequestId != 0)
		return sceHttpDeleteRequest(httpRequestId);
	return 0;
}

int npWebApiDeleteRequest(SceInt64 requestId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	int userCtxId;
	int result;
	int httpRequestId;

	httpRequestId = 0;
	request = NULL;
	userCtx = NULL;
	userCtxId = (int)(requestId >> 32);
	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx == NULL) {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		goto out;
	}
	npWebApiLibContextLock(libCtx);
	userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
	if (userCtx == NULL) {
		result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		goto releaseUser;
	}
	request = npWebApiUserContextFindRequest(userCtx, requestId);
	if (request == NULL) {
		result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
		goto releaseRequest;
	}
	if (g_sdkVersion < NP_WEBAPI_SDK_3_30) {
		if (npWebApiRequestHasOtherRefs(request)) {
			result = NP_WEBAPI_ERROR_REQUEST_REFERENCED;
			goto releaseRequest;
		}
	} else {
		if (npWebApiRequestGetFlag389(request) != 0) {
			result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			goto releaseRequest;
		}
		npWebApiRequestSetFlag389(request, 1);
		npWebApiLibContextUnlock(libCtx);
		npWebApiAbortRequest(requestId);
		while (1) {
			npWebApiLibContextLock(libCtx);
			if (!npWebApiRequestHasOtherRefs(request))
				break;
			npWebApiLibContextUnlock(libCtx);
			sceKernelDelayThread(50000);
		}
	}
	result = 0;
	npWebApiUserContextRemoveRequest(userCtx, request);
	npWebApiRequestRelease(request);
	npWebApiRequestDestroy(request, &httpRequestId);
	goto releaseUser;
releaseRequest:
	if (request != NULL)
		npWebApiRequestRelease(request);
releaseUser:
	if (userCtx != NULL)
		npWebApiUserContextRelease(userCtx);
out:
	if (libCtx != NULL) {
		npWebApiLibContextUnlock(libCtx);
		npWebApiLibContextRelease(libCtx);
	}
	if (httpRequestId != 0)
		npWebApiDeleteHttpRequest(httpRequestId);
	return result;
}
