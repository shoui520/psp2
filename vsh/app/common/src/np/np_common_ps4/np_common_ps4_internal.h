/*
 * SceNpCommonPs4: the library's own types and the exports the library calls itself. Sony's names
 * for them are not known: the exports are named by their NID, the internal names are ours, taken
 * from what the code does.
 */
#ifndef NP_COMMON_PS4_INTERNAL_H
#define NP_COMMON_PS4_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include "np_common_ps4.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the errors the library returns */
#define NP_COMMON_PS4_ERROR_OUT_OF_MEMORY	-2141880313	/* 0x80553007 */
#define NP_COMMON_PS4_ERROR_INVALID_ARGUMENT	-2141880315	/* 0x80553005 */

/* an allocator: three functions and the context they take as their last argument */
struct NpCommonPs4Allocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
};

/* a block of memory the library allocated, and its size */
void *NpCommonPs4MspaceAllocate(SceSize size, void *context);
void *NpCommonPs4MspaceReallocate(void *pointer, SceSize size, void *context);
void NpCommonPs4MspaceDeallocate(void *pointer, void *context);

struct NpCommonPs4MemBlock {
	SceSize size;
	SceUID uid;
	void *base;
};

/* a heap: how it was made (1: on a given mspace, 2: on given memory, 0: on its own block), its
   block and the mspace on it */
struct NpCommonPs4Heap {
	int state;
	struct NpCommonPs4MemBlock block;
	void *mspace;
};

/* an allocator that also names the error its caller gets when it fails */
struct NpCommonPs4AllocatorWithError {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
	int error;
};

/* what SceNpCommonPs4_A7165D2D reports of a heap */
struct NpCommonPs4HeapStats {
	SceSize capacity;
	SceSize peakInUse;
	SceSize currentInUse;
};

int SceNpCommonPs4_F56B0EAB(SceInt64 *pTime);
int SceNpCommonPs4_A25DA29A(SceUInt32 *pTime);
int SceNpCommonPs4_93DA3594(const char *data, int length);
void SceNpCommonPs4_0A7044DA(const unsigned char *data, SceSize size);
int SceNpCommonPs4_F50BD96A(void);
void SceNpCommonPs4_31DB2D97(void);
int SceNpCommonPs4_AA980113(struct NpCommonPs4Allocator *allocator, void *pointer);
int SceNpCommonPs4_67BBE5F9(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size);
int SceNpCommonPs4_6B428829(struct NpCommonPs4Allocator *allocator, void *pointer);
char *SceNpCommonPs4_1CAB4A2E(struct NpCommonPs4Allocator *allocator, const char *string);
char *SceNpCommonPs4_83496021(struct NpCommonPs4Allocator *allocator, const char *string, SceSize maxLength);
int SceNpCommonPs4_299ADFC3(struct NpCommonPs4Allocator *allocator, void *pointer);
int SceNpCommonPs4_38CC9AF5(struct NpCommonPs4Allocator *allocator, void *pointer);
int SceNpCommonPs4_5BF297C0(void **pMspace, void *memory, SceSize size);
int SceNpCommonPs4_B3C0FE19(void **pMspace, void *mspace);
void *SceNpCommonPs4_1B0517EF(void **pMspace);
int SceNpCommonPs4_A7165D2D(void **pMspace, struct NpCommonPs4HeapStats *stats);
void *SceNpCommonPs4_EB6E37A8(void **pMspace, SceSize size);
void *SceNpCommonPs4_4117508A(void **pMspace, void *pointer, SceSize size);
void SceNpCommonPs4_1514F370(void **pMspace, void *pointer);
void *SceNpCommonPs4_6EF4BA9A(void *context, struct NpCommonPs4Allocator *allocator);
char *SceNpCommonPs4_EBF8CB79(void **pMspace, const char *string);
char *SceNpCommonPs4_8CBF4614(void **pMspace, const char *string, SceSize maxLength);
int SceNpCommonPs4_167D13A4(SceSize size, const char *name, void **pBase, struct NpCommonPs4MemBlock *block);
int SceNpCommonPs4_39208D69(struct NpCommonPs4MemBlock *block);
int SceNpCommonPs4_39CF37A2(struct NpCommonPs4Heap *heap, SceSize size, const char *name);
int SceNpCommonPs4_87E42067(struct NpCommonPs4Heap *heap, void *mspace);
void *SceNpCommonPs4_C52C787A(struct NpCommonPs4Heap *heap);

#ifdef __cplusplus
}
#endif

#endif
