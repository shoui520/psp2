/*
 * np_friend.cpp: the shell's side of the NP friend libraries (0x810F5E88-0x810F60FC): the memory
 * block they allocate from (SceNpFriendVshMemBlock, an mspace), and their start: the friend
 * storage on its own heap, the importer on another, and paf's allocator for the friend list. A
 * failed step undoes those before it. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <memory/heap_allocator.h>
#include "shell.h"

using namespace paf;

/* the heaps the storage and the importer get */
#define NP_FRIEND_MODEL_HEAP_SIZE	0x5000
#define NP_FRIEND_IMPORTER_HEAP_SIZE	0xC000

/* (0x81544764, 0x81544768) the memory block and the mspace made of it */
static SceUID s_memBlock = 0;
static void *s_mspace = NULL;

/* (0x8154476C, 0x81544770) the heaps' memory, from the mspace */
static void *s_modelMem = NULL;
static void *s_importerMem = NULL;

/* (0x8155959C, 0x81559598) the heaps */
static memory::HeapAllocator *s_modelHeap;
static memory::HeapAllocator *s_importerHeap;

/* (0x810F5E88) */
int shellNpFriendMemInit(SceSize size)
{
	void *base = NULL;

	s_memBlock = sceKernelAllocMemBlock("SceNpFriendVshMemBlock", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					    size, NULL);
	if (s_memBlock < 0)
		return -1;

	int ret = sceKernelGetMemBlockBase(s_memBlock, &base);

	if (ret >= 0) {
		s_mspace = sceClibMspaceCreate(base, size);
		if (s_mspace != NULL)
			return ret;
		ret = -1;
	}
	sceKernelFreeMemBlock(s_memBlock);
	s_memBlock = 0;
	return ret;
}

/* (0x810F5F18) */
void *shellNpFriendMalloc(SceSize size)
{
	if (s_mspace == NULL)
		return NULL;
	return sceClibMspaceMalloc(s_mspace, size);
}

/* (0x810F5F34) */
void shellNpFriendFree(void *ptr)
{
	if (ptr != NULL)
		sceClibMspaceFree(s_mspace, ptr);
}

/* (0x810F5F4A) */
void shellNpFriendInit(void)
{
	if (s_modelMem == NULL) {
		s_modelMem = shellNpFriendMalloc(NP_FRIEND_MODEL_HEAP_SIZE);
		if (s_modelMem == NULL)
			return;
		s_modelHeap = new memory::HeapAllocator(s_modelMem, NP_FRIEND_MODEL_HEAP_SIZE,
							"SceNpFriendVshModelAlloc", NULL);
		if (s_modelHeap == NULL)
			goto free_model;
		if (npFriendStorageInit(s_modelHeap) < 0)
			goto delete_model;
	}
	if (s_importerMem == NULL) {
		s_importerMem = shellNpFriendMalloc(NP_FRIEND_IMPORTER_HEAP_SIZE);
		if (s_importerMem == NULL)
			goto term_storage;
		s_importerHeap = new memory::HeapAllocator(s_importerMem, NP_FRIEND_IMPORTER_HEAP_SIZE,
							   "SceNpFriendImporterAlloc", NULL);
		if (s_importerHeap == NULL)
			goto free_importer;
		if (npFriendImporterInit(s_importerHeap) < 0)
			goto delete_importer;
	}
	{
		NpFriendAllocator allocator = { sce_paf_malloc, sce_paf_free };

		if (npFriendListSetAllocator(&allocator) >= 0)
			return;
	}
	npFriendImporterTerm();
delete_importer:
	if (s_importerHeap != NULL)
		delete s_importerHeap;
	s_importerHeap = NULL;
free_importer:
	shellNpFriendFree(s_importerMem);
	s_importerMem = NULL;
term_storage:
	npFriendStorageTerm();
delete_model:
	if (s_modelHeap != NULL)
		delete s_modelHeap;
	s_modelHeap = NULL;
free_model:
	shellNpFriendFree(s_modelMem);
	s_modelMem = NULL;
}
