/* Full SCE native timer owner, adapted from pristine IMG osfunc.c. */
#ifndef GPU_OSFUNC_SCE_H
#define GPU_OSFUNC_SCE_H

#include <gpu/sce_ddk_port.h>
#include <systimer/systimer.h>

typedef IMG_VOID (*PFN_TIMER_FUNC)(IMG_VOID *pvData);

typedef enum _img_verify_test {
	PVR_VERIFY_WRITE = 0,
	PVR_VERIFY_READ
} IMG_VERIFY_TEST;

IMG_BOOL OSAccessOK(IMG_VERIFY_TEST eVerification, IMG_VOID *pvUserPtr,
		IMG_SIZE_T ui32Bytes);

typedef struct TIMER_CALLBACK_DATA_TAG {
	PFN_TIMER_FUNC pfnTimerFunc;
	IMG_VOID *pvData;
	/* SCE replaces Linux timer_list with a native system timer id. */
	SceSysTimerId sTimer;
	IMG_BOOL bActive;
} TIMER_CALLBACK_DATA;

_Static_assert(sizeof(TIMER_CALLBACK_DATA) == 16, "SCE timer owner");

/* the services' memory helpers (gpu_es4.c) */
int OSProcHasPrivSrvInit(void);
void *OSAllocMem(unsigned size);
int OSFreeMem(void *address);
void *OSMemCopy(void *destination, const void *source, unsigned size);
void *OSMemSet(void *destination, int value, unsigned size);

/* env/playstation/osfunc.c */
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);

/* env/playstation/osfunc.c */
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
IMG_INT32 OSSNPrintf(IMG_CHAR *pStr, IMG_UINT32 ui32Size, const IMG_CHAR *pszFormat, ...);
PVRSRV_ERROR OSCreateResource(PVRSRV_RESOURCE *psResource);
PVRSRV_ERROR OSDestroyResource(PVRSRV_RESOURCE *psResource);
PVRSRV_ERROR OSInitEnvData(IMG_VOID **ppvEnvSpecificData);
PVRSRV_ERROR OSDeInitEnvData(IMG_VOID *pvEnvSpecificData);
IMG_UINT32 OSClockus(IMG_VOID);
IMG_VOID OSWaitus(IMG_UINT32 ui32Timeus);
IMG_VOID OSSleepms(IMG_UINT32 ui32Timems);
PVRSRV_ERROR OSInstallSystemLISR(IMG_VOID *pvSysData, IMG_UINT32 ui32Irq);
PVRSRV_ERROR OSUninstallSystemLISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSInstallMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSUninstallMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSScheduleMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSLockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
PVRSRV_ERROR OSUnlockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
IMG_BOOL OSIsResourceLocked(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
PVRSRV_ERROR OSPowerLockWrap(IMG_VOID);
IMG_VOID OSPowerLockUnwrap(IMG_VOID);
IMG_UINT32 OSReadHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset);
IMG_VOID OSWriteHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Value);
IMG_HANDLE OSAddTimer(PFN_TIMER_FUNC pfnTimerFunc, IMG_VOID *pvData, IMG_UINT32 ui32MsTimeout);
PVRSRV_ERROR OSRemoveTimer(IMG_HANDLE hTimer);
PVRSRV_ERROR OSEnableTimer(IMG_HANDLE hTimer);
PVRSRV_ERROR OSDisableTimer(IMG_HANDLE hTimer);
PVRSRV_ERROR CopyToUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Bytes);
PVRSRV_ERROR CopyFromUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Bytes);
int InitGpuMemorySupport(void);
void GpuShutdownHook(void);
void GpuStartupHook(void);

#endif
