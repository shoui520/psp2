/* IMG 1.7 hash owners, ARM 32-bit ABI; pristine candidate in contrib.
 * SCE portability declarations are kept here, outside the upstream tree.
 */
#ifndef GPU_HASH_SCE_H
#define GPU_HASH_SCE_H

#include <gpu/sce_ddk_port.h>

typedef unsigned u32;

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

/* hash.c */
u32 HASH_Func_Default(u32 uKeySize, void *pKey, u32 uHashTabLen);
u32 HASH_Key_Comp_Default(u32 uKeySize, void *pKey1, void *pKey2);
HASH_TABLE *HASH_Create_Extended(IMG_UINT32 uInitialLen, IMG_SIZE_T uKeySize,
		HASH_FUNC *pfnHashFunc, HASH_KEY_COMP *pfnKeyComp);
HASH_TABLE *HASH_Create(IMG_UINT32 uInitialLen);
IMG_VOID HASH_Delete(HASH_TABLE *pHash);
IMG_BOOL HASH_Insert_Extended(HASH_TABLE *pHash, IMG_VOID *pKey, IMG_UINTPTR_T v);
IMG_UINTPTR_T HASH_Remove_Extended(HASH_TABLE *pHash, IMG_VOID *pKey);
IMG_UINTPTR_T HASH_Remove(HASH_TABLE *pHash, IMG_UINTPTR_T k);
IMG_UINTPTR_T HASH_Retrieve_Extended(HASH_TABLE *pHash, IMG_VOID *pKey);
IMG_UINTPTR_T HASH_Retrieve(HASH_TABLE *pHash, IMG_UINTPTR_T k);

#endif
