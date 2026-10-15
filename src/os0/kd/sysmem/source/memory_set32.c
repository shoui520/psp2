/*
 * The word fill SceSysmem uses to clear and poison its objects: as the
 * Sysclib fill engine (SetMemoryCore), with a 32-bit pattern in place of a
 * byte. Sony's name is unknown.
 */
#include <stddef.h>
#include <stdint.h>
#include "sysmem_internal.h"

typedef uint32_t Fill32Vector16 __attribute__((vector_size(16), aligned(1), may_alias));
typedef uint32_t Fill32Vector8 __attribute__((vector_size(8), aligned(1), may_alias));
typedef uint32_t Fill32Word __attribute__((aligned(1), may_alias));
typedef uint16_t Fill32Halfword __attribute__((aligned(1), may_alias));

void *SetMemory32Core(void *destination, SceUInt32 pattern, SceSize size)
{
	uintptr_t cursor = (uintptr_t)destination;
	Fill32Vector16 block = { pattern, pattern, pattern, pattern };
	Fill32Vector8 half = { pattern, pattern };

	if (size >= 16) {
		uintptr_t tail = cursor + size - 16;

		*(volatile Fill32Vector16 *)cursor = block;
		if (size == 16)
			return destination;
		cursor = (cursor + 16) & ~(uintptr_t)15;
		while (cursor < tail) {
			*(Fill32Word *)(cursor + 0) = pattern;
			*(Fill32Word *)(cursor + 4) = pattern;
			*(Fill32Word *)(cursor + 8) = pattern;
			*(Fill32Word *)(cursor + 12) = pattern;
			cursor += 16;
		}
		*(volatile Fill32Vector16 *)tail = block;
	} else if (size >= 8) {
		*(volatile Fill32Vector8 *)cursor = half;
		*(volatile Fill32Vector8 *)(cursor + size - 8) = half;
	} else {
		if (size & 4) {
			*(Fill32Word *)cursor = pattern;
			cursor += 4;
		}
		if (size & 2) {
			*(Fill32Halfword *)cursor = pattern;
			cursor += 2;
		}
		if (size & 1)
			*(volatile uint8_t *)cursor = pattern;
	}
	return destination;
}
