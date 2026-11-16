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
	int count;

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
		handle->reserved4 = 0;
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
