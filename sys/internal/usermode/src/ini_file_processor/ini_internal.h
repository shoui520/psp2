/*
 * SceIniFileProcessor: the library's own types. The processor reads and writes INI files; its work
 * goes through an allocator that is either the application's (a pair of functions) or a pool the
 * library makes from a memory block.
 */

#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>

/* the libkernel mspace calls the tree has no header of yet */
typedef void *SceClibMspace;
extern "C" {
void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
void sceClibMspaceFree(SceClibMspace mspace, void *ptr);
SceClibMspace sceClibMspaceCreate(void *base, SceSize size);
int sceClibMspaceDestroy(SceClibMspace mspace);
}

#define SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY	-2138832895	/* 0x80840001 */
#define SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN		-2138832894	/* 0x80840002 */
#define SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT	-2138832890	/* 0x80840006 */

/* the application's allocator: one function to take memory and one to give it back */
struct IniAllocatorOps {
	void *(*alloc)(SceSize size, void *context);
	void (*free)(void *ptr, void *context);
};

/* the library's pool of 4096 byte blocks, which it makes from a memory block when it is first used */
struct IniPool : IniAllocatorOps {
	SceUID memBlock;
	SceClibMspace mspace;
	SceSize size;
};

/* what a processor allocates with: the functions, their context and whether the library owns them */
struct IniAllocatorRef {
	IniAllocatorOps *ops;
	void *context;
	bool owned;
};

/* what the application gives the processor: the size of the pool, or its own allocator */
struct IniConfig {
	SceSize poolSize;
	SceUInt32 reserved;
	IniAllocatorOps *ops;
	void *context;
};

extern int g_poolsCreated;	/* 0x81005010 */
extern int g_poolsDestroyed;	/* 0x81005014 */

extern IniAllocatorRef *iniAllocatorRefInit(IniAllocatorRef *ref);
extern IniAllocatorRef *iniAllocatorRefRelease(IniAllocatorRef *ref);
extern int iniAllocatorRefSet(IniAllocatorRef *ref, const IniConfig *config);
extern int iniAllocatorRefCreatePool(IniAllocatorRef *ref, SceSize size);
extern void *iniAlloc(IniAllocatorRef *ref, SceSize size);
extern void iniFree(IniAllocatorRef *ref, void *ptr);
extern IniAllocatorOps *iniDefaultAllocator(void);
