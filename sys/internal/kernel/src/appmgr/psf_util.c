/* Standalone memory, string and ordering utilities used by the PSF library. */

#include <scetypes.h>
#include <sysmem/uid_guid.h>
#include <sysmem/sysmem.h>
#include <arm_neon.h>
#include <stdint.h>
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
	SceSize index, copied, blocks;

	if (size == 0)
		return destination;
	if (size > 15 && ((uintptr_t)output >= (uintptr_t)input + 16 ||
			  (uintptr_t)input >= (uintptr_t)output + 16)) {
		unsigned char *blockOutput = output;
		const unsigned char *blockInput = input;

		blocks = size >> 4;
		copied = blocks << 4;
		index = 0;
		do {
			uint8x16_t block = vld1q_u8(blockInput);

			blockInput += 16;
			vst1q_u8(blockOutput, block);
			blockOutput += 16;
			++index;
		} while (index < blocks);
		if (size != copied) {
			SceSize last = size - 1 - copied;
			unsigned char *tailOutput = output + copied;
			const unsigned char *tailInput = input + copied;

			output[copied] = input[copied];
			if (last == 0)
				goto out;
			tailOutput[1] = tailInput[1];
			if (last == 1)
				goto out;
			tailOutput[2] = tailInput[2];
			if (last == 2)
				goto out;
			tailOutput[3] = tailInput[3];
			if (last == 3)
				goto out;
			tailOutput[4] = tailInput[4];
			if (last == 4)
				goto out;
			tailOutput[5] = tailInput[5];
			if (last == 5)
				goto out;
			tailOutput[6] = tailInput[6];
			if (last == 6)
				goto out;
			tailOutput[7] = tailInput[7];
			if (last == 7)
				goto out;
			tailOutput[8] = tailInput[8];
			if (last == 8)
				goto out;
			tailOutput[9] = tailInput[9];
			if (last == 9)
				goto out;
			tailOutput[10] = tailInput[10];
			if (last == 10)
				goto out;
			tailOutput[11] = tailInput[11];
			if (last == 11)
				goto out;
			tailOutput[12] = tailInput[12];
			if (last == 12)
				goto out;
			tailOutput[13] = tailInput[13];
			if (last == 13)
				goto out;
			tailOutput[14] = tailInput[14];
		}
	} else {
		unsigned char *end = output + size;

		do {
			*output++ = *input++;
		} while (output != end);
	}
out:
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
	SceSize last = size - 1;
	SceSize index = 0;
	int leftByte, rightByte;

	if (size == 0)
		return 0;
	leftByte = *left;
	rightByte = *right;
	if (leftByte > rightByte)
		return 1;
	if (leftByte < rightByte)
		return -1;
	while (index++ != last) {
		leftByte = *++left;
		rightByte = *++right;
		if (leftByte > rightByte)
			return 1;
		if (leftByte < rightByte)
			return -1;
	}
	return 0;
}

SceSize appmgrPsfStringLength(const signed char *string)
{
	SceSize length = *string;
	const signed char *cursor = string + 1;

	if (length != 0) {
		do {
			length = cursor - string;
		} while (*cursor++ != 0);
	}
	return length;
}

int appmgrPsfCompareString(const unsigned char *left, const unsigned char *right)
{
	int difference = *left - *right;
	int leftByte, rightByte;

	if (difference != 0)
		return difference;
	if (*left == 0)
		return *left;
	for (;;) {
		leftByte = *++left;
		rightByte = *++right;
		difference = leftByte - rightByte;
		if (leftByte != rightByte)
			return difference;
		if (leftByte == 0)
			return leftByte;
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
	SceSize gap = count >> 1;
	SceSize blocks = width >> 4;
	SceSize vectorSize = blocks << 4;
	SceSize item, index, block, offset;
	unsigned char *left, *right;
	uint8x16_t value;

	for (; gap != 0; gap >>= 1) {
		for (item = gap; item < count; item++) {
			index = item - gap;
			left = (unsigned char *)array + width * index;
			right = left + width * gap;
			while ((int)index >= 0) {
				if (compare(left, right) <= 0)
					break;
				if (width != 0) {
					offset = 0;
					if (width > 15 &&
					    ((uintptr_t)left >= (uintptr_t)right + 16 ||
					     (uintptr_t)right >= (uintptr_t)left + 16)) {
						for (block = 0; block < blocks; block++) {
							offset = block << 4;
							value = veorq_u8(vld1q_u8(left + offset),
								vld1q_u8(right + offset));
							vst1q_u8(left + offset, value);
							value = veorq_u8(value, vld1q_u8(right + offset));
							vst1q_u8(right + offset, value);
							value = veorq_u8(value, vld1q_u8(left + offset));
							vst1q_u8(left + offset, value);
						}
						offset = vectorSize;
					}
					while (offset < width) {
						left[offset] ^= right[offset];
						right[offset] ^= left[offset];
						left[offset] ^= right[offset];
						offset++;
					}
				}
				index -= gap;
				left -= width * gap;
				right -= width * gap;
			}
		}
	}
}
