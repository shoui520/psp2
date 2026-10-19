/* IMG hash implementation with explicitly marked SCE portability changes. */
#include "hash_sce.h"
#include <gpu/osfunc_sce.h>
typedef unsigned u32;
typedef int s32;

/* IMG 1.7 default hash; SCE assertion disabled. ARM pointer words are 32-bit. */
u32 HASH_Func_Default(u32 uKeySize, void *pKey, u32 uHashTabLen)
{
	u32 *p = pKey;
	u32 uKeyLen = uKeySize / sizeof(*p);
	u32 ui;
	u32 uHashKey = 0;

	(void)uHashTabLen;
	for (ui = 0; ui < uKeyLen; ui++) {
		u32 uHashPart = *p++;

		uHashPart += uHashPart << 12;
		uHashPart ^= uHashPart >> 22;
		uHashPart += uHashPart << 4;
		uHashPart ^= uHashPart >> 9;
		uHashPart += uHashPart << 10;
		uHashPart ^= uHashPart >> 2;
		uHashPart += uHashPart << 7;
		uHashPart ^= uHashPart >> 12;
		uHashKey += uHashPart;
	}
	return uHashKey;
}

/* IMG 1.7 default comparator; SCE assertion disabled. */
u32 HASH_Key_Comp_Default(u32 uKeySize, void *pKey1, void *pKey2)
{
	u32 *p1 = pKey1;
	u32 *p2 = pKey2;
	u32 uKeyLen = uKeySize / sizeof(*p1);
	u32 ui;

	for (ui = 0; ui < uKeyLen; ui++) {
		if (*p1++ != *p2++)
			return 0;
	}
	return 1;
}

/* IMG _Resize with _Rehash/_ChainInsert inlined by Sony. SCE removes
 * the redundant equal-size check and the failed-rehash allocation cleanup.
 */
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 flags, IMG_SIZE_T size,
		IMG_VOID **output, IMG_HANDLE *handle);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 flags, IMG_SIZE_T size,
		IMG_VOID *address, IMG_HANDLE handle);

IMG_BOOL _Resize(HASH_TABLE *pHash, IMG_UINT32 uNewSize)
{
	BUCKET **ppNewTable;
	BUCKET **ppOldTable;
	IMG_UINT32 uOldSize;
	IMG_UINT32 uIndex;

	OSAllocMem_Impl(1, sizeof(*ppNewTable) * uNewSize,
			(IMG_VOID **)&ppNewTable, 0);
	if (!ppNewTable)
		return IMG_FALSE;
	for (uIndex = 0; uIndex < uNewSize; uIndex++)
		ppNewTable[uIndex] = 0;
	ppOldTable = pHash->ppBucketTable;
	uOldSize = pHash->uSize;
	for (uIndex = 0; uIndex < uOldSize; uIndex++) {
		BUCKET *pBucket = ppOldTable[uIndex];

		while (pBucket) {
			BUCKET *pNextBucket = pBucket->pNext;
			IMG_UINT32 uSlot;

			if (!pBucket || !ppNewTable || !uNewSize)
				return IMG_FALSE;
			uSlot = pHash->pfnHashFunc(pHash->uKeySize, pBucket->k,
					uNewSize) % uNewSize;
			pBucket->pNext = ppNewTable[uSlot];
			ppNewTable[uSlot] = pBucket;
			pBucket = pNextBucket;
		}
	}
	OSFreeMem_Impl(1, sizeof(*pHash->ppBucketTable) * pHash->uSize,
			pHash->ppBucketTable, 0);
	pHash->ppBucketTable = ppNewTable;
	pHash->uSize = uNewSize;
	return IMG_TRUE;
}

/* IMG constructor; SCE diagnostics disabled, native OS allocation boundary. */
HASH_TABLE *HASH_Create_Extended(IMG_UINT32 uInitialLen, IMG_SIZE_T uKeySize,
		HASH_FUNC *pfnHashFunc, HASH_KEY_COMP *pfnKeyComp)
{
	HASH_TABLE *pHash;
	IMG_UINT32 uIndex;

	if (OSAllocMem_Impl(1, sizeof(*pHash), (IMG_VOID **)&pHash, 0) != PVRSRV_OK)
		return 0;
	pHash->uCount = 0;
	pHash->uSize = uInitialLen;
	pHash->uMinimumSize = uInitialLen;
	pHash->uKeySize = uKeySize;
	pHash->pfnHashFunc = pfnHashFunc;
	pHash->pfnKeyComp = pfnKeyComp;
	OSAllocMem_Impl(1, sizeof(*pHash->ppBucketTable) * pHash->uSize,
			(IMG_VOID **)&pHash->ppBucketTable, 0);
	if (!pHash->ppBucketTable) {
		OSFreeMem_Impl(1, sizeof(*pHash), pHash, 0);
		return 0;
	}
	for (uIndex = 0; uIndex < pHash->uSize; uIndex++)
		pHash->ppBucketTable[uIndex] = 0;
	return pHash;
}

/* IMG single-word hash constructor. */
HASH_TABLE *HASH_Create(IMG_UINT32 uInitialLen)
{
	return HASH_Create_Extended(uInitialLen, sizeof(IMG_UINTPTR_T),
			HASH_Func_Default, (HASH_KEY_COMP *)HASH_Key_Comp_Default);
}

