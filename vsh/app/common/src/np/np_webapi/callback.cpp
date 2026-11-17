/*
 * SceNpWebApi: the push event callbacks' export backends. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiUnregisterPushEventCallback(int userCtxId, int callbackId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			if (g_sdkVersion < NP_WEBAPI_SDK_3_50)
				result = npWebApiUserContextUnregisterPushEventCallbackOld(userCtx, callbackId);
			else
				result = npWebApiUserContextUnregisterPushEventCallback(userCtx, callbackId);
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

int npWebApiRegisterPushEventCallback(int userCtxId, int filterId, SceNpWebApiPushEventCallback cb, void *arg)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			if (g_sdkVersion < NP_WEBAPI_SDK_3_50 || npWebApiLibContextHasPushEventFilter(libCtx, filterId) != 0)
				result = npWebApiUserContextRegisterPushEventCallback(userCtx, filterId, cb, arg);
			else
				result = NP_WEBAPI_ERROR_FILTER_NOT_FOUND;
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

int npWebApiUnregisterServicePushEventCallback(int userCtxId, int callbackId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			if (g_sdkVersion < NP_WEBAPI_SDK_3_50)
				result = npWebApiUserContextUnregisterServicePushEventCallbackOld(userCtx, callbackId);
			else
				result = npWebApiUserContextUnregisterServicePushEventCallback(userCtx, callbackId);
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

int npWebApiRegisterServicePushEventCallback(int userCtxId, int filterId, SceNpWebApiServicePushEventCallback cb,
	SceNpWebApiServicePushEventCallback cbEx, void *arg)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			if (g_sdkVersion < NP_WEBAPI_SDK_3_50 || npWebApiLibContextHasServicePushEventFilter(libCtx, filterId) != 0)
				result = npWebApiUserContextRegisterServicePushEventCallback(userCtx, filterId, cb, cbEx, arg);
			else
				result = NP_WEBAPI_ERROR_SERVICE_FILTER_NOT_FOUND;
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

int npWebApiUnregisterExtdPushEventCallback(int userCtxId, int callbackId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			if (g_sdkVersion < NP_WEBAPI_SDK_3_50)
				result = npWebApiUserContextUnregisterExtdPushEventCallbackOld(userCtx, callbackId);
			else
				result = npWebApiUserContextUnregisterExtdPushEventCallback(userCtx, callbackId);
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

int npWebApiRegisterExtdPushEventCallback(int userCtxId, int filterId, SceNpWebApiExtdPushEventCallback cb, void *arg)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	int result;

	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			result = NP_WEBAPI_ERROR_EXTD_FILTER_NOT_FOUND;
			if (npWebApiLibContextFindExtdPushEventFilter(libCtx, filterId) != NULL)
				result = npWebApiUserContextRegisterExtdPushEventCallback(userCtx, filterId, cb, arg);
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
