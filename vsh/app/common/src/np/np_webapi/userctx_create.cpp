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
