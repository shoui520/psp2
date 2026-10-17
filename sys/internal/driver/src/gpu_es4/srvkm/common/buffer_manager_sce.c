/* SCE replaces IMG's generic BM context with the native GPU memory context.
 * Upstream reference: services4/srvkm/common/buffer_manager.c in the pristine
 * img-sgx-ddk-1.7.862890 vendor tree; this Sony implementation is separate.
 */
#include "../device_sce.h"
#include "../gpu-mmu.h"
#include "../gpu-blockheap.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>

PVRSRV_DEVICE_NODE *SetGpuDeviceNode(PVRSRV_DEVICE_NODE *psDeviceNode);
int sceGpuInitMemoryContexts(void);
int GetKernelMemoryContext(GpuMemoryContext **output);
int sceGpuMmuAllocatePageTablePool(GpuPageTablePool *pool, unsigned pid);
int AllocateGpuPageDirectory(GpuMmuContext *context);
int RemoveMemoryContextFromList(GpuMemoryContext *context, unsigned pid);
int CreateGpuHeap(unsigned base, unsigned size, unsigned lookupKey,
		unsigned attributes, GpuHeap **output);
int AddGpuHeapToMemoryContext(GpuMemoryContext *context, GpuHeap *heap);
int RemoveGpuHeapFromMemoryContext(GpuMemoryContext *context, GpuHeap *heap);

IMG_HANDLE BM_CreateContext(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_PHYADDR *psPDDevPAddr, PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_BOOL *pbCreated)
{
	GpuMemoryContext *pBMContext;
	GpuProcessMemory *processMemory;
	unsigned groupId;
	int ret, uid, storage;

	SCE_DBG_ASSERT(psPerProc == 0, 22);
	if (pbCreated)
		*pbCreated = IMG_FALSE;
	if (!psDeviceNode || !psDeviceNode->pvDevice) {
		SCE_DBG_PRINTF(0, 28, "%s: Missing device node information.\n",
				__func__);
		return 0;
	}
	SetGpuDeviceNode(psDeviceNode);
	ret = sceGpuInitMemoryContexts();
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 35,
				"%s: Failed to initialise GPU memory contexts with error %08x.\n",
				__func__, ret);
		return 0;
	}
	ret = GetKernelMemoryContext(&pBMContext);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 41,
				"%s: Failed to get kernel memory context with error %08x.\n",
				__func__, ret);
		return 0;
	}
	ret = sceGpuMmuAllocatePageTablePool(&pBMContext->mmu.pageTablePool,
			0x10005);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 47,
				"%s: Failed to create kernel page table pool with error %08x.\n",
				__func__, ret);
		return 0;
	}
	uid = ksceKernelAllocMemBlock("SceGpuKernelDevMem", 0x10208006,
			0x80000, 0);
	if (uid < 0) {
		SCE_DBG_PRINTF(0, 53,
				"%s: Failed to create kernel device memory with error %08x.\n",
				__func__, uid);
		return 0;
	}
	groupId = 1;
	ret = sceGpuMemBlockAddBlock(GetKernelMemBlockHeap(), uid, &groupId, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 60,
				"%s: Failed to register kernel device memory with error %08x.\n",
				__func__, ret);
		return 0;
	}
	ret = AllocateGpuPageDirectory(&pBMContext->mmu);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 66,
				"%s: Failed to create kernel page directory with error %08x.\n",
				__func__, ret);
		return 0;
	}
	storage = ksceKernelCreateProcessLocalStorage("SceGpu", 16);
	if (storage < 0) {
		SCE_DBG_PRINTF(0, 72,
				"%s: Failed to register kernel process memory with error %08x.\n",
				__func__, storage);
	} else {
		pBMContext->processLocalStorageUid = storage;
		processMemory = ksceKernelGetProcessLocalStorageAddr(storage);
		if (processMemory) {
			processMemory->pageDirectoryMemblockUid =
				pBMContext->mmu.pageDirectoryMemblockUid;
			processMemory->pageDirectory = pBMContext->mmu.pageDirectory;
			processMemory->pageDirectoryPhysicalAddress =
				pBMContext->mmu.pageDirectoryPhysicalAddress;
			processMemory->reserved = 0;
		} else {
			SCE_DBG_PRINTF(0, 77, "%s: Failed to get kernel process memory.\n",
					__func__);
		}
	}
	psDeviceNode->sDevMemoryInfo.pBMKernelContext = pBMContext;
	if (psPDDevPAddr)
		psPDDevPAddr->uiAddr = pBMContext->mmu.pageDirectoryPhysicalAddress;
	if (pbCreated)
		*pbCreated = IMG_TRUE;
	return pBMContext;
}

/* SCE drops generic BM reference/resource cleanup in favor of the native list.
 * The native removal result is deliberately discarded in both TOOL originals.
 */
PVRSRV_ERROR BM_DestroyContext(IMG_HANDLE hBMContext, IMG_BOOL *pbDestroyed)
{
	GpuMemoryContext *pBMContext = hBMContext;

	if (pbDestroyed)
		*pbDestroyed = IMG_FALSE;
	if (!pBMContext)
		return PVRSRV_ERROR_INVALID_PARAMS;
	RemoveMemoryContextFromList(pBMContext, ksceKernelGetProcessId());
	if (pbDestroyed)
		*pbDestroyed = IMG_TRUE;
	return PVRSRV_OK;
}

/* SCE replaces IMG arenas with native GPU heaps and context links. */
IMG_HANDLE BM_CreateHeap(IMG_HANDLE hBMContext,
		DEVICE_MEMORY_HEAP_INFO *psDevMemHeapInfo)
{
	GpuMemoryContext *pBMContext = hBMContext;
	GpuHeap *psBMHeap;
	int ret;

	ret = CreateGpuHeap(psDevMemHeapInfo->sDevVAddrBase.uiAddr,
			psDevMemHeapInfo->ui32HeapSize, psDevMemHeapInfo->ui32HeapID,
			psDevMemHeapInfo->ui32Attribs, &psBMHeap);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 129,
				"%s: Failed to create GPU heap %08x with error %08x.\n",
				__func__, psDevMemHeapInfo->ui32HeapID, ret);
	if (psDevMemHeapInfo->DevMemHeapType == 3)
		psBMHeap->shared = 1;
	ret = AddGpuHeapToMemoryContext(pBMContext, psBMHeap);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 137,
				"%s: Failed to add GPU heap %08x to memory context with error %08x.\n",
				__func__, psDevMemHeapInfo->ui32HeapID, ret);
	return psBMHeap;
}

/* SCE heap destruction delegates to the native context's heap list. */
IMG_VOID BM_DestroyHeap(IMG_HANDLE hDevMemHeap)
{
	GpuHeap *psBMHeap = hDevMemHeap;

	if (psBMHeap && psBMHeap->memoryContext)
		RemoveGpuHeapFromMemoryContext(psBMHeap->memoryContext, psBMHeap);
}
