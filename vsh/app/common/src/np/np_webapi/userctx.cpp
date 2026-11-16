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

NpWebApiUserContext *npWebApiUserContextConstruct(NpWebApiUserContext *userCtx, NpWebApiLibContext *libCtx,
	const SceNpOnlineId *onlineId)
{
	userCtx->vtable = g_userContextVtable;
	userCtx->flag98 = 0;
	userCtx->requestCount = 0;
	userCtx->field60 = 0;
	userCtx->field64 = 0;
	userCtx->containerA = 0;
	userCtx->field72 = 0;
	userCtx->field76 = 0;
	userCtx->containerB = 0;
	userCtx->field84 = 0;
	userCtx->field88 = 0;
	userCtx->containerC = 0;
	userCtx->next = NULL;
	userCtx->requestHead = NULL;
	if (onlineId != NULL) {
		sceClibMemset(&userCtx->onlineId, 0, sizeof(userCtx->onlineId));
		sceClibMemcpy_safe(&userCtx->onlineId, onlineId, 0x10);
	}
	userCtx->libContext = libCtx;
	userCtx->refCount = 0;
	userCtx->id = 0;
	userCtx->httpTemplateId = 0;
	userCtx->requests = NULL;
	userCtx->object44 = NULL;
	userCtx->containerA = SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(libCtx));
	userCtx->containerB = SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(libCtx));
	userCtx->containerC = SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(libCtx));
	userCtx->flag96 = 0;
	userCtx->flag97 = 0;
	userCtx->param0 = 0;
	userCtx->param1 = 0;
	userCtx->flag158 = 0;
	return userCtx;
}

static int s_userContextCounter;		/* 0x81012078 */

int npWebApiUserContextAssignId(NpWebApiUserContext *userCtx)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *found;
	int id;
	int *pId;
	unsigned int count;

	libCtx = userCtx->libContext;
	count = 1;
	for (;;) {
		npWebApiLibContextLock(libCtx);
		if ((unsigned int)++s_userContextCounter > 0xFFFF)
			s_userContextCounter = 1;
		id = s_userContextCounter | (npWebApiLibContextGetId(libCtx) << 16);
		npWebApiLibContextUnlock(libCtx);
		found = npWebApiLibContextFindUserContext(libCtx, id);
		if (found == NULL)
			break;
		npWebApiLibContextLock(found->libContext);
		found->refCount--;
		npWebApiLibContextUnlock(found->libContext);
		if (++count >= 0xFFFF)
			return -2141902565;
	}
	pId = &userCtx->id;
	if (pId != NULL)
		*pId = id;
	return 0;
}

int npWebApiUserContextAddRequest(NpWebApiUserContext *userCtx, NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;
	int count;

	npWebApiLibContextLock(userCtx->libContext);
	if (userCtx->requestHead != NULL)
		npWebApiRequestSetNext(request, userCtx->requestHead);
	count = userCtx->requestCount + 1;
	libCtx = userCtx->libContext;
	userCtx->requestHead = request;
	userCtx->requestCount = count;
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiUserContextRemoveRequest(NpWebApiUserContext *userCtx, NpWebApiRequest *request)
{
	NpWebApiRequest *node;
	NpWebApiRequest *prev;
	int count;

	npWebApiLibContextLock(userCtx->libContext);
	node = userCtx->requestHead;
	for (prev = NULL; node != NULL; node = npWebApiRequestNext(node)) {
		if (node == request)
			break;
		prev = node;
	}
	if (node == NULL)
		return npWebApiLibContextUnlock(userCtx->libContext);
	if (prev != NULL) {
		npWebApiRequestSetNext(prev, npWebApiRequestNext(node));
		count = userCtx->requestCount;
	} else {
		NpWebApiRequest *next;

		next = npWebApiRequestNext(node);
		count = userCtx->requestCount;
		userCtx->requestHead = next;
	}
	userCtx->requestCount = count - 1;
	return npWebApiLibContextUnlock(userCtx->libContext);
}

NpWebApiRequest *npWebApiUserContextFindRequest(NpWebApiUserContext *userCtx, SceInt64 requestId)
{
	NpWebApiRequest *node;

	npWebApiLibContextLock(userCtx->libContext);
	node = userCtx->requestHead;
	if (node != NULL) {
		while (npWebApiRequestGetId(node) != requestId) {
			node = npWebApiRequestNext(node);
			if (node == NULL)
				goto done;
		}
		npWebApiRequestAddRef(node);
	}
done:
	npWebApiLibContextUnlock(userCtx->libContext);
	return node;
}
