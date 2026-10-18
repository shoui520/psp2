/* SceSdstor device handles (Sony source: sdstor.c). */
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "sdstor_internal.h"

SceUID sdstorHeapId = 0;
uint32_t sdstorOpenCount[5] = {0};

int sdstor_initialize(void)
{
	SceKernelHeapCreateOpt opt;

	opt.size = sizeof(opt);
	opt.flags = HEAP_AUTO_EXTEND;
	sdstorHeapId = sceKernelCreateHeap("SceSdStorDeviceHandle", 0x2000, &opt);
	if (sdstorHeapId < 0) {
		SCE_DBG_PRINTF(15, 47, "0x%08x\n", sdstorHeapId);
		return sdstorHeapId;
	}
	return 0;
}

int sdstor_construct(SceSdstorPartitionEntry *partition, int flags,
		     SceSdstorFd **resultHandle)
{
	SceSdstorFd *handle;
	SceSdstorSdpart *table;
	int result, port;

	SCE_DBG_ASSERT(partition && resultHandle, 71);
	handle = sceKernelAllocHeapMemory(sdstorHeapId, sizeof(*handle));
	if (!handle)
		return 0x8001000c;
	result = sceKernelInitializeFastMutex(&handle->fastmutex,
			"SceSdStorDeviceHandle", 0, 0);
	if (result != 0) {
		SCE_DBG_PRINTF(15, 89, "0x%08x\n", result);
		sceKernelFreeHeapMemory(sdstorHeapId, handle);
		return result;
	}
	table = partition->sdpart;
	handle->flags = flags;
	handle->pentry = partition;
	result = lockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	handle->n_sectors = partition->n_sectors;
	handle->blockSize = table->blockSize;
	handle->start_lba = partition->start_lba;
	handle->sddev = table->sddev;
	port = sddev_get_port(handle->sddev);
	SCE_DBG_ASSERT(port <= 4, 107);
	++sdstorOpenCount[port];
	result = sceKernelUnlockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	*resultHandle = handle;
	return 0;
}

int sdstor_destruct(SceSdstorFd *handle)
{
	int result, port;

	SCE_DBG_ASSERT(handle != NULL, 122);
	result = sceKernelFinalizeFastMutex(&handle->fastmutex);
	if (result != 0) {
		SCE_DBG_PRINTF(15, 126, "0x%08x\n", result);
		return result;
	}
	port = sddev_get_port(handle->sddev);
	SCE_DBG_ASSERT(port <= 4, 131);
	handle->pentry = NULL;
	handle->sddev = NULL;
	handle->start_lba = 0;
	handle->n_sectors = 0;
	handle->blockSize = 0;
	handle->flags = 0;
	--sdstorOpenCount[port];
	result = sceKernelFreeHeapMemory(sdstorHeapId, handle);
	if (result != 0)
		SCE_DBG_PRINTF(15, 143, "0x%08x\n", result);
	return result;
}

int sdstor_read(SceSdstorFd *handle, int64_t offset, void *buffer, uint32_t size)
{
	uint32_t blockSize, sectorCount, sector, transferred, chunk, blocks;
	uint64_t capacity;
	SceSdstorSddev *device;
	uint8_t *destination = buffer;
	int result;

	SCE_DBG_ASSERT(handle != NULL, 153);
	blockSize = handle->blockSize;
	sectorCount = handle->n_sectors;
	sector = handle->start_lba + (uint32_t)(offset / (int64_t)blockSize);
	device = handle->sddev;
	capacity = (uint64_t)blockSize * sectorCount;
	SCE_DBG_ASSERT(handle != NULL, 168);
	if (!(handle->flags & 1))
		return 0x8001000d;
	if ((offset & (blockSize - 1)) || (size & (blockSize - 1)))
		return 0x80010022;
	if ((capacity < (uint64_t)offset) | (offset < 0))
		return 0x80010021;
	if (capacity - (uint64_t)offset < size)
		return 0x80010021;
	if (!size)
		return 0;
	transferred = 0;
	for (;;) {
		chunk = size - transferred;
		if (chunk >= 0x2000000 - blockSize)
			chunk = 0x2000000 - blockSize;
		blocks = chunk / blockSize;
		result = sddev_read(device, destination, sector, blocks);
		if (result != 0)
			break;
		transferred += chunk;
		sector += blocks;
		destination += chunk;
		if (size <= transferred)
			return transferred;
	}
	if (transferred)
		return transferred;
	return result;
}

int sdstor_write(SceSdstorFd *handle, int64_t offset, const void *buffer,
		 uint32_t size)
{
	uint32_t blockSize, sectorCount, sector, transferred, chunk, blocks;
	uint64_t capacity;
	SceSdstorSddev *device;
	const uint8_t *source = buffer;
	int result;

	SCE_DBG_ASSERT(handle != NULL, 213);
	blockSize = handle->blockSize;
	sectorCount = handle->n_sectors;
	sector = handle->start_lba + (uint32_t)(offset / (int64_t)blockSize);
	device = handle->sddev;
	capacity = (uint64_t)blockSize * sectorCount;
	SCE_DBG_ASSERT(handle != NULL, 228);
	if (!(handle->flags & 2))
		return 0x8001000d;
	if ((offset & (blockSize - 1)) || (size & (blockSize - 1)))
		return 0x80010022;
	if ((capacity < (uint64_t)offset) | (offset < 0))
		return 0x80010021;
	if (capacity - (uint64_t)offset < size)
		return 0x80010021;
	if (!size)
		return 0;
	transferred = 0;
	for (;;) {
		chunk = size - transferred;
		if (chunk >= 0x2000000 - blockSize)
			chunk = 0x2000000 - blockSize;
		blocks = chunk / blockSize;
		result = sddev_write(device, source, sector, blocks);
		if (result != 0)
			break;
		transferred += chunk;
		sector += blocks;
		source += chunk;
		if (size <= transferred)
			return transferred;
	}
	if (transferred)
		return transferred;
	return result;
}
