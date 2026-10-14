/* Shared Sysclib fill engine: overlapping head/tail stores stay in range. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include "aliases.h"

typedef uint8_t FillVector16 __attribute__((vector_size(16), aligned(1), may_alias));
typedef uint8_t FillVector8 __attribute__((vector_size(8), aligned(1), may_alias));
typedef uint32_t FillWord __attribute__((aligned(1), may_alias));
typedef uint16_t FillHalfword __attribute__((aligned(1), may_alias));

void *SetMemoryCore(void *destination, int value, size_t size)
{
	uint8_t byte = value;
	uint32_t word = (uint32_t)byte * 0x01010101U;
	uintptr_t cursor = (uintptr_t)destination;
	FillVector16 block = { byte, byte, byte, byte, byte, byte, byte, byte,
		byte, byte, byte, byte, byte, byte, byte, byte };
	FillVector8 half = { byte, byte, byte, byte, byte, byte, byte, byte };

	if (size >= 16) {
		uintptr_t tail = cursor + size - 16;

		*(volatile FillVector16 *)cursor = block;
		if (size == 16)
			return destination;
		cursor = (cursor + 16) & ~(uintptr_t)15;
		while (cursor < tail) {
			*(FillWord *)(cursor + 0) = word;
			*(FillWord *)(cursor + 4) = word;
			*(FillWord *)(cursor + 8) = word;
			*(FillWord *)(cursor + 12) = word;
			cursor += 16;
		}
		*(volatile FillVector16 *)tail = block;
	} else if (size >= 8) {
		*(volatile FillVector8 *)cursor = half;
		*(volatile FillVector8 *)(cursor + size - 8) = half;
	} else {
		if (size & 4) {
			*(FillWord *)cursor = word;
			cursor += 4;
		}
		if (size & 2) {
			*(FillHalfword *)cursor = word;
			cursor += 2;
		}
		if (size & 1)
			*(volatile uint8_t *)cursor = byte;
	}
	return destination;
}

void *__memset_chk(void *destination, int value, size_t size, size_t capacity)
{
	if (capacity < size)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return SetMemoryCore(destination, value, size);
}

#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
void *memset(void *, int, size_t) SYSCLIB_ALIAS_TARGET(SetMemoryCore);
#endif
