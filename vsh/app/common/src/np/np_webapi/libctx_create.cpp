/*
 * SceNpWebApi: a library context's life: its pool (a memory block or the caller's buffer, with an
 * mspace over it), its construction, the contexts of the process and their deletion. The names are
 * ours.
 */

#include "np_webapi_internal.h"

static int s_libContextId;			/* 0x81010040 */
static int s_libContextCount;			/* 0x81010044 */

const void *const g_libContextVtable[] = {	/* 0x8100eb8c */
	(const void *)npWebApiLibContextDestruct,
	(const void *)npWebApiLibContextDeleteObject,
	NULL,
	NULL,
};

int npWebApiHeapBlockCreate(SceSize poolSize, void *pool, NpWebApiHeapBlock *block, int id)
{
	SceUID memBlock;
	SceUID uid;
	void *mspace;
	void *base;
	char name[32];
	NpWebApiPoolCallbacks *callbacks;

	memBlock = 0;
	base = NULL;
	sceClibMemset(name, 0, sizeof(name));
	sceClibSnprintf(name, sizeof(name), "%s%04X", "SceNpWebApi", id);
	if (pool == NULL) {
		memBlock = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, poolSize, NULL);
		if (memBlock < 0)
			return memBlock;
	}
	uid = memBlock;
	if (pool != NULL) {
		mspace = sceClibMspaceCreate(pool, poolSize);
	} else {
		memBlock = sceKernelGetMemBlockBase(memBlock, &base);
		if (memBlock < 0)
			goto fail;
		mspace = sceClibMspaceCreate(base, poolSize);
	}
	if (mspace == NULL) {
		memBlock = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
fail:
		if (uid != 0)
			sceKernelFreeMemBlock(uid);
		return memBlock;
	}
	if (block != NULL) {
		if (pool != NULL) {
			uid = 0;
			block->owner = pool;
		}
		block->memBlock = uid;
		block->mspace = mspace;
		*(SceSize *)block->reserved0 = poolSize;
		callbacks = (NpWebApiPoolCallbacks *)block->poolObject;
		callbacks->allocate = npWebApiMspaceAlloc;
		callbacks->mspace = mspace;
		callbacks->release = npWebApiMspaceFree;
		callbacks->reallocate = npWebApiMspaceRealloc;
		callbacks->error = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	}
	return 0;
}

NpWebApiLibContext *npWebApiLibContextConstruct(NpWebApiLibContext *ctx, NpWebApiHeapBlock *heapBlock, int state, int id)
{
	ctx->deleting = 0;
	ctx->aborted = 0;
	ctx->flag121 = 0;
	ctx->handles.sentinel = NULL;
	ctx->handles.count = 0;
	ctx->handles.allocator = NULL;
	ctx->filters.sentinel = NULL;
	ctx->filters.count = 0;
	ctx->filters.allocator = NULL;
	ctx->serviceFilters.sentinel = NULL;
	ctx->serviceFilters.count = 0;
	ctx->serviceFilters.allocator = NULL;
	ctx->extdFilters.sentinel = NULL;
	ctx->extdFilters.count = 0;
	ctx->extdFilters.allocator = NULL;
	ctx->vtable = g_libContextVtable;
	ctx->next = NULL;
	ctx->userContextHead = NULL;
	ctx->userContextCount = 0;
	ctx->heapBlock = heapBlock;
	ctx->id = id;
	ctx->pushEventObject = NULL;
	ctx->reserved116 = 0;
	ctx->refCount = 0;
	ctx->state = state;
	return ctx;
}

NpWebApiLibContext *npWebApiLibContextDestruct(NpWebApiLibContext *ctx)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;

	ctx->vtable = g_libContextVtable;
	sentinel = ctx->extdFilters.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = ctx->extdFilters.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = ctx->extdFilters.allocator;
					sentinel = ctx->extdFilters.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	sentinel = ctx->serviceFilters.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = ctx->serviceFilters.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = ctx->serviceFilters.allocator;
					sentinel = ctx->serviceFilters.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	sentinel = ctx->filters.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = ctx->filters.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = ctx->filters.allocator;
					sentinel = ctx->filters.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	sentinel = ctx->handles.sentinel;
	if (sentinel != NULL) {
		node = sentinel->next;
		allocator = ctx->handles.allocator;
		if (node != sentinel) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = ctx->handles.allocator;
					sentinel = ctx->handles.sentinel;
				}
				node = next;
			} while (next != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
	}
	ctx->vtable = g_baseVtable;
	return ctx;
}

int npWebApiLibContextDeleteObject(NpWebApiLibContext *ctx)
{
	npWebApiLibContextDestruct(ctx);
	return SceNpCommonPs4_BB5282C8(ctx);
}

