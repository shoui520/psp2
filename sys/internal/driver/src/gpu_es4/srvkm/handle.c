/* IMG handle core adapted for the SCE port. Pristine reference and license:
 * contrib/img-sgx-ddk-1.7.862890/services4/srvkm/common/handle.c.
 */
#include <stddef.h>
#include "handle-types.h"
#include <gpu/osfunc_sce.h>

IMG_UINT32 GetRemoteAllocProcess(void);
IMG_UINT32 SetRemoteAllocProcess(IMG_UINT32 owner);
extern PVRSRV_HANDLE_BASE *gpsKernelHandleBase;

/* IMG GetHandleStructure; SCE omits the generic diagnostic/debug-break paths. */
PVRSRV_ERROR GetHandleStructure(PVRSRV_HANDLE_BASE *psBase,
		struct sHandle **ppsHandle, IMG_UINT32 hHandle, IMG_UINT32 eType)
{
	IMG_UINT32 ui32Index = hHandle - 1;
	struct sHandle *psHandle;

	if (ui32Index >= psBase->ui32TotalHandCount)
		return PVRSRV_ERROR_HANDLE_INDEX_OUT_OF_RANGE;
	psHandle = psBase->psHandleArray[ui32Index >> 8].psHandle +
			(ui32Index & 0xff);
	if (psHandle->eType == 0)
		return PVRSRV_ERROR_HANDLE_NOT_ALLOCATED;
	if (eType != 0 && eType != psHandle->eType)
		return PVRSRV_ERROR_HANDLE_TYPE_MISMATCH;
	*ppsHandle = psHandle;
	return PVRSRV_OK;
}

