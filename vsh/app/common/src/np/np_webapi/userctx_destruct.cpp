/*
 * SceNpWebApi: the user context's destructor. The name is ours.
 */

#include "np_webapi_internal.h"

/* frees the nodes of one of the user context's rings (the compiler's code for it is Sony's inline) */
#define FREE_RING(head, allocator, count, clear)				\
	ring = userCtx->head;							\
	if (ring != NULL) {							\
		node = ring->next;						\
		block = userCtx->allocator;					\
		if (node != ring) {						\
			do {							\
				next = node->next;				\
				if (block != NULL) {				\
					SceNpCommonPs4_6B428829(block, node);	\
					block = userCtx->allocator;		\
					ring = userCtx->head;			\
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
	FREE_RING(listCHead, listCAllocator, listCCount, 1)
	FREE_RING(listBHead, listBAllocator, listBCount, 1)
	FREE_RING(listAHead, listAAllocator, listACount, 1)
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
	FREE_RING(listCHead, listCAllocator, listCCount, 0)
	FREE_RING(listBHead, listBAllocator, listBCount, 0)
	FREE_RING(listAHead, listAAllocator, listACount, 0)
	userCtx->vtable = g_baseVtable;
	return userCtx;
}
