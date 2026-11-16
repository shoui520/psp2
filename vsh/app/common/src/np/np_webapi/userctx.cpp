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
