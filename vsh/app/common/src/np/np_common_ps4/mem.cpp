/*
 * SceNpCommonPs4: the clock, the debug print and the allocators.
 */

#include "np_common_ps4_internal.h"

extern "C" {

volatile SceUInt32 g_counter;			/* 0x8102B000 */
struct NpCommonPs4Heap g_globalHeap;	/* 0x8102B004 */
struct NpCommonPs4AllocatorWithError g_globalAllocatorWithError;	/* 0x8102B138 */
struct NpCommonPs4Allocator g_globalAllocator;	/* 0x8102B14C */

extern SceUInt32 SceThreadmgr_47F6DE49(void);
__attribute__((noinline)) SceUInt32 SceNpCommonPs4_741F88FE(volatile SceUInt32 *counter);
extern SceInt64 sceKernelGetSystemTimeWide(void);
extern int sceKernelGetProcessId(void);

int SceNpCommonPs4_F56B0EAB(SceInt64 *pTime)
{
	*pTime = sceKernelGetSystemTimeWide();
	return 0;
}

int SceNpCommonPs4_A25DA29A(SceUInt32 *pTime)
{
	*pTime = SceThreadmgr_47F6DE49();
	return 0;
}

int SceNpCommonPs4_ABAEA6ED(void)
{
	int result;

	result = SceNpCommonPs4_741F88FE(&g_counter) + 1;
	if (result == 0)
		return SceNpCommonPs4_741F88FE(&g_counter) + 1;
	return result;
}

int SceNpCommonPs4_93DA3594(const char *data, int length)
{
	return sceClibPrintf("%.*s", length, data);
}

void SceNpCommonPs4_0A7044DA(const unsigned char *data, SceSize size)
{
	char line[128];
	char *out;
	const unsigned char *hex;
	SceSize hexIndex;
	SceSize textIndex;
	int hexDone;
	int textDone;
	int i;
	int notDone;
	int terminator;

	hexIndex = 0;
	notDone = 0;
	hex = data;
	textIndex = 0;
	if (size != 0) {
		terminator = 0;
		do {
			hexDone = notDone;
			out = line;
			while (1) {
				if (hexDone != 0)
					break;
				i = hexIndex & 0xF;
				if (size > hexIndex) {
					sceClibSnprintf(out, 128, "%.2x", *hex++);
					out += 2;
					if ((hexIndex & 1) == 1)
						*out++ = ' ';
					if (i == 15)
						hexDone = 1;
					hexIndex++;
					continue;
				}
				while (i < 16) {
					out[0] = ' ';
					out[1] = ' ';
					out += 2;
					if ((i & 1) == 1) {
						*out = ' ';
						out++;
					}
					i++;
				}
				break;
			}
			textDone = notDone;
			*out++ = ' ';
			do {
				if (size <= textIndex)
					break;
				if (*data < 0x20 || *data > 0x7E)
					*out = '.';
				else
					*out = *data;
				out++;
				data++;
				if ((textIndex & 0xF) == 0xF)
					textDone = 1;
				textIndex++;
			} while (textDone == 0);
			*out = terminator;
			sceClibPrintf("%s\n", line);
		} while (size > hexIndex);
	}
	sceClibPrintf("\n");
}

int SceNpCommonPs4_F50BD96A(int printed)
{
	*(volatile int *)0 = 0;
	return 0;
}

struct NpCommonPs4Allocator *SceNpCommonPs4_31DB2D97(struct NpCommonPs4Allocator *pool)
{
	return pool;
}

int SceNpCommonPs4_AA980113(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->allocate)(pointer, allocator->context);
}

int SceNpCommonPs4_67BBE5F9(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size)
{
	return ((int (*)(void *, SceSize, void *))allocator->reallocate)(pointer, size, allocator->context);
}

int SceNpCommonPs4_6B428829(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->deallocate)(pointer, allocator->context);
}

char *SceNpCommonPs4_1CAB4A2E(struct NpCommonPs4Allocator *allocator, const char *string)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, 0xFFFFFFFF) + 1;
	copy = (char *)allocator->allocate(length, allocator->context);
	if (copy != NULL)
		sceClibStrncpy(copy, string, length);
	return copy;
}

