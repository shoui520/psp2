/* IMG 1.7 hash owners, ARM 32-bit ABI; pristine candidate in src/vendor.
 * SCE portability declarations are kept here, outside the upstream tree.
 */
#ifndef GPU_HASH_SCE_H
#define GPU_HASH_SCE_H

#include "sce_ddk_port.h"

typedef IMG_UINT32 HASH_FUNC(IMG_SIZE_T uKeySize, IMG_VOID *pKey,
		IMG_UINT32 uHashTabLen);
typedef IMG_BOOL HASH_KEY_COMP(IMG_SIZE_T uKeySize, IMG_VOID *pKey1,
		IMG_VOID *pKey2);

typedef struct _BUCKET_ {
	struct _BUCKET_ *pNext;
	IMG_UINTPTR_T v;
	IMG_UINTPTR_T k[];
} BUCKET;

typedef struct _HASH_TABLE_ {
	BUCKET **ppBucketTable;
	IMG_UINT32 uSize;
	IMG_UINT32 uCount;
	IMG_UINT32 uMinimumSize;
	IMG_UINT32 uKeySize;
	HASH_FUNC *pfnHashFunc;
	HASH_KEY_COMP *pfnKeyComp;
} HASH_TABLE;

#endif