int npWebApiLibContextInit(NpWebApiLibContext *ctx, const char *name)
{
	char lockName[32];
	SceSize length;
	void *memory;
	int result;
	NpWebApiObjectList *list;

	sceClibMemset(lockName, 0, sizeof(lockName));
	sceClibSnprintf(lockName, sizeof(lockName), "%s%d", "SceNpWebApiLock", ctx->id);
	result = SceNpCommonPs4_4858EFB6(&ctx->lock, lockName, 1);
	if (result >= 0) {
		list = NULL;
		memory = SceNpCommonPs4_D154D84F(16, ctx->heapBlock->poolObject);
		if (memory != NULL)
			list = npWebApiObjectListConstruct((NpWebApiObjectList *)memory, ctx);
		ctx->pushEventObject = list;
		if (list != NULL) {
			if (name != NULL) {
				length = sceClibStrnlen(name, 0xFFFFFFFF);
				if (length > 0x20)
					length = 32;
				ctx->reserved116 = (int)SceNpCommonPs4_299ADFC3(ctx->heapBlock->poolObject, length + 1);
				if ((void *)ctx->reserved116 == NULL)
					goto fail;
				sceClibMemset((void *)ctx->reserved116, 0, length + 1);
				sceClibStrncpy((char *)ctx->reserved116, name, length);
			}
			ctx->handles.allocator = SceNpCommonPs4_31DB2D97(ctx->heapBlock->poolObject);
			ctx->filters.allocator = SceNpCommonPs4_31DB2D97(ctx->heapBlock->poolObject);
			ctx->serviceFilters.allocator = SceNpCommonPs4_31DB2D97(ctx->heapBlock->poolObject);
			ctx->extdFilters.allocator = SceNpCommonPs4_31DB2D97(ctx->heapBlock->poolObject);
			return result;
		}
fail:
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		SceNpCommonPs4_29895EBB(&ctx->lock);
		if ((void *)ctx->reserved116 != NULL) {
			SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, (void *)ctx->reserved116);
			ctx->reserved116 = 0;
		}
		if (ctx->pushEventObject != NULL) {
			((void (*)(void *))(*(void ***)ctx->pushEventObject)[1])(ctx->pushEventObject);
			ctx->pushEventObject = NULL;
		}
	}
	return result;
}

void npWebApiLibContextClearHandles(NpWebApiLibContext *ctx)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;

	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	for (; node != sentinel; node = node->next) {
		SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, node->handle);
		sentinel = ctx->handles.sentinel;
	}
	if (sentinel != NULL) {
		next = sentinel->next;
		allocator = ctx->handles.allocator;
		if (next != sentinel) {
			do {
				node = next->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, next);
					allocator = ctx->handles.allocator;
					sentinel = ctx->handles.sentinel;
				}
				next = node;
			} while (node != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, next);
		ctx->handles.sentinel = NULL;
		ctx->handles.count = 0;
	}
}

void npWebApiLibContextClearFilters(NpWebApiLibContext *ctx)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;

	sentinel = ctx->filters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	for (; node != sentinel; node = node->next) {
		if (node->handle != NULL) {
			((void (*)(void *))(*(void ***)node->handle)[1])(node->handle);
			sentinel = ctx->filters.sentinel;
		}
	}
	if (sentinel != NULL) {
		next = sentinel->next;
		allocator = ctx->filters.allocator;
		if (next != sentinel) {
			do {
				node = next->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, next);
					allocator = ctx->filters.allocator;
					sentinel = ctx->filters.sentinel;
				}
				next = node;
			} while (node != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, next);
		ctx->filters.sentinel = NULL;
		ctx->filters.count = 0;
	}
}

void npWebApiLibContextClearServiceFilters(NpWebApiLibContext *ctx)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;

	sentinel = ctx->serviceFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	for (; node != sentinel; node = node->next) {
		if (node->handle != NULL) {
			((void (*)(void *))(*(void ***)node->handle)[1])(node->handle);
			sentinel = ctx->serviceFilters.sentinel;
		}
	}
	if (sentinel != NULL) {
		next = sentinel->next;
		allocator = ctx->serviceFilters.allocator;
		if (next != sentinel) {
			do {
				node = next->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, next);
					allocator = ctx->serviceFilters.allocator;
					sentinel = ctx->serviceFilters.sentinel;
				}
				next = node;
			} while (node != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, next);
		ctx->serviceFilters.sentinel = NULL;
		ctx->serviceFilters.count = 0;
	}
}

