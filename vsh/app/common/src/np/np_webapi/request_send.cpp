/*
 * SceNpWebApi: sending a request. The request is found by its ID, its access is made, the HTTP request is
 * created on first use and the body (or a part of it) is sent; when the request is complete, the response
 * headers and the status code are read, and the error body when the status is an error. The names are ours.
 */

#include "np_webapi_internal.h"

#define NP_WEBAPI_ERROR_METHOD_NOT_ALLOWED_PARTS	-2141902570	/* 0x80552916 */
#define NP_WEBAPI_ERROR_NOT_SIGNED_IN			-2141902585	/* 0x80552907 */
#define NP_WEBAPI_ERROR_SERVICE_NOT_CONFIGURED		-2111831423	/* 0x8202c001 */
#define NP_WEBAPI_HTTP_ERROR_RETRY_A			-2143088606	/* 0x80431022 */
#define NP_WEBAPI_HTTP_ERROR_RETRY_B			-2143072222	/* 0x80435022 */

/* tells a missing service configuration of the api group */
static void npWebApiReportServiceNotConfigured(const char *apiGroup)
{
	char text[128];

	sceClibMemset(text, 0, 128);
	sceClibSnprintf(text, 127, "NpWebApi\n(ApiGroup: %s)\nError: ServiceNotConfigured", apiGroup);
	SceNpManager_72936716(text, 0);
}