/* IMG ReallocHandleArray, with generic diagnostics disabled by SCE. */
PVRSRV_ERROR ReallocHandleArray(PVRSRV_HANDLE_BASE *psBase,
		IMG_UINT32 ui32NewCount)
{
	struct sHandleIndex *psOldArray = psBase->psHandleArray;
	IMG_HANDLE hOldArrayBlockAlloc = psBase->hArrayBlockAlloc;
	IMG_UINT32 ui32OldCount = psBase->ui32TotalHandCount;
	struct sHandleIndex *psNewArray = 0;
	IMG_HANDLE hNewArrayBlockAlloc = 0;
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eReturn = PVRSRV_OK;
	IMG_UINT32 ui32Index;
	IMG_UINT32 previousOwner;

	if (ui32NewCount == ui32OldCount)
		return PVRSRV_OK;
	if (ui32NewCount && !psBase->bPurgingEnabled &&
	    ui32NewCount < ui32OldCount)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if ((ui32OldCount | ui32NewCount) & 0xff)
		return PVRSRV_ERROR_INVALID_PARAMS;

	/* SCE: kernel-base allocations belong to the kernel process. */
	previousOwner = GetRemoteAllocProcess();
	if (psBase == gpsKernelHandleBase)
		SetRemoteAllocProcess(0x10005);
	if (ui32NewCount) {
		eError = OSAllocMem_Impl(2,
				((ui32NewCount + 0xff) >> 8) * sizeof(*psNewArray),
				(IMG_VOID **)&psNewArray, &hNewArrayBlockAlloc);
		if (eError != PVRSRV_OK) {
			eReturn = eError;
			goto error;
		}
		if (ui32OldCount) {
			IMG_UINT32 ui32CopyCount = ui32NewCount < ui32OldCount ?
					ui32NewCount : ui32OldCount;

			OSMemCopy(psNewArray, psOldArray,
					((ui32CopyCount + 0xff) >> 8) * sizeof(*psNewArray));
		}
	}
	for (ui32Index = ui32NewCount; ui32Index < ui32OldCount;
	     ui32Index += 0x100) {
		struct sHandleIndex *psIndex = &psOldArray[ui32Index >> 8];

		OSFreeMem_Impl(1, sizeof(struct sHandle) * 0x100,
				psIndex->psHandle, psIndex->hBlockAlloc);
	}
	for (ui32Index = ui32OldCount; ui32Index < ui32NewCount;
	     ui32Index += 0x100) {
		struct sHandleIndex *psIndex = &psNewArray[ui32Index >> 8];

		eError = OSAllocMem_Impl(2, sizeof(struct sHandle) * 0x100,
				(IMG_VOID **)&psIndex->psHandle, &psIndex->hBlockAlloc);
		if (eError != PVRSRV_OK) {
			psIndex->psHandle = 0;
			eReturn = eError;
		} else {
			IMG_UINT32 ui32SubIndex;

			psIndex->ui32FreeHandBlockCount = 0x100;
			for (ui32SubIndex = 0; ui32SubIndex < 0x100; ui32SubIndex++) {
				struct sHandle *psHandle = psIndex->psHandle + ui32SubIndex;

				psHandle->ui32Index = ui32SubIndex + ui32Index;
				psHandle->eType = 0;
				psHandle->eInternalFlag = 0;
				psHandle->ui32NextIndexPlusOne = 0;
			}
		}
	}
	if (eReturn != PVRSRV_OK)
		goto error;
	if (psOldArray)
		OSFreeMem_Impl(1, ((ui32OldCount + 0xff) >> 8) * sizeof(*psOldArray),
				psOldArray, hOldArrayBlockAlloc);
	psBase->psHandleArray = psNewArray;
	psBase->hArrayBlockAlloc = hNewArrayBlockAlloc;
	psBase->ui32TotalHandCount = ui32NewCount;
	psBase->ui32FreeHandCount += ui32NewCount - ui32OldCount;
	if (ui32NewCount > ui32OldCount) {
		if (!psBase->ui32FirstFreeIndex) {
			psBase->ui32FirstFreeIndex = ui32OldCount;
		} else if (!psBase->bPurgingEnabled) {
			IMG_UINT32 ui32LastIndex = psBase->ui32LastFreeIndexPlusOne - 1;

			psNewArray[ui32LastIndex >> 8].psHandle[ui32LastIndex & 0xff].
					ui32NextIndexPlusOne = ui32OldCount + 1;
		}
		if (!psBase->bPurgingEnabled)
			psBase->ui32LastFreeIndexPlusOne = ui32NewCount;
	} else if (!ui32NewCount) {
		psBase->ui32FirstFreeIndex = 0;
		psBase->ui32LastFreeIndexPlusOne = 0;
	}
	/* SCE: restore the allocator owner on both exits. */
	SetRemoteAllocProcess(previousOwner);
	return PVRSRV_OK;

error:
	if (psNewArray) {
		for (ui32Index = ui32OldCount; ui32Index < ui32NewCount;
		     ui32Index += 0x100) {
			struct sHandleIndex *psIndex = &psNewArray[ui32Index >> 8];

			if (psIndex->psHandle)
				OSFreeMem_Impl(1, sizeof(struct sHandle) * 0x100,
						psIndex->psHandle, psIndex->hBlockAlloc);
		}
		OSFreeMem_Impl(1, ((ui32NewCount + 0xff) >> 8) * sizeof(*psNewArray),
				psNewArray, hNewArrayBlockAlloc);
	}
	SetRemoteAllocProcess(previousOwner);
	return eReturn;
}

