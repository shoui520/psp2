/* IMG shared system memory, with the SCE allocation boundary kept separate
 * from pristine services4/srvkm/common/mem.c in the vendor tree.
 */
#include <gpu/memory_sce.h>
#include <gpu/perproc_sce.h>
#include <gpu/osfunc_sce.h>

/* IMG services.h allocation flags. */
#define PVRSRV_HAP_MAPTYPE_MASK 0x000f8000U
#define PVRSRV_HAP_MULTI_PROCESS (1U << 17)
#define RESMAN_TYPE_SHARED_MEM_INFO 18U

int FreeProcessGpuMemory(IMG_VOID *memory);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 flags, IMG_SIZE_T size,
		IMG_VOID *address, IMG_HANDLE handle);

/* SCE replaces OSFreePages with its per-process heap. The ordinary IMG
 * descriptor teardown and unused callback arguments are retained.
 */
PVRSRV_ERROR FreeSharedSysMemCallBack(IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		IMG_BOOL bDummy)
{
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo = pvParam;

	(void)ui32Param;
	(void)bDummy;
	FreeProcessGpuMemory(psKernelMemInfo->pvLinAddrKM);
	OSFreeMem_Impl(1, sizeof(*psKernelMemInfo), psKernelMemInfo, 0);
	return PVRSRV_OK;
}

PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 flags, IMG_SIZE_T size,
		IMG_VOID **output, IMG_HANDLE *handle);
IMG_VOID *AllocProcessGpuMemory(IMG_SIZE_T size);
struct Resource *ResManRegisterRes(struct ResourceContext *context,
		IMG_UINT32 resourceType, IMG_VOID *param, IMG_UINT32 value,
		PVRSRV_ERROR (*callback)(IMG_VOID *, IMG_UINT32));

/* SCE adds the process-reference argument and allocates from its process heap
 * instead of OSAllocPages. Registration and descriptor setup are IMG's.
 */
PVRSRV_ERROR PVRSRVAllocSharedSysMemoryKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_SID hMemBlockProcRef, IMG_UINT32 ui32Flags, IMG_SIZE_T uSize,
		PVRSRV_KERNEL_MEM_INFO **ppsKernelMemInfo)
{
	PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo;

	(void)hMemBlockProcRef;
	if (OSAllocMem_Impl(1, sizeof(*psKernelMemInfo),
			(IMG_VOID **)&psKernelMemInfo, 0) != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(psKernelMemInfo, 0, sizeof(*psKernelMemInfo));
	ui32Flags &= ~PVRSRV_HAP_MAPTYPE_MASK;
	ui32Flags |= PVRSRV_HAP_MULTI_PROCESS;
	psKernelMemInfo->ui32Flags = ui32Flags;
	psKernelMemInfo->uAllocSize = uSize;
	psKernelMemInfo->pvLinAddrKM = AllocProcessGpuMemory(uSize);
	if (!psKernelMemInfo->pvLinAddrKM) {
		OSFreeMem_Impl(1, sizeof(*psKernelMemInfo), psKernelMemInfo, 0);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	psKernelMemInfo->hResItem = ResManRegisterRes(psPerProc->hResManContext,
			RESMAN_TYPE_SHARED_MEM_INFO, psKernelMemInfo, 0,
			(PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32))FreeSharedSysMemCallBack);
	*ppsKernelMemInfo = psKernelMemInfo;
	return PVRSRV_OK;
}

PVRSRV_ERROR ResManFreeResByPtr(struct Resource *resource, IMG_BOOL forceCleanup);

/* IMG shared descriptor release; SCE resource cleanup uses IMG_FALSE. */
PVRSRV_ERROR PVRSRVFreeSharedSysMemoryKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	PVRSRV_ERROR eError;

	if (psKernelMemInfo->hResItem)
		eError = ResManFreeResByPtr(psKernelMemInfo->hResItem, IMG_FALSE);
	else
		eError = FreeSharedSysMemCallBack(psKernelMemInfo, 0, IMG_FALSE);
	return eError;
}

PVRSRV_ERROR ResManDissociateRes(struct Resource *resource,
		struct ResourceContext *context);

/* IMG resource detachment; diagnostic printing is disabled by SCE. */
PVRSRV_ERROR PVRSRVDissociateMemFromResmanKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!psKernelMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psKernelMemInfo->hResItem) {
		eError = ResManDissociateRes(psKernelMemInfo->hResItem, 0);
		if (eError != PVRSRV_OK)
			return eError;
		psKernelMemInfo->hResItem = 0;
	}
	return eError;
}