/* IMG deletion; SCE leak diagnostics/assertion disabled. */
IMG_VOID HASH_Delete(HASH_TABLE *pHash)
{
	if (pHash) {
		OSFreeMem_Impl(1, sizeof(*pHash->ppBucketTable) * pHash->uSize,
				pHash->ppBucketTable, 0);
		pHash->ppBucketTable = 0;
		OSFreeMem_Impl(1, sizeof(*pHash), pHash, 0);
	}
}

/* IMG insertion with _ChainInsert inlined; SCE skips equal-size growth. */
IMG_BOOL HASH_Insert_Extended(HASH_TABLE *pHash, IMG_VOID *pKey, IMG_UINTPTR_T v)
{
	BUCKET *pBucket;
	BUCKET **ppBucketTable;
	IMG_UINT32 uSize;
	IMG_UINT32 uIndex;

	if (!pHash)
		return IMG_FALSE;
	if (OSAllocMem_Impl(1, sizeof(*pBucket) + pHash->uKeySize,
			(IMG_VOID **)&pBucket, 0) != PVRSRV_OK)
		return IMG_FALSE;
	pBucket->v = v;
	OSMemCopy(pBucket->k, pKey, pHash->uKeySize);
	ppBucketTable = pHash->ppBucketTable;
	uSize = pHash->uSize;
	if (!pBucket || !ppBucketTable || !uSize) {
		OSFreeMem_Impl(1, sizeof(*pBucket) + pHash->uKeySize, pBucket, 0);
		return IMG_FALSE;
	}
	uIndex = pHash->pfnHashFunc(pHash->uKeySize, pBucket->k, uSize) % uSize;
	pBucket->pNext = ppBucketTable[uIndex];
	ppBucketTable[uIndex] = pBucket;
	pHash->uCount++;
	if (pHash->uCount << 1 > pHash->uSize) {
		IMG_UINT32 uNewSize = pHash->uSize << 1;

		if (uNewSize != pHash->uSize)
			_Resize(pHash, uNewSize);
	}
	return IMG_TRUE;
}

/* IMG single-word insertion; SCE diagnostics disabled. */
IMG_BOOL HASH_Insert(HASH_TABLE *pHash, IMG_UINTPTR_T k, IMG_UINTPTR_T v)
{
	return HASH_Insert_Extended(pHash, &k, v);
}

/* IMG removal; SCE diagnostics disabled, equal-size shrink suppressed. */
IMG_UINTPTR_T HASH_Remove_Extended(HASH_TABLE *pHash, IMG_VOID *pKey)
{
	BUCKET **ppBucket;
	IMG_UINT32 uIndex;

	if (!pHash)
		return 0;
	uIndex = pHash->pfnHashFunc(pHash->uKeySize, pKey, pHash->uSize) %
			pHash->uSize;
	for (ppBucket = &pHash->ppBucketTable[uIndex]; ppBucket && *ppBucket;
			ppBucket = &(*ppBucket)->pNext) {
		if (pHash->pfnKeyComp(pHash->uKeySize, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			IMG_UINTPTR_T v = pBucket->v;

			*ppBucket = pBucket->pNext;
			OSFreeMem_Impl(1, sizeof(*pBucket) + pHash->uKeySize, pBucket, 0);
			pHash->uCount--;
			if (pHash->uSize > pHash->uCount << 2 &&
					pHash->uSize > pHash->uMinimumSize) {
				IMG_UINT32 uNewSize = pHash->uSize >> 1;

				if (uNewSize < pHash->uMinimumSize)
					uNewSize = pHash->uMinimumSize;
				if (uNewSize != pHash->uSize)
					_Resize(pHash, uNewSize);
			}
			return v;
		}
	}
	return 0;
}

/* IMG single-word removal; SCE diagnostics disabled. */
IMG_UINTPTR_T HASH_Remove(HASH_TABLE *pHash, IMG_UINTPTR_T k)
{
	return HASH_Remove_Extended(pHash, &k);
}

/* IMG retrieval; SCE diagnostics/assertion disabled. */
IMG_UINTPTR_T HASH_Retrieve_Extended(HASH_TABLE *pHash, IMG_VOID *pKey)
{
	BUCKET **ppBucket;
	IMG_UINT32 uIndex;

	if (!pHash)
		return 0;
	uIndex = pHash->pfnHashFunc(pHash->uKeySize, pKey, pHash->uSize) %
			pHash->uSize;
	for (ppBucket = &pHash->ppBucketTable[uIndex]; *ppBucket;
			ppBucket = &(*ppBucket)->pNext) {
		if (pHash->pfnKeyComp(pHash->uKeySize, (*ppBucket)->k, pKey)) {
			BUCKET *pBucket = *ppBucket;
			IMG_UINTPTR_T v = pBucket->v;

			return v;
		}
	}
	return 0;
}

/* IMG single-word retrieval; SCE diagnostics disabled. */
IMG_UINTPTR_T HASH_Retrieve(HASH_TABLE *pHash, IMG_UINTPTR_T k)
{
	return HASH_Retrieve_Extended(pHash, &k);
}
