/*
 * SceNpWebApi: push event filters, the library context's list of them and the exports' backends.
 * The names are ours.
 */

#include "np_webapi_internal.h"

static unsigned int s_filterCounter;		/* 0x81010020 */
static unsigned int s_serviceFilterCounter;	/* 0x81010028 */
static unsigned int s_extdFilterCounter;	/* 0x81010030 */

NpWebApiPushFilter *npWebApiPushFilterConstruct(NpWebApiPushFilter *filter, NpWebApiLibContext *libCtx)
{
	filter->libContext = libCtx;
	filter->vtable = g_pushFilterVtable;
	filter->id = 0;
	filter->types = NULL;
	filter->typeCount = 0;
	return filter;
}

int npWebApiPushFilterInit(NpWebApiPushFilter *filter, const char *types, int typeCount)
{
	const char *source;
	int offset;
	int i;
	int id;

	source = types;
	if (types != NULL && typeCount != 0) {
		filter->types = (void *)npWebApiLibContextPoolAlloc(filter->libContext, 65 * typeCount);
		if (filter->types == NULL)
			return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		sceClibMemset(filter->types, 0, 65 * typeCount);
		if (typeCount != 0) {
			offset = 0;
			for (i = typeCount; i != 0; i--) {
				sceClibMemcpy_safe((char *)filter->types + offset, source, 0x41);
				offset += 65;
				source += 65;
			}
		}
		filter->typeCount = typeCount;
	}
	s_filterCounter++;
	id = s_filterCounter;
	if (s_filterCounter >= 0xEFFFFFFF) {
		id = 1;
		s_filterCounter = 1;
	}
	filter->id = id;
	return id;
}

int npWebApiPushFilterGetId(NpWebApiPushFilter *filter)
{
	return filter->id;
}

NpWebApiPushFilter *npWebApiPushFilterDestruct(NpWebApiPushFilter *filter)
{
	if (filter->types != NULL) {
		filter->vtable = g_pushFilterVtable;
		npWebApiLibContextDestroyPool(filter->libContext, filter->types);
	}
	filter->vtable = g_baseVtable;
	return filter;
}

int npWebApiPushFilterDelete(NpWebApiPushFilter *filter)
{
	if (filter->types != NULL) {
		filter->vtable = g_pushFilterVtable;
		npWebApiLibContextDestroyPool(filter->libContext, filter->types);
	}
	filter->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(filter);
}

const void *g_pushFilterVtable[] = {		/* 0x8100ed9c */
	(const void *)npWebApiPushFilterDestruct,
	(const void *)npWebApiPushFilterDelete,
	NULL,
	NULL,
};

/* the filter list's insert: the handle list's, for filters */
void npWebApiFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiPushFilter **pFilter)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	NpWebApiPushFilter *filter;
	void *allocator;
	unsigned long count;

	allocator = list->allocator;
	if (list->sentinel == NULL) {
		sentinel = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 12);
			if (memory != NULL) {
				node = (NpWebApiHandleNode *)operator new(12, memory);
				if (node != NULL) {
					node->handle = NULL;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
		allocator = list->allocator;
	}
	filter = *pFilter;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 12);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(12, memory);
			if (result != NULL) {
				result->handle = (NpWebApiHandle *)filter;
				result->prev = list->sentinel->prev;
				node = list->sentinel;
				result->next = node;
				node->prev->next = result;
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

int npWebApiFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiHandleNode **pAt)
{
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	NpWebApiHandleNode *link;
	NpWebApiHandleNode *prev;
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

int npWebApiLibContextCreatePushEventFilter(NpWebApiLibContext *ctx, const char *types, int typeCount)
{
	NpWebApiPushFilter *filter;
	NpWebApiHandleNode *node;
	void *memory;
	int result;

	filter = NULL;
	memory = SceNpCommonPs4_D154D84F(20, ctx->heapBlock->poolObject);
	if (memory != NULL)
		filter = npWebApiPushFilterConstruct((NpWebApiPushFilter *)memory, ctx);
	if (filter != NULL) {
		result = npWebApiPushFilterInit(filter, types, typeCount);
		if (result >= 0) {
			SceNpCommonPs4_F5840D6E(&ctx->lock);
			npWebApiFilterListInsert(&node, &ctx->filters, &filter);
			if (node != NULL) {
				SceNpCommonPs4_4A1C1C3C(&ctx->lock);
				return npWebApiPushFilterGetId(filter);
			}
			result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
			SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		}
	} else {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	}
	if (filter != NULL)
		((void (*)(NpWebApiPushFilter *))filter->vtable[1])(filter);
	return result;
}

int npWebApiLibContextDeletePushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *erased;
	NpWebApiHandleNode *at;
	NpWebApiPushFilter *filter;
	int result;

	result = 0;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->filters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node == sentinel) {
		result = NP_WEBAPI_ERROR_FILTER_NOT_FOUND;
	} else {
		while (npWebApiPushFilterGetId((NpWebApiPushFilter *)node->handle) != filterId) {
			node = node->next;
			if (node == ctx->filters.sentinel) {
				result = NP_WEBAPI_ERROR_FILTER_NOT_FOUND;
				goto out;
			}
		}
		filter = (NpWebApiPushFilter *)node->handle;
		if (filter != NULL)
			((void (*)(NpWebApiPushFilter *))filter->vtable[1])(filter);
		at = node;
		npWebApiFilterListErase(&erased, &ctx->filters, &at);
	}
out:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return result;
}

int npWebApiCreatePushEventFilter(int libCtxId, const SceNpWebApiPushEventDataType *pDataType, unsigned int num)
{
	NpWebApiLibContext *libCtx;
	int result;

	result = npWebApiResolveUserContext(libCtxId, 1);
	if (result >= 0) {
		libCtx = npWebApiFindLibContext(libCtxId, 0);
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		if (libCtx != NULL)
			result = npWebApiLibContextCreatePushEventFilter(libCtx, (const char *)pDataType, num);
		if (libCtx != NULL)
			npWebApiLibContextRelease(libCtx);
	}
	return result;
}

int npWebApiDeletePushEventFilter(int libCtxId, int filterId)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextDeletePushEventFilter(libCtx, filterId);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

/* the service filter list's insert */
void npWebApiServiceFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiServiceFilter **pFilter)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	NpWebApiServiceFilter *filter;
	void *allocator;
	unsigned long count;

	allocator = list->allocator;
	if (list->sentinel == NULL) {
		sentinel = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 12);
			if (memory != NULL) {
				node = (NpWebApiHandleNode *)operator new(12, memory);
				if (node != NULL) {
					node->handle = NULL;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
		allocator = list->allocator;
	}
	filter = *pFilter;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 12);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(12, memory);
			if (result != NULL) {
				result->handle = (NpWebApiHandle *)filter;
				result->prev = list->sentinel->prev;
				node = list->sentinel;
				result->next = node;
				node->prev->next = result;
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


int npWebApiServiceFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiHandleNode **pAt)
{
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	NpWebApiHandleNode *link;
	NpWebApiHandleNode *prev;
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

int npWebApiServiceFilterGetId(NpWebApiServiceFilter *filter)
{
	return filter->id;
}

char *npWebApiServiceFilterGetName(NpWebApiServiceFilter *filter)
{
	return filter->name;
}

int npWebApiServiceFilterGetHandle(NpWebApiServiceFilter *filter)
{
	return filter->handle;
}

int npWebApiLibContextDeleteServicePushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *erased;
	NpWebApiHandleNode *at;
	NpWebApiServiceFilter *filter;
	int result;

	result = 0;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->serviceFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node == sentinel) {
		result = NP_WEBAPI_ERROR_SERVICE_FILTER_NOT_FOUND;
	} else {
		while (npWebApiServiceFilterGetId((NpWebApiServiceFilter *)node->handle) != filterId) {
			node = node->next;
			if (node == ctx->serviceFilters.sentinel) {
				result = NP_WEBAPI_ERROR_SERVICE_FILTER_NOT_FOUND;
				goto out;
			}
		}
		filter = (NpWebApiServiceFilter *)node->handle;
		if (filter != NULL)
			((void (*)(NpWebApiServiceFilter *))filter->vtable[1])(filter);
		at = node;
		npWebApiServiceFilterListErase(&erased, &ctx->serviceFilters, &at);
	}
out:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return result;
}

int npWebApiDeleteServicePushEventFilter(int libCtxId, int filterId)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextDeleteServicePushEventFilter(libCtx, filterId);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

