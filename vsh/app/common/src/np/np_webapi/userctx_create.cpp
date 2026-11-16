/*
 * SceNpWebApi: making a user context: the library context's pool gives the object, the object is
 * given an ID and put in the library context's list. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiCreateUserContextObject(int libCtxId, const SceNpOnlineId *onlineId, unsigned char failIfExists)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiUserContext *existing;
	void *memory;
	const void **vtable;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		if (failIfExists != 0 && (existing = npWebApiLibContextFindUserContextByOnlineId(libCtx, onlineId)) != NULL) {
			npWebApiUserContextRelease(existing);
			result = NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXISTS;
		} else {
			userCtx = NULL;
			memory = SceNpCommonPs4_D154D84F(sizeof(NpWebApiUserContext), npWebApiLibContextGetPool(libCtx));
			if (memory != NULL
			    && (userCtx = npWebApiUserContextConstruct((NpWebApiUserContext *)memory, libCtx, onlineId)) != NULL) {
				result = npWebApiUserContextAssignId(userCtx);
				if (result >= 0) {
					npWebApiLibContextAddUserContext(libCtx, userCtx);
					npWebApiLibContextRelease(libCtx);
					return npWebApiUserContextGetId(userCtx);
				}
			} else {
				result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
			}
			if (userCtx != NULL) {
				npWebApiUserContextReleaseResources(userCtx);
				vtable = (const void **)userCtx->vtable;
				((void (*)(NpWebApiUserContext *))vtable[1])(userCtx);
			}
		}
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}

int npWebApiDeleteUserContextObject(int userCtxId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	const void **vtable;
	int result;

	result = 0;
	libCtx = npWebApiFindLibContext((unsigned int)userCtxId >> 16, 0);
	if (libCtx == NULL) {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		goto out;
	}
	npWebApiLibContextLock(libCtx);
	userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
	if (userCtx == NULL) {
		result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		goto release;
	}
	if (g_sdkVersion >= NP_WEBAPI_SDK_3_30) {
		npWebApiUserContextAbortRequests(userCtx);
		if (npWebApiUserContextFlag158(userCtx) != 0) {
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
			goto release;
		}
		npWebApiUserContextSetFlag158(userCtx, 1);
		while (npWebApiUserContextHasOtherRefs(userCtx) != 0 || npWebApiUserContextHasBusyRequests(userCtx) != 0) {
			npWebApiLibContextUnlock(libCtx);
			sceKernelDelayThread(50000);
			npWebApiLibContextLock(libCtx);
		}
	} else {
		if (npWebApiUserContextHasOtherRefs(userCtx) != 0) {
			result = -2141902574;
			goto release;
		}
		if (npWebApiUserContextHasBusyRequests(userCtx) != 0) {
			result = -2141902573;
			goto release;
		}
	}
	npWebApiUserContextReleaseResources(userCtx);
	npWebApiLibContextRemoveUserContext(libCtx, userCtx);
	npWebApiUserContextRelease(userCtx);
	vtable = (const void **)userCtx->vtable;
	((void (*)(NpWebApiUserContext *))vtable[1])(userCtx);
	goto out;
release:
	if (userCtx != NULL)
		npWebApiUserContextRelease(userCtx);
out:
	if (libCtx != NULL) {
		npWebApiLibContextUnlock(libCtx);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}
