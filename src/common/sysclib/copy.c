/* Native forward-copy groups retain behavior even when buffers overlap. */
#include <stddef.h>
#include <stdint.h>
#include "fw.h"
#include "aliases.h"

typedef uint8_t CopyVector16 __attribute__((vector_size(16), aligned(1), may_alias));
typedef uint32_t CopyWord __attribute__((aligned(1), may_alias));
typedef uint16_t CopyHalfword __attribute__((aligned(1), may_alias));

void *CopyMemoryForward(void *destination, const void *source, size_t size)
{
	uintptr_t dst = (uintptr_t)destination;
	uintptr_t src = (uintptr_t)source;

	if (size >= 32) {
		while (dst & 3) {
			uint8_t byte = *(const volatile uint8_t *)src++;

			*(volatile uint8_t *)dst++ = byte;
			size--;
		}
		while (dst & 31) {
			uint32_t word = *(const volatile CopyWord *)src;

			*(CopyWord *)dst = word;
			src += 4;
			dst += 4;
			size -= 4;
		}
		if (!(src & 3)) {
			while (size >= 32) {
				uint32_t lower[4];
				CopyVector16 upper;

				lower[0] = *(const volatile CopyWord *)(src + 0);
				lower[1] = *(const volatile CopyWord *)(src + 4);
				lower[2] = *(const volatile CopyWord *)(src + 8);
				lower[3] = *(const volatile CopyWord *)(src + 12);
				upper = *(const volatile CopyVector16 *)(src + 16);
				*(CopyWord *)(dst + 0) = lower[0];
				*(CopyWord *)(dst + 4) = lower[1];
				*(CopyWord *)(dst + 8) = lower[2];
				*(CopyWord *)(dst + 12) = lower[3];
				*(volatile CopyVector16 *)(dst + 16) = upper;
				src += 32;
				dst += 32;
				size -= 32;
			}
		} else {
			while (size >= 32) {
				CopyVector16 lower, upper;

				lower = *(const volatile CopyVector16 *)src;
				upper = *(const volatile CopyVector16 *)(src + 16);
				*(volatile CopyVector16 *)dst = lower;
				*(volatile CopyVector16 *)(dst + 16) = upper;
				src += 32;
				dst += 32;
				size -= 32;
			}
		}
	}
	while (size >= 4) {
		uint32_t word = *(const volatile CopyWord *)src;

		*(CopyWord *)dst = word;
		src += 4;
		dst += 4;
		size -= 4;
	}
	if (size & 2) {
		uint16_t halfword = *(const volatile CopyHalfword *)src;

		*(CopyHalfword *)dst = halfword;
		src += 2;
		dst += 2;
	}
	if (size & 1) {
		uint8_t byte = *(const volatile uint8_t *)src;

		*(volatile uint8_t *)dst = byte;
	}
	return destination;
}

void *__memcpy_chk(void *destination, const void *source, size_t size,
		   size_t capacity)
{
	if (capacity < size)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return CopyMemoryForward(destination, source, size);
}

#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
void *memcpy(void *, const void *, size_t)
	SYSCLIB_ALIAS_TARGET(CopyMemoryForward);
#endif
