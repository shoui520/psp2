/* The constant-time compare; memcmp itself is CompareMemoryCore (compare_core.S). */
#include <stddef.h>
#include <stdint.h>
#include <sce/barrier.h>

int timingsafe_memcmp(const void *left, const void *right, size_t count)
{
	const volatile unsigned char *a = left;
	const volatile unsigned char *b = right;
	uint32_t seen = 0, result = 0;

	while (count--) {
		uint32_t aByte, bByte, difference, negative, order;

		dmb();
		aByte = *a++;
		bByte = *b++;
		difference = aByte - bByte;
		negative = 0U - difference;
		order = (negative >> 31) | (uint32_t)((int32_t)difference >> 31);
		result |= order & ~seen;
		seen |= (uint32_t)((int32_t)(difference | negative) >> 31);
		isb();
	}
	return (int32_t)result;
}
