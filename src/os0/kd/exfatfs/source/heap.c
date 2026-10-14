/* Common heap used by the FAT and exFAT implementations. */
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <sce/sysmem.h>
#include <sce/debug.h>

struct SceExfatfsDrive;

SceUInt32 g_heapLock;
SceUID g_commonHeapId = -1;
void *g_heapBlockCache[4];
int g_heapBlockCacheCount;

int sceFatfsInitializeHeap(int reuseHeap)
{
	SceKernelHeapCreateOpt opt;
	SceUID heapId;

	opt.size = sizeof(opt);
	opt.flags = HEAP_AUTO_EXTEND;
	if (reuseHeap) {
		heapId = g_commonHeapId;
		reuseHeap = 0;
	} else {
		if (g_commonHeapId != -1) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 65, "%s: already has heapid[%#x]\n",
					     "sceFatfsInitializeHeap", g_commonHeapId);
#endif
			return 0x80010011;
		}
		heapId = ksceKernelCreateHeap("SceExfatfsCommon", 0x7f000, &opt);
		g_commonHeapId = heapId;
	}
#if FW_VERSION >= 0x04000000
	if (heapId < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 74, "sceKernelCreateHeap failed (0x%08X)\n",
				     heapId);
		SCE_DBG_ASSERT(0, 75);
	}
#endif
	return reuseHeap;
}

int sceFatfsFinalizeHeap(int reuseHeap)
{
	int result = 0;

	if (!reuseHeap) {
		if (g_commonHeapId == -1) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 107, "%s: not have heapid[%#x]\n",
					     "sceFatfsFinalizeHeap", g_commonHeapId);
#endif
			return 0x80010002;
		}
		result = ksceKernelDeleteHeap(g_commonHeapId);
		if (result < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 112, "%s: sceKernelDeleteHeap failed[%#x]\n",
					     "sceFatfsFinalizeHeap", result);
#endif
		} else {
			g_commonHeapId = -1;
		}
		result = reuseHeap;
	}
	return result;
}

void *allocateHeapBuffer(struct SceExfatfsDrive *unusedDrive)
{
	void *buffer;
	int intrState;

	(void)unusedDrive;
	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_heapLock);
	if (g_heapBlockCacheCount <= 0)
		buffer = ksceKernelAllocHeapMemory(g_commonHeapId, 0x800);
	else
		buffer = g_heapBlockCache[--g_heapBlockCacheCount];
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_heapLock, intrState);
	return buffer;
}

void freeHeapBuffer(int unused, void *buffer)
{
	int intrState;
	int count;

	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_heapLock);
	count = g_heapBlockCacheCount;
	if (count > 3) {
		ksceKernelFreeHeapMemory(g_commonHeapId, buffer);
	} else {
		g_heapBlockCacheCount = count + 1;
		g_heapBlockCache[count] = buffer;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_heapLock, intrState);
}

void *allocateCommonHeapMemory(int unused, SceSize size)
{
	int intrState;
	void *memory;

	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_heapLock);
	memory = ksceKernelAllocHeapMemory(g_commonHeapId, size);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_heapLock, intrState);
	return memory;
}

void freeCommonHeapMemory(int unused, void *memory)
{
	int intrState;

	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_heapLock);
	ksceKernelFreeHeapMemory(g_commonHeapId, memory);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_heapLock, intrState);
}
