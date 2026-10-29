/* PAF's exported recursive quicksort partitioner and its checked wrapper. */
#include "stdlib.h"
#include "string.h"
#include <stdint.h>

extern unsigned int __stack_chk_guard;

__attribute__((noinline))
void *ScePafStdc_E8BD0273(void *base, size_t element_size,
		int (*compare)(const void *, const void *),
		int32_t first, int32_t last)
{
	int32_t initial_first = first;
	int32_t initial_last = last;
	int32_t middle = (int32_t)((uint32_t)first + (uint32_t)last) / 2;
	unsigned char pivot[element_size];
	unsigned char temporary[element_size];
	unsigned char *left = (unsigned char *)((uintptr_t)base +
		(uint32_t)first * (uint32_t)element_size);
	unsigned char *right = (unsigned char *)((uintptr_t)base +
		(uint32_t)last * (uint32_t)element_size);

	sce_paf_memcpy(pivot, (const void *)((uintptr_t)base +
		(uint32_t)middle * (uint32_t)element_size), element_size);
	do {
		while (compare(left, pivot) < 0) {
			left += element_size;
			first = (int32_t)((uint32_t)first + 1U);
		}
		while (compare(pivot, right) < 0) {
			last = (int32_t)((uint32_t)last - 1U);
			right -= element_size;
		}
		if (first <= last) {
			sce_paf_memcpy(temporary, left, element_size);
			sce_paf_memcpy(left, right, element_size);
			sce_paf_memcpy(right, temporary, element_size);
			first = (int32_t)((uint32_t)first + 1U);
			left += element_size;
			last = (int32_t)((uint32_t)last - 1U);
			right -= element_size;
		}
	} while (first <= last);
	if (last > initial_first)
		ScePafStdc_E8BD0273(base, element_size, compare,
			initial_first, last);
	if (first < initial_last)
		ScePafStdc_E8BD0273(base, element_size, compare,
			first, initial_last);
	return &__stack_chk_guard;
}

void sce_paf_qsort(void *base, size_t count, size_t element_size,
		int (*compare)(const void *, const void *))
{
	if (!base || !element_size || !compare || !count)
		return;
	ScePafStdc_E8BD0273(base, element_size, compare, 0,
		(int32_t)((uint32_t)count - 1));
}
