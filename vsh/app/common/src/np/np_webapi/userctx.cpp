/*
 * SceNpWebApi: the user context's small accessors. The names are ours.
 */

#include "np_webapi_internal.h"

NpWebApiUserContext *npWebApiUserContextNext(NpWebApiUserContext *userCtx)
{
	return userCtx->next;
}

void *npWebApiUserContextRequests(NpWebApiUserContext *userCtx)
{
	return userCtx->requests;
}

int npWebApiUserContextGetId(NpWebApiUserContext *userCtx)
{
	return userCtx->id;
}

NpWebApiUserContext *npWebApiUserContextSetNext(NpWebApiUserContext *userCtx, NpWebApiUserContext *next)
{
	userCtx->next = next;
	return userCtx;
}

SceNpOnlineId *npWebApiUserContextOnlineId(NpWebApiUserContext *userCtx)
{
	return &userCtx->onlineId;
}

int npWebApiUserContextAddRef(NpWebApiUserContext *userCtx)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(userCtx->libContext);
	libCtx = userCtx->libContext;
	userCtx->refCount++;
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiUserContextFlag158(NpWebApiUserContext *userCtx)
{
	return userCtx->flag158;
}

int npWebApiUserContextRelease(NpWebApiUserContext *userCtx)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(userCtx->libContext);
	libCtx = userCtx->libContext;
	userCtx->refCount--;
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiUserContextHasOtherRefs(NpWebApiUserContext *userCtx)
{
	NpWebApiLibContext *libCtx;
	int others;

	npWebApiLibContextLock(userCtx->libContext);
	others = userCtx->refCount > 1;
	libCtx = userCtx->libContext;
	npWebApiLibContextUnlock(libCtx);
	return others;
}

NpWebApiUserContext *npWebApiUserContextSetFlag158(NpWebApiUserContext *userCtx, char value)
{
	userCtx->flag158 = value;
	return userCtx;
}

void *npWebApiUserContextSetOnlineId(NpWebApiUserContext *userCtx, const SceNpOnlineId *onlineId)
{
	if (onlineId != NULL)
		return sceClibMemcpy_safe(&userCtx->onlineId, onlineId, sizeof(userCtx->onlineId));
	return userCtx;
}
