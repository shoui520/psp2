/* Sysclib comparison preserves the native word-prefix return values. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include "aliases.h"
#include <sce/barrier.h>

int CompareMemoryCore(const void *left, const void *right, size_t size)
{
	uintptr_t leftAddress = (uintptr_t)left;
	uintptr_t rightAddress = (uintptr_t)right;
	uintptr_t difference = leftAddress - rightAddress;

	if (!difference)
		return 0;
	/* Below four bytes Sony tests the left pointer itself, not its alignment. */
	if ((size >= 4 ? difference & 3 : leftAddress) || (rightAddress & 3)) {
		while (size--) {
			unsigned a = *(const volatile uint8_t *)leftAddress++;
			unsigned b = *(const volatile uint8_t *)rightAddress++;

			if (a != b)
				return (int)a - (int)b;
		}
		return 0;
	}
	for (;;) {
		uint32_t a = *(const volatile uint32_t *)leftAddress;
		uint32_t b = *(const volatile uint32_t *)rightAddress;

		if (a != b) {
			a = __builtin_bswap32(a);
			b = __builtin_bswap32(b);
			if (size >= 4)
				return a > b ? 1 : -1;
			/* ARM's register shift by32 clears the value for a zero count. */
			if (!size)
				return 0;
			return (int32_t)((a >> ((4 - size) * 8)) -
					 (b >> ((4 - size) * 8)));
		}
		if (size <= 4)
			return 0;
		size -= 4;
		leftAddress += 4;
		rightAddress += 4;
	}
}

#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
int memcmp(const void *, const void *, size_t)
	SYSCLIB_ALIAS_TARGET(CompareMemoryCore);
#endif

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
