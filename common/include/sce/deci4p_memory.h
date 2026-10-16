/* Shared byte-copy and aligned-clear routines used by the DECI managers. */
#ifndef SCE_DECI4P_MEMORY_H
#define SCE_DECI4P_MEMORY_H

#include <stdint.h>
#include <psp2/types.h>

static inline void deci4pCopyBytes(void *destination, const void *source,
				 SceSize size)
{
	char *out = destination;
	const char *in = source;

	if (!size)
		return;
	do
		*(volatile char *)out++ = *in++;
	while (out != (char *)destination + size);
}

static inline void deci4pFillMemory(void *destination, int value, SceSize size)
{
	uint32_t *words;
	char *bytes;
	SceSize i;

	if (!value && (((uintptr_t)destination | size) & 3) == 0) {
		words = destination;
		for (i = 0; i != size >> 2; i++)
			*(volatile uint32_t *)words++ = 0;
	} else if ((int)size > 0) {
		for (bytes = destination; bytes != (char *)destination + size; bytes++)
			*(volatile char *)bytes = value;
	}
}

#endif
