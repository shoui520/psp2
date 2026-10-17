/* hwd0_vapa.c */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "../venezia_internal.h"
#include <vnz_wrapper/venezia_debug.h>
#include <string.h>

extern void (*SceSysrootForDriver_483EF108(void))(unsigned int, unsigned int, ...);
SceKernelDmaOpChainParam g_veneziaHwdDmaChainParam = {
	.size = sizeof(SceKernelDmaOpChainParam),
	.coherencyMask = 0x3ffff,
};
VeneziaHwdDmaState g_veneziaHwdDmaState;

static inline __attribute__((always_inline)) int isVeneziaHwdUncachedAddress(uintptr_t address)
{
	if ((unsigned int)(address - 0x18000000) < 0x08000000)
		return ksceKernelIsUncacheAddressInTmpFsGame((void *)address);
	return (unsigned int)(address - 0x20000000) < 0x20000000 ||
		(unsigned int)(address - 0x60000000) < 0x21000000;
}
int _hwd0DmacMemcpyBlocking(const VeneziaHwdDmaRequest *request)
{
	SceKernelDmaOpTag tags[8];
	unsigned int channel, count, index;
	int ret;

#if FW_DEBUG
	ksceKernelAssert(request != NULL, SCE_DBGINFO_FUNC(282, "_hwd0DmacMemcpyBlocking"), __builtin_return_address(0));
#endif
	count = request->count;
	channel = request->channel;
#if FW_DEBUG
	ksceKernelAssert(channel == 0, SCE_DBGINFO_FUNC(286, "_hwd0DmacMemcpyBlocking"), __builtin_return_address(0));
	ksceKernelAssert(count <= 8, SCE_DBGINFO_FUNC(287, "_hwd0DmacMemcpyBlocking"), __builtin_return_address(0));
#endif
	if (!count)
		return 0;
	index = 0;
	do {
		const VeneziaHwdDmaTransfer *transfer = &request->transfers[index];
		uintptr_t source = (uintptr_t)transfer->source;
		uintptr_t destination = (uintptr_t)transfer->destination;
		SceSize size = transfer->size;
		SceKernelDmaOpTag *tag = &tags[index];
		SceKernelDmaOpTag *previous = index ? &tags[index - 1] : NULL;
		unsigned int command;

		if (!size)
			return (int)0x80010016;
		if (source <= destination) {
			if (destination < source + size)
				return (int)0x80010016;
		} else if (source <= destination + size) {
			return (int)0x80010016;
		}
		command = isVeneziaHwdUncachedAddress(source) ? 0x2000 : 0x01002000;
		if (!isVeneziaHwdUncachedAddress(destination))
			command += 0x01ffe000;
#if FW_DEBUG
		ksceKernelAssert(index <= 7, SCE_DBGINFO_FUNC(363, "_dmacMemcpy"), __builtin_return_address(0));
#endif
		tag->src = transfer->source;
		tag->dst = transfer->destination;
		tag->len = size;
		tag->cmd = command;
		tag->blockSize = 0;
		tag->pNext = SCE_KERNEL_DMAC_CHAIN_END;
		if (previous)
			previous->pNext = tag;
		else
			__builtin_prefetch(&tags[3].dst);
		index++;
	} while (index != count);
	ksceKernelLockFastMutex(g_veneziaHwdDmaState.activeMutex);
	ret = ksceKernelDmaOpSetupChain(g_veneziaHwdDmaState.operations[channel], tags,
		&g_veneziaHwdDmaChainParam, 17);
	if (ret >= 0) {
		ret = ksceKernelDmaOpEnQueue(g_veneziaHwdDmaState.operations[channel]);
		if (ret >= 0)
			ksceKernelDmaOpSync(g_veneziaHwdDmaState.operations[channel], 2, 0, 0);
	}
	ksceKernelUnlockFastMutex(g_veneziaHwdDmaState.activeMutex);
	return ret;
}

int _hwd0DmacMemcpyBlocking(const VeneziaHwdDmaRequest *request);

typedef struct VeneziaVipInputBufferList {
	unsigned int count;
	const VeneziaVipInputBuffer *buffers;
} VeneziaVipInputBufferList;

