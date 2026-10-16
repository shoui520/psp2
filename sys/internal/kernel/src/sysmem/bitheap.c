/*
 * SceSysmem: bit heaps, allocators of small indices. A set bit in the free
 * map is a free index. Freed indices collect in a second map and are reused
 * only once the free map runs out, when the two maps swap.
 */

#include "sysmem_internal.h"

#define BITHEAP_MAGIC			0xba46d66f

typedef struct Bitheap {
	SceUInt32 magic;
	SceUInt32 count;			/* indices */
	SceUInt32 words;			/* per map */
	SceUInt32 used;
	SceUInt32 free;				/* set bits in freeMap */
	SceUInt32 freed;			/* set bits in freedMap */
	SceUInt32 *freeMap;
	SceUInt32 *freedMap;
} Bitheap;

/* name unknown: count indices over maps at buffer (2 * count / 8 bytes) */
int BitheapInit(Bitheap *heap, SceUInt32 count, SceUInt32 *buffer)
{
	SceUInt32 words = count >> 5, *freed = buffer + words, i;

	heap->count = count;
	heap->free = count;
	heap->words = words;
	heap->freeMap = buffer;
	heap->magic = BITHEAP_MAGIC;
	heap->freedMap = freed;
	heap->used = 0;
	heap->freed = 0;
	if (words != 0) {
		for (i = 0; i < heap->words; i++)
			buffer[i] = 0xffffffff;
		for (i = 0; i < heap->words; i++)
			freed[i] = 0;
	}
	return 0;
}

/* the lowest set bit, 32 when none (RBIT + CLZ, and CLZ of 0 is 32 on ARM) */
static inline SceUInt32 BitheapLowestBit(SceUInt32 word)
{
	return __builtin_ctz(word);
}

/* name unknown: the lowest free index */
int BitheapAlloc(Bitheap *heap)
{
	SceUInt32 *map, *freed, i, bit;

	if (heap->used >= heap->count)
		return SCE_KERNEL_ERROR_CANNOT_ALLOC_BITHEAP;
	freed = heap->freedMap;
	map = heap->freeMap;
	if (heap->free == 0) {
		heap->freedMap = heap->freeMap;
		heap->freeMap = freed;
		heap->free = heap->freed;
		heap->freed = 0;
		map = freed;
	}
	for (i = 0; i < heap->words; i++, map++) {
		bit = BitheapLowestBit(*map);
		if (bit != 32) {
			heap->used++;
			heap->free--;
			*map &= ~(1 << bit);
			return bit + i * 32;
		}
	}
	return SCE_KERNEL_ERROR_CANNOT_ALLOC_BITHEAP;
}

/* takes a given index */
int sceKernelBitheapUse(Bitheap *heap, int index)
{
	SceUInt32 bit, *word;

	if (index < 0) {
		SCE_DBG_ASSERT_FAIL(111);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if ((SceUInt32)index >= heap->count) {
		SCE_DBG_ASSERT_FAIL(115);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	bit = 1 << (index & 0x1f);
	word = &heap->freeMap[index >> 5];
	if (!(*word & bit))
		SCE_DBG_ASSERT_FAIL(122);
	heap->freeMap[index >> 5] &= ~bit;
	heap->used++;
	heap->free--;
	return 0;
}

int sceKernelBitheapFree(Bitheap *heap, int index)
{
	SceUInt32 bit;

	if (index < 0) {
		SCE_DBG_ASSERT_FAIL(142);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if ((SceUInt32)index >= heap->count)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	bit = 1 << (index & 0x1f);
	if (heap->freeMap[index >> 5] & bit)
		SCE_DBG_ASSERT_FAIL(156);
	heap->freedMap[index >> 5] |= bit;
	heap->used--;
	heap->freed++;
	return 0;
}

/* name unknown: nothing to set up */
int BitheapModuleStart(void)
{
	return 0;
}
