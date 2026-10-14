/* IMG handle owners; names/layout from the pristine handle.c candidate.
 * SCE uses 32-bit handle IDs at the native boundary.
 */
#ifndef GPU_HANDLE_TYPES_H
#define GPU_HANDLE_TYPES_H
#include "sce_ddk_port.h"
#include "hash_sce.h"
typedef unsigned u32;
typedef int s32;
struct sHandleList {
	u32 ui32Prev;
	u32 ui32Next;
	u32 hParent;
};
struct sHandle {
	u32 eType;
	IMG_VOID *pvData;
	u32 ui32NextIndexPlusOne;
	u32 eInternalFlag;
	u32 eFlag;
	u32 ui32Index;
	struct sHandleList sChildren;
	struct sHandleList sSiblings;
};
struct sHandleIndex {
	struct sHandle *psHandle;
	IMG_HANDLE hBlockAlloc;
	u32 ui32FreeHandBlockCount;
};
struct _PVRSRV_HANDLE_BASE_ {
	IMG_HANDLE hBaseBlockAlloc;
	IMG_HANDLE hArrayBlockAlloc;
	struct sHandleIndex *psHandleArray;
	struct _HASH_TABLE_ *psHashTab;
	u32 ui32FreeHandCount;
	u32 ui32FirstFreeIndex;
	u32 ui32MaxIndexPlusOne;
	u32 ui32TotalHandCount;
	u32 ui32LastFreeIndexPlusOne;
	u32 ui32HandBatchSize;
	u32 ui32TotalHandCountPreBatch;
	u32 ui32FirstBatchIndexPlusOne;
	u32 ui32BatchHandAllocFailures;
	u32 bPurgingEnabled;
};
typedef struct _PVRSRV_HANDLE_BASE_ PVRSRV_HANDLE_BASE;
#endif