void npWebApiLibContextClearExtdFilters(NpWebApiLibContext *ctx)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *next;
	void *allocator;

	sentinel = ctx->extdFilters.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	for (; node != sentinel; node = node->next) {
		if (node->handle != NULL) {
			((void (*)(void *))(*(void ***)node->handle)[1])(node->handle);
			sentinel = ctx->extdFilters.sentinel;
		}
	}
	if (sentinel != NULL) {
		next = sentinel->next;
		allocator = ctx->extdFilters.allocator;
		if (next != sentinel) {
			do {
				node = next->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, next);
					allocator = ctx->extdFilters.allocator;
					sentinel = ctx->extdFilters.sentinel;
				}
				next = node;
			} while (node != sentinel);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, next);
		ctx->extdFilters.sentinel = NULL;
		ctx->extdFilters.count = 0;
	}
}

int npWebApiCreateLibContext(SceSize poolSize, void *pool, const char *name, int flags)
{
	NpWebApiHeapBlock heapBlock;
	NpWebApiHeapBlock *heap;
	NpWebApiLibContext *ctx;
	NpWebApiLibContext *other;
	NpWebApiLibContext *found;
	void *memory;
	int id;
	int tries;
	int result;
	int used;

	ctx = NULL;
	id = 0;
	tries = 0;
	do {
		tries++;
		if ((unsigned int)tries >= 0xEFFF) {
			result = -2141902566;
			goto check;
		}
		npWebApiGlobalLock();
		s_libContextId++;
		other = g_libContexts;
		found = (NpWebApiLibContext *)(unsigned int)s_libContextId;
		id = s_libContextId;
		if ((unsigned int)s_libContextId > 0xEFFF) {
			s_libContextId = 1;
			id = 1;
		}
		found = other;
		if (other != NULL) {
			do {
				if (found->id == id)
					break;
				found = found->next;
			} while (found != NULL);
		}
		npWebApiGlobalUnlock();
	} while (found != NULL);
	result = 0;
check:
	if (result < 0)
		return result;
	sceClibMemset(&heapBlock, 0, sizeof(heapBlock));
	if (pool != NULL) {
		used = 0;
		npWebApiGlobalLock();
		for (other = g_libContexts; other != NULL; other = other->next) {
			if (other->heapBlock->owner == pool) {
				used = 1;
				break;
			}
		}
		npWebApiGlobalUnlock();
		if (used != 0)
			return -2141870591;
	}
	result = npWebApiHeapBlockCreate(poolSize, pool, &heapBlock, id);
	if (result < 0)
		return result;
	heap = (NpWebApiHeapBlock *)SceNpCommonPs4_299ADFC3(heapBlock.poolObject, 40);
	if (heap == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemcpy_safe(heap, &heapBlock, sizeof(heapBlock));
		memory = SceNpCommonPs4_D154D84F(176, heap->poolObject);
		if (memory != NULL)
			ctx = npWebApiLibContextConstruct((NpWebApiLibContext *)memory, heap, flags, id);
		if (ctx == NULL) {
			result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
		} else {
			result = npWebApiLibContextInit(ctx, name);
			if (result >= 0) {
				npWebApiGlobalLock();
				npWebApiGlobalLock();
				if (g_libContexts != NULL)
					ctx->next = g_libContexts;
				g_libContexts = ctx;
				s_libContextCount++;
				npWebApiGlobalUnlock();
				npWebApiGlobalUnlock();
				return ctx->id;
			}
		}
	}
	if (heap != NULL)
		SceNpCommonPs4_38CC9AF5(heapBlock.poolObject, heap);
	if (ctx != NULL)
		((void (*)(NpWebApiLibContext *))ctx->vtable[1])(ctx);
	if (heapBlock.mspace != NULL)
		sceClibMspaceDestroy(heapBlock.mspace);
	if (heapBlock.memBlock != 0)
		sceKernelFreeMemBlock(heapBlock.memBlock);
	return result;
}