/* the extended filter list's insert */
void npWebApiExtdFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiExtdFilter **pFilter)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	NpWebApiExtdFilter *filter;
	void *allocator;
	unsigned long count;

	allocator = list->allocator;
	if (list->sentinel == NULL) {
		sentinel = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 12);
			if (memory != NULL) {
				node = (NpWebApiHandleNode *)operator new(12, memory);
				if (node != NULL) {
					node->handle = NULL;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
		allocator = list->allocator;
	}
	filter = *pFilter;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 12);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(12, memory);
			if (result != NULL) {
				result->handle = (NpWebApiHandle *)filter;
				result->prev = list->sentinel->prev;
				node = list->sentinel;
				result->next = node;
				node->prev->next = result;
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


int npWebApiExtdFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiHandleNode **pAt)
{
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	NpWebApiHandleNode *link;
	NpWebApiHandleNode *prev;
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

int npWebApiExtdFilterGetId(NpWebApiExtdFilter *filter)
{
	return filter->id;
}

int npWebApiExtdFilterGetHandle(NpWebApiExtdFilter *filter)
{
	return filter->handle;
}

int npWebApiLibContextDeleteExtdPushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *erased;
	NpWebApiHandleNode *at;
	NpWebApiExtdFilter *filter;
	int result;

	result = 0;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->extdFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node == sentinel) {
		result = NP_WEBAPI_ERROR_EXTD_FILTER_NOT_FOUND;
	} else {
		while (npWebApiExtdFilterGetId((NpWebApiExtdFilter *)node->handle) != filterId) {
			node = node->next;
			if (node == ctx->extdFilters.sentinel) {
				result = NP_WEBAPI_ERROR_EXTD_FILTER_NOT_FOUND;
				goto out;
			}
		}
		filter = (NpWebApiExtdFilter *)node->handle;
		if (filter != NULL)
			((void (*)(NpWebApiExtdFilter *))filter->vtable[1])(filter);
		at = node;
		npWebApiExtdFilterListErase(&erased, &ctx->extdFilters, &at);
	}
out:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return result;
}

NpWebApiExtdFilter *npWebApiLibContextFindExtdPushEventFilter(NpWebApiLibContext *ctx, int filterId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiExtdFilter *found;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->extdFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	found = NULL;
	if (node != sentinel) {
		do {
			if (npWebApiExtdFilterGetId((NpWebApiExtdFilter *)node->handle) == filterId) {
				found = (NpWebApiExtdFilter *)node->handle;
				goto out;
			}
			node = node->next;
		} while (node != ctx->extdFilters.sentinel);
		found = NULL;
	}
out:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return found;
}

int npWebApiDeleteExtdPushEventFilter(int libCtxId, int filterId)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextDeleteExtdPushEventFilter(libCtx, filterId);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

NpWebApiServiceFilter *npWebApiServiceFilterConstruct(NpWebApiServiceFilter *filter, NpWebApiLibContext *libCtx)
{
	filter->vtable = g_serviceFilterVtable;
	filter->all = 0;
	filter->list.sentinel = NULL;
	filter->list.count = 0;
	filter->list.allocator = NULL;
	filter->id = 0;
	filter->types = NULL;
	filter->typeCount = 0;
	filter->libContext = libCtx;
	sceClibMemset(filter->name, 0, 33);
	sceClibMemset(&filter->handle, 0, 4);
	filter->list.allocator = (void *)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool(libCtx));
	return filter;
}

static inline void serviceFilterTeardown(NpWebApiServiceFilter *filter)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;
	void *types;

	types = filter->types;
	filter->vtable = g_serviceFilterVtable;
	if (types != NULL)
		npWebApiLibContextDestroyPool(filter->libContext, types);
	sentinel = filter->list.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = filter->list.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = filter->list.allocator;
					sentinel = filter->list.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	filter->vtable = g_baseVtable;
}

NpWebApiServiceFilter *npWebApiServiceFilterDestruct(NpWebApiServiceFilter *filter)
{
	serviceFilterTeardown(filter);
	return filter;
}

int npWebApiServiceFilterDelete(NpWebApiServiceFilter *filter)
{
	serviceFilterTeardown(filter);
	return SceNpCommonPs4_BB5282C8(filter);
}

const void *g_serviceFilterVtable[] = {	/* 0x8100edbc */
	(const void *)npWebApiServiceFilterDestruct,
	(const void *)npWebApiServiceFilterDelete,
	NULL,
};

