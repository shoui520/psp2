/* SCE process mapping records owned by common/memorymanager.c. */
#ifndef MAPPED_MEMORY_SCE_H
#define MAPPED_MEMORY_SCE_H

#include <gpu/sce_ddk_port.h>

struct GpuHeap;
struct Resource;

typedef struct GpuMappedMemory {
	IMG_VOID *cpuAddress;
	IMG_DEV_VIRTADDR deviceAddress;
	IMG_SIZE_T size;
	IMG_UINT32 flags;
	struct GpuHeap *heap;
	struct Resource *resource;
	IMG_UINT32 mmuFlags;
	struct GpuMappedMemory *next;
} GpuMappedMemory;

typedef struct GpuMappedMemoryList {
	GpuMappedMemory *head;
} GpuMappedMemoryList;

#endif