/* IMG IncreaseHandleArraySize; SCE disables generic diagnostics. */
PVRSRV_ERROR IncreaseHandleArraySize(PVRSRV_HANDLE_BASE *psBase,
		IMG_UINT32 ui32Delta)
{
	IMG_UINT32 ui32DeltaAdjusted = (ui32Delta + 0xff) & ~0xffu;
	IMG_UINT32 ui32RequestedCount = psBase->ui32TotalHandCount +
			ui32DeltaAdjusted;
	IMG_UINT32 ui32NewTotalHandCount = psBase->ui32MaxIndexPlusOne;

	if (ui32RequestedCount <= ui32NewTotalHandCount &&
	    ui32RequestedCount > psBase->ui32TotalHandCount) {
		ui32NewTotalHandCount = ui32RequestedCount;
	} else {
		ui32DeltaAdjusted = ui32NewTotalHandCount - psBase->ui32TotalHandCount;
		if (ui32DeltaAdjusted < ui32Delta)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	return ReallocHandleArray(psBase, ui32NewTotalHandCount);
}

#define INDEX_TO_HANDLE_STRUCT_PTR(psBase, index) \
	(&(psBase)->psHandleArray[(index) >> 8].psHandle[(index) & 0xff])

/* IMG's list pointer of entry i: its child list when i is the parent, else its siblings */
#define LIST_PTR_FROM_INDEX_AND_OFFSET(psBase, i, p, po, eo) \
	((struct sHandleList *)((IMG_CHAR *)INDEX_TO_HANDLE_STRUCT_PTR(psBase, i) + \
		((i) == (p) ? (po) : (eo))))
#define CHILDREN_OFFSET offsetof(struct sHandle, sChildren)
#define SIBLINGS_OFFSET offsetof(struct sHandle, sSiblings)

/* IMG FreeHandle and its inlined list helpers, without generic diagnostics. */
PVRSRV_ERROR FreeHandle(PVRSRV_HANDLE_BASE *psBase, struct sHandle *psHandle)
{
	IMG_UINT32 aKey[3];
	IMG_UINT32 ui32Index = psHandle->ui32Index;
	IMG_UINT32 ui32EntryIndex;
	IMG_UINT32 ui32ChildIndex;
	IMG_UINT32 ui32Parent;
	PVRSRV_ERROR eError;

	aKey[0] = (IMG_UINT32)psHandle->pvData;
	aKey[1] = psHandle->eType;
	aKey[2] = psHandle->eFlag & 4 ? psHandle->sSiblings.hParent : 0;
	if (!(psHandle->eFlag & 2) && !(psHandle->eInternalFlag & 2))
		HASH_Remove_Extended(psBase->psHashTab, aKey);
	ui32EntryIndex = psHandle->ui32Index;
	if (psHandle->sSiblings.ui32Next != ui32EntryIndex) {
		struct sHandleList *psPrev = LIST_PTR_FROM_INDEX_AND_OFFSET(psBase,
				psHandle->sSiblings.ui32Prev, psHandle->sSiblings.hParent - 1,
				CHILDREN_OFFSET, SIBLINGS_OFFSET);
		struct sHandleList *psNext = LIST_PTR_FROM_INDEX_AND_OFFSET(psBase,
				psHandle->sSiblings.ui32Next, psHandle->sSiblings.hParent - 1,
				CHILDREN_OFFSET, SIBLINGS_OFFSET);

		psPrev->ui32Next = psHandle->sSiblings.ui32Next;
		psNext->ui32Prev = psHandle->sSiblings.ui32Prev;
		psHandle->sSiblings.ui32Prev = ui32EntryIndex;
		psHandle->sSiblings.ui32Next = ui32EntryIndex;
		psHandle->sSiblings.hParent = 0;
	}
	ui32Parent = psHandle->sChildren.hParent - 1;
	for (ui32ChildIndex = psHandle->sChildren.ui32Next;
	     ui32ChildIndex != ui32Parent;) {
		struct sHandle *psChild = INDEX_TO_HANDLE_STRUCT_PTR(psBase,
				ui32ChildIndex);

		ui32ChildIndex = psChild->sSiblings.ui32Next;
		eError = FreeHandle(psBase, psChild);
		if (eError != PVRSRV_OK)
			return eError;
	}
	psHandle->eType = 0;
	if ((psHandle->eInternalFlag & 3) == 1) {
		psHandle->eInternalFlag |= 2;
		return PVRSRV_OK;
	}
	if (!psBase->bPurgingEnabled) {
		if (!psBase->ui32FreeHandCount)
			psBase->ui32FirstFreeIndex = ui32Index;
		else
			INDEX_TO_HANDLE_STRUCT_PTR(psBase,
					psBase->ui32LastFreeIndexPlusOne - 1)->
					ui32NextIndexPlusOne = ui32Index + 1;
		psBase->ui32LastFreeIndexPlusOne = ui32Index + 1;
	}
	psBase->ui32FreeHandCount++;
	psBase->psHandleArray[ui32Index >> 8].ui32FreeHandBlockCount++;
	return PVRSRV_OK;
}

/* IMG batch completion; SCE omits generic diagnostics and assertions. */
PVRSRV_ERROR PVRSRVHandleBatchCommitOrRelease(PVRSRV_HANDLE_BASE *psBase,
		IMG_BOOL bCommit)
{
	IMG_UINT32 ui32IndexPlusOne = psBase->ui32FirstBatchIndexPlusOne;

	if (psBase->ui32BatchHandAllocFailures)
		bCommit = IMG_FALSE;
	while (ui32IndexPlusOne) {
		struct sHandle *psHandle = INDEX_TO_HANDLE_STRUCT_PTR(psBase,
				ui32IndexPlusOne - 1);

		ui32IndexPlusOne = psHandle->ui32NextIndexPlusOne;
		psHandle->ui32NextIndexPlusOne = 0;
		if (bCommit && !(psHandle->eInternalFlag & 2)) {
			psHandle->eInternalFlag &= ~1u;
		} else {
			if (!(psHandle->eInternalFlag & 2))
				psHandle->eInternalFlag &= ~1u;
			FreeHandle(psBase, psHandle);
		}
	}
	psBase->ui32HandBatchSize = 0;
	psBase->ui32TotalHandCountPreBatch = 0;
	psBase->ui32FirstBatchIndexPlusOne = 0;
	psBase->ui32BatchHandAllocFailures = 0;
	return PVRSRV_OK;
}

/* IMG AllocHandle under the SCE 32-bit token interface; diagnostics disabled. */
PVRSRV_ERROR AllocHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID *phHandle,
		IMG_VOID *pvData, IMG_UINT32 eType, IMG_UINT32 eFlag, IMG_SID hParent)
{
	IMG_UINT32 ui32NewIndex = 0x7fffff00;
	struct sHandle *psNewHandle = 0;
	IMG_SID hHandle;
	IMG_UINT32 aKey[3];
	PVRSRV_ERROR eError;

	if (!psBase->ui32FreeHandCount) {
		eError = IncreaseHandleArraySize(psBase, 1);
		if (eError != PVRSRV_OK)
			return eError;
	}
	if (!psBase->bPurgingEnabled) {
		ui32NewIndex = psBase->ui32FirstFreeIndex;
		psNewHandle = INDEX_TO_HANDLE_STRUCT_PTR(psBase, ui32NewIndex);
	} else {
		IMG_UINT32 ui32BlockedIndex;

		for (ui32BlockedIndex = psBase->ui32FirstFreeIndex & ~0xffu;
		     ui32BlockedIndex < psBase->ui32TotalHandCount;
		     ui32BlockedIndex += 0x100) {
			struct sHandleIndex *psIndex =
					&psBase->psHandleArray[ui32BlockedIndex >> 8];

			if (!psIndex->ui32FreeHandBlockCount)
				continue;
			for (ui32NewIndex = ui32BlockedIndex;
			     ui32NewIndex < ui32BlockedIndex + 0x100; ui32NewIndex++) {
				psNewHandle = INDEX_TO_HANDLE_STRUCT_PTR(psBase, ui32NewIndex);
				if (!psNewHandle->eType && !psNewHandle->eInternalFlag)
					break;
			}
		}
		psBase->ui32FirstFreeIndex = 0;
	}
	hHandle = ui32NewIndex + 1;
	if (!(eFlag & 2)) {
		aKey[0] = (IMG_UINT32)pvData;
		aKey[1] = eType;
		aKey[2] = hParent;
		if (!HASH_Insert_Extended(psBase->psHashTab, aKey, hHandle))
			return PVRSRV_ERROR_UNABLE_TO_ADD_HANDLE;
	}
	psBase->ui32FreeHandCount--;
	psBase->psHandleArray[ui32NewIndex >> 8].ui32FreeHandBlockCount--;
	if (!psBase->bPurgingEnabled) {
		if (!psBase->ui32FreeHandCount) {
			psBase->ui32LastFreeIndexPlusOne = 0;
			psBase->ui32FirstFreeIndex = 0;
		} else {
			psBase->ui32FirstFreeIndex = !psNewHandle->ui32NextIndexPlusOne ?
					ui32NewIndex + 1 : psNewHandle->ui32NextIndexPlusOne - 1;
		}
	}
	psNewHandle->eType = eType;
	psNewHandle->pvData = pvData;
	psNewHandle->eInternalFlag = 0;
	psNewHandle->eFlag = eFlag;
	psNewHandle->sChildren.ui32Prev = psNewHandle->ui32Index;
	psNewHandle->sChildren.ui32Next = psNewHandle->ui32Index;
	psNewHandle->sChildren.hParent = psNewHandle->ui32Index + 1;
	psNewHandle->sSiblings.ui32Prev = psNewHandle->ui32Index;
	psNewHandle->sSiblings.ui32Next = psNewHandle->ui32Index;
	psNewHandle->sSiblings.hParent = 0;
	if (psBase->ui32HandBatchSize) {
		psNewHandle->ui32NextIndexPlusOne = psBase->ui32FirstBatchIndexPlusOne;
		psBase->ui32FirstBatchIndexPlusOne = ui32NewIndex + 1;
		psNewHandle->eInternalFlag |= 1;
	} else {
		psNewHandle->ui32NextIndexPlusOne = 0;
	}
	*phHandle = hHandle;
	return PVRSRV_OK;
}

