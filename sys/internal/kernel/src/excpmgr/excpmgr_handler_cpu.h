/* Shared exception cpu definitions. */
#pragma once

#include <excpmgr/excpmgr_cpu.h>

/* The vector page is read-only; domain manager access permits patching it. */
#define DACR_ALL_MANAGER	0xffff0000
#define DACR_KERNEL	0x15450000

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
	SceCpuForDriver_9CB9F0CE((void *)cacheLineDown(address),
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
	SceCpuForDriver_9CB9F0CE((void *)cacheLineDown(address),
		cacheLineDown(address + 4 + 31) - cacheLineDown(address));
	writeDacr(dacr);
	dsb();
}

static inline void excpSetVector(unsigned int code, SceExcpmgrHandler *handler)
{
	unsigned int entry = (unsigned int)handler + 8;

	excpPokeVector(&g_excpHandlers.vectors[0x40 + code], &entry);
}