/* the list of the 12-byte service records: 20-byte nodes (prev, next, the record) */
void npWebApiFilterRecordListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, const unsigned char *record)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	void *allocator;
	unsigned char bytes[12];
	unsigned long count;
	int i;

	allocator = list->allocator;
	if (list->sentinel == NULL) {
		sentinel = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 20);
			if (memory != NULL) {
				node = (NpWebApiHandleNode *)operator new(20, memory);
				if (node != NULL) {
					for (i = 8; i < 20; i++)
						((unsigned char *)node)[i] = 0;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
		allocator = list->allocator;
	}
	for (i = 0; i < 12; i++)
		bytes[i] = record[i];
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 20);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(20, memory);
			if (result != NULL) {
				for (i = 0; i < 12; i++)
					((unsigned char *)result)[8 + i] = bytes[i];
				result->prev = list->sentinel->prev;
				node = list->sentinel;
				result->next = node;
				node->prev->next = result;
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

/* looks the name up for the user and keeps the answer in the filter's record list */
int npWebApiServiceFilterResolve(NpWebApiServiceFilter *filter, NpWebApiHandle *handle, const char *name, int arg)
{
	unsigned char aborted;
	NpWebApiHandleNode *inserted;
	int params[2];
	unsigned char object[16];
	unsigned char npId[36];
	struct {
		unsigned char data[144];
		int tail;
	} record;
	int result;
	int value;
	int state;

	value = 0;
	aborted = 0;
	state = npWebApiLibContextGetAborted(filter->libContext, &aborted);
	if (SceNpManager_A775A452(state) == 0) {
		result = -2141902576;
		goto out;
	}
	record.tail = 0;
	sceClibMemset(npId, 0, 36);
	result = sceNpManagerGetNpId(npId);
	if (result < 0)
		goto out;
	sceClibMemset(params, 0, 8);
	params[0] = npWebApiLibContextZero();
	params[1] = (int)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool(filter->libContext));
	if (aborted != 0)
		result = SceNpCommonPs4_FB0B3FBD(params);
	else
		result = SceNpCommonPs4_639086A3(params);
	if (result < 0)
		goto out;
	npWebApiLibContextLock(filter->libContext);
	value = result;
	handle->object = result;
	if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) != 0) {
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	if (npWebApiLibContextIsStarted(filter->libContext, (NpWebApiLibContext *)handle) != 0) {
		result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	npWebApiLibContextUnlock(filter->libContext);
	sceClibMemset(object, 0, 16);
	result = SceNpCommonPs4_17EB36C2(object, 0);
	if (result < 0)
		goto out;
	sceClibMemset(record.data, 0, 144);
	if (aborted != 0)
		result = SceNpCommonPs4_AB41A67D(value, npId, name, arg, record.data);
	else
		result = SceNpCommonPs4_6E443E9B(value, npId, name, object, 0, arg, record.data);
	if (result < 0)
		goto out;
	npWebApiLibContextLock(filter->libContext);
	if (aborted != 0)
		result = SceNpCommonPs4_9C4F1236(value);
	else
		result = SceNpCommonPs4_530CAFDA(value);
	value = 0;
	if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) != 0) {
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	if (npWebApiLibContextIsStarted(filter->libContext, (NpWebApiLibContext *)handle) != 0) {
		result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	handle->object = record.tail;
	npWebApiLibContextUnlock(filter->libContext);
	npWebApiFilterRecordListInsert(&inserted, &filter->list, record.data);
	if (inserted == NULL)
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
out:
	npWebApiLibContextLock(filter->libContext);
	if (value != 0) {
		if (aborted != 0)
			SceNpCommonPs4_9C4F1236(value);
		else
			SceNpCommonPs4_530CAFDA(value);
		if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) == 0)
			handle->object = 0;
	}
	npWebApiLibContextUnlock(filter->libContext);
	return result;
}

