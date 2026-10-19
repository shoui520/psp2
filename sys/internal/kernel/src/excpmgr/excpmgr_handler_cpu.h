/* Shared exception cpu definitions. */
#pragma once

#include <excpmgr/excpmgr_cpu.h>

static inline unsigned int cacheLineDown(unsigned int address)
{
	return address & ~31U;
}

/*
 * name unknown: store one word into read-only kernel text (a handler's link
 * word) and clean its cache line.
 */
static inline void excpPokeWord(unsigned int address, unsigned int value)
{
	unsigned int *word = (unsigned int *)(address & ~3U);
	unsigned int dacr = readDacr();

	writeDacr(DACR_ALL_MANAGER);
	*word++ = value;
	sceKernelDcacheCleanRangeForL1WBWA((void *)cacheLineDown(address),
		cacheLineDown((unsigned int)word + 31) - cacheLineDown(address));
	writeDacr(dacr);
}

/* name unknown: store one word of the vector page and make it visible */
static inline void excpPokeVector(unsigned int *slot, const volatile unsigned int *value)
{
	unsigned int dacr = readDacr();
	unsigned int address = (unsigned int)slot;

	writeDacr(DACR_ALL_MANAGER);
	*slot = *value;
	sceKernelDcacheCleanRangeForL1WBWA((void *)cacheLineDown(address),
		cacheLineDown(address + 4 + 31) - cacheLineDown(address));
	writeDacr(dacr);
	dsb();
}

static inline void excpSetVector(unsigned int code, SceExcpmgrHandler *handler)
{
	unsigned int entry = (unsigned int)handler + 8;

	excpPokeVector(&g_excpHandlers.vectors[0x40 + code], &entry);
}

