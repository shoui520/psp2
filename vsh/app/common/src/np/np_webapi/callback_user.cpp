/*
 * SceNpWebApi: the callbacks a user context keeps for push events (RegisterPushEventCallback and its
 * service and extended forms), and the filter lookups their registration checks. The names are ours.
 */

#include "np_webapi_internal.h"

static unsigned int s_callbackCounter;		/* 0x8101002C */

/* the ID of the filter in the library context's list, or 0 */
int npWebApiLibContextHasPushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandle *found;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->filters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	found = NULL;
	if (node != sentinel) {
		do {
			if (npWebApiPushFilterGetId((NpWebApiPushFilter *)node->handle) == filterId) {
				found = node->handle;
				goto done;
			}
			node = node->next;
		} while (node != ctx->filters.sentinel);
		found = NULL;
	}
done:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return (int)found;
}

int npWebApiLibContextHasServicePushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandle *found;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->serviceFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	found = NULL;
	if (node != sentinel) {
		do {
			if (npWebApiServiceFilterGetId((NpWebApiServiceFilter *)node->handle) == filterId) {
				found = node->handle;
				goto done;
			}
			node = node->next;
		} while (node != ctx->serviceFilters.sentinel);
		found = NULL;
	}
done:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return (int)found;
}

/* the list's insert: a ring of 28-byte nodes around a sentinel made when the first comes */
void npWebApiCallbackListInsert(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list, NpWebApiCallbackRecord *record)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *newNode;
	NpWebApiCallbackNode *result;
	void *memory;
	void *allocator;
	void *callback;
	void *argument;
	int id;
	int filterId;
	unsigned char state;
	unsigned long count;

	if (list->sentinel == NULL) {
		allocator = list->allocator;
		newNode = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 28);
			if (memory != NULL) {
				sentinel = (NpWebApiCallbackNode *)operator new(28, memory);
				if (sentinel != NULL) {
					sentinel->id = 0;
					sentinel->filterId = 0;
					sentinel->callback = NULL;
					sentinel->argument = NULL;
					sentinel->state = 0;
					newNode = sentinel;
				}
			}
		}
		list->sentinel = newNode;
		if (newNode == NULL) {
			*pResult = NULL;
			return;
		}
		newNode->prev = newNode;
		newNode->next = newNode;
	}
	id = record->id;
	filterId = record->filterId;
	callback = record->callback;
	argument = record->argument;
	allocator = list->allocator;
	state = record->state;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 28);
		if (memory != NULL) {
			result = (NpWebApiCallbackNode *)operator new(28, memory);
			if (result != NULL) {
				result->callback = callback;
				result->argument = argument;
				result->state = state;
				result->prev = list->sentinel->prev;
				result->id = id;
				result->filterId = filterId;
				sentinel = list->sentinel;
				result->next = sentinel;
				sentinel->prev->next = result;
				count = list->count + 1;
				list->sentinel->prev = result;
				list->count = count;
				*pResult = result;
				return;
			}
		}
	}
	*pResult = NULL;
}

int npWebApiCallbackListErase(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list, NpWebApiCallbackNode **pAt)
{
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *next;
	NpWebApiCallbackNode *link;
	NpWebApiCallbackNode *prev;
	void *allocator;
	unsigned long count;

	node = *pAt;
	next = node->next;
	link = next;
	if (node == list->sentinel) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "erase",
			"../../../../../app/common/src/np/include/np_common_ps4/npext/list.h", 364, "HALT_ASSERT",
			"at != end()"));
		link = node->next;
	}
	prev = node->prev;
	link->prev = prev;
	prev->next = node->next;
	allocator = list->allocator;
	if (allocator != NULL)
		SceNpCommonPs4_6B428829(allocator, node);
	count = list->count - 1;
	list->count = count;
	*pResult = next;
	return count;
}