int npWebApiServiceFilterInit(NpWebApiServiceFilter *filter, NpWebApiHandle *handle, const char *name, int arg,
	const char *types, unsigned int typeCount)
{
	int result;
	SceSize length;
	const char *source;
	int offset;
	unsigned int i;
	int id;

	if (name != NULL) {
		result = npWebApiServiceFilterResolve(filter, handle, name, arg);
		if (result < 0)
			return result;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		sceClibStrncpy(filter->name, name, length);
	} else {
		filter->all = 1;
	}
	filter->handle = arg;
	if (types != NULL && typeCount != 0) {
		filter->types = (void *)npWebApiLibContextPoolAlloc(filter->libContext, 65 * typeCount);
		if (filter->types == NULL)
			return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		sceClibMemset(filter->types, 0, 65 * typeCount);
		source = types;
		offset = 0;
		for (i = typeCount; i != 0; i--) {
			sceClibMemcpy_safe((char *)filter->types + offset, source, 0x41);
			offset += 65;
			source += 65;
		}
		filter->typeCount = typeCount;
	}
	s_serviceFilterCounter++;
	id = s_serviceFilterCounter;
	if (s_serviceFilterCounter >= 0xEFFFFFFF) {
		id = 1;
		s_serviceFilterCounter = 1;
	}
	filter->id = id;
	return id;
}

int npWebApiLibContextCreateServicePushEventFilter(NpWebApiLibContext *ctx, int handleId, const char *name, int arg,
	const char *types, unsigned int typeCount)
{
	NpWebApiServiceFilter *filter;
	NpWebApiHandle *handle;
	NpWebApiHandleNode *node;
	void *memory;
	int result;

	handle = NULL;
	filter = NULL;
	memory = SceNpCommonPs4_D154D84F(76, ctx->heapBlock->poolObject);
	if (memory != NULL)
		filter = npWebApiServiceFilterConstruct((NpWebApiServiceFilter *)memory, ctx);
	if (filter == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	result = npWebApiLibContextAcquireHandle(ctx, handleId, &handle);
	if (result < 0)
		goto fail;
	result = npWebApiServiceFilterInit(filter, handle, name, arg, types, typeCount);
	if (result < 0)
		goto fail;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	npWebApiServiceFilterListInsert(&node, &ctx->serviceFilters, &filter);
	if (node == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		goto fail;
	}
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	if (handle != NULL) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->refCount--;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	return npWebApiServiceFilterGetId(filter);
fail:
	if (handle != NULL) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->refCount--;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	if (filter != NULL)
		((void (*)(NpWebApiServiceFilter *))filter->vtable[1])(filter);
	return result;
}

int npWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char *service, SceNpServiceLabel label,
	const SceNpWebApiPushEventDataType *pDataType, unsigned int num)
{
	NpWebApiLibContext *libCtx;
	int result;

	result = npWebApiResolveUserContext(libCtxId, 1);
	if (result >= 0) {
		libCtx = npWebApiFindLibContext(libCtxId, 0);
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		if (libCtx != NULL)
			result = npWebApiLibContextCreateServicePushEventFilter(libCtx, handleId, service, label,
				(const char *)pDataType, num);
		if (libCtx != NULL)
			npWebApiLibContextRelease(libCtx);
	}
	return result;
}

