/*
 * SceDtcpIp: the shared memory heap. The library owns one memblock, named
 * "SceLibDtcpIpGlobalHeap", and allocates everything it hands the middleware (the uri
 * strings, the session objects) from a mspace over it. The small utility wrappers the
 * library's other files share sit here too (ours).
 */

#include <stdarg.h>
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include <dtcpip/error.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the shared heap: its mspace and the memblock it sits in */
struct SceDtcpIpHeap {
	void *mspace;
	SceUID memBlock;
};

static struct SceDtcpIpHeap g_heap = {
	.mspace = NULL,
	.memBlock = -1,
};

int dtcpIpCreateHeap(SceSize size)
{
	SceUID memBlock;
	int ret;
	void *base;

	memBlock = sceKernelAllocMemBlock("SceLibDtcpIpGlobalHeap", 0xC20D060, size, NULL);
	ret = memBlock;
	if (memBlock >= 0) {
		ret = sceKernelGetMemBlockBase(memBlock, &base);
		if (ret >= 0) {
			g_heap.mspace = sceClibMspaceCreate(base, size);
			if (g_heap.mspace != NULL) {
				g_heap.memBlock = memBlock;
				return 0;
			}
			ret = SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
		}
		if (memBlock != -1)
			sceKernelFreeMemBlock(memBlock);
	}
	return ret;
}

int dtcpIpDestroyHeap(void)
{
	int ret;

	if (g_heap.mspace != NULL) {
		sceClibMspaceIsHeapEmpty(g_heap.mspace);
		ret = sceClibMspaceDestroy(g_heap.mspace);
		if (ret < 0)
			return ret;
		g_heap.mspace = NULL;
	}
	if (g_heap.memBlock != -1) {
		sceKernelFreeMemBlock(g_heap.memBlock);
		g_heap.memBlock = -1;
	}
	return 0;
}

/* the library's allocator, and the adapter that takes another one */
void *dtcpIpHeapMalloc(SceSize size)
{
	return sceClibMspaceMalloc(g_heap.mspace, size);
}

void dtcpIpHeapFree(void *pointer)
{
	sceClibMspaceFree(g_heap.mspace, pointer);
}

void *dtcpIpAllocWith(void *(*allocator)(SceSize size), SceSize size)
{
	void *(*function)(SceSize) = allocator;

	if (function == NULL)
		function = dtcpIpHeapMalloc;
	return function(size);
}

void dtcpIpFreeWith(void (*deallocator)(void *pointer), void *pointer)
{
	void (*function)(void *) = deallocator;

	if (function == NULL)
		function = dtcpIpHeapFree;
	function(pointer);
}

/* the heap's allocator again, for the library's own objects (Sony's calls it whole) */
__attribute__((noinline)) void *dtcpIpMalloc(SceSize size)
{
	return sceClibMspaceMalloc(g_heap.mspace, size);
}

__attribute__((noinline)) void dtcpIpMspaceFree(void *pointer)
{
	sceClibMspaceFree(g_heap.mspace, pointer);
}

void *dtcpIpAlloc(SceSize size)
{
	return dtcpIpMalloc(size);
}

void dtcpIpFree(void *pointer)
{
	dtcpIpMspaceFree(pointer);
}

/* the library's shared print and string helpers */
int dtcpIpPrintf(const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	return sceClibVprintf(format, arguments);
}

int dtcpIpSnprintf(char *buffer, const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	return sceClibVsnprintf(buffer, 0x100000, format, arguments);
}

SceSize dtcpIpStrnlen(const char *string)
{
	return sceClibStrnlen(string, 0x100000);
}

char *dtcpIpStrncpy(char *destination, const char *source, SceSize count)
{
	return sceClibStrncpy(destination, source, count);
}
