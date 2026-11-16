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

NpWebApiLibContext *npWebApiUserContextGetLibContext(NpWebApiUserContext *userCtx)
{
	return userCtx->libContext;
}

int npWebApiUserContextGetHttpTemplateId(NpWebApiUserContext *userCtx)
{
	return userCtx->httpTemplateId;
}

void *npWebApiUserContextGetObject44(NpWebApiUserContext *userCtx)
{
	return userCtx->object44;
}

NpWebApiUserContext *npWebApiUserContextSetFlag97(NpWebApiUserContext *userCtx, char value)
{
	userCtx->flag97 = value;
	return userCtx;
}

int npWebApiUserContextGetFlag97(NpWebApiUserContext *userCtx)
{
	return userCtx->flag97;
}

int npWebApiUserContextGetParam0(NpWebApiUserContext *userCtx)
{
	return userCtx->param0;
}

int npWebApiUserContextGetParam1(NpWebApiUserContext *userCtx)
{
	return userCtx->param1;
}

int npWebApiUserContextUnlockLib(NpWebApiUserContext *userCtx)
{
	return npWebApiLibContextUnlock(userCtx->libContext);
}

int npWebApiUserContextLockLib(NpWebApiUserContext *userCtx)
{
	return npWebApiLibContextLock(userCtx->libContext);
}
