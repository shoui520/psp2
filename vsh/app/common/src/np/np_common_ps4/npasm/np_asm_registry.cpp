/*
 * SceNpCommonPs4, NpAsm: the handles the exports hand out for requests (a list of them, under the layer's own mutex),
 * and the exports that act on a request through its handle.
 */

#include "np_asm.h"

extern "C" {

SceKernelLwMutexWork g_npAsmWrapMutex;		/* 0x8102b9c0 */
struct NpAsmHandle *g_npAsmHandleList;		/* 0x8102b07c */
SceUInt32 g_npAsmHandleIdCounter;		/* 0x8102b084 */
SceUInt8 g_npAsmSecondaryEnabled;		/* 0x8102b080 */

/* find the handle with the id the caller passed, under the mutex */
#define NP_ASM_FIND_HANDLE(handleId, handle, result) \
	do { \
		struct NpAsmHandle *scan_; \
		struct NpAsmHandle *next_; \
		if ((handleId) != 0) { \
			SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex); \
			scan_ = g_npAsmHandleList; \
			if (scan_ != NULL) { \
				while (1) { \
					next_ = scan_->next; \
					if (scan_->id == (handleId)) \
						break; \
					scan_ = next_; \
					if (next_ == NULL) \
						goto notFound; \
				} \
				(handle) = scan_; \
				SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex); \
			} else { \
notFound: \
				SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex); \
				(result) = NP_ASM_ERROR_INVALID_ARGUMENT; \
			} \
		} else { \
			(result) = NP_ASM_ERROR_INVALID_ARGUMENT; \
		} \
	} while (0)

/*SORT-START*/
int SceNpCommonPs4_AA96CC6A(void)
{
	return SceNpCommonPs4_4858EFB6(&g_npAsmWrapMutex, "SceNpAsmWrap", 1);
}

int SceNpCommonPs4_4EA26D7A(void)
{
	struct NpAsmHandle *handle;
	struct NpAsmHandle *next;
	struct NpCommonPs4Allocator allocator;

	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	handle = g_npAsmHandleList;
	if (handle != NULL) {
		do {
			next = handle->next;
			if (handle->secondary != 0) {
				SceNpCommonPs4_9AC24306(handle->requestId);
				SceNpCommonPs4_9C4F1236(handle->requestId);
			} else {
				SceNpCommonPs4_68D08C92(handle->requestId);
				SceNpCommonPs4_530CAFDA(handle->requestId);
			}
			allocator = handle->allocator;
			SceNpCommonPs4_6B428829(&allocator, handle);
			handle = next;
		} while (next != NULL);
	}
	g_npAsmHandleList = NULL;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
	return SceNpCommonPs4_29895EBB(&g_npAsmWrapMutex);
}

int SceNpCommonPs4_A525742F(const struct NpCommonPs4Allocator *pAllocator)
{
	struct NpAsmRequestParam param;
	struct NpCommonPs4Allocator allocator;
	struct NpAsmHandle *handle;
	struct NpAsmHandle *head;
	struct NpAsmHandle *scan;
	SceUInt32 id;
	int result;
	int found;

	sceClibMemset(&param, 0, 8);
	allocator = *pAllocator;
	handle = (struct NpAsmHandle *)SceNpCommonPs4_AA980113(&allocator, sizeof(struct NpAsmHandle));
	if (handle == NULL) {
		result = NP_ASM_ERROR_OUT_OF_MEMORY;
fail:
		if (handle != NULL)
			SceNpCommonPs4_6B428829(&allocator, handle);
		return result;
	}
	sceClibMemset(handle, 0, sizeof(struct NpAsmHandle));
	handle->allocator = *pAllocator;
	param.httpContextId = 0;
	param.allocator = &handle->allocator;
	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	if (g_npAsmSecondaryEnabled != 0) {
		result = SceNpCommonPs4_FB0B3FBD(&param);
		if (result < 0) {
			SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
			goto fail;
		}
		handle->secondary = 1;
	} else {
		result = SceNpCommonPs4_639086A3(&param);
		if (result < 0) {
			SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
			goto fail;
		}
	}
	handle->requestId = result;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	head = g_npAsmHandleList;
	for (id = g_npAsmHandleIdCounter + 1;; ++id) {
		g_npAsmHandleIdCounter = id;
		if (id > 0xEFFFFFFF) {
			id = 1;
			g_npAsmHandleIdCounter = 1;
		}
		scan = g_npAsmHandleList;
		if (scan == NULL)
			break;
		while (1) {
			found = scan->id == (int)id;
			scan = scan->next;
			if (found)
				break;
			if (scan == NULL)
				goto assigned;
		}
	}
assigned:
	handle->id = id;
	if (head != NULL)
		handle->next = head;
	g_npAsmHandleList = handle;
	SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
	return handle->id;
}

