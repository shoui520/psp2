/* IMG devices/sgx/pb.c, GPL-2.0; pristine candidate and license are in
 * contrib/img-sgx-ddk-1.7.862890. SCE omits the create-lock assertion.
 */
#include <gpu/perproc_sce.h>
#include <gpu/resman_sce.h>
#include "pb_sce.h"
#include <gpu/device_sce.h>
#include <stddef.h>
#include <gpu/memory_sce.h>

#define PVRSRV_CLEANUPCMD_PB 0x5U
#define CLEANUP_WITH_POLL IMG_FALSE

PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);
RESMAN_ITEM *ResManRegisterRes(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32ResType, IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		RESMAN_FREE_FN pfnFreeResource);
PVRSRV_ERROR ResManFreeResByPtr(RESMAN_ITEM *psResItem,
		IMG_BOOL bForceCleanup);
PVRSRV_ERROR PVRSRVFreeSharedSysMemoryKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);
PVRSRV_ERROR PVRSRVDissociateMemFromResmanKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);
PVRSRV_ERROR SGXCleanupRequest(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR *psHWDataDevVAddr, IMG_UINT32 ui32CleanupType,
		IMG_BOOL bForceCleanup);

static RESMAN_ITEM *psResItemCreateSharedPB = NULL;
static PVRSRV_PER_PROCESS_DATA *psPerProcCreateSharedPB = NULL;

PVRSRV_ERROR SGXCleanupSharedPBDescCreateLockCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bDummy)
{
	(void)pvParam;
	(void)ui32Param;
	(void)bDummy;
	psPerProcCreateSharedPB = NULL;
	psResItemCreateSharedPB = NULL;
	return PVRSRV_OK;
}

/* Sony inlines the pristine SGXCleanupSharedPBDescKM into its callback. */
PVRSRV_ERROR SGXCleanupSharedPBDescCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bDummy)
{
	PVRSRV_STUB_PBDESC *psStubPBDescIn = pvParam;
	PVRSRV_DEVICE_NODE *psDeviceNode = psStubPBDescIn->hDevCookie;
	IMG_UINT32 i;

	(void)ui32Param;
	(void)bDummy;
	psStubPBDescIn->ui32RefCount--;
	if (psStubPBDescIn->ui32RefCount == 0) {
		IMG_DEV_VIRTADDR sHWPBDescDevVAddr =
				psStubPBDescIn->sHWPBDescDevVAddr;

		*psStubPBDescIn->ppsThis = psStubPBDescIn->psNext;
		if (psStubPBDescIn->psNext != NULL)
			psStubPBDescIn->psNext->ppsThis = psStubPBDescIn->ppsThis;
		for (i = 0; i < psStubPBDescIn->ui32SubKernelMemInfosCount; i++)
			PVRSRVFreeDeviceMemKM(psStubPBDescIn->hDevCookie,
					psStubPBDescIn->ppsSubKernelMemInfos[i]);
		OSFreeMem_Impl(2, sizeof(PVRSRV_KERNEL_MEM_INFO *) *
				psStubPBDescIn->ui32SubKernelMemInfosCount,
				psStubPBDescIn->ppsSubKernelMemInfos, NULL);
		psStubPBDescIn->ppsSubKernelMemInfos = NULL;
		PVRSRVFreeSharedSysMemoryKM(psStubPBDescIn->psBlockKernelMemInfo);
		PVRSRVFreeDeviceMemKM(psStubPBDescIn->hDevCookie,
				psStubPBDescIn->psHWBlockKernelMemInfo);
		PVRSRVFreeDeviceMemKM(psStubPBDescIn->hDevCookie,
				psStubPBDescIn->psHWPBDescKernelMemInfo);
		PVRSRVFreeSharedSysMemoryKM(psStubPBDescIn->psSharedPBDescKernelMemInfo);
		OSFreeMem_Impl(2, sizeof(PVRSRV_STUB_PBDESC), psStubPBDescIn, NULL);
		SGXCleanupRequest(psDeviceNode, &sHWPBDescDevVAddr,
				PVRSRV_CLEANUPCMD_PB, CLEANUP_WITH_POLL);
	}
	return PVRSRV_OK;
}

