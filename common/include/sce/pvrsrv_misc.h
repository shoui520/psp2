/* Complete SCE PowerVR miscellaneous-query and client-registration records. */
#pragma once

#include <psp2/types.h>
#include <sce/img_bool.h>

typedef struct PVRSRV_EVENTOBJECT {
	char szName[50];
	SceUInt32 hOSEventKM;		/* IMG_SID */
} PVRSRV_EVENTOBJECT;

typedef enum PVRSRV_MISC_INFO_CPUCACHEOP_TYPE {
	PVRSRV_MISC_INFO_CPUCACHEOP_NONE = 0,
	PVRSRV_MISC_INFO_CPUCACHEOP_CLEAN,
	PVRSRV_MISC_INFO_CPUCACHEOP_FLUSH
} PVRSRV_MISC_INFO_CPUCACHEOP_TYPE;

/* SCE moves the memory string first and retains strided cache controls. */
typedef struct PVRSRV_MISC_INFO {
	SceUInt32 ui32StateRequest;
	SceUInt32 ui32StatePresent;
	char *pszMemoryStr;
	SceUInt32 ui32MemoryStrLen;
	void *pvSOCTimerRegisterUM;
	SceUInt32 hSOCTimerRegisterOSMemHandle;
	SceUInt32 hSOCTimerRegisterMappingInfo;
	void *pvSOCClockGateRegs;
	SceUInt32 ui32SOCClockGateRegsSize;
	PVRSRV_EVENTOBJECT sGlobalEventObject;
	SceUInt32 hOSGlobalEvent;
	struct {
		IMG_BOOL bDeferOp;
		PVRSRV_MISC_INFO_CPUCACHEOP_TYPE eCacheOpType;
		void *pvBaseVAddr;
		SceUInt32 ui32Length;
		SceUInt8 *pbRowStart;
		SceUInt8 *pbRowEnd;
		SceUInt8 *pbRowThresh;
		IMG_BOOL bStridedCacheOp;
		SceUInt32 ui32Stride;
	} sCacheOpCtl;
	struct {
		SceUInt32 ui32RefCount;
	} sGetRefCountCtl;
	SceUInt32 aui32DDKVersion[4];
} PVRSRV_MISC_INFO;

typedef struct SGX_CLIENT_INFO {
	SceUInt32 ui32ProcessID;
	void *pvProcess;
	PVRSRV_MISC_INFO sMiscInfo;
	SceUInt32 asDevData[24];
} SGX_CLIENT_INFO;

_Static_assert(sizeof(IMG_BOOL) == 4, "IMG_BOOL size");
_Static_assert(sizeof(PVRSRV_EVENTOBJECT) == 56, "PVRSRV_EVENTOBJECT size");
_Static_assert(sizeof(PVRSRV_MISC_INFO) == 152, "PVRSRV_MISC_INFO size");
_Static_assert(sizeof(SGX_CLIENT_INFO) == 256, "SGX_CLIENT_INFO size");
