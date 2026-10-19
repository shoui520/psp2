/* IMG sgxinfokm.h shared-PB owner, GPL-2.0; see pristine vendor source. */
#ifndef PB_SCE_H
#define PB_SCE_H

#include <gpu/memory_sce.h>

typedef struct PVRSRV_STUB_PBDESC {
	IMG_UINT32 ui32RefCount;
	IMG_UINT32 ui32TotalPBSize;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO **ppsSubKernelMemInfos;
	IMG_UINT32 ui32SubKernelMemInfosCount;
	IMG_HANDLE hDevCookie;
	PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	struct PVRSRV_STUB_PBDESC *psNext;
	struct PVRSRV_STUB_PBDESC **ppsThis;
} PVRSRV_STUB_PBDESC;

_Static_assert(sizeof(PVRSRV_STUB_PBDESC) == 48, "shared PB descriptor");

#endif