/* SCE omits IMG diagnostics and the size-mismatch warning. */
PVRSRV_ERROR SGXFindSharedPBDescKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevCookie, IMG_BOOL bLockOnFailure,
		IMG_UINT32 ui32TotalPBSize, IMG_HANDLE *phSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsHWPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsHWBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO ***pppsSharedPBDescSubKernelMemInfos,
		IMG_UINT32 *ui32SharedPBDescSubKernelMemInfosCount)
{
	PVRSRV_STUB_PBDESC *psStubPBDesc;
	PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescSubKernelMemInfos = NULL;
	PVRSRV_SGXDEV_INFO *psSGXDevInfo;
	PVRSRV_ERROR eError;

	(void)ui32TotalPBSize;
	psSGXDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookie)->pvDevice;
	psStubPBDesc = psSGXDevInfo->psStubPBDescListKM;
	if (psStubPBDesc != NULL) {
		IMG_UINT32 i;
		RESMAN_ITEM *psResItem;

		if (OSAllocMem_Impl(1, sizeof(PVRSRV_KERNEL_MEM_INFO *) *
				psStubPBDesc->ui32SubKernelMemInfosCount,
				(IMG_VOID **)&ppsSharedPBDescSubKernelMemInfos, NULL) != PVRSRV_OK) {
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto ExitNotFound;
		}
		psResItem = ResManRegisterRes(psPerProc->hResManContext,
				RESMAN_TYPE_SHARED_PB_DESC, psStubPBDesc, 0,
				SGXCleanupSharedPBDescCallback);
		if (psResItem == NULL) {
			OSFreeMem_Impl(2, sizeof(PVRSRV_KERNEL_MEM_INFO *) *
					psStubPBDesc->ui32SubKernelMemInfosCount,
					ppsSharedPBDescSubKernelMemInfos, NULL);
			eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
			goto ExitNotFound;
		}
		*ppsSharedPBDescKernelMemInfo = psStubPBDesc->psSharedPBDescKernelMemInfo;
		*ppsHWPBDescKernelMemInfo = psStubPBDesc->psHWPBDescKernelMemInfo;
		*ppsBlockKernelMemInfo = psStubPBDesc->psBlockKernelMemInfo;
		*ppsHWBlockKernelMemInfo = psStubPBDesc->psHWBlockKernelMemInfo;
		*ui32SharedPBDescSubKernelMemInfosCount = psStubPBDesc->ui32SubKernelMemInfosCount;
		*pppsSharedPBDescSubKernelMemInfos = ppsSharedPBDescSubKernelMemInfos;
		for (i = 0; i < psStubPBDesc->ui32SubKernelMemInfosCount; i++)
			ppsSharedPBDescSubKernelMemInfos[i] = psStubPBDesc->ppsSubKernelMemInfos[i];
		psStubPBDesc->ui32RefCount++;
		*phSharedPBDesc = psResItem;
		return PVRSRV_OK;
	}
	eError = PVRSRV_OK;
	if (bLockOnFailure) {
		if (psResItemCreateSharedPB == NULL) {
			psResItemCreateSharedPB = ResManRegisterRes(psPerProc->hResManContext,
					RESMAN_TYPE_SHARED_PB_DESC_CREATE_LOCK, psPerProc, 0,
					SGXCleanupSharedPBDescCreateLockCallback);
			if (psResItemCreateSharedPB == NULL) {
				eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
				goto ExitNotFound;
			}
			psPerProcCreateSharedPB = psPerProc;
		} else {
			eError = PVRSRV_ERROR_PROCESSING_BLOCKED;
		}
	}
ExitNotFound:
	*phSharedPBDesc = NULL;
	return eError;
}

/* SCE omits IMG's nonnull assertion. */
PVRSRV_ERROR SGXUnrefSharedPBDescKM(IMG_HANDLE hSharedPBDesc)
{
	return ResManFreeResByPtr(hSharedPBDesc, CLEANUP_WITH_POLL);
}