/* IMG PVRSRVAllocHandle; SCE omits diagnostic/debug-break paths. */
PVRSRV_ERROR PVRSRVAllocHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID *phHandle,
		IMG_VOID *pvData, IMG_UINT32 eType, IMG_UINT32 eFlag)
{
	IMG_SID hHandle = 0;
	PVRSRV_ERROR eError;

	*phHandle = 0;
	if (psBase->ui32HandBatchSize)
		psBase->ui32BatchHandAllocFailures++;
	if (!(eFlag & 2)) {
		IMG_UINT32 aKey[3];

		aKey[0] = (IMG_UINT32)pvData;
		aKey[1] = eType;
		aKey[2] = 0;
		hHandle = HASH_Retrieve_Extended(psBase->psHashTab, aKey);
		if (hHandle) {
			struct sHandle *psHandle;

			eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
			if (eError != PVRSRV_OK)
				return eError;
			if (!(psHandle->eFlag & eFlag & 1))
				return PVRSRV_ERROR_HANDLE_NOT_SHAREABLE;
			*phHandle = hHandle;
			goto exit;
		}
	}
	eError = AllocHandle(psBase, phHandle, pvData, eType, eFlag, 0);
exit:
	if (psBase->ui32HandBatchSize && eError == PVRSRV_OK)
		psBase->ui32BatchHandAllocFailures--;
	return eError;
}

