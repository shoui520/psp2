#pragma once

#include <stddef.h>
#include <stdint.h>

/* Performance-monitor timebase services copied into Sysroot by NID C94C76FA. */
struct PamgrTimebaseProvider {
	uint32_t size;
	unsigned int (*getFrequency)(void);
	uint64_t (*getValue)(void);
	volatile uint32_t *(*getRegister)(unsigned int bank, unsigned int index);
};

_Static_assert(sizeof(struct PamgrTimebaseProvider) == 0x10,
	       "Sysroot performance-monitor timebase table size");
_Static_assert(offsetof(struct PamgrTimebaseProvider, getRegister) == 0xc,
	       "Sysroot performance-monitor register query offset");