/* SCE retains IMG ownership transfer and unwind; diagnostics are omitted. */
PVRSRV_ERROR SGXAddSharedPBDescKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevCookie,
		PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo,
		IMG_UINT32 ui32TotalPBSize, IMG_HANDLE *phSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescSubKernelMemInfos,
		IMG_UINT32 ui32SharedPBDescSubKernelMemInfosCount,
		IMG_DEV_VIRTADDR sHWPBDescDevVAddr)
{
	PVRSRV_STUB_PBDESC *psStubPBDesc = NULL;
	PVRSRV_ERROR eRet = PVRSRV_ERROR_INVALID_PERPROC;
	IMG_UINT32 i;
	PVRSRV_SGXDEV_INFO *psSGXDevInfo;
	RESMAN_ITEM *psResItem;

	if (psPerProcCreateSharedPB != psPerProc)
		goto NoAdd;
	ResManFreeResByPtr(psResItemCreateSharedPB, CLEANUP_WITH_POLL);
	psSGXDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookie)->pvDevice;
	psStubPBDesc = psSGXDevInfo->psStubPBDescListKM;
	if (psStubPBDesc != NULL) {
		psResItem = ResManRegisterRes(psPerProc->hResManContext,
				RESMAN_TYPE_SHARED_PB_DESC, psStubPBDesc, 0,
				SGXCleanupSharedPBDescCallback);
		if (psResItem == NULL)
			goto NoAddKeepPB;
		psStubPBDesc->ui32RefCount++;
		*phSharedPBDesc = psResItem;
		eRet = PVRSRV_OK;
		goto NoAddKeepPB;
	}
	if (OSAllocMem_Impl(2, sizeof(PVRSRV_STUB_PBDESC),
			(IMG_VOID **)&psStubPBDesc, NULL) != PVRSRV_OK) {
		eRet = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto NoAdd;
	}
	psStubPBDesc->ppsSubKernelMemInfos = NULL;
	if (OSAllocMem_Impl(2, sizeof(PVRSRV_KERNEL_MEM_INFO *) *
			ui32SharedPBDescSubKernelMemInfosCount,
			(IMG_VOID **)&psStubPBDesc->ppsSubKernelMemInfos, NULL) != PVRSRV_OK) {
		eRet = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto NoAdd;
	}
	if (PVRSRVDissociateMemFromResmanKM(psSharedPBDescKernelMemInfo) != PVRSRV_OK)
		goto NoAdd;
	if (PVRSRVDissociateMemFromResmanKM(psHWPBDescKernelMemInfo) != PVRSRV_OK)
		goto NoAdd;
	if (PVRSRVDissociateMemFromResmanKM(psBlockKernelMemInfo) != PVRSRV_OK)
		goto NoAdd;
	if (PVRSRVDissociateMemFromResmanKM(psHWBlockKernelMemInfo) != PVRSRV_OK)
		goto NoAdd;
	psStubPBDesc->ui32RefCount = 1;
	psStubPBDesc->ui32TotalPBSize = ui32TotalPBSize;
	psStubPBDesc->psSharedPBDescKernelMemInfo = psSharedPBDescKernelMemInfo;
	psStubPBDesc->psHWPBDescKernelMemInfo = psHWPBDescKernelMemInfo;
	psStubPBDesc->psBlockKernelMemInfo = psBlockKernelMemInfo;
	psStubPBDesc->psHWBlockKernelMemInfo = psHWBlockKernelMemInfo;
	psStubPBDesc->ui32SubKernelMemInfosCount = ui32SharedPBDescSubKernelMemInfosCount;
	for (i = 0; i < ui32SharedPBDescSubKernelMemInfosCount; i++) {
		psStubPBDesc->ppsSubKernelMemInfos[i] = ppsSharedPBDescSubKernelMemInfos[i];
		if (PVRSRVDissociateMemFromResmanKM(ppsSharedPBDescSubKernelMemInfos[i]) != PVRSRV_OK)
			goto NoAdd;
	}
	psStubPBDesc->sHWPBDescDevVAddr = sHWPBDescDevVAddr;
	psResItem = ResManRegisterRes(psPerProc->hResManContext,
			RESMAN_TYPE_SHARED_PB_DESC, psStubPBDesc, 0,
			SGXCleanupSharedPBDescCallback);
	if (psResItem == NULL)
		goto NoAdd;
	psStubPBDesc->hDevCookie = hDevCookie;
	psStubPBDesc->ppsThis = &psSGXDevInfo->psStubPBDescListKM;
	psStubPBDesc->psNext = psSGXDevInfo->psStubPBDescListKM;
	psSGXDevInfo->psStubPBDescListKM = psStubPBDesc;
	if (psStubPBDesc->psNext != NULL)
		psStubPBDesc->psNext->ppsThis = &psStubPBDesc->psNext;
	*phSharedPBDesc = psResItem;
	return PVRSRV_OK;

NoAdd:
	if (psStubPBDesc != NULL) {
		if (psStubPBDesc->ppsSubKernelMemInfos != NULL) {
			OSFreeMem_Impl(2, sizeof(PVRSRV_KERNEL_MEM_INFO *) *
					ui32SharedPBDescSubKernelMemInfosCount,
					psStubPBDesc->ppsSubKernelMemInfos, NULL);
			psStubPBDesc->ppsSubKernelMemInfos = NULL;
		}
		OSFreeMem_Impl(2, sizeof(PVRSRV_STUB_PBDESC), psStubPBDesc, NULL);
	}
NoAddKeepPB:
	for (i = 0; i < ui32SharedPBDescSubKernelMemInfosCount; i++)
		PVRSRVFreeDeviceMemKM(hDevCookie, ppsSharedPBDescSubKernelMemInfos[i]);
	PVRSRVFreeSharedSysMemoryKM(psSharedPBDescKernelMemInfo);
	PVRSRVFreeDeviceMemKM(hDevCookie, psHWPBDescKernelMemInfo);
	PVRSRVFreeSharedSysMemoryKM(psBlockKernelMemInfo);
	PVRSRVFreeDeviceMemKM(hDevCookie, psHWBlockKernelMemInfo);
	return eRet;
}