char *SceNpCommonPs4_83496021(struct NpCommonPs4Allocator *allocator, const char *string, SceSize maxLength)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, maxLength) + 1;
	copy = (char *)allocator->allocate(length, allocator->context);
	if (copy != NULL)
		sceClibStrncpy(copy, string, length);
	return copy;
}

void *SceNpCommonPs4_299ADFC3(struct NpCommonPs4Allocator *allocator, SceSize size)
{
	return allocator->allocate(size, allocator->context);
}

void SceNpCommonPs4_38CC9AF5(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	allocator->deallocate(pointer, allocator->context);
}

extern "C" char *npCommonPs4AllocatorDuplicateString(struct NpCommonPs4Allocator *allocator, const char *string)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, 0xFFFFFFFF) + 1;
	copy = (char *)allocator->allocate(length, allocator->context);
	if (copy != NULL)
		sceClibStrncpy(copy, string, length);
	return copy;
}

int NpCommonPs4AllocatorReallocate(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size)
{
	return ((int (*)(void *, SceSize, void *))allocator->reallocate)(pointer, size, allocator->context);
}

__attribute__((noinline)) int SceNpCommonPs4_5BF297C0(void **pMspace, void *memory, SceSize size)
{
	void *mspace;

	mspace = sceClibMspaceCreate(memory, size);
	if (mspace == NULL)
		return NP_COMMON_PS4_ERROR_OUT_OF_MEMORY;
	*pMspace = mspace;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_B3C0FE19(void **pMspace, void *mspace)
{
	*pMspace = mspace;
	return 0;
}

__attribute__((noinline)) void *SceNpCommonPs4_1B0517EF(void **pMspace)
{
	void *mspace;

	mspace = *pMspace;
	if (mspace != NULL) {
		sceClibMspaceDestroy(mspace);
		*pMspace = NULL;
		return NULL;
	}
	return mspace;
}

int SceNpCommonPs4_A7165D2D(void **pMspace, struct NpCommonPs4HeapStats *stats)
{
	struct MspaceStats mspaceStats[2];
	int result;

	sceClibMemset(mspaceStats, 0, 32);
	result = sceClibMspaceMallocStats(*pMspace, mspaceStats);
	if (result >= 0) {
		result = 0;
		stats->capacity = mspaceStats[0].capacity;
		stats->peakInUse = mspaceStats[0].peakInUse;
		stats->currentInUse = mspaceStats[0].currentInUse;
	}
	return result;
}

__attribute__((noinline)) void *SceNpCommonPs4_EB6E37A8(void **pMspace, SceSize size)
{
	return sceClibMspaceMalloc(*pMspace, size);
}

__attribute__((noinline)) void *SceNpCommonPs4_4117508A(void **pMspace, void *pointer, SceSize size)
{
	return sceClibMspaceRealloc(*pMspace, pointer, size);
}

__attribute__((noinline)) void SceNpCommonPs4_1514F370(void **pMspace, void *pointer)
{
	if (pointer != NULL)
		sceClibMspaceFree(*pMspace, pointer);
}

void *SceNpCommonPs4_6EF4BA9A(void *context, struct NpCommonPs4Allocator *allocator)
{
	allocator->context = context;
	allocator->allocate = NpCommonPs4MspaceAllocate;
	allocator->reallocate = NpCommonPs4MspaceReallocate;
	allocator->deallocate = NpCommonPs4MspaceDeallocate;
	return (void *)NpCommonPs4MspaceDeallocate;
}

char *SceNpCommonPs4_EBF8CB79(void **pMspace, const char *string)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, 0xFFFFFFFF);
	copy = (char *)sceClibMspaceMalloc(*pMspace, length + 1);
	if (copy == NULL)
		return NULL;
	sceClibStrncpy(copy, string, length);
	copy[length] = '\0';
	return copy;
}

char *SceNpCommonPs4_8CBF4614(void **pMspace, const char *string, SceSize maxLength)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, maxLength);
	copy = (char *)sceClibMspaceMalloc(*pMspace, length + 1);
	if (copy == NULL)
		return NULL;
	sceClibStrncpy(copy, string, length);
	copy[length] = '\0';
	return copy;
}

void *NpCommonPs4MspaceAllocate(SceSize size, void *context)
{
	return sceClibMspaceMalloc(*(void **)context, size);
}

void *NpCommonPs4MspaceReallocate(void *pointer, SceSize size, void *context)
{
	return sceClibMspaceRealloc(*(void **)context, pointer, size);
}