int npWebApiRequestSend(SceInt64 requestId, int partId, const void *data, SceSize size, unsigned char flag,
	NpWebApiResponseBuffer *response)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	NpWebApiAuthObject *auth;
	NpWebApiPushObject *pushObject;
	unsigned char aborted;
	char flagA;
	char flagB;
	char found;
	char *serviceUrl;
	char *header;
	SceSize headerSize;
	int connId;
	int statusCode;
	int result;
	int retry;
	int code;
	int errorCode;
	int httpId;
	char *buffer;
	int libId;
	int userId;

	libCtx = NULL;
	userCtx = NULL;
	request = NULL;
	aborted = 0;
	connId = 0;
	statusCode = 0;
	libId = npWebApiRequestIdGetLibContextId(requestId);
	userId = npWebApiRequestIdGetUserContextId(requestId);
	libCtx = npWebApiFindLibContext(libId, 0);
	if (libCtx == NULL) {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		goto cleanup;
	}
	userCtx = npWebApiLibContextFindUserContext(libCtx, userId);
	if (userCtx == NULL) {
		result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		goto cleanup;
	}
	request = npWebApiUserContextFindRequest(userCtx, requestId);
	if (request == NULL) {
		result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
		goto cleanup;
	}
	if (npWebApiRequestGetMultipart(request) != 0) {
		if (partId == 0) {
			result = NP_WEBAPI_ERROR_METHOD_NOT_ALLOWED_PARTS;
			goto cleanup;
		}
		if (npWebApiRequestGetFlag204(request) == 0) {
			result = npWebApiRequestPrepareMultipart(request);
			if (result < 0)
				goto cleanup;
		}
	}
	if (g_sdkVersion >= NP_WEBAPI_SDK_3_50 && npWebApiRequestGetFlag328(request) == 0)
		npWebApiRequestSetFlag328(request, 1);
	npWebApiLibContextLock(libCtx);
	npWebApiRequestSetKind(request, 0);
	if (npWebApiRequestGetBusy(request) != 0)
		goto busy;
	if (SceNpManager_A775A452(npWebApiLibContextUnlock(libCtx)) == 0) {
		result = NP_WEBAPI_ERROR_NOT_SIGNED_IN;
		goto cleanup;
	}
	npWebApiLibContextLock(libCtx);
	npWebApiRequestSetKind(request, 1);
	if (npWebApiRequestGetBusy(request) != 0) {
busy:
		result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(libCtx);
		goto cleanup;
	}
	npWebApiLibContextUnlock(libCtx);
	npWebApiLibContextGetAborted(libCtx, &aborted);
	flagA = 0;
	flagB = 0;
	auth = (NpWebApiAuthObject *)npWebApiUserContextGetObject44(userCtx);
	result = npWebApiAuthObjectGetAccess(auth, &flagA, &flagB, aborted);
	if (result < 0)
		goto cleanup;
	if (flagA == 0 || flagB == 0) {
		retry = 0;
		if (flagA == 0)
			npWebApiCloseAllConnections();
		for (;;) {
			result = npWebApiAuthObjectRequestAccess((NpWebApiAuthObject *)npWebApiUserContextGetObject44(userCtx),
				request, aborted);
			if (SceNpCommon_BBC13621() != 0 && result == NP_WEBAPI_ERROR_SERVICE_NOT_CONFIGURED)
				npWebApiReportServiceNotConfigured(npWebApiRequestGetPath(request));
			if ((result != NP_WEBAPI_HTTP_ERROR_RETRY_A && result != NP_WEBAPI_HTTP_ERROR_RETRY_B) || retry > 0)
				break;
			retry = (unsigned char)(retry + 1);
			if (flagA == 0)
				npWebApiCloseAllConnections();
		}
		if (result < 0)
			goto cleanup;
	}
	if (npWebApiRequestGetHttpRequestId(request) == 0) {
		serviceUrl = NULL;
		if (libCtx != NULL && request != NULL) {
			npWebApiLibContextLock(libCtx);
			npWebApiRequestSetKind(request, 2);
			if (npWebApiRequestGetBusy(request) != 0) {
				result = NP_WEBAPI_ERROR_REQUEST_BUSY;
				npWebApiLibContextUnlock(libCtx);
				goto cleanup;
			}
			npWebApiLibContextUnlock(libCtx);
		}
		found = 0;
		pushObject = (NpWebApiPushObject *)npWebApiLibContextGetPushEventObject(libCtx);
		result = npWebApiPushObjectGetServiceUrl(pushObject, npWebApiRequestGetPath(request), &found, &serviceUrl);
		if (result < 0)
			goto cleanup;
		if (found == 0) {
			retry = 0;
			for (;;) {
				npWebApiCloseAllConnections();
				pushObject = (NpWebApiPushObject *)npWebApiLibContextGetPushEventObject(libCtx);
				result = npWebApiPushObjectRequestServiceUrl(pushObject, npWebApiRequestGetPath(request),
					&serviceUrl, request);
				if (SceNpCommon_BBC13621() != 0 && result == NP_WEBAPI_ERROR_SERVICE_NOT_CONFIGURED)
					npWebApiReportServiceNotConfigured(npWebApiRequestGetPath(request));
				if ((result != NP_WEBAPI_HTTP_ERROR_RETRY_A && result != NP_WEBAPI_HTTP_ERROR_RETRY_B) ||
					retry > 0)
					break;
				retry = (unsigned char)(retry + 1);
			}
			if (result < 0)
				goto cleanup;
		}
		npWebApiLibContextLock(libCtx);
		result = npWebApiConnectionPoolGetConnection((NpWebApiConnectionPool *)npWebApiUserContextRequests(userCtx),
			npWebApiRequestGetPath(request), serviceUrl, &connId);
		if (result < 0 || (result = npWebApiRequestCreateHttpRequest(request, connId, serviceUrl)) < 0) {
			npWebApiLibContextUnlock(libCtx);
			goto cleanup;
		}
		npWebApiLibContextUnlock(libCtx);
	}
	if (npWebApiLibContextIsIdle(libCtx) && npWebApiIsUserProfileService(npWebApiRequestGetPath(request))) {
		if (libCtx != NULL && request != NULL) {
			npWebApiLibContextLock(libCtx);
			npWebApiRequestSetKind(request, 3);
			if (npWebApiRequestGetBusy(request) != 0)
				goto busy2;
			npWebApiLibContextUnlock(libCtx);
		}
		npWebApiRequestRegisterTitle(libCtx, request);
	}
	if (libCtx != NULL && request != NULL) {
		npWebApiLibContextLock(libCtx);
		npWebApiRequestSetKind(request, 4);
		if (npWebApiRequestGetBusy(request) != 0) {
busy2:
			result = NP_WEBAPI_ERROR_REQUEST_BUSY;
			npWebApiLibContextUnlock(libCtx);
			goto cleanup;
		}
		npWebApiLibContextUnlock(libCtx);
	}
	if (npWebApiRequestGetMultipart(request) != 0) {
		result = npWebApiRequestSendMultipartPart(request, data, size, partId);
		if (result < 0)
			goto cleanup;
	} else {
		result = npWebApiRequestSendBody(request, data, size);
		if (result < 0)
			goto cleanup;
	}
	if (npWebApiRequestIsSendComplete(request) == 0)
		goto cleanup;
	header = NULL;
	headerSize = 0;
	result = sceHttpGetAllResponseHeaders(npWebApiRequestGetHttpRequestId(request), &header, &headerSize);
	if (result < 0)
		goto cleanup;
	npWebApiRequestSetResponseHeaders(request, (int)header, headerSize);
	if (npWebApiLibContextIsIdle(libCtx) && SceNpCommon_BBC13621() == 1)
		SceNpCommonPs4_EF0A4C60(npWebApiRequestGetHttpRequestId(request), "NpWebApi");
	result = sceHttpGetStatusCode(npWebApiRequestGetHttpRequestId(request), &statusCode);
	if (result < 0)
		goto cleanup;
	if (response != NULL)
		response->statusCode = statusCode;
	if (statusCode < 400)
		goto cleanup;
	if (flag != 0) {
		result = npWebApiRequestReadResponse(request, response);
		if (result < 0)
			goto cleanup;
	} else {
		httpId = npWebApiRequestGetHttpRequestId(request);
		buffer = (char *)npWebApiRequestGetData(request);
		result = sceHttpReadData(httpId, buffer, npWebApiRequestGetFixedSize() - 1);
		if (result == NP_WEBAPI_HTTP_ERROR_SSL)
			result = npWebApiConvertHttpError(npWebApiRequestGetHttpRequestId(request), NP_WEBAPI_HTTP_ERROR_SSL);
		if (result < 0)
			goto cleanup;
		if (result > 0) {
			npWebApiRequestSetField316(request, result);
			result = 0;
			npWebApiRequestFinishBodyHash(request);
		}
	}
	errorCode = 0x82FFFFFF;
	code = 0x82FFFFFF;
	if (npWebApiRequestGetField312(request) >= 0 && npWebApiRequestGetField312(request) <= 12582911)
		errorCode = npWebApiRequestGetField312(request) | 0x82000000;
	if (((errorCode & 0xFFF00000) == 0x82200000 &&
		((errorCode & 0xFFF) == 513 || (errorCode & 0xFFF) == 514 || (errorCode & 0xFFF) == 525)) ||
		((errorCode & 0xFFF00000) == 0x82300000 && (errorCode & 0xFFF) == 0xF12))
		SceNpManager_F66A7AAE();
	if (g_sdkVersion < NP_WEBAPI_SDK_3_55) {
		if (flag != 0 && npWebApiRequestGetField312(request) > 0) {
			if (npWebApiRequestGetField312(request) == -1) {
				if (statusCode >= 100 && statusCode <= 599)
					code = statusCode | 0x82F00000;
				result = code;
			} else {
				if (npWebApiRequestGetField312(request) >= 0 && npWebApiRequestGetField312(request) <= 12582911)
					code = npWebApiRequestGetField312(request) | 0x82000000;
				result = code;
			}
		}
	} else if (flag != 0) {
		if (npWebApiRequestGetField312(request) <= 0) {
			if (statusCode >= 100 && statusCode <= 599)
				code = statusCode | 0x82F00000;
			result = code;
		} else if (npWebApiRequestGetField312(request) == -1) {
			if (statusCode >= 100 && statusCode <= 599)
				code = statusCode | 0x82F00000;
			result = code;
		} else {
			if (npWebApiRequestGetField312(request) >= 0 && npWebApiRequestGetField312(request) <= 12582911)
				code = npWebApiRequestGetField312(request) | 0x82000000;
			result = code;
		}
	}
cleanup:
	if (libCtx != NULL) {
		if (request == NULL)
			goto release;
		npWebApiLibContextLock(libCtx);
		npWebApiRequestSetKind(request, 0);
		if (npWebApiRequestGetBusy(request) != 0)
			result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(libCtx);
	}
	if (request != NULL) {
		if (npWebApiRequestGetField236(request) != 0) {
			SceNpCommonPs4_530CAFDA(npWebApiRequestGetField236(request));
			npWebApiRequestSetField236(request, 0);
		}
		npWebApiRequestRelease(request);
	}
release:
	if (userCtx != NULL)
		npWebApiUserContextRelease(userCtx);
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}
