/*
 * SceLibHttp's lock and memory: the lock its lists and settings are changed under, the pool
 * sceHttpInit makes (an mspace in a memory block of its size), the default allocator on it,
 * the calls that take a template's allocator or fall back to the default, and zlib's
 * allocator and memcpy.
 */

#include <kernel/sysmem/memblock.h>

#include "http_internal.h"

SceKernelLwMutexWork g_httpLock;
void *g_httpPool = NULL;
SceUID g_httpPoolUid = -1;

int _sceHttpCreateLock(void)
{
	return sceKernelCreateLwMutex(&g_httpLock, "SceLibHttp", SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				      0, NULL);
}

int _sceHttpDeleteLock(void)
{
	return sceKernelDeleteLwMutex(&g_httpLock);
}

int _sceHttpLock(int which)
{
	return sceKernelLockLwMutex(which == 0 ? &g_httpLock : NULL, 1, NULL);
}

int _sceHttpUnlock(int which)
{
	return sceKernelUnlockLwMutex(which == 0 ? &g_httpLock : NULL, 1);
}

int _sceHttpPoolCreate(SceSize size)
{
	void *base;
	void *pool;
	SceUID uid;
	int ret;

	uid = sceKernelAllocMemBlock("SceLibhttpGlobalHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size,
				     NULL);
	if (uid < 0)
		return uid;
	ret = sceKernelGetMemBlockBase(uid, &base);
	if (ret >= 0) {
		pool = sceClibMspaceCreate(base, size);
		if (pool != NULL) {
			g_httpPoolUid = uid;
			g_httpPool = pool;
			return 0;
		}
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (uid != -1)
		sceKernelFreeMemBlock(uid);
	return ret;
}

int _sceHttpPoolDestroy(void)
{
	int ret;

	if (g_httpPool != NULL) {
		sceClibMspaceIsHeapEmpty(g_httpPool);
		ret = sceClibMspaceDestroy(g_httpPool);
		if (ret < 0)
			return ret;
		g_httpPool = NULL;
	}
	if (g_httpPoolUid != -1) {
		sceKernelFreeMemBlock(g_httpPoolUid);
		g_httpPoolUid = -1;
	}
	return 0;
}

int _sceHttpDefaultGetStats(SceSize *poolSize, SceSize *maxInuseSize, SceSize *currentInuseSize)
{
	struct MspaceStats stats;
	int ret;

	ret = sceClibMspaceMallocStats(g_httpPool, &stats);
	if (ret >= 0) {
		if (ret == 1) {
			ret = SCE_HTTP_ERROR_BROKEN;
		} else {
			if (poolSize != NULL)
				*poolSize = stats.capacity;
			if (currentInuseSize != NULL)
				*currentInuseSize = stats.currentInUse;
			if (maxInuseSize != NULL)
				*maxInuseSize = stats.peakInUse;
		}
	}
	return ret;
}

void *_sceHttpDefaultMalloc(SceSize size)
{
	struct MspaceStats stats;
	void *ptr;

	ptr = sceClibMspaceMalloc(g_httpPool, size);
	if (ptr == NULL)
		sceClibMspaceMallocStats(g_httpPool, &stats);
	return ptr;
}

void *_sceHttpDefaultRealloc(void *ptr, SceSize size)
{
	struct MspaceStats stats;
	void *p;

	p = sceClibMspaceRealloc(g_httpPool, ptr, size);
	if (p == NULL)
		sceClibMspaceMallocStats(g_httpPool, &stats);
	return p;
}

void _sceHttpDefaultFree(void *ptr)
{
	sceClibMspaceFree(g_httpPool, ptr);
}

void *_sceHttpMalloc(void *(*func)(SceSize size), SceSize size)
{
	if (func == NULL)
		func = _sceHttpDefaultMalloc;
	return func(size);
}

void *_sceHttpRealloc(void *(*func)(void *ptr, SceSize size), void *ptr, SceSize size)
{
	if (func == NULL)
		func = _sceHttpDefaultRealloc;
	return func(ptr, size);
}

void _sceHttpFree(void (*func)(void *ptr), void *ptr)
{
	if (func == NULL)
		func = _sceHttpDefaultFree;
	func(ptr);
}

int _sceHttpPoolGetStats(HttpPoolStatsFunc func, SceHttpMemoryPoolStats *stats)
{
	int ret;

	if (func == NULL)
		func = _sceHttpDefaultGetStats;
	ret = SCE_HTTP_ERROR_INVALID_VALUE;
	if (stats != NULL)
		return func(&stats->poolSize, &stats->maxInuseSize, &stats->currentInuseSize);
	return ret;
}

/* frees a list of allocations, each with its own free function */
int _sceHttpFreeList(HttpAllocation *list)
{
	HttpAllocation *next;
	void (*func)(void *ptr);

	for (; list != NULL; list = next) {
		func = list->free;
		if (func == NULL)
			func = _sceHttpDefaultFree;
		func(list->ptr);
		next = list->next;
		func(list);
	}
	return 0;
}

void zmemcpy(unsigned char *dest, const unsigned char *source, unsigned int len)
{
	sceClibMemcpy_safe(dest, source, len);
}

/* a request's inflate stream allocates with the request's allocator */
void *_sceHttpZalloc(void *opaque, unsigned int items, unsigned int size)
{
	HttpRequest *req = opaque;

	return _sceHttpMalloc(req->tmpl.allocator.malloc, items * size);
}

void _sceHttpZfree(void *opaque, void *ptr)
{
	HttpRequest *req = opaque;

	_sceHttpFree(req->tmpl.allocator.free, ptr);
}

void *zcalloc(void *opaque, unsigned int items, unsigned int size)
{
	HttpRequest *req = opaque;

	return _sceHttpMalloc(req->tmpl.allocator.malloc, items * size);
}

void zcfree(void *opaque, void *ptr)
{
	HttpRequest *req = opaque;

	_sceHttpFree(req->tmpl.allocator.free, ptr);
}
