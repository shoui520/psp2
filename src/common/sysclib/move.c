/* Native backward-copy grouping for overlapping Sysclib memory moves. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include "aliases.h"

typedef uint8_t MoveVector16 __attribute__((vector_size(16), aligned(1), may_alias));
typedef uint32_t MoveWord __attribute__((aligned(1), may_alias));
typedef uint16_t MoveHalfword __attribute__((aligned(1), may_alias));

extern void *CopyMemoryForward(void *destination, const void *source, size_t size);

void *MoveMemoryCore(void *destination, const void *source, size_t size)
{
	uintptr_t dst = (uintptr_t)destination;
	uintptr_t src = (uintptr_t)source;

	if (!size || dst == src)
		return destination;
	if (dst - src >= size)
		return CopyMemoryForward(destination, source, size);
	dst += size;
	src += size;
	if (size >= 32) {
		while (dst & 3) {
			uint8_t byte = *(const volatile uint8_t *)--src;

			*(volatile uint8_t *)--dst = byte;
			size--;
		}
		while (dst & 31) {
			uint32_t word;

			src -= 4;
			dst -= 4;
			word = *(const volatile MoveWord *)src;
			*(MoveWord *)dst = word;
			size -= 4;
		}
		if (!(src & 3)) {
			while (size >= 32) {
				uint32_t upper[4];
				MoveVector16 lower;

				src -= 16;
				upper[0] = *(const volatile MoveWord *)(src + 0);
				upper[1] = *(const volatile MoveWord *)(src + 4);
				upper[2] = *(const volatile MoveWord *)(src + 8);
				upper[3] = *(const volatile MoveWord *)(src + 12);
				src -= 16;
				lower = *(const volatile MoveVector16 *)src;
				dst -= 16;
				*(MoveWord *)(dst + 0) = upper[0];
				*(MoveWord *)(dst + 4) = upper[1];
				*(MoveWord *)(dst + 8) = upper[2];
				*(MoveWord *)(dst + 12) = upper[3];
				dst -= 16;
				*(volatile MoveVector16 *)dst = lower;
				size -= 32;
			}
		} else {
			while (size >= 32) {
				MoveVector16 lower, upper;

				src -= 32;
				lower = *(const volatile MoveVector16 *)src;
				upper = *(const volatile MoveVector16 *)(src + 16);
				dst -= 32;
				*(volatile MoveVector16 *)dst = lower;
				*(volatile MoveVector16 *)(dst + 16) = upper;
				size -= 32;
			}
		}
	}
	while (size >= 4) {
		uint32_t word;

		src -= 4;
		dst -= 4;
		word = *(const volatile MoveWord *)src;
		*(MoveWord *)dst = word;
		size -= 4;
	}
	if (size & 2) {
		uint16_t halfword;

		src -= 2;
		dst -= 2;
		halfword = *(const volatile MoveHalfword *)src;
		*(MoveHalfword *)dst = halfword;
	}
	if (size & 1) {
		uint8_t byte = *(const volatile uint8_t *)(src - 1);

		*(volatile uint8_t *)(dst - 1) = byte;
	}
	return destination;
}

void *__memmove_chk(void *destination, const void *source, size_t size,
		    size_t capacity)
{
	if (capacity < size)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return MoveMemoryCore(destination, source, size);
}

#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
void *memmove(void *, const void *, size_t)
	SYSCLIB_ALIAS_TARGET(MoveMemoryCore);
#endif
