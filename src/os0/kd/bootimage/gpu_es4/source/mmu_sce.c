/* Sony devices/sgx/mmu_sce.c: native SCE replacement for DDK reset pages.
 * Pristine IMG implementation/license remain in the vendor directory.
 */
#include <sce/sysmem.h>
#include <sce/debug.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysmem/address_space.h>
#include "device_sce.h"
#include <stddef.h>

IMG_VOID *OSMemSet(IMG_VOID *pvDest, IMG_INT32 iValue, IMG_SIZE_T ui32Size);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_MMU_BIFResetPDAlloc_L25 = {
	0x2156171b, 0x8f3117a8, 0x6105f3ce,
	"devices/sgx/mmu_sce.c", 25, "MMU_BIFResetPDAlloc"
};
static const SceKernelDebugInfo dbg_MMU_BIFResetPDAlloc_L32 = {
	0x2156171b, 0xb4e6d187, 0x6105f3ce,
	"devices/sgx/mmu_sce.c", 32, "MMU_BIFResetPDAlloc"
};
static const SceKernelDebugInfo dbg_MMU_BIFResetPDAlloc_L38 = {
	0x2156171b, 0x2eed9d0d, 0x6105f3ce,
	"devices/sgx/mmu_sce.c", 38, "MMU_BIFResetPDAlloc"
};
static const SceKernelDebugInfo dbg_MMU_BIFResetPDAlloc_L44 = {
	0x2156171b, 0x6cb82a7c, 0x6105f3ce,
	"devices/sgx/mmu_sce.c", 44, "MMU_BIFResetPDAlloc"
};
#define BIF_PHYS_ERROR(line) \
	ksceKernelPrintfLevelWithInfo(1, 0, &dbg_MMU_BIFResetPDAlloc_L##line, \
			"%s: Failed to get physical address for %p with error %08x\n", \
			"MMU_BIFResetPDAlloc", pui32BIFResetPD, res)
#else
#define BIF_PHYS_ERROR(line) ((void)0)
#endif

PVRSRV_ERROR MMU_BIFResetPDAlloc(PVRSRV_SGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 *pui32BIFResetPD;
	SceUID uid;
	int res;

	uid = ksceKernelAllocMemBlock("SceGpuBifReset",
			SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, 0x3000, NULL);
	if (uid < 0)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	res = ksceKernelGetMemBlockBase(uid, (IMG_VOID **)&pui32BIFResetPD);
	if (res < 0) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0, &dbg_MMU_BIFResetPDAlloc_L25,
				"%s: Failed to get address for %08x with error %08x\n",
				"MMU_BIFResetPDAlloc", uid, res);
#endif
		goto fail;
	}
	res = ksceKernelVAtoPA(pui32BIFResetPD, &psDevInfo->sBIFResetPDDevPAddr.uiAddr);
	if (res < 0) {
		BIF_PHYS_ERROR(32);
		goto fail;
	}
	res = ksceKernelVAtoPA(pui32BIFResetPD + 1024,
			&psDevInfo->sBIFResetPTDevPAddr.uiAddr);
	if (res < 0) {
		BIF_PHYS_ERROR(38);
		goto fail;
	}
	res = ksceKernelVAtoPA(pui32BIFResetPD + 2048,
			&psDevInfo->sBIFResetPageDevPAddr.uiAddr);
	if (res < 0) {
		BIF_PHYS_ERROR(44);
		goto fail;
	}
	psDevInfo->pui32BIFResetPD = pui32BIFResetPD;
	psDevInfo->pui32BIFResetPT = pui32BIFResetPD + 1024;
	/* SCE stores the native memblock UID in this opaque DDK handle. */
	psDevInfo->hBIFResetPDOSMemHandle = (IMG_HANDLE)uid;
	OSMemSet(psDevInfo->pui32BIFResetPD, 0, 0x1000);
	OSMemSet(psDevInfo->pui32BIFResetPT, 0, 0x1000);
	OSMemSet(pui32BIFResetPD + 2048, 0xdb, 0x1000);
	return PVRSRV_OK;
fail:
	ksceKernelFreeMemBlock(uid);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

IMG_VOID MMU_BIFResetPDFree(PVRSRV_SGXDEV_INFO *psDevInfo)
{
	ksceKernelFreeMemBlock((SceUID)psDevInfo->hBIFResetPDOSMemHandle);
}
