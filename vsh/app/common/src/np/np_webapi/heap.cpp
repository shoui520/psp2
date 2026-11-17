/*
 * SceNpWebApi: the library's own heap (an mspace over a static buffer) and the small wrappers that
 * allocate from a context's pool. The names are ours.
 */

#include "np_webapi_internal.h"

static unsigned char s_heapReady;		/* 0x8101001c */
static void *s_heapMspace;		/* 0x81010068 */
static unsigned char s_heapBuffer[0x2000];	/* 0x81010070 */

int npWebApiGlobalLockDestroy(void)
{
	return SceNpCommonPs4_29895EBB(&g_globalLock);
}

void *npWebApiMspaceAlloc(SceSize size, void *mspace)
{
	if (mspace != NULL)
		return sceClibMspaceMalloc(mspace, size);
	return NULL;
}

void npWebApiMspaceFree(void *ptr, void *mspace)
{
	if (mspace != NULL)
		sceClibMspaceFree(mspace, ptr);
}

void *npWebApiMspaceRealloc(void *ptr, SceSize size, void *mspace)
{
	if (mspace != NULL)
		return sceClibMspaceRealloc(mspace, ptr, size);
	return NULL;
}

int npWebApiMspaceGetStats(void *mspace, SceSize *stats)
{
	struct MspaceStats mspaceStats[2];
	int result;

	if (mspace != NULL && stats != NULL) {
		sceClibMemset(mspaceStats, 0, sizeof(mspaceStats));
		result = sceClibMspaceMallocStats(mspace, mspaceStats);
		if (result >= 0) {
			stats[1] = mspaceStats[0].peakInUse;
			stats[2] = mspaceStats[0].currentInUse;
			stats[0] = mspaceStats[0].capacity;
		}
	} else {
		result = NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int npWebApiHeapCreate(void)
{
	if (s_heapReady != 0)
		return NP_WEBAPI_ERROR_ALREADY_INITIALIZED;
	s_heapMspace = sceClibMspaceCreate(s_heapBuffer, sizeof(s_heapBuffer));
	if (s_heapMspace == NULL)
		return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	s_heapReady = 1;
	return 0;
}

void npWebApiHeapDestroy(void)
{
	if (s_heapReady != 0) {
		sceClibMspaceDestroy(s_heapMspace);
		s_heapReady = 0;
	}
}

int npWebApiHeapGet(void **pMspace)
{
	if (s_heapReady == 0)
		return NP_WEBAPI_ERROR_NOT_INITIALIZED;
	*pMspace = s_heapMspace;
	return 0;
}
