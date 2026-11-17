/*
 * SceNpCommonPs4, NpAsm: a request of the client (its id, the NpManager login and HTTP connection it waits on, the
 * list link) and the limitation of a title's access to the server, set when a token is refused.
 */

#include "np_asm.h"

extern "C" {

extern const void *g_npAsmRequestVtable[];	/* 0x8102ab98 */


SceUInt32 g_npAsmRequestIdCounter;		/* 0x8102b99c */
SceUInt8 g_npAsmLimitTitleId[16];		/* 0x8102b9a0 */
SceUInt64 g_npAsmLimitExpires;			/* 0x8102b9b0 */
SceUInt64 g_npAsmLimitRecheck;			/* 0x8102b9b8 */

/*SORT-START*/
struct NpAsmRequest *npAsmRequestInit(struct NpAsmRequest *request, const struct NpAsmRequestParam *pParam)
{
	struct NpAsmClient *client;
	SceUInt32 id;

	request->vtable = NP_COMMON_PS4_VTABLE(g_npAsmRequestVtable);
	client = npAsmClientGet();
	npAsmClientLock(client);
	id = ++g_npAsmRequestIdCounter;
	if (g_npAsmRequestIdCounter > 0xEFFFFFFF) {
		id = 1;
		g_npAsmRequestIdCounter = 1;
	}
	request->id = id;
	npAsmClientUnlock(client);
	request->npManagerId = 0;
	request->connection = NULL;
	sceClibMemset(&request->param, 0, 8);
	if (pParam != NULL)
		request->param.allocator = pParam->allocator;
	request->aborted = 0;
	request->next = NULL;
	sceClibMemset(&request->results, 0, 16);
	return request;
}

struct NpAsmRequest *npAsmRequestTerm(struct NpAsmRequest *request)
{
	request->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return request;
}

int npAsmRequestDelete(struct NpAsmRequest *request)
{
	request->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(request);
}

void npAsmRequestNullsub(void)
{
}

int npAsmRequestGetId(struct NpAsmRequest *request)
{
	return request->id;
}

int npAsmRequestGetNpManagerId(struct NpAsmRequest *request)
{
	return request->npManagerId;
}

void npAsmRequestSetNpManagerId(struct NpAsmRequest *request, int npManagerId)
{
	struct NpAsmClient *client;

	client = npAsmClientGet();
	npAsmClientLock(client);
	request->npManagerId = npManagerId;
	npAsmClientUnlock(client);
}

void *npAsmRequestGetConnection(struct NpAsmRequest *request)
{
	return request->connection;
}

void npAsmRequestSetConnection(struct NpAsmRequest *request, void *connection)
{
	struct NpAsmClient *client;

	client = npAsmClientGet();
	npAsmClientLock(client);
	request->connection = connection;
	npAsmClientUnlock(client);
}

struct NpAsmRequestParam *npAsmRequestGetParam(struct NpAsmRequest *request)
{
	return &request->param;
}

struct NpAsmRequest *npAsmRequestSetNext(struct NpAsmRequest *request, struct NpAsmRequest *next)
{
	request->next = next;
	return request;
}

struct NpAsmRequest *npAsmRequestGetNext(struct NpAsmRequest *request)
{
	return request->next;
}

int npAsmRequestIsAborted(struct NpAsmRequest *request)
{
	return request->aborted;
}

int npAsmRequestCancel(struct NpAsmRequest *request)
{
	struct NpAsmClient *client;
	int result;

	result = 0;
	client = npAsmClientGet();
	npAsmClientLock(client);
	request->aborted = 1;
	if ((request->npManagerId == 0 || (result = SceNpManager_737B2820(request->npManagerId)) >= 0) && request->connection != NULL)
		result = npAsmConnectionAbort(request->connection);
	npAsmClientUnlock(client);
	return result;
}

int npAsmRequestFreeResults(struct NpAsmRequest *request, struct NpCommonPs4Allocator *allocator)
{
	int result;

	result = (int)allocator;
	if (allocator != NULL) {
		if (request->results != NULL) {
			SceNpCommonPs4_6B428829(allocator, request->results);
			request->results = NULL;
		}
		request->resultCount = 0;
		return 0;
	}
	return result;
}

int npAsmLimitationCheck(const char *pTitleId, int kind, SceUInt8 *pExpired, SceUInt64 *pTick)
{
	SceRtcTick tick;
	char zero[16];
	int result;
	SceUInt8 *expiredOut;
	int expired = 0;

	sceClibMemset(zero, 0, 16);
	expiredOut = pExpired;
	if (pTitleId == NULL
		|| (sceClibStrncmp(zero, (const char *)g_npAsmLimitTitleId, 13) != 0
			&& sceClibStrncmp(pTitleId, (const char *)g_npAsmLimitTitleId, 13) != 0)) {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	} else if (kind == 1 || kind == 2) {
		result = sceRtcGetCurrentRetainedNetworkTick(&tick);
		if (result >= 0) {
			if (kind > 1) {
				expired = tick.tick <= g_npAsmLimitRecheck;
				if (pTick != NULL)
					sceClibMemcpy_safe(pTick, &g_npAsmLimitRecheck, 8);
			} else {
				expired = tick.tick <= g_npAsmLimitExpires;
				if (pTick != NULL)
					sceClibMemcpy_safe(pTick, &g_npAsmLimitExpires, 8);
			}
			if (expiredOut != NULL)
				*expiredOut = expired;
		}
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int npAsmLimitationSet(const void *pTitleId, const SceUInt64 *pExpires)
{
	int result;
	SceRtcTick now;

	if (pTitleId != NULL && pExpires != NULL) {
		sceClibMemset(&now, 0, 8);
		result = sceRtcGetCurrentRetainedNetworkTick(&now);
		if (result >= 0) {
			sceClibMemcpy_safe(g_npAsmLimitTitleId, pTitleId, 16);
			sceClibMemcpy_safe(&g_npAsmLimitExpires, pExpires, 8);
			if (*pExpires - now.tick > 0xD693A400)
				g_npAsmLimitRecheck = now.tick + 3600000000u;
		}
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}
/*SORT-END*/

const void *g_npAsmRequestVtable[] = {		/* 0x8102ab98 */
	NULL,
	NULL,
	(const void *)npAsmRequestTerm,
	(const void *)npAsmRequestDelete,
};

}