/* the list's insert: a ring of 28-byte nodes around a sentinel made when the first comes */
void npWebApiExtdCallbackListInsert(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list, NpWebApiCallbackRecord *record)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *newNode;
	NpWebApiCallbackNode *result;
	void *memory;
	void *allocator;
	void *callback;
	void *argument;
	int id;
	int filterId;
	unsigned char state;
	unsigned long count;

	if (list->sentinel == NULL) {
		allocator = list->allocator;
		newNode = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 28);
			if (memory != NULL) {
				sentinel = (NpWebApiCallbackNode *)operator new(28, memory);
				if (sentinel != NULL) {
					sentinel->id = 0;
					sentinel->filterId = 0;
					sentinel->callback = NULL;
					sentinel->argument = NULL;
					sentinel->state = 0;
					newNode = sentinel;
				}
			}
		}
		list->sentinel = newNode;
		if (newNode == NULL) {
			*pResult = NULL;
			return;
		}
		newNode->prev = newNode;
		newNode->next = newNode;
	}
	id = record->id;
	filterId = record->filterId;
	callback = record->callback;
	argument = record->argument;
	allocator = list->allocator;
	state = record->state;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 28);
		if (memory != NULL) {
			result = (NpWebApiCallbackNode *)operator new(28, memory);
			if (result != NULL) {
				result->callback = callback;
				result->argument = argument;
				result->state = state;
				result->prev = list->sentinel->prev;
				result->id = id;
				result->filterId = filterId;
				sentinel = list->sentinel;
				result->next = sentinel;
				sentinel->prev->next = result;
				count = list->count + 1;
				list->sentinel->prev = result;
				list->count = count;
				*pResult = result;
				return;
			}
		}
	}
	*pResult = NULL;
}

int npWebApiExtdCallbackListErase(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list, NpWebApiCallbackNode **pAt)
{
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *next;
	NpWebApiCallbackNode *link;
	NpWebApiCallbackNode *prev;
	void *allocator;
	unsigned long count;

	node = *pAt;
	next = node->next;
	link = next;
	if (node == list->sentinel) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "erase",
			"../../../../../app/common/src/np/include/np_common_ps4/npext/list.h", 364, "HALT_ASSERT",
			"at != end()"));
		link = node->next;
	}
	prev = node->prev;
	link->prev = prev;
	prev->next = node->next;
	allocator = list->allocator;
	if (allocator != NULL)
		SceNpCommonPs4_6B428829(allocator, node);
	count = list->count - 1;
	list->count = count;
	*pResult = next;
	return count;
}

/* the list's insert: a ring of 32-byte nodes around a sentinel made when the first comes */
void npWebApiServiceCallbackListInsert(NpWebApiServiceCallbackNode **pResult, NpWebApiServiceCallbackList *list, NpWebApiServiceCallbackRecord *record)
{
	NpWebApiServiceCallbackNode *sentinel;
	NpWebApiServiceCallbackNode *newNode;
	NpWebApiServiceCallbackNode *result;
	void *memory;
	void *allocator;
	void *callback;
	void *callbackEx;
	void *argument;
	int id;
	int filterId;
	unsigned char state;
	unsigned long count;

	if (list->sentinel == NULL) {
		allocator = list->allocator;
		newNode = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 32);
			if (memory != NULL) {
				sentinel = (NpWebApiServiceCallbackNode *)operator new(32, memory);
				if (sentinel != NULL) {
					sentinel->id = 0;
					sentinel->filterId = 0;
					sentinel->callback = NULL;
					sentinel->callbackEx = NULL;
					sentinel->argument = NULL;
					sentinel->state = 0;
					newNode = sentinel;
				}
			}
		}
		list->sentinel = newNode;
		if (newNode == NULL) {
			*pResult = NULL;
			return;
		}
		newNode->prev = newNode;
		newNode->next = newNode;
	}
	id = record->id;
	filterId = record->filterId;
	callback = record->callback;
	callbackEx = record->callbackEx;
	argument = record->argument;
	allocator = list->allocator;
	state = record->state;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 32);
		if (memory != NULL) {
			result = (NpWebApiServiceCallbackNode *)operator new(32, memory);
			if (result != NULL) {
				result->callback = callback;
				result->callbackEx = callbackEx;
				result->argument = argument;
				result->state = state;
				result->prev = list->sentinel->prev;
				result->id = id;
				result->filterId = filterId;
				sentinel = list->sentinel;
				result->next = sentinel;
				sentinel->prev->next = result;
				count = list->count + 1;
				list->sentinel->prev = result;
				list->count = count;
				*pResult = result;
				return;
			}
		}
	}
	*pResult = NULL;
}