/* IMG sub-handle allocation with inlined AdoptChild; SCE diagnostics omitted. */
PVRSRV_ERROR PVRSRVAllocSubHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID *phHandle,
		IMG_VOID *pvData, IMG_UINT32 eType, IMG_UINT32 eFlag, IMG_SID hParent)
{
	struct sHandle *psPHand;
	struct sHandle *psCHand;
	PVRSRV_ERROR eError;
	IMG_SID hParentKey;
	IMG_SID hHandle;

	*phHandle = 0;
	if (psBase->ui32HandBatchSize)
		psBase->ui32BatchHandAllocFailures++;
	hParentKey = eFlag & 4 ? hParent : 0;
	eError = GetHandleStructure(psBase, &psPHand, hParent, 0);
	if (eError != PVRSRV_OK)
		return eError;
	if (!(eFlag & 2)) {
		IMG_UINT32 aKey[3];

		aKey[0] = (IMG_UINT32)pvData;
		aKey[1] = eType;
		aKey[2] = hParentKey;
		hHandle = HASH_Retrieve_Extended(psBase->psHashTab, aKey);
		if (hHandle) {
			struct sHandle *psCHandle;

			eError = GetHandleStructure(psBase, &psCHandle, hHandle, eType);
			if (eError != PVRSRV_OK)
				return eError;
			if (!(psCHandle->eFlag & eFlag & 1) ||
			    INDEX_TO_HANDLE_STRUCT_PTR(psBase, hHandle - 1)->
					sSiblings.hParent != hParent)
				return PVRSRV_ERROR_HANDLE_NOT_SHAREABLE;
			*phHandle = hHandle;
			goto exit_ok;
		}
	}
	{
		struct sHandle *psPrev;
		struct sHandleList *psPrevIns;
		IMG_UINT32 ui32Parent;

		eError = AllocHandle(psBase, &hHandle, pvData, eType, eFlag, hParentKey);
		if (eError != PVRSRV_OK)
			return eError;
		psPHand = INDEX_TO_HANDLE_STRUCT_PTR(psBase, hParent - 1);
		psCHand = INDEX_TO_HANDLE_STRUCT_PTR(psBase, hHandle - 1);
		ui32Parent = psPHand->sChildren.hParent - 1;
		psPrev = INDEX_TO_HANDLE_STRUCT_PTR(psBase, psPHand->sChildren.ui32Prev);
		psPrevIns = psPHand->sChildren.ui32Prev == ui32Parent ?
				&psPrev->sChildren : &psPrev->sSiblings;
		psCHand->sSiblings.ui32Prev = psPHand->sChildren.ui32Prev;
		psPHand->sChildren.ui32Prev = psCHand->ui32Index;
		psCHand->sSiblings.ui32Next = ui32Parent;
		psPrevIns->ui32Next = psCHand->ui32Index;
		psCHand->sSiblings.hParent = psPHand->sChildren.hParent;
	}
	*phHandle = hHandle;
exit_ok:
	if (psBase->ui32HandBatchSize)
		psBase->ui32BatchHandAllocFailures--;
	return PVRSRV_OK;
}

