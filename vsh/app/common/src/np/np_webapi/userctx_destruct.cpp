/*
 * SceNpWebApi: the user context's destructor. The name is ours.
 */

#include "np_webapi_internal.h"

/* frees the nodes of one of the user context's rings (the compiler's code for it is Sony's inline) */
#define FREE_RING(head, allocator, count, clear)				\
	ring = (NpWebApiListNode *)userCtx->head;							\
	if (ring != NULL) {							\
		node = ring->next;						\
		block = userCtx->allocator;					\
		if (node != ring) {						\
			do {							\
				next = node->next;				\
				if (block != NULL) {				\
					SceNpCommonPs4_6B428829(block, node);	\
					block = userCtx->allocator;		\
					ring = (NpWebApiListNode *)userCtx->head;			\
				}						\
				node = next;					\
			} while (next != ring);					\
		}								\
		if (block != NULL)						\
			SceNpCommonPs4_6B428829(block, node);			\
		if (clear) {							\
			userCtx->head = NULL;					\
			userCtx->count = 0;					\
		}								\
	}

NpWebApiUserContext *npWebApiUserContextDestruct(NpWebApiUserContext *userCtx)
{
	NpWebApiRequest *request;
	NpWebApiRequest *requestNext;
	NpWebApiListNode *ring;
	NpWebApiListNode *node;
	NpWebApiListNode *next;
	void *block;

	userCtx->vtable = g_userContextVtable;
	FREE_RING(extdCallbacks.sentinel, extdCallbacks.allocator, extdCallbacks.count, 1)
	FREE_RING(serviceCallbacks.sentinel, serviceCallbacks.allocator, serviceCallbacks.count, 1)
	FREE_RING(pushCallbacks.sentinel, pushCallbacks.allocator, pushCallbacks.count, 1)
	npWebApiLibContextLock(userCtx->libContext);
	request = userCtx->requestHead;
	if (request != NULL) {
		do {
			requestNext = npWebApiRequestNext(request);
			npWebApiRequestDestroy(request, NULL);
			request = requestNext;
		} while (requestNext != NULL);
	}
	userCtx->requestCount = 0;
	npWebApiLibContextUnlock(userCtx->libContext);
	FREE_RING(extdCallbacks.sentinel, extdCallbacks.allocator, extdCallbacks.count, 0)
	FREE_RING(serviceCallbacks.sentinel, serviceCallbacks.allocator, serviceCallbacks.count, 0)
	FREE_RING(pushCallbacks.sentinel, pushCallbacks.allocator, pushCallbacks.count, 0)
	userCtx->vtable = g_baseVtable;
	return userCtx;
}

int npWebApiUserContextDelete(NpWebApiUserContext *userCtx)
{
	npWebApiUserContextDestruct(userCtx);
	return SceNpCommonPs4_BB5282C8(userCtx);
}

const void *g_userContextVtable[] = {		/* 0x8100eddc */
	(const void *)npWebApiUserContextDestruct,
	(const void *)npWebApiUserContextDelete,
};
