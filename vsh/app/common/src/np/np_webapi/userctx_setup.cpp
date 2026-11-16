/*
 * SceNpWebApi: setting a user context up: its HTTP template, its connection pool and its user object,
 * and the user's push registration. The name is ours.
 */

#include "np_webapi_internal.h"

int npWebApiUserContextSetup(NpWebApiUserContext *userCtx)
{
	const char *extra;
	NpWebApiConnectionPool *pool;
	NpWebApiUserObject *object;
	void *memory;
	int result;
	char version[8];
	char userAgent[64];

	extra = (const char *)npWebApiLibContextGetField116(userCtx->libContext);
	sceClibMemset(version, 0, 8);
	SceNpCommon_0624AE44(version);
	sceClibMemset(userAgent, 0, 0x40);
	if (extra != NULL)
		sceClibSnprintf(userAgent, 0x40, "NpWebApi/%s (%s)", version, extra);
	else
		sceClibSnprintf(userAgent, 0x40, "NpWebApi/%s", version);
	result = sceHttpCreateTemplate(userAgent, 2, 1);
	if (result >= 0) {
		pool = NULL;
		object = NULL;
		userCtx->httpTemplateId = result;
		memory = SceNpCommonPs4_D154D84F(84, npWebApiLibContextGetPool(userCtx->libContext));
		if (memory != NULL)
			pool = npWebApiConnectionPoolConstruct((NpWebApiConnectionPool *)memory, userCtx);
		userCtx->requests = pool;
		if (pool == NULL) {
			result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		memory = SceNpCommonPs4_D154D84F(1320, npWebApiLibContextGetPool(userCtx->libContext));
		if (memory != NULL)
			object = npWebApiUserObjectConstruct((NpWebApiUserObject *)memory, userCtx->libContext, userCtx);
		userCtx->object44 = object;
		if (object == NULL) {
			result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		if (!npWebApiLibContextIsStopped(userCtx->libContext)) {
			result = npPushRegisterUser(&userCtx->onlineId);
			if (result < 0)
				goto fail;
			userCtx->flag96 = 1;
		}
		userCtx->flag98 = 1;
		return result;
	}
fail:
	if (!npWebApiLibContextIsStopped(userCtx->libContext) && userCtx->flag96 != 0) {
		npPushUnregisterUser(&userCtx->onlineId);
		userCtx->flag96 = 0;
	}
	if (userCtx->requests != NULL) {
		((void (*)(void *))(*(const void *const **)userCtx->requests)[1])(userCtx->requests);
		userCtx->requests = NULL;
	}
	if (userCtx->object44 != NULL) {
		((void (*)(void *))(*(const void *const **)userCtx->object44)[1])(userCtx->object44);
		userCtx->object44 = NULL;
	}
	return result;
}

/* finds the library context, makes a handle to its user context, and when asked gives the user
   context its user and its setup; the result is the user context's ID */
int npWebApiResolveUserContext(int libCtxId, unsigned char setup)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContextHandle *handle;
	int result;
	unsigned char user[328];

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx == NULL) {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		goto out;
	}
	npWebApiLibContextLock(libCtx);
	handle = npWebApiLibContextCreateUserContextHandle(libCtx);
	if (handle == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	if (setup != 0) {
		sceClibMemset(user, 0, 0x148);
		result = SceNpManager_E45E4003(user);
		if (result < 0)
			goto out;
		npWebApiUserContextSetOnlineId(handle->userContext, (const SceNpOnlineId *)user);
		if (handle->userContext->flag98 == 0) {
			result = npWebApiUserContextSetup(handle->userContext);
			if (result < 0)
				goto out;
		}
		if (g_sdkVersion >= 0x3300000 && npWebApiUserContextFlag158(handle->userContext) != 0) {
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
			goto out;
		}
	}
	result = npWebApiUserContextGetId(handle->userContext);
	((void (*)(NpWebApiUserContextHandle *))handle->vtable[1])(handle);
out:
	if (libCtx != NULL) {
		npWebApiLibContextUnlock(libCtx);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

int npWebApiStopLibContext(int libCtxId)
{
	int result;

	result = npWebApiResolveUserContext(libCtxId, 0);
	if (result >= 0)
		return npWebApiDeleteUserContextObject(result);
	return result;
}