/* IMG PVRSRVFindHandle; SCE disables generic diagnostics. */
PVRSRV_ERROR PVRSRVFindHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID *phHandle,
		IMG_VOID *pvData, IMG_UINT32 eType)
{
	IMG_UINT32 aKey[3];
	IMG_SID hHandle;

	aKey[0] = (IMG_UINT32)pvData;
	aKey[1] = eType;
	aKey[2] = 0;
	hHandle = HASH_Retrieve_Extended(psBase->psHashTab, aKey);
	if (!hHandle)
		return PVRSRV_ERROR_HANDLE_NOT_FOUND;
	*phHandle = hHandle;
	return PVRSRV_OK;
}

/* IMG lookup; SCE omits generic diagnostics/debug-break paths. */
PVRSRV_ERROR PVRSRVLookupHandle(PVRSRV_HANDLE_BASE *psBase, IMG_VOID **ppvData,
		IMG_SID hHandle, IMG_UINT32 eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK)
		return eError;
	*ppvData = psHandle->pvData;
	return PVRSRV_OK;
}

/* IMG release; SCE diagnostics omitted. */
PVRSRV_ERROR PVRSRVReleaseHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID hHandle,
		IMG_UINT32 eType)
{
	struct sHandle *psHandle;
	PVRSRV_ERROR eError;

	eError = GetHandleStructure(psBase, &psHandle, hHandle, eType);
	if (eError != PVRSRV_OK)
		return eError;
	return FreeHandle(psBase, psHandle);
}

/* IMG batch reservation; SCE diagnostics omitted. */
PVRSRV_ERROR PVRSRVNewHandleBatch(PVRSRV_HANDLE_BASE *psBase,
		IMG_UINT32 ui32BatchSize)
{
	PVRSRV_ERROR eError;

	if (psBase->ui32HandBatchSize)
		return PVRSRV_ERROR_HANDLE_BATCH_IN_USE;
	if (!ui32BatchSize)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psBase->ui32FreeHandCount < ui32BatchSize) {
		eError = IncreaseHandleArraySize(psBase,
				ui32BatchSize - psBase->ui32FreeHandCount);
		if (eError != PVRSRV_OK)
			return eError;
	}
	psBase->ui32HandBatchSize = ui32BatchSize;
	psBase->ui32TotalHandCountPreBatch = psBase->ui32TotalHandCount;
	return PVRSRV_OK;
}

/* SCE: the no-batch check is moved out of the IMG completion helper. */
PVRSRV_ERROR PVRSRVCommitHandleBatch(PVRSRV_HANDLE_BASE *psBase)
{
	if (!psBase->ui32HandBatchSize)
		return PVRSRV_ERROR_INVALID_PARAMS;
	return PVRSRVHandleBatchCommitOrRelease(psBase, IMG_TRUE);
}

/* SCE: releasing without an active batch is a no-op. */
IMG_VOID PVRSRVReleaseHandleBatch(PVRSRV_HANDLE_BASE *psBase)
{
	if (psBase->ui32HandBatchSize)
		PVRSRVHandleBatchCommitOrRelease(psBase, IMG_FALSE);
}

/* IMG base teardown and inlined FreeAllHandles/FreeHandleArray, no diagnostics. */
PVRSRV_ERROR FreeHandleBase(PVRSRV_HANDLE_BASE *psBase)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	if (psBase->ui32HandBatchSize)
		PVRSRVReleaseHandleBatch(psBase);
	if (psBase->ui32FreeHandCount != psBase->ui32TotalHandCount) {
		for (i = 0; i < psBase->ui32TotalHandCount; i++) {
			struct sHandle *psHandle = INDEX_TO_HANDLE_STRUCT_PTR(psBase, i);

			if (psHandle->eType) {
				eError = FreeHandle(psBase, psHandle);
				if (eError != PVRSRV_OK)
					return eError;
				if (psBase->ui32FreeHandCount == psBase->ui32TotalHandCount)
					break;
			}
		}
	}
	eError = ReallocHandleArray(psBase, 0);
	if (eError != PVRSRV_OK)
		return eError;
	if (psBase->psHashTab)
		HASH_Delete(psBase->psHashTab);
	return OSFreeMem_Impl(1, sizeof(*psBase), psBase, psBase->hBaseBlockAlloc);
}