/* name unknown: cursor through the caller's scatter/gather input buffers. */
typedef struct VeneziaVipInputIterator {
	unsigned int bufferIndex;
	SceSize bufferOffset;
	SceSize position;
	SceSize totalSize;
	const VeneziaVipInputBufferList *list;
} VeneziaVipInputIterator;

extern int snprintf(char *destination, unsigned int capacity, const char *format, ...);

int allocateVeneziaVipMemoryBlock(VeneziaVipMemoryBlock *block,
	const char *name, unsigned int type, SceSize size, uintptr_t physicalAddress)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID uid;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if (physicalAddress) {
		opt.paddr = physicalAddress;
		opt.attr = 2;
		uid = ksceKernelAllocMemBlock(name, type, size, &opt);
	} else {
		uid = ksceKernelAllocMemBlock(name, type, size, NULL);
	}
	if (uid < 0 || !block)
		return uid;
	block->uid = uid;
	ksceKernelGetMemBlockBase(uid, (void **)&block->region.virtualAddress);
	ret = ksceKernelVAtoPA((void *)block->region.virtualAddress,
		&block->region.physicalAddress);
	if (ret >= 0) {
		ret = 0;
		block->region.size = size;
	}
	return ret;
}

int releaseVeneziaVipMemoryBlock(VeneziaVipMemoryBlock *block)
{
	ksceKernelFreeMemBlock(block->uid);
	block->uid = -1;
	block->region.physicalAddress = 0;
	block->region.virtualAddress = 0;
	block->region.size = 0;
	return 0;
}

int offsetVeneziaVipMemoryRegion(VeneziaVipMemoryRegion *destination, const VeneziaVipMemoryRegion *source, SceSize offset)
{
	uintptr_t physicalAddress;
	uint32_t virtualAddress;
	SceSize size;

	if (!source || !destination)
		return -2141060862;
	virtualAddress = source->virtualAddress;
	size = source->size;
	physicalAddress = source->physicalAddress + offset;
	destination->physicalAddress = physicalAddress;
	destination->virtualAddress = virtualAddress + offset;
	destination->size = source->physicalAddress + size - physicalAddress;
	return 0;
}

int setVeneziaVipMemoryRegion(VeneziaVipMemoryRegion *destination, const VeneziaVipMemoryRegion *source, SceSize offset, SceSize size)
{
	uintptr_t physicalAddress;
	uint32_t virtualAddress;

	if (!source || !destination)
		return -2141060862;
	physicalAddress = source->physicalAddress;
	virtualAddress = source->virtualAddress;
	destination->size = size;
	destination->physicalAddress = physicalAddress + offset;
	destination->virtualAddress = virtualAddress + offset;
	return 0;
}

/* name unknown: initialize the one blocking-copy DMA channel. */
int initializeVeneziaHwdDma(void)
{
	char name[32];
	int ret;

	g_veneziaHwdDmaState.activeMutex = NULL;
	g_veneziaHwdDmaState.operations[0] = -1;
	ret = ksceKernelInitializeFastMutex(&g_veneziaHwdDmaState.mutex,
		"SceHwd0Dma", 2, 0);
	if (ret >= 0) {
		g_veneziaHwdDmaState.activeMutex = &g_veneziaHwdDmaState.mutex;
		snprintf(name, sizeof(name), "SceHwd0DmacMemcpy%02u", 0);
		ret = ksceKernelDmaOpAlloc(name);
		if (ret >= 0) {
			g_veneziaHwdDmaState.operations[0] = ret;
			ret = ksceKernelDmaOpAssign(ret, 16, 0);
		}
	}
	return ret;
}

SceSize getVeneziaVipInputRemaining(const VeneziaVipInputIterator *iterator)
{
	return iterator->totalSize - iterator->position;
}

int initializeVeneziaVipInputIterator(VeneziaVipInputIterator *iterator, const VeneziaVipInputBufferList *list)
{
	unsigned int index;
	SceSize total = 0;

	iterator->list = list;
	iterator->bufferIndex = 0;
	iterator->bufferOffset = 0;
	iterator->position = 0;
	for (index = 0; index < list->count; index++)
		total += list->buffers[index].size;
	iterator->totalSize = total;
	return 0;
}