/* the same for an extended filter */
int npWebApiExtdFilterResolve(NpWebApiExtdFilter *filter, NpWebApiHandle *handle, const char *name, int arg)
{
	unsigned char aborted;
	NpWebApiHandleNode *inserted;
	int params[2];
	unsigned char object[16];
	unsigned char npId[36];
	struct {
		unsigned char data[144];
		int tail;
	} record;
	int result;
	int value;
	int state;

	value = 0;
	aborted = 0;
	state = npWebApiLibContextGetAborted(filter->libContext, &aborted);
	if (SceNpManager_A775A452(state) == 0) {
		result = -2141902576;
		goto out;
	}
	record.tail = 0;
	sceClibMemset(npId, 0, 36);
	result = sceNpManagerGetNpId(npId);
	if (result < 0)
		goto out;
	sceClibMemset(params, 0, 8);
	params[0] = npWebApiLibContextZero();
	params[1] = (int)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool(filter->libContext));
	if (aborted != 0)
		result = SceNpCommonPs4_FB0B3FBD(params);
	else
		result = SceNpCommonPs4_639086A3(params);
	if (result < 0)
		goto out;
	npWebApiLibContextLock(filter->libContext);
	value = result;
	handle->object = result;
	if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) != 0) {
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	if (npWebApiLibContextIsStarted(filter->libContext, (NpWebApiLibContext *)handle) != 0) {
		result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	npWebApiLibContextUnlock(filter->libContext);
	sceClibMemset(object, 0, 16);
	result = SceNpCommonPs4_17EB36C2(object, 0);
	if (result < 0)
		goto out;
	sceClibMemset(record.data, 0, 144);
	if (aborted != 0)
		result = SceNpCommonPs4_AB41A67D(value, npId, name, arg, record.data);
	else
		result = SceNpCommonPs4_6E443E9B(value, npId, name, object, 0, arg, record.data);
	if (result < 0)
		goto out;
	npWebApiLibContextLock(filter->libContext);
	if (aborted != 0)
		result = SceNpCommonPs4_9C4F1236(value);
	else
		result = SceNpCommonPs4_530CAFDA(value);
	value = 0;
	if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) != 0) {
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	if (npWebApiLibContextIsStarted(filter->libContext, (NpWebApiLibContext *)handle) != 0) {
		result = NP_WEBAPI_ERROR_REQUEST_BUSY;
		npWebApiLibContextUnlock(filter->libContext);
		goto out;
	}
	handle->object = record.tail;
	npWebApiLibContextUnlock(filter->libContext);
	npWebApiFilterRecordListInsert(&inserted, &filter->list, record.data);
	if (inserted == NULL)
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
out:
	npWebApiLibContextLock(filter->libContext);
	if (value != 0) {
		if (aborted != 0)
			SceNpCommonPs4_9C4F1236(value);
		else
			SceNpCommonPs4_530CAFDA(value);
		if (npWebApiLibContextIsHandleUnusable(filter->libContext, handle) == 0)
			handle->object = 0;
	}
	npWebApiLibContextUnlock(filter->libContext);
	return result;
}


NpWebApiExtdFilter *npWebApiExtdFilterConstruct(NpWebApiExtdFilter *filter, NpWebApiLibContext *libCtx)
{
	filter->vtable = g_extdFilterVtable;
	filter->all = 0;
	filter->list.sentinel = NULL;
	filter->list.count = 0;
	filter->list.allocator = NULL;
	filter->id = 0;
	filter->records = NULL;
	filter->recordCount = 0;
	filter->libContext = libCtx;
	sceClibMemset(filter->name, 0, 33);
	sceClibMemset(&filter->handle, 0, 4);
	filter->list.allocator = (void *)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool(libCtx));
	return filter;
}

NpWebApiExtdFilter *npWebApiExtdFilterDestruct(NpWebApiExtdFilter *filter)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	NpWebApiExtdFilterRecord *record;
	NpWebApiExtdFilterRecord *base;
	NpWebApiLibContext *libCtx;
	unsigned int count;
	unsigned int i;
	void *allocator;

	record = filter->records;
	base = record;
	filter->vtable = g_extdFilterVtable;
	if (record != NULL) {
		count = filter->recordCount;
		if (count != 0) {
			libCtx = filter->libContext;
			if (libCtx != NULL) {
				for (i = 0; i < count; i++) {
					if (record->names != NULL) {
						npWebApiLibContextDestroyPool(libCtx, record->names);
						record->names = NULL;
					}
					if (record->serviceObject != NULL) {
						npWebApiExtdServiceDestroy(record->serviceObject);
						record->serviceObject = NULL;
					}
					record++;
				}
				npWebApiLibContextDestroyPool(libCtx, base);
			}
			filter->records = NULL;
			filter->recordCount = 0;
		}
	}
	sentinel = filter->list.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = filter->list.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = filter->list.allocator;
					sentinel = filter->list.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	filter->vtable = g_baseVtable;
	return filter;
}

int npWebApiExtdFilterDelete(NpWebApiExtdFilter *filter)
{
	npWebApiExtdFilterDestruct(filter);
	return SceNpCommonPs4_BB5282C8(filter);
}