int npWebApiDestroyLibContext(int libCtxId)
{
	NpWebApiLibContext *ctx;
	NpWebApiLibContext *node;
	NpWebApiLibContext *prev;
	NpWebApiHeapBlock heapBlock;
	NpWebApiHeapBlock *heap;
	int result;

	result = 0;
	ctx = npWebApiFindLibContext(libCtxId, 1);
	if (ctx != NULL) {
		npWebApiGlobalLock();
		node = g_libContexts;
		prev = NULL;
		if (node != NULL) {
			while (node != ctx) {
				prev = node;
				node = node->next;
				if (node == NULL)
					goto notFound;
			}
		}
		if (node != NULL) {
			if (prev != NULL)
				prev->next = node->next;
			else
				g_libContexts = node->next;
			s_libContextCount--;
			npWebApiGlobalUnlock();
		} else {
notFound:
			npWebApiGlobalUnlock();
		}
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		ctx->refCount--;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		if (ctx->heapBlock != NULL) {
			npWebApiLibContextClearHandles(ctx);
			npWebApiLibContextClearFilters(ctx);
			npWebApiLibContextClearServiceFilters(ctx);
			npWebApiLibContextClearExtdFilters(ctx);
			if ((void *)ctx->reserved116 != NULL) {
				SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, (void *)ctx->reserved116);
				ctx->reserved116 = 0;
			}
			if (ctx->pushEventObject != NULL) {
				((void (*)(void *))(*(void ***)ctx->pushEventObject)[1])(ctx->pushEventObject);
				ctx->pushEventObject = NULL;
			}
			SceNpCommonPs4_29895EBB(&ctx->lock);
			sceClibMemcpy_safe(&heapBlock, ctx->heapBlock, sizeof(heapBlock));
			heap = ctx->heapBlock;
			if (ctx != NULL)
				((void (*)(NpWebApiLibContext *))ctx->vtable[1])(ctx);
			SceNpCommonPs4_38CC9AF5(heapBlock.poolObject, heap);
			if (heapBlock.mspace != NULL)
				sceClibMspaceDestroy(heapBlock.mspace);
			if (heapBlock.memBlock != 0)
				sceKernelFreeMemBlock(heapBlock.memBlock);
		}
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	return result;
}

int npWebApiLibContextDeleteUserContexts(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;
	NpWebApiUserContext *node;
	int ids[32];
	int *id;
	int count;
	int result;

	result = 0;
	lock = &ctx->lock;
	for (;; SceNpCommonPs4_4A1C1C3C(lock)) {
		sceClibMemset(ids, 0, sizeof(ids));
		SceNpCommonPs4_F5840D6E(lock);
		node = ctx->userContextHead;
		count = 0;
		id = ids;
		do {
			if (node == NULL)
				break;
			*id++ = npWebApiUserContextGetId(node);
			count++;
			node = npWebApiUserContextNext(node);
		} while (count < 32);
		SceNpCommonPs4_4A1C1C3C(lock);
		count = 0;
		id = ids;
		do {
			if (*id == 0)
				break;
			result = npWebApiDeleteUserContextObject(*id);
			if (result < 0) {
				if (npWebApiGetSdkVersion() < NP_WEBAPI_SDK_3_30 || result != NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND)
					return result;
				result = 0;
			}
			count++;
			id++;
		} while (count < 32);
		SceNpCommonPs4_F5840D6E(lock);
		if (ctx->userContextHead == NULL || ctx->userContextCount == 0)
			break;
	}
	SceNpCommonPs4_4A1C1C3C(lock);
	return result;
}

int npWebApiLibContextAbortHandles(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			npWebApiAbortHandle(ctx->id, node->handle->id);
			node = node->next;
		} while (node != ctx->handles.sentinel);
	}
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextHasBusyHandles(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandle *handle;
	int result;
	int busy;

	result = 0;
	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			handle = node->handle;
			if (handle != NULL) {
				SceNpCommonPs4_F5840D6E(lock);
				busy = handle->refCount > 1;
				SceNpCommonPs4_4A1C1C3C(lock);
				sentinel = ctx->handles.sentinel;
				if (busy) {
					result = 1;
					break;
				}
			}
			node = node->next;
		} while (node != sentinel);
	}
	SceNpCommonPs4_4A1C1C3C(lock);
	return result;
}

int npWebApiDeleteLibContext(int libCtxId)
{
	NpWebApiLibContext *ctx;
	int result;

	ctx = npWebApiFindLibContext(libCtxId, 0);
	if (ctx == NULL) {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
		goto out;
	}
	if (g_sdkVersion < NP_WEBAPI_SDK_3_30 && npWebApiLibContextHasOtherUsers(ctx) != 0) {
		result = -2141902575;
		goto out;
	}
	result = npWebApiLibContextDeleteUserContexts(ctx);
	if (result >= 0) {
		npWebApiLibContextLock(ctx);
		if (g_sdkVersion >= NP_WEBAPI_SDK_3_30) {
			npWebApiLibContextAbortHandles(ctx);
			if (npWebApiLibContextIsDeleting(ctx) != 0) {
				result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
				npWebApiLibContextUnlock(ctx);
				goto out;
			}
			npWebApiLibContextSetDeleting(ctx);
			while (npWebApiLibContextHasOtherUsers(ctx) != 0 || npWebApiLibContextHasBusyHandles(ctx) != 0) {
				npWebApiLibContextUnlock(ctx);
				sceKernelDelayThread(50000);
				npWebApiLibContextLock(ctx);
			}
		}
		npWebApiLibContextUnlock(ctx);
		npWebApiLibContextRelease(ctx);
		return npWebApiDestroyLibContext(libCtxId);
	}
out:
	if (ctx != NULL)
		npWebApiLibContextRelease(ctx);
	return result;
}