int SceNpCommonPs4_09C98408(int handleId)
{
	struct NpAsmHandle *handle;
	struct NpAsmHandle *previous;
	struct NpAsmHandle *scan;
	struct NpCommonPs4Allocator allocator;
	int result;

	handle = NULL;
	previous = NULL;
	result = 0;
	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	NP_ASM_FIND_HANDLE(handleId, handle, result);
	if (result >= 0) {
		if (handle->secondary != 0)
			result = SceNpCommonPs4_9C4F1236(handle->requestId);
		else
			result = SceNpCommonPs4_530CAFDA(handle->requestId);
		SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
		for (scan = g_npAsmHandleList; scan != NULL; scan = scan->next) {
			if (scan->id == handle->id && scan == handle)
				break;
			previous = scan;
		}
		if (scan != NULL) {
			if (previous != NULL)
				previous->next = scan->next;
			else
				g_npAsmHandleList = scan->next;
		}
		SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
		if (handle != NULL) {
			allocator = handle->allocator;
			SceNpCommonPs4_6B428829(&allocator, handle);
		}
	}
	SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
	return result;
}

int SceNpCommonPs4_602C6A9A(int handleId)
{
	struct NpAsmHandle *handle;
	int result;

	handle = NULL;
	result = 0;
	NP_ASM_FIND_HANDLE(handleId, handle, result);
	if (result >= 0) {
		if (handle->secondary != 0)
			return SceNpCommonPs4_9AC24306(handle->requestId);
		return SceNpCommonPs4_68D08C92(handle->requestId);
	}
	return result;
}

int SceNpCommonPs4_337328A1(int handleId, const char *id, const char *serviceName, unsigned int *pCount)
{
	struct NpAsmHandle *handle;
	int result;
	SceUInt8 npId[36];

	handle = NULL;
	result = 0;
	sceClibMemset(npId, 0, 36);
	NP_ASM_FIND_HANDLE(handleId, handle, result);
	if (result >= 0) {
		result = sceNpManagerGetNpId(npId);
		if (result >= 0) {
			if (handle->secondary != 0)
				result = npAsmSecondaryWrapRequestGetRelatedTitleIds(handle->requestId, npId, id, serviceName, pCount);
			else
				result = SceNpCommonPs4_B6CD5BEB(handle->requestId, npId, id, serviceName, pCount);
		}
	}
	return result;
}

int SceNpCommonPs4_CA8A4A5E(int handleId, void *buffer, unsigned int count)
{
	struct NpAsmHandle *handle;
	int result;

	handle = NULL;
	result = 0;
	NP_ASM_FIND_HANDLE(handleId, handle, result);
	if (result >= 0) {
		if (handle->secondary != 0)
			return npAsmSecondaryRequestCopyRelatedTitleIds(handle->requestId, buffer, count);
		return SceNpCommonPs4_E2FC8250(handle->requestId, buffer, count);
	}
	return result;
}

int SceNpCommonPs4_9C1D3462(int handleId, int *pMaxAge)
{
	struct NpAsmHandle *handle;
	int result;

	handle = NULL;
	result = 0;
	NP_ASM_FIND_HANDLE(handleId, handle, result);
	if (result >= 0) {
		if (handle->secondary != 0)
			return npAsmSecondaryRequestGetMaxAge(handle->requestId, pMaxAge);
		return npAsmWrapRequestGetMaxAge(handle->requestId, pMaxAge);
	}
	return result;
}

int SceNpCommonPs4_3C570D94(SceUInt8 *isSet, SceRtcTick *tick)
{
	return npAsmWrapGetTokenLimitation(isSet, tick);
}

int SceNpCommonPs4_32D6023F(void)
{
	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	g_npAsmSecondaryEnabled = 1;
	return SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
}

int SceNpCommonPs4_05C0A191(void)
{
	SceNpCommonPs4_F5840D6E(&g_npAsmWrapMutex);
	g_npAsmSecondaryEnabled = 0;
	return SceNpCommonPs4_4A1C1C3C(&g_npAsmWrapMutex);
}
/*SORT-END*/

}