int npWebApiServiceCallbackListErase(NpWebApiServiceCallbackNode **pResult, NpWebApiServiceCallbackList *list, NpWebApiServiceCallbackNode **pAt)
{
	NpWebApiServiceCallbackNode *node;
	NpWebApiServiceCallbackNode *next;
	NpWebApiServiceCallbackNode *link;
	NpWebApiServiceCallbackNode *prev;
	void *allocator;
	unsigned long count;

	node = *pAt;
	next = node->next;
	link = next;
	if (node == list->sentinel) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "erase",
			"../../../../../app/common/src/np/include/np_common_ps4/npext/list.h", 364, "HALT_ASSERT",
			"at != end()"));
		link = node->next;
	}
	prev = node->prev;
	link->prev = prev;
	prev->next = node->next;
	allocator = list->allocator;
	if (allocator != NULL)
		SceNpCommonPs4_6B428829(allocator, node);
	count = list->count - 1;
	list->count = count;
	*pResult = next;
	return count;
}

int npWebApiUserContextRegisterPushEventCallback(NpWebApiUserContext *userCtx, int filterId, SceNpWebApiPushEventCallback cb, void *arg)
{
	NpWebApiCallbackRecord record;
	NpWebApiCallbackNode *node;
	unsigned int id;

	npWebApiLibContextLock(userCtx->libContext);
	s_callbackCounter++;
	id = s_callbackCounter;
	if (s_callbackCounter >= 0xEFFFFFFF) {
		id = 1;
		s_callbackCounter = 1;
	}
	record.id = id;
	record.filterId = filterId;
	record.callback = (void *)cb;
	record.argument = arg;
	record.state = 0;
	npWebApiCallbackListInsert(&node, &userCtx->pushCallbacks, &record);
	if (node != NULL) {
		npWebApiLibContextUnlock(userCtx->libContext);
		return record.id;
	}
	npWebApiLibContextUnlock(userCtx->libContext);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

int npWebApiUserContextUnregisterPushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *erased;
	NpWebApiCallbackNode *at;
	int found;
	int result;

	found = 0;
	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->pushCallbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	result = 0;
	if (node != sentinel) {
		do {
			if (node->id == callbackId) {
				found = 1;
				at = node;
				npWebApiCallbackListErase(&erased, &userCtx->pushCallbacks, &at);
				break;
			}
			node = node->next;
		} while (node != sentinel);
	}
	if (found == 0)
		result = NP_WEBAPI_ERROR_CALLBACK_NOT_FOUND;
	npWebApiLibContextUnlock(userCtx->libContext);
	return result;
}

