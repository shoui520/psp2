/* Licensed IMG client parameter-buffer component, unavailable in the
 * published 1.7@862890 kernel DDK. Recovered from the approved TOOL originals;
 * this is not attributed to Sony as a modification of pristine IMG source.
 */
#ifndef CLIENT_PB_H
#define CLIENT_PB_H

#include <gpu/memory_sce.h>

typedef struct SGX_PBDESC SGX_PBDESC;

typedef struct SGX_HWPBBLOCK {
	IMG_UINT32 ui32PageCount;
	IMG_UINT16 ui16Head;
	IMG_UINT16 ui16Tail;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	IMG_DEV_VIRTADDR sNextDevVAddr;
} SGX_HWPBBLOCK;

typedef struct SGX_PBBLOCK {
	IMG_UINT32 ui32PageCount;
	IMG_UINT16 ui16Head;
	IMG_UINT16 ui16Tail;
	PVRSRV_KERNEL_MEM_INFO *psParameterMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockMemInfo;
	SGX_HWPBBLOCK *psHWBlock;
	IMG_DEV_VIRTADDR sHWBlockDevVAddr;
	SGX_PBDESC *psPBDesc;
	struct SGX_PBBLOCK *psPrev;
	struct SGX_PBBLOCK *psNext;
} SGX_PBBLOCK;

typedef struct SGX_HWPBDESC {
	IMG_UINT32 ui32Control;
	IMG_UINT32 ui32TotalPages;
	IMG_UINT32 ui32InitialFreeListHT;
	IMG_UINT32 ui32InitialPrev;
	IMG_DEV_VIRTADDR sPageTableDevVAddr;
	IMG_UINT32 ui32AllocatedPages;
	IMG_UINT32 ui32FreeListHT;
	IMG_UINT32 ui32Prev;
	IMG_UINT32 ui32LocalPages;
	IMG_UINT32 ui32GlobalPages;
	IMG_UINT32 ui32TAThreshold;
	IMG_UINT32 ui32ZLSThreshold;
	IMG_UINT32 ui32GlobalThreshold;
	IMG_UINT32 ui32PDSThreshold;
	IMG_DEV_VIRTADDR sPBBlockListDevVAddr;
} SGX_HWPBDESC;

struct SGX_PBDESC {
	IMG_UINT32 ui32Control;
	IMG_UINT32 ui32UsableSize;
	IMG_UINT32 ui32TotalPages;
	IMG_UINT32 ui32InitialFreeListHT;
	IMG_UINT32 ui32InitialPrev;
	IMG_UINT32 ui32TAThreshold;
	IMG_UINT32 ui32ZLSThreshold;
	IMG_UINT32 ui32GlobalThreshold;
	IMG_UINT32 ui32PDSThreshold;
	IMG_DEV_VIRTADDR sPBBaseDevVAddr;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	SGX_PBBLOCK *psPBBlockList;
	SGX_PBBLOCK *psPBBlockListTail;
	SGX_HWPBDESC *psHWPBDesc;
	IMG_BOOL bPerContextPB;
	IMG_BOOL bDetachedBlockAllocation;
};

typedef struct SGX_CLIENTPBDESC {
	IMG_HANDLE hSharedPBDesc;
	PVRSRV_KERNEL_MEM_INFO *psSharedPBDescMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psPageTableMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psPBBlockMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psHWBlockMemInfo;
	SGX_PBDESC *psPBDesc;
} SGX_CLIENTPBDESC;

_Static_assert(sizeof(SGX_HWPBBLOCK) == 16, "IMG hardware PB block");
_Static_assert(sizeof(SGX_PBBLOCK) == 36, "IMG client PB block");
_Static_assert(sizeof(SGX_HWPBDESC) == 60, "IMG hardware PB descriptor");
_Static_assert(sizeof(SGX_PBDESC) == 64, "IMG client PB descriptor");
_Static_assert(sizeof(SGX_CLIENTPBDESC) == 28, "IMG client PB wrapper");

#endif