/* IMG handle purging enablement; SCE diagnostics omitted. */
PVRSRV_ERROR PVRSRVEnableHandlePurging(PVRSRV_HANDLE_BASE *psBase)
{
	if (!psBase->bPurgingEnabled) {
		if (psBase->ui32TotalHandCount)
			return PVRSRV_ERROR_INVALID_PARAMS;
		psBase->bPurgingEnabled = IMG_TRUE;
	}
	return PVRSRV_OK;
}

/* IMG purging; SCE diagnostics omitted. */
PVRSRV_ERROR PVRSRVPurgeHandles(PVRSRV_HANDLE_BASE *psBase)
{
	IMG_UINT32 ui32BlockIndex;
	IMG_UINT32 ui32NewHandCount;

	if (!psBase->bPurgingEnabled)
		return PVRSRV_ERROR_NOT_SUPPORTED;
	if (psBase->ui32HandBatchSize)
		return PVRSRV_ERROR_INVALID_PARAMS;
	ui32BlockIndex = psBase->ui32TotalHandCount >> 8;
	while (ui32BlockIndex) {
		if (psBase->psHandleArray[ui32BlockIndex - 1].ui32FreeHandBlockCount != 0x100)
			break;
		ui32BlockIndex--;
	}
	ui32NewHandCount = ui32BlockIndex << 8;
	if (ui32NewHandCount <= psBase->ui32TotalHandCount / 2)
		return ReallocHandleArray(psBase, ui32NewHandCount);
	return PVRSRV_OK;
}

/* IMG public base teardown; generic assertion/diagnostics disabled by SCE. */
PVRSRV_ERROR PVRSRVFreeHandleBase(PVRSRV_HANDLE_BASE *psBase)
{
	return FreeHandleBase(psBase);
}

typedef IMG_UINT32 (*HashCallback)(IMG_UINT32 bytes, const IMG_VOID *key,
		IMG_UINT32 count);
typedef IMG_UINT32 (*KeyEqualCallback)(IMG_UINT32 bytes,
		const IMG_VOID *left, const IMG_VOID *right);

/* IMG base construction; SCE generic diagnostics omitted. */
PVRSRV_ERROR PVRSRVAllocHandleBase(PVRSRV_HANDLE_BASE **ppsBase)
{
	PVRSRV_HANDLE_BASE *psBase;
	IMG_HANDLE hBlockAlloc;
	PVRSRV_ERROR eError;

	eError = OSAllocMem_Impl(2, sizeof(*psBase), (IMG_VOID **)&psBase,
			&hBlockAlloc);
	if (eError != PVRSRV_OK)
		return eError;
	OSMemSet(psBase, 0, sizeof(*psBase));
	psBase->psHashTab = HASH_Create_Extended(32, 3 * sizeof(IMG_UINT32),
			(HASH_FUNC *)HASH_Func_Default, (HASH_KEY_COMP *)HASH_Key_Comp_Default);
	if (!psBase->psHashTab) {
		PVRSRVFreeHandleBase(psBase);
		return PVRSRV_ERROR_UNABLE_TO_CREATE_HASH_TABLE;
	}
	psBase->hBaseBlockAlloc = hBlockAlloc;
	psBase->ui32MaxIndexPlusOne = 0x7fffff00;
	*ppsBase = psBase;
	return PVRSRV_OK;
}

/* IMG kernel-base teardown; SCE generic diagnostics disabled. */
PVRSRV_ERROR PVRSRVHandleDeInit(IMG_VOID)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (gpsKernelHandleBase) {
		eError = FreeHandleBase(gpsKernelHandleBase);
		if (eError == PVRSRV_OK)
			gpsKernelHandleBase = 0;
	}
	return eError;
}

/* IMG kernel-base initialization; SCE generic diagnostics disabled. */
PVRSRV_ERROR PVRSRVHandleInit(IMG_VOID)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVAllocHandleBase(&gpsKernelHandleBase);
	if (eError != PVRSRV_OK)
		goto error;
	eError = PVRSRVEnableHandlePurging(gpsKernelHandleBase);
	if (eError != PVRSRV_OK)
		goto error;
	return PVRSRV_OK;
error:
	PVRSRVHandleDeInit();
	return eError;
}