int npWebApiExtdFilterInit(NpWebApiExtdFilter *filter, NpWebApiHandle *handle, const char *name, int arg,
	const char *records, unsigned int count)
{
	NpWebApiLibContext *libCtx;
	NpWebApiExtdFilterRecord *copies;
	NpWebApiExtdFilterRecord *copy;
	const char *source;
	SceSize length;
	unsigned int i;
	unsigned int j;
	int result;
	int id;

	if (name != NULL) {
		result = npWebApiExtdFilterResolve(filter, handle, name, arg);
		if (result < 0)
			return result;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		sceClibStrncpy(filter->name, name, length);
	} else {
		arg = -1;
	}
	filter->handle = arg;
	if (records != NULL && count != 0) {
		libCtx = filter->libContext;
		if (libCtx == NULL)
			return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
		copies = (NpWebApiExtdFilterRecord *)npWebApiLibContextPoolAlloc(libCtx, 80 * count);
		if (copies == NULL) {
			return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		}
		sceClibMemset(copies, 0, 80 * count);
		copy = copies;
		source = records;
		for (i = 0; i < count; i++) {
			sceClibMemcpy_safe(copy, source, 0x41);
			result = npWebApiExtdServiceCreate(npWebApiLibContextGetPool2(libCtx), copy->service,
				&copy->serviceObject);
			if (result < 0)
				goto fail;
			if (*(const int *)(source + 68) != 0 && *(const int *)(source + 72) != 0) {
				copy->names = (char *)npWebApiLibContextPoolAlloc(libCtx, 33 * *(const int *)(source + 72));
				if (copy->names == NULL) {
					result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
					goto fail;
				}
				sceClibMemset(copy->names, 0, 33 * *(const int *)(source + 72));
				sceClibMemcpy_safe(copy->names, *(const void *const *)(source + 68), 33 * *(const int *)(source + 72));
				copy->nameCount = *(const unsigned int *)(source + 72);
				for (j = 0; j < copy->nameCount; j++) {
					result = npWebApiExtdServiceAddName(copy->serviceObject, copy->names + 33 * j);
					if (result < 0)
						goto fail;
				}
			}
			copy++;
			source += 76;
		}
		filter->records = copies;
		filter->recordCount = count;
		goto done;
fail:
		for (i = 0; i < count; i++) {
			if (copies[i].names != NULL) {
				npWebApiLibContextDestroyPool(libCtx, copies[i].names);
				copies[i].names = NULL;
			}
			if (copies[i].serviceObject != NULL) {
				npWebApiExtdServiceDestroy(copies[i].serviceObject);
				copies[i].serviceObject = NULL;
			}
		}
		npWebApiLibContextDestroyPool(libCtx, copies);
		return result;
	}
done:
	s_extdFilterCounter++;
	id = s_extdFilterCounter;
	if (s_extdFilterCounter >= 0xEFFFFFFF) {
		id = 1;
		s_extdFilterCounter = 1;
	}
	filter->id = id;
	return id;
}

const void *g_extdFilterVtable[] = {		/* 0x8100ef70 */
	(const void *)npWebApiExtdFilterDestruct,
	(const void *)npWebApiExtdFilterDelete,
	NULL,
};

int npWebApiLibContextCreateExtdPushEventFilter(NpWebApiLibContext *ctx, int handleId, const char *name, int arg,
	const char *types, unsigned int typeCount)
{
	NpWebApiExtdFilter *filter;
	NpWebApiHandle *handle;
	NpWebApiHandleNode *node;
	void *memory;
	int result;

	handle = NULL;
	filter = NULL;
	memory = SceNpCommonPs4_D154D84F(76, ctx->heapBlock->poolObject);
	if (memory != NULL)
		filter = npWebApiExtdFilterConstruct((NpWebApiExtdFilter *)memory, ctx);
	if (filter == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	result = npWebApiLibContextAcquireHandle(ctx, handleId, &handle);
	if (result < 0)
		goto fail;
	result = npWebApiExtdFilterInit(filter, handle, name, arg, types, typeCount);
	if (result < 0)
		goto fail;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	npWebApiExtdFilterListInsert(&node, &ctx->extdFilters, &filter);
	if (node == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		goto fail;
	}
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	if (handle != NULL) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->refCount--;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	return npWebApiExtdFilterGetId(filter);
fail:
	if (handle != NULL) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->refCount--;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	if (filter != NULL)
		((void (*)(NpWebApiExtdFilter *))filter->vtable[1])(filter);
	return result;
}

int npWebApiCreateExtdPushEventFilter(int libCtxId, int handleId, const char *service, SceNpServiceLabel label,
	const SceNpWebApiExtdPushEventFilterParameter *pParam, SceSize num)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextCreateExtdPushEventFilter(libCtx, handleId, service, label,
			(const char *)pParam, num);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}