void NpCommonPs4MspaceDeallocate(void *pointer, void *context)
{
	if (pointer != NULL)
		sceClibMspaceFree(*(void **)context, pointer);
}

int SceNpCommonPs4_167D13A4(SceSize size, const char *name, void **pBase, struct NpCommonPs4MemBlock *block)
{
	void *base;
	SceUID uid;
	int result;

	base = NULL;
	uid = sceKernelAllocMemBlock(name, 0x0C20D060, size, NULL);
	result = uid;
	if (uid >= 0) {
		result = sceKernelGetMemBlockBase(uid, &base);
		if (result < 0) {
			sceKernelFreeMemBlock(uid);
			return result;
		}
		block->uid = uid;
		block->size = size;
		block->base = base;
		if (pBase != NULL)
			*pBase = base;
	}
	return result;
}

int SceNpCommonPs4_39208D69(struct NpCommonPs4MemBlock *block)
{
	if (block == NULL)
		return NP_COMMON_PS4_ERROR_INVALID_ARGUMENT;
	if (block->uid != 0)
		sceKernelFreeMemBlock(block->uid);
	block->size = 0;
	block->uid = 0;
	block->base = NULL;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_39CF37A2(struct NpCommonPs4Heap *heap, SceSize size, const char *name)
{
	void *base;
	SceUID uid;
	int result;

	base = NULL;
	result = sceKernelAllocMemBlock(name, 0x0C20D060, size, NULL);
	uid = result;
	if (result >= 0) {
		result = sceKernelGetMemBlockBase(uid, &base);
		if (result >= 0) {
			heap->block.size = size;
			heap->block.uid = uid;
			heap->block.base = base;
		} else {
			sceKernelFreeMemBlock(uid);
		}
	}
	if (result >= 0) {
		result = SceNpCommonPs4_5BF297C0(&heap->mspace, heap->block.base, heap->block.size);
		if (result < 0) {
			if (&heap->block != NULL) {
				if (heap->block.uid != 0)
					sceKernelFreeMemBlock(heap->block.uid);
				heap->block.size = 0;
				heap->block.uid = 0;
				heap->block.base = NULL;
			}
			sceClibMemset(heap, 0, 20);
		} else {
			result = 0;
		}
	}
	return result;
}

int SceNpCommonPs4_87E42067(struct NpCommonPs4Heap *heap, void *mspace)
{
	int result;

	result = SceNpCommonPs4_B3C0FE19(&heap->mspace, mspace);
	if (result >= 0) {
		heap->state = 1;
		return 0;
	}
	return result;
}

__attribute__((noinline)) void *SceNpCommonPs4_C52C787A(struct NpCommonPs4Heap *heap)
{
	if (heap->state > 1) {
		if (heap->state <= 2)
			return SceNpCommonPs4_1B0517EF(&heap->mspace);
	} else if (heap->state != 0) {
		return heap;
	}
	SceNpCommonPs4_1B0517EF(&heap->mspace);
	if (&heap->block != NULL) {
		if (heap->block.uid != 0)
			sceKernelFreeMemBlock(heap->block.uid);
		heap->block.size = 0;
		heap->block.uid = 0;
		heap->block.base = NULL;
		return NULL;
	}
	return &heap->block;
}

void *NpCommonPs4HeapAllocate(SceSize size, struct NpCommonPs4Heap *heap)
{
	return SceNpCommonPs4_EB6E37A8(&heap->mspace, size);
}

void *NpCommonPs4HeapReallocate(void *pointer, SceSize size, struct NpCommonPs4Heap *heap)
{
	return SceNpCommonPs4_4117508A(&heap->mspace, pointer, size);
}

void NpCommonPs4HeapDeallocate(void *pointer, struct NpCommonPs4Heap *heap)
{
	SceNpCommonPs4_1514F370(&heap->mspace, pointer);
}

void *SceNpCommonPs4_EDB4C46E(struct NpCommonPs4Heap *heap, struct NpCommonPs4Allocator *allocator)
{
	allocator->context = heap;
	allocator->allocate = (void *(*)(SceSize, void *))NpCommonPs4HeapAllocate;
	allocator->reallocate = (void *(*)(void *, SceSize, void *))NpCommonPs4HeapReallocate;
	allocator->deallocate = (void (*)(void *, void *))NpCommonPs4HeapDeallocate;
	return (void *)NpCommonPs4HeapDeallocate;
}

void *SceNpCommonPs4_5A6F85BC(struct NpCommonPs4Heap *heap, int error, struct NpCommonPs4AllocatorWithError *allocator)
{
	allocator->context = heap;
	allocator->allocate = (void *(*)(SceSize, void *))NpCommonPs4HeapAllocate;
	allocator->error = error;
	allocator->reallocate = (void *(*)(void *, SceSize, void *))NpCommonPs4HeapReallocate;
	allocator->deallocate = (void (*)(void *, void *))NpCommonPs4HeapDeallocate;
	return (void *)NpCommonPs4HeapReallocate;
}

int SceNpCommonPs4_2D7AF34A(SceSize size)
{
	int result;

	result = SceNpCommonPs4_39CF37A2(&g_globalHeap, size, "SceNpGlobalHeap");
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_0D1288D4(void *mspace)
{
	int result;

	result = SceNpCommonPs4_B3C0FE19(&g_globalHeap.mspace, mspace);
	if (result >= 0) {
		g_globalHeap.state = 1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_D213292D(SceSize size, void *memory)
{
	int result;

	result = SceNpCommonPs4_5BF297C0(&g_globalHeap.mspace, memory, size);
	if (result >= 0) {
		g_globalHeap.state = 2;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_33B5FE83(void)
{
	return (int)SceNpCommonPs4_C52C787A(&g_globalHeap);
}

struct NpCommonPs4Allocator *SceNpCommonPs4_53484B79(void)
{
	g_globalAllocator.allocate = (void *(*)(SceSize, void *))NpCommonPs4HeapAllocate;
	g_globalAllocator.reallocate = (void *(*)(void *, SceSize, void *))NpCommonPs4HeapReallocate;
	g_globalAllocator.deallocate = (void (*)(void *, void *))NpCommonPs4HeapDeallocate;
	g_globalAllocator.context = &g_globalHeap;
	return &g_globalAllocator;
}

struct NpCommonPs4AllocatorWithError *SceNpCommonPs4_03C9ACE2(void)
{
	g_globalAllocatorWithError.allocate = (void *(*)(SceSize, void *))NpCommonPs4HeapAllocate;
	g_globalAllocatorWithError.reallocate = (void *(*)(void *, SceSize, void *))NpCommonPs4HeapReallocate;
	g_globalAllocatorWithError.deallocate = (void (*)(void *, void *))NpCommonPs4HeapDeallocate;
	g_globalAllocatorWithError.context = &g_globalHeap;
	g_globalAllocatorWithError.error = NP_COMMON_PS4_ERROR_8055800B;
	return &g_globalAllocatorWithError;
}

void *SceNpCommonPs4_342A935D(SceSize size)
{
	return SceNpCommonPs4_EB6E37A8(&g_globalHeap.mspace, size);
}

void *SceNpCommonPs4_4A93F656(void *pointer, SceSize size)
{
	return SceNpCommonPs4_4117508A(&g_globalHeap.mspace, pointer, size);
}

void SceNpCommonPs4_6106C031(void *pointer)
{
	SceNpCommonPs4_1514F370(&g_globalHeap.mspace, pointer);
}

void NpCommonPs4Nop(void)
{
}

int SceNpCommonPs4_A37CE123(void)
{
	return sceKernelGetProcessId();
}

__attribute__((noinline)) SceUInt32 SceNpCommonPs4_741F88FE(volatile SceUInt32 *counter)
{
	SceUInt32 value;

	__dmb();
	do {
		value = __ldrex(counter);
	} while (__strex(value + 1, counter) != 0);
	__dmb();
	return value;
}

SceUInt32 SceNpCommonPs4_7B512A98(volatile SceUInt32 *counter)
{
	SceUInt32 value;

	__dmb();
	do {
		value = __ldrex(counter);
	} while (__strex(value - 1, counter) != 0);
	__dmb();
	return value;
}

SceUInt32 SceNpCommonPs4_76118391(volatile SceUInt32 *target, SceUInt32 expected, SceUInt32 desired)
{
	SceUInt32 value;

	__dmb();
	do {
		value = __ldrex(target);
	} while (value == expected && __strex(desired, target) != 0);
	__dmb();
	return value;
}

}
