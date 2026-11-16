/*
 * SceNpWebApi: the handles a library context keeps (sceNpWebApiCreateHandle and friends). The names
 * are ours.
 */

#include "np_webapi_internal.h"

static unsigned int s_handleCounter;		/* 0x81010018 */

int npWebApiGetErrorCode(void)
{
	int result;

	if (g_sdkVersion < NP_WEBAPI_SDK_3_50)
		result = SceNpCommonPs4_FB63A152();
	else
		result = SceNpCommonPs4_CF591432();
	if (result < 0)
		return npWebApiConvertError(result);
	return result;
}

/* the handle list's insert: a ring of 12-byte nodes around a sentinel made when the first comes */
void npWebApiHandleListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiHandle **pHandle)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	NpWebApiHandle *handle;
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
	handle = *pHandle;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 12);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(12, memory);
			if (result != NULL) {
				result->handle = handle;
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

int npWebApiLibContextCreateHandle(NpWebApiLibContext *ctx)
{
	NpWebApiHandle *handle;
	NpWebApiHandleNode *node;
	unsigned int id;

	handle = (NpWebApiHandle *)SceNpCommonPs4_299ADFC3(ctx->heapBlock->poolObject, 16);
	if (handle != NULL) {
		sceClibMemset(handle, 0, 16);
		s_handleCounter++;
		id = s_handleCounter;
		if (s_handleCounter > 0xEFFFFFFF) {
			id = 1;
			s_handleCounter = 1;
		}
		handle->id = id;
		handle->object = 0;
		handle->state = 0;
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		npWebApiHandleListInsert(&node, &ctx->handles, &handle);
		if (node != NULL) {
			SceNpCommonPs4_4A1C1C3C(&ctx->lock);
			return handle->id;
		}
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	if (handle != NULL)
		SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, handle);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

int npWebApiCreateHandle(int libCtxId)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextCreateHandle(libCtx);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

int npWebApiHandleListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiHandleNode **pAt)
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

/* nonzero when the handle is not in the library context's list or has been told to abort */
int npWebApiLibContextIsHandleUnusable(NpWebApiLibContext *ctx, NpWebApiHandle *handle)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	int unusable;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	unusable = 1;
	if (node != sentinel) {
		while (node->handle != handle) {
			node = node->next;
			if (node == sentinel)
				goto done;
		}
		unusable = (handle->state & 2) != 0;
	}
done:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return unusable;
}

/* finds a handle by ID and takes a reference to it */
int npWebApiLibContextAcquireHandle(NpWebApiLibContext *ctx, int handleId, NpWebApiHandle **pHandle)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandle *handle;
	int result;

	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	result = 0;
	if (node == sentinel)
		goto notFound;
	while (1) {
		handle = node->handle;
		if (handle->id == handleId)
			break;
		node = node->next;
		if (node == sentinel)
			goto notFound;
	}
	if (handle == NULL) {
notFound:
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
	} else {
		handle->refCount++;
		if (pHandle != NULL)
			*pHandle = handle;
	}
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return result;
}

int npWebApiLibContextReleaseHandle(NpWebApiLibContext *ctx, NpWebApiHandle *handle)
{
	if (handle != NULL) {
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->refCount--;
		return SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	}
	return (int)ctx;
}

int npWebApiAbortHandle(int libCtxId, int handleId)
{
	NpWebApiLibContext *libCtx;
	NpWebApiHandle *handle;
	int result;

	handle = NULL;
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		result = npWebApiLibContextAcquireHandle(libCtx, handleId, &handle);
		if (result >= 0) {
			npWebApiLibContextLock(libCtx);
			npWebApiLibContextSetStarted(NULL, (NpWebApiLibContext *)handle);
			if (handle->object != 0)
				result = SceNpCommonPs4_68D08C92(handle->object);
			npWebApiLibContextUnlock(libCtx);
		}
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL) {
		if (handle != NULL)
			npWebApiLibContextReleaseHandle(libCtx, handle);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}

int npWebApiLibContextDeleteHandle(NpWebApiLibContext *ctx, int handleId)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *erased;
	NpWebApiHandleNode *at;
	NpWebApiHandle *handle;
	int result;
	int busy;

	result = 0;
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	sentinel = ctx->handles.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node == sentinel) {
		result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
		goto out;
	}
	while (node->handle->id != handleId) {
		node = node->next;
		if (node == sentinel) {
			result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
			goto out;
		}
	}
	if (npWebApiGetSdkVersion() >= NP_WEBAPI_SDK_3_30) {
		if (npWebApiLibContextIsHandleUnusable(ctx, node->handle) != 0) {
			result = NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
			goto out;
		}
		handle = node->handle;
		SceNpCommonPs4_F5840D6E(&ctx->lock);
		handle->state |= 2;
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		SceNpCommonPs4_4A1C1C3C(&ctx->lock);
		npWebApiAbortHandle(ctx->id, handleId);
		while (1) {
			SceNpCommonPs4_F5840D6E(&ctx->lock);
			handle = node->handle;
			handle->refCount++;
			if (handle != NULL) {
				SceNpCommonPs4_F5840D6E(&ctx->lock);
				busy = handle->refCount > 1;
				SceNpCommonPs4_4A1C1C3C(&ctx->lock);
				handle = node->handle;
			} else {
				busy = 0;
			}
			if (!busy)
				break;
			handle->refCount--;
			SceNpCommonPs4_4A1C1C3C(&ctx->lock);
			sceKernelDelayThread(50000);
		}
		handle->refCount--;
	}
	SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, handle);
	at = node;
	npWebApiHandleListErase(&erased, &ctx->handles, &at);
out:
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	return result;
}

int npWebApiDeleteHandle(int libCtxId, int handleId)
{
	NpWebApiLibContext *libCtx;
	int result;

	libCtx = npWebApiFindLibContext(libCtxId, 0);
	result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	if (libCtx != NULL) {
		result = npWebApiLibContextDeleteHandle(libCtx, handleId);
		npWebApiLibContextRelease(libCtx);
	}
	return result;
}