int copyVeneziaVipInput(uint32_t destination, SceSize size, SceSize *copied, VeneziaVipInputIterator *iterator)
{
	VeneziaHwdDmaRequest request;
	unsigned int count = 0;

	*copied = 0;
	request.channel = 0;
	request.count = 0;
	request.transfers[0] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[1] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[2] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[3] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[4] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[5] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[6] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	request.transfers[7] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
	while (*copied < size && iterator->bufferIndex < iterator->list->count) {
		const VeneziaVipInputBuffer *buffer = &iterator->list->buffers[iterator->bufferIndex];
		SceSize offset = iterator->bufferOffset;
		SceSize length;
		const void *source;

		if (offset >= buffer->size)
			return -1;
		length = buffer->size - offset;
		if (size - *copied < length)
			length = size - *copied;
		source = (const char *)buffer->data + offset;
		if (count > 7) {
			_hwd0DmacMemcpyBlocking(&request);
			request.channel = 0;
			request.transfers[0] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[1] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[2] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[3] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[4] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[5] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[6] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			request.transfers[7] = (VeneziaHwdDmaTransfer){ .unidentifiedTransferControl = 1 };
			count = 0;
		}
		*copied += length;
		request.transfers[count].destination = (void *)destination;
		destination += length;
		request.transfers[count].source = source;
		request.transfers[count].size = length;
		count++;
		request.count = count;
		iterator->bufferOffset += length;
		iterator->position += length;
		if (iterator->bufferOffset >= buffer->size) {
			iterator->bufferOffset = 0;
			iterator->bufferIndex++;
		}
	}
	_hwd0DmacMemcpyBlocking(&request);
	return 0;
}

unsigned int decodeVeneziaVipNalLength(const uint8_t *bytes, unsigned int size)
{
	switch (size) {
	case 1:
		return bytes[0];
	case 2:
		return bytes[1] | (bytes[0] << 8);
	case 3:
		return (bytes[1] << 8) | (bytes[0] << 16) | bytes[2];
	case 4:
		return __builtin_bswap32(*(const uint32_t *)bytes);
	default:
		return 0;
	}
}

int SceCodecEngineWrapperForDriver_FAF590A6(void *destination,
	const void *source, SceSize size)
{
#if FW_DEBUG
	VeneziaHwdDmaRequest request __attribute__((aligned(8)));

	request.transfers[0].destination = destination;
	request.transfers[0].size = size;
	request.count = 1;
	request.transfers[0].unidentifiedTransferControl = 1;
	request.transfers[0].source = source;
	request.channel = 0;
	request.transfers[0].unidentifiedTransferFlags = 0;
	return _hwd0DmacMemcpyBlocking(&request);
#else
	SceKernelDmaOpTag tags[8];
	uintptr_t sourceAddress = (uintptr_t)source;
	uintptr_t destinationAddress = (uintptr_t)destination;
	unsigned int command;
	int ret;

	if (!size)
		return (int)0x80010016;
	if (destinationAddress >= sourceAddress) {
		if (destinationAddress < sourceAddress + size)
			return (int)0x80010016;
	} else if (sourceAddress <= destinationAddress + size) {
		return (int)0x80010016;
	}
	command = isVeneziaHwdUncachedAddress(sourceAddress) ? 0x2000 : 0x01002000;
	if (!isVeneziaHwdUncachedAddress(destinationAddress))
		command += 0x01ffe000;
	tags[0].src = source;
	tags[0].dst = destination;
	tags[0].blockSize = 0;
	tags[0].pNext = SCE_KERNEL_DMAC_CHAIN_END;
	tags[0].len = size;
	tags[0].cmd = command;
	ksceKernelLockFastMutex(g_veneziaHwdDmaState.activeMutex);
	ret = ksceKernelDmaOpSetupChain(g_veneziaHwdDmaState.operations[0], tags,
		&g_veneziaHwdDmaChainParam, 17);
	if (ret >= 0) {
		ret = ksceKernelDmaOpEnQueue(g_veneziaHwdDmaState.operations[0]);
		if (ret >= 0)
			ksceKernelDmaOpSync(g_veneziaHwdDmaState.operations[0], 2, 0, 0);
	}
	ksceKernelUnlockFastMutex(g_veneziaHwdDmaState.activeMutex);
	return ret;
#endif
}