int npWebApiUserContextUnregisterPushEventCallback(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *erased;
	NpWebApiCallbackNode *at;
	int found;
	int busy;
	int result;

	result = 0;
	while (1) {
		busy = 0;
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->pushCallbacks.sentinel;
		node = NULL;
		if (sentinel != NULL)
			node = sentinel->next;
		found = 0;
		if (node != sentinel) {
			do {
				if (node->id == callbackId) {
					found = 1;
					if (node->state == 2) {
						busy = 1;
					} else {
						at = node;
						npWebApiCallbackListErase(&erased, &userCtx->pushCallbacks, &at);
					}
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(userCtx->libContext);
		if (found == 0)
			break;
		if (busy == 0)
			return result;
		sceKernelDelayThread(100000);
	}
	result = NP_WEBAPI_ERROR_CALLBACK_NOT_FOUND;
	return result;
}

int npWebApiUserContextRegisterServicePushEventCallback(NpWebApiUserContext *userCtx, int filterId, SceNpWebApiServicePushEventCallback cb,
	SceNpWebApiServicePushEventCallback cbEx, void *arg)
{
	NpWebApiServiceCallbackRecord record;
	NpWebApiServiceCallbackNode *node;
	unsigned int id;

	npWebApiLibContextLock(userCtx->libContext);
	s_callbackCounter++;
	id = s_callbackCounter;
	if (s_callbackCounter >= 0xEFFFFFFF) {
		id = 1;
		s_callbackCounter = 1;
	}
	record.id = id;
	record.filterId = filterId;
	record.callback = (void *)cb;
	record.callbackEx = (void *)cbEx;
	record.argument = arg;
	record.state = 0;
	npWebApiServiceCallbackListInsert(&node, &userCtx->serviceCallbacks, &record);
	if (node != NULL) {
		npWebApiLibContextUnlock(userCtx->libContext);
		return record.id;
	}
	npWebApiLibContextUnlock(userCtx->libContext);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

int npWebApiUserContextUnregisterServicePushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiServiceCallbackNode *sentinel;
	NpWebApiServiceCallbackNode *node;
	NpWebApiServiceCallbackNode *erased;
	NpWebApiServiceCallbackNode *at;
	int found;
	int result;

	found = 0;
	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->serviceCallbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	result = 0;
	if (node != sentinel) {
		do {
			if (node->id == callbackId) {
				found = 1;
				at = node;
				npWebApiServiceCallbackListErase(&erased, &userCtx->serviceCallbacks, &at);
				break;
			}
			node = node->next;
		} while (node != sentinel);
	}
	if (found == 0)
		result = NP_WEBAPI_ERROR_SERVICE_CALLBACK_NOT_FOUND;
	npWebApiLibContextUnlock(userCtx->libContext);
	return result;
}

int npWebApiUserContextUnregisterServicePushEventCallback(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiServiceCallbackNode *sentinel;
	NpWebApiServiceCallbackNode *node;
	NpWebApiServiceCallbackNode *erased;
	NpWebApiServiceCallbackNode *at;
	int found;
	int busy;
	int result;

	result = 0;
	while (1) {
		busy = 0;
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->serviceCallbacks.sentinel;
		node = NULL;
		if (sentinel != NULL)
			node = sentinel->next;
		found = 0;
		if (node != sentinel) {
			do {
				if (node->id == callbackId) {
					found = 1;
					if (node->state == 2) {
						busy = 1;
					} else {
						at = node;
						npWebApiServiceCallbackListErase(&erased, &userCtx->serviceCallbacks, &at);
					}
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(userCtx->libContext);
		if (found == 0)
			break;
		if (busy == 0)
			return result;
		sceKernelDelayThread(100000);
	}
	result = NP_WEBAPI_ERROR_SERVICE_CALLBACK_NOT_FOUND;
	return result;
}

int npWebApiUserContextRegisterExtdPushEventCallback(NpWebApiUserContext *userCtx, int filterId, SceNpWebApiExtdPushEventCallback cb, void *arg)
{
	NpWebApiCallbackRecord record;
	NpWebApiCallbackNode *node;
	unsigned int id;

	npWebApiLibContextLock(userCtx->libContext);
	s_callbackCounter++;
	id = s_callbackCounter;
	if (s_callbackCounter >= 0xEFFFFFFF) {
		id = 1;
		s_callbackCounter = 1;
	}
	record.id = id;
	record.filterId = filterId;
	record.callback = (void *)cb;
	record.argument = arg;
	record.state = 0;
	npWebApiExtdCallbackListInsert(&node, &userCtx->extdCallbacks, &record);
	if (node != NULL) {
		npWebApiLibContextUnlock(userCtx->libContext);
		return record.id;
	}
	npWebApiLibContextUnlock(userCtx->libContext);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

int npWebApiUserContextUnregisterExtdPushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *erased;
	NpWebApiCallbackNode *at;
	int found;
	int result;

	found = 0;
	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->extdCallbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	result = 0;
	if (node != sentinel) {
		do {
			if (node->id == callbackId) {
				found = 1;
				at = node;
				npWebApiExtdCallbackListErase(&erased, &userCtx->extdCallbacks, &at);
				break;
			}
			node = node->next;
		} while (node != sentinel);
	}
	if (found == 0)
		result = NP_WEBAPI_ERROR_EXTD_CALLBACK_NOT_FOUND;
	npWebApiLibContextUnlock(userCtx->libContext);
	return result;
}

int npWebApiUserContextUnregisterExtdPushEventCallback(NpWebApiUserContext *userCtx, int callbackId)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *erased;
	NpWebApiCallbackNode *at;
	int found;
	int busy;
	int result;

	result = 0;
	while (1) {
		busy = 0;
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->extdCallbacks.sentinel;
		node = NULL;
		if (sentinel != NULL)
			node = sentinel->next;
		found = 0;
		if (node != sentinel) {
			do {
				if (node->id == callbackId) {
					found = 1;
					if (node->state == 2) {
						busy = 1;
					} else {
						at = node;
						npWebApiExtdCallbackListErase(&erased, &userCtx->extdCallbacks, &at);
					}
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(userCtx->libContext);
		if (found == 0)
			break;
		if (busy == 0)
			return result;
		sceKernelDelayThread(100000);
	}
	result = NP_WEBAPI_ERROR_EXTD_CALLBACK_NOT_FOUND;
	return result;
}
