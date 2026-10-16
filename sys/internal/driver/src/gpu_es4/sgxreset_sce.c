/* IMG devices/sgx/sgxreset.c, GPL-2.0. Pristine candidate and license
 * remain in contrib/img-sgx-ddk-1.7.862890.
 */
#include "device_sce.h"
#include "../../../../../contrib/img-sgx-ddk-1.7.862890/services4/srvkm/hwdefs/sgx543_v1.164defs.h"
#include "../../../../../contrib/img-sgx-ddk-1.7.862890/services4/srvkm/hwdefs/sgxmpdefs.h"

/* SCE MP configuration: eight BIF lists, with list seven reserved for EDM. */
#define SGX_FEATURE_BIF_NUM_DIRLISTS 8
#define SGX_BIF_DIR_LIST_INDEX_EDM 7

IMG_VOID OSWriteHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset,
		IMG_UINT32 ui32Value);
IMG_VOID SGXWaitClocks(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_UINT32 ui32SGXClocks);

/* SCE: PDUMP operations are absent; the original caller retains the argument. */
IMG_VOID SGXInitClocks(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_UINT32 ui32PDUMPFlags)
{
	IMG_UINT32 ui32RegVal;

	(void)ui32PDUMPFlags;
	ui32RegVal = psDevInfo->ui32ClkGateCtl;
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_CLKGATECTL, ui32RegVal);
	ui32RegVal = psDevInfo->ui32ClkGateCtl2;
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_CLKGATECTL2, ui32RegVal);
}

static inline IMG_VOID SGXResetInitBIFContexts(PVRSRV_SGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32DirList, ui32DirListReg;

	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_CTRL, 0);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_BANK_SET, 0);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_BANK0, 0);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_DIR_LIST_BASE0, 0);
	for (ui32DirList = 1; ui32DirList < SGX_FEATURE_BIF_NUM_DIRLISTS; ui32DirList++) {
		ui32DirListReg = EUR_CR_BIF_DIR_LIST_BASE1 + 4 * (ui32DirList - 1);
		OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32DirListReg, 0);
	}
}

static inline IMG_VOID SGXResetSetupBIFContexts(PVRSRV_SGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32RegVal, ui32EDMDirListReg;

	ui32RegVal = SGX_BIF_DIR_LIST_INDEX_EDM << EUR_CR_BIF_BANK0_INDEX_EDM_SHIFT;
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_BIF_BANK0, ui32RegVal);
	ui32EDMDirListReg = EUR_CR_BIF_DIR_LIST_BASE1 + 4 * (SGX_BIF_DIR_LIST_INDEX_EDM - 1);
	ui32RegVal = psDevInfo->sKernelPDDevPAddr.uiAddr;
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32EDMDirListReg, ui32RegVal);
}

IMG_VOID SGXReset(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_BOOL bHardwareRecovery,
		IMG_UINT32 ui32PDUMPFlags)
{
	IMG_UINT32 ui32RegVal;

	(void)bHardwareRecovery;
	ui32RegVal = EUR_CR_MASTER_SOFT_RESET_BIF_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_IPF_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_DPM_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_MCI_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_VDM_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_PTLA_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_SLC_RESET_MASK |
			EUR_CR_MASTER_SOFT_RESET_CORE_RESET_MASK(0) |
			EUR_CR_MASTER_SOFT_RESET_CORE_RESET_MASK(1) |
			EUR_CR_MASTER_SOFT_RESET_CORE_RESET_MASK(2) |
			EUR_CR_MASTER_SOFT_RESET_CORE_RESET_MASK(3);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_SOFT_RESET, ui32RegVal);
	/* SCE: SGXResetSleep reduces to its pristine SGXWaitClocks call. */
	SGXWaitClocks(psDevInfo, 100);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_BIF_CTRL, 0);
	ui32RegVal = EUR_CR_MASTER_SLC_CTRL_USSE_INVAL_REQ0_MASK |
			(4U << EUR_CR_MASTER_SLC_CTRL_ADDR_DECODE_MODE_SHIFT) |
			(0xcU << EUR_CR_MASTER_SLC_CTRL_ARB_PAGE_SIZE_SHIFT);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_SLC_CTRL, ui32RegVal);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_SLC_CTRL_BYPASS,
			EUR_CR_MASTER_SLC_CTRL_BYPASS_BYP_CC_MASK);
	SGXWaitClocks(psDevInfo, 100);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_MASTER_SOFT_RESET, 0);
	SGXWaitClocks(psDevInfo, 100);
	SGXInitClocks(psDevInfo, ui32PDUMPFlags);
	SGXWaitClocks(psDevInfo, 100);
	SGXResetInitBIFContexts(psDevInfo);
	SGXResetSetupBIFContexts(psDevInfo);
}
