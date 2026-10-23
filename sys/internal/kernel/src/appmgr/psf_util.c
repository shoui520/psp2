/* Standalone memory, string and ordering utilities used by the PSF library. */

#include <scetypes.h>
#include <sysmem/uid_guid.h>
#include <sysmem/sysmem.h>
#include <string.h>

void *appmgrPsfAllocMemory(SceSize size)
{
	return sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, size);
}

int appmgrPsfFreeMemory(void *memory)
{
	return sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, memory);
}

void *appmgrPsfCopyMemory(void *destination, const void *source, SceSize size)
{
	unsigned char *output = destination;
	const unsigned char *input = source;

	while (size-- != 0)
		*output++ = *input++;
	return destination;
}

void *appmgrPsfSetMemory(void *destination, int value, SceSize size)
{
	if (size != 0)
		memset(destination, (signed char)value, size);
	return destination;
}

int appmgrPsfCompareMemory(const signed char *left, const signed char *right,
			   SceSize size)
{
	while (size-- != 0) {
		if (*left > *right)
			return 1;
		if (*left < *right)
			return -1;
		left++;
		right++;
	}
	return 0;
}

SceSize appmgrPsfStringLength(const signed char *string)
{
	const signed char *end = string;

	while (*end != 0)
		end++;
	return end - string;
}

int appmgrPsfCompareString(const unsigned char *left, const unsigned char *right)
{
	int difference;

	for (;;) {
		difference = *left - *right;
		if (difference != 0)
			return difference;
		if (*left == 0)
			return *left;
		left++;
		right++;
	}
}

void *appmgrPsfBinarySearch(const void *key, void *array, SceSize count,
	SceSize width, int (*compare)(const void *, const void *))
{
	SceSize low = 0;
	SceSize high = count - 1;
	SceSize middle;
	unsigned char *element;
	int result;

	if (count == 0)
		return NULL;
	do {
		middle = (low + high) >> 1;
		element = (unsigned char *)array + width * middle;
		result = compare(key, element);
		if (result == 0)
			return element;
		if (low == high)
			return NULL;
		if (result > 0) {
			low = middle + 1;
		} else {
			high = middle - 1;
			if (middle == 0)
				high = 0;
		}
	} while (low <= high);
	return NULL;
}

void appmgrPsfShellSort(void *array, SceSize count, SceSize width,
	int (*compare)(const void *, const void *))
{
	SceSize gap, item, offset;
	int index;
	unsigned char *left, *right;

	for (gap = count >> 1; gap != 0; gap >>= 1) {
		for (item = gap; item < count; item++) {
			for (index = item - gap; index >= 0; index -= gap) {
				left = (unsigned char *)array + width * index;
				right = left + width * gap;
				if (compare(left, right) <= 0)
					break;
				for (offset = 0; offset < width; offset++) {
					left[offset] ^= right[offset];
					right[offset] ^= left[offset];
					left[offset] ^= right[offset];
				}
			}
		}
	}
}
