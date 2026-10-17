/* Process-owned allocations and the GPU backup heap. */
#include <stdint.h>
#include "../../gpu_es4/srvkm/gpu-blockheap.h"
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysmem.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/hostmem.c"

extern int sceKernelFreeRemoteProcessHeap(unsigned pid, void *address);
extern void *OSAllocMem(unsigned size);
int InitKernelMemBlockHeap(void);

unsigned remoteAllocProcess = (unsigned)-1;
unsigned userFreeSuppressed;
int hostMemHeapUid = -1;
GpuMemBlockHeap *kernelMemBlockHeap;

unsigned GetRemoteAllocProcess(void)
{
	return remoteAllocProcess;
}

/* Original private names are unknown; lifecycle policy remains unresolved. */
unsigned SetRemoteAllocProcess(unsigned pid)
{
	unsigned previous = remoteAllocProcess;

	remoteAllocProcess = pid;
	return previous;
}

void *AllocProcessGpuMemory(unsigned size)
{
	return sceKernelAllocRemoteProcessHeap(remoteAllocProcess, size, 0);
}

int FreeProcessGpuMemory(void *address)
{
	if (userFreeSuppressed && remoteAllocProcess != 0x10005)
		return 0;
	return sceKernelFreeRemoteProcessHeap(remoteAllocProcess, address);
}

int sceGpuHostMemInit(void)
{
	int ret;

	ret = SceSysmemForDriver_9328E0E8("SceGpuBackup", 0x80000, 0);
	hostMemHeapUid = ret;
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 109, "%s: Failed to create memblock (%08x)\n",
			       __func__, ret);
		return hostMemHeapUid;
	}
	return 0;
}

/* SCE native memory initialization propagates the first failing operation. */
int sceGpuMemoryInit(void)
{
	int ret = sceGpuHostMemInit();

	if (ret == 0)
		ret = InitKernelMemBlockHeap();
	return ret;
}

int InitKernelMemBlockHeap(void)
{
	kernelMemBlockHeap = OSAllocMem(sizeof(*kernelMemBlockHeap));
	if (!kernelMemBlockHeap)
		return (int)0x804c0001;
	memset(kernelMemBlockHeap, 0, sizeof(*kernelMemBlockHeap));
	return 0;
}
