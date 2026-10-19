/* SCE system/environment owners, adapted from IMG syscommon.h/env_data.h.
 * The pristine candidate remains in contrib/img-sgx-ddk-1.7.862890.
 */
#ifndef GPU_SYSTEM_SCE_H
#define GPU_SYSTEM_SCE_H

#include <gpu/device_sce.h>
#include <stdarg.h>
#include <gpu/sgxinit_sce.h>

typedef struct _SYS_DATA_TAG_ SYS_DATA;
typedef struct _PVRSRV_POWER_DEV_TAG_ PVRSRV_POWER_DEV;

typedef struct _SYS_DEVICE_ID_TAG {
	IMG_UINT32 uiID;
	IMG_BOOL bInUse;
} SYS_DEVICE_ID;

/* SCE replaces Linux workqueues with a kernel thread and event flag. */
typedef struct _ENV_DATA_TAG {
	IMG_CHAR *pvBridgeData;
	IMG_BOOL bLISRInstalled;
	IMG_BOOL bMISRInstalled;
	IMG_UINT32 ui32IRQ;
	SYS_DATA *pvISRCookie;
	IMG_INT misrThread;
	IMG_INT misrEventFlag;
	IMG_BOOL misrRunning;
	IMG_VOID (*pfnMISR)(IMG_VOID *pvData);
	IMG_VOID *pvMISRData;
} ENV_DATA;

/* SCE omits generic queue/shared-sync state and the unused platform machinery.
 * Both 80-byte owners are zeroed by SysInitialise. All observed readers/writers
 * end at pvEnvSpecificData; the trailing five words remain unused.
 */
struct _SYS_DATA_TAG_ {
	IMG_UINT32 ui32NumDevices;
	SYS_DEVICE_ID sDeviceID[3];
	PVRSRV_DEVICE_NODE *psDeviceNodeList;
	PVRSRV_POWER_DEV *psPowerDeviceList;
	PVRSRV_RESOURCE sPowerStateChangeResource;
	PVRSRV_SYS_POWER_STATE eCurrentPowerState;
	PVRSRV_SYS_POWER_STATE eFailedPowerState;
	IMG_UINT32 ui32CurrentOSPowerState;
	ENV_DATA *pvEnvSpecificData;
	IMG_UINT32 reserved[5];
};

/* common/pvrsrv.c */
IMG_VOID PVRSRVDeInit(SYS_DATA *psSysData);
PVRSRV_ERROR PVRSRVInit(SYS_DATA *psSysData);
PVRSRV_ERROR PVRSRVRegisterDevice(SYS_DATA *psSysData,
		PVRSRV_ERROR (*pfnRegisterDevice)(PVRSRV_DEVICE_NODE *),
		IMG_UINT32 ui32SOCInterruptBit, IMG_UINT32 *pui32DeviceIndex);
PVRSRV_ERROR PVRSRVInitialiseDevice(IMG_UINT32 ui32DevIndex);
PVRSRV_ERROR PVRSRVDeinitialiseDevice(IMG_UINT32 ui32DevIndex);
PVRSRV_ERROR PollForValueKM(volatile IMG_UINT32 *pui32LinMemAddr,
		IMG_UINT32 ui32Value, IMG_UINT32 ui32Mask, IMG_UINT32 ui32Timeoutus,
		IMG_UINT32 ui32PollPeriodus, IMG_BOOL bAllowPreemption);
IMG_BOOL PVRSRVSystemLISR(IMG_VOID *pvSysData);
IMG_VOID PVRSRVMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR PVRSRVProcessConnect(IMG_UINT32 ui32PID, IMG_UINT32 ui32Flags);
IMG_VOID PVRSRVProcessDisconnect(IMG_UINT32 ui32PID);
IMG_VOID PVRSRVScheduleDeviceCallbacks(IMG_VOID);

/* device_sce.c */
PVRSRV_DEVICE_NODE *SetGpuDeviceNode(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_DEVICE_NODE *GetGpuDeviceNode(void);

/* env/playstation/mutex.c */
void LinuxInitMutex(SceUID *psPVRSRVMutex);
unsigned LinuxLockMutexForPid(SceUID *psPVRSRVMutex, SceUID pid);
unsigned LinuxLockMutex(SceUID *psPVRSRVMutex);
int LinuxTryLockMutex(SceUID *psPVRSRVMutex);
int LinuxUnLockMutex(SceUID *psPVRSRVMutex);

/* env/playstation/process.c */
int sceGpuProcessCreateEvent(SceUID pid);
int sceGpuProcessExitEvent(SceUID pid);
int sceGpuProcessKillEvent(SceUID pid);
int sceGpuProcessStopEvent(SceUID pid, int eventType);
int sceGpuProcessStartEvent(SceUID pid, int eventType);
int sceGpuSysEventHandler(int eventId, int eventType);

/* env/playstation/pvr_debug.c */
IMG_VOID PVRDPFInit(IMG_VOID);
IMG_VOID VBAppend(const IMG_CHAR *pszFormat, ...);

/* lists.c */
PVRSRV_ERROR List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(PVRSRV_DEVICE_NODE *psHead,
		PVRSRV_ERROR (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode));
IMG_VOID *List_PVRSRV_DEVICE_NODE_Any_va(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID *(*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode, va_list va), ...);
IMG_VOID List_PVRSRV_DEVICE_NODE_ForEach(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode));
IMG_VOID List_PVRSRV_DEVICE_NODE_ForEach_va(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode, va_list va), ...);
IMG_VOID List_PVRSRV_DEVICE_NODE_Insert(PVRSRV_DEVICE_NODE **ppsHead,
		PVRSRV_DEVICE_NODE *psNewNode);
IMG_VOID List_PVRSRV_DEVICE_NODE_Remove(PVRSRV_DEVICE_NODE *psNode);
IMG_VOID *List_PVRSRV_POWER_DEV_Any_va(PVRSRV_POWER_DEV *psHead,
		IMG_VOID *(*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...);
PVRSRV_ERROR List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(PVRSRV_POWER_DEV *psHead,
		PVRSRV_ERROR (*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...);
IMG_VOID List_PVRSRV_POWER_DEV_Insert(PVRSRV_POWER_DEV **ppsHead, PVRSRV_POWER_DEV *psNewNode);
IMG_VOID List_PVRSRV_POWER_DEV_Remove(PVRSRV_POWER_DEV *psNode);
IMG_VOID *MatchDeviceKM_AnyVaCb(PVRSRV_DEVICE_NODE *psDeviceNode, va_list va);
IMG_VOID *MatchPowerDeviceIndex_AnyVaCb(PVRSRV_POWER_DEV *psPowerDev, va_list va);

/* sysconfig_sce.c */
PVRSRV_ERROR SysFinalise(IMG_VOID);
PVRSRV_ERROR SysInitialise(IMG_VOID);
PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE eDeviceType, SGX_DEVICE_MAP **ppsDeviceMap);
IMG_UINT32 SysGetInterruptSource(IMG_VOID);
IMG_VOID SysClearInterrupts(SYS_DATA *psSysData, IMG_UINT32 ui32ClearBits);
PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState);
PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState);
PVRSRV_ERROR SysDevicePrePowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SysDevicePostPowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);

/* the lock owner of the kernel's own requests (PowerVR's osfunc.h) */
#define KERNEL_ID 0xffffffffU

#endif
