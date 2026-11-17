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

/* a heap over a memory block: the block, then the mspace made on it */
struct NpCommonPs4MemBlock {
	SceSize size;
	SceUID uid;
	void *base;
};

struct NpCommonPs4Heap {
	struct NpCommonPs4MemBlock block;
	void *mspace;
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

#ifdef __cplusplus
}
#endif

#endif
