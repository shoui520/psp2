/* Venezia DSP DMA copy operations. */
#include <sce/debug.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include "venezia_internal.h"

SceKernelDmaOpId g_veneziaDmaOperations[9];
SceKernelDmaOpChainParam g_veneziaDmaChainParam = {
	.size = sizeof(SceKernelDmaOpChainParam),
	.coherencyMask = 0x3ffff,
};
int sceCodecEngineDsp0ConvertPhysicalToVirtual(VeneziaRpcThunk *thunk);
int sceCodecEngineDsp0ConvertVirtualToPhysical(VeneziaRpcThunk *thunk, void *base,
	SceSize size, const VeneziaMemoryOption *option);
int sceCodecEngineDsp0AllocCommunicationMemories(VeneziaRpcThunk *thunk);
int sceCodecEngineDsp0FreeCommunicationMemories(VeneziaRpcThunk *thunk);

/* name unknown: allocate one DMA operation per RPC channel. */
int initVeneziaRpcDma(void)
{
	char name[32];
	unsigned int channel;
	int ret;

	memset(g_veneziaDmaOperations, -1, sizeof(g_veneziaDmaOperations));
	for (channel = 0; channel < 9; channel++) {
		snprintf(name, sizeof(name), "SceVeneziaDmacMemcpy%02u", channel);
		ret = ksceKernelDmaOpAlloc(name);
		if (ret < 0)
			break;
		g_veneziaDmaOperations[channel] = ret;
		ret = ksceKernelDmaOpAssign(ret, 16, 0);
		if (ret < 0)
			break;
	}
	return ret;
}

static inline __attribute__((always_inline)) int isVeneziaUncachedAddress(uintptr_t address)
{
	if ((unsigned int)(address - 0x18000000) < 0x08000000)
		return ksceKernelIsUncacheAddressInTmpFsGame((void *)address);
	return (unsigned int)(address - 0x20000000) < 0x20000000 ||
		(unsigned int)(address - 0x60000000) < 0x21000000;
}

int sceCodecEngineDsp0MemcpyChain(VeneziaRpcThunk *thunk)
{
	SceKernelDmaOpTag tags[16];
	unsigned int tagCount = 0;
	unsigned int i, count, channel;
	int packing, ret;

#if FW_DEBUG
	ksceKernelAssert(thunk != NULL, SCE_DBGINFO_FUNC(390, "sceCodecEngineDsp0MemcpyChain"), __builtin_return_address(0));
	ksceKernelAssert(thunk->channel <= 8, SCE_DBGINFO_FUNC(391, "sceCodecEngineDsp0MemcpyChain"), __builtin_return_address(0));
	ksceKernelAssert(thunk->allocationCount <= 16, SCE_DBGINFO_FUNC(392, "sceCodecEngineDsp0MemcpyChain"), __builtin_return_address(0));
#endif
	channel = thunk->channel;
	count = thunk->allocationCount;
	packing = thunk->packing;
	if (!count)
		return 0;
	i = 0;
	do {
		VeneziaRpcMemoryTransfer *transfer = &thunk->memories[i];
		uintptr_t mapped, source, destination;
		const void *copySource;
		void *copyDestination;
		SceSize size;
		SceKernelDmaOpTag *tag, *previous;

		if (!transfer->copyRequired)
			goto nextTransfer;
		__builtin_prefetch(&thunk->memories[i + 1].nonContiguous);
		if (!transfer->nonContiguous) {
			if (transfer->flags & 4) {
				if (packing) {
					mapped = veneziaPhysicalToVirtual(transfer->physicalBase);
					ret = ksceKernelCopyFromUser((void *)mapped, transfer->virtualBase, transfer->size);
#if FW_DEBUG
					ksceKernelAssert(ret == 0, SCE_DBGINFO_FUNC(426, "sceCodecEngineDsp0MemcpyChain"), __builtin_return_address(0));
#endif
				} else {
					void *base = transfer->virtualBase;
					mapped = veneziaPhysicalToVirtual(transfer->physicalBase);
					ret = ksceKernelCopyToUser(base, (void *)mapped, transfer->size);
#if FW_DEBUG
					ksceKernelAssert(ret == 0, SCE_DBGINFO_FUNC(433, "sceCodecEngineDsp0MemcpyChain"), __builtin_return_address(0));
#endif
				}
				if (ret < 0)
					return ret;
			} else if (packing) {
				mapped = veneziaPhysicalToVirtual(transfer->physicalBase);
				memcpy((void *)mapped, transfer->virtualBase, transfer->size);
			} else {
				void *base = transfer->virtualBase;
				mapped = veneziaPhysicalToVirtual(transfer->physicalBase);
				memcpy(base, (void *)mapped, transfer->size);
			}
			goto nextTransfer;
		}
		size = transfer->size;
		if (!size)
			return 0x80600001;
		if (packing) {
			copySource = transfer->virtualBase;
			copyDestination = (void *)transfer->physicalBase;
			source = (uintptr_t)copySource;
			mapped = veneziaPhysicalToVirtual((uintptr_t)copyDestination);
			destination = mapped;
		} else {
			copySource = (void *)transfer->physicalBase;
			copyDestination = transfer->virtualBase;
			destination = (uintptr_t)copyDestination;
			mapped = veneziaPhysicalToVirtual((uintptr_t)copySource);
			source = mapped;
		}
		if (destination >= source) {
			if (destination < source + size)
				return 0x80600004;
		} else if (source <= destination + size) {
			return 0x80600004;
		}
#if FW_DEBUG
		ksceKernelAssert(tagCount <= 15, SCE_DBGINFO_FUNC(500, "_dmacMemcpy"), __builtin_return_address(0));
#endif
		tag = &tags[tagCount];
		previous = tagCount ? &tags[tagCount - 1] : NULL;
		tag->src = copySource;
		tag->dst = copyDestination;
		tag->len = size;
		tag->cmd = 0x2000;
		if (packing) {
			if (!isVeneziaUncachedAddress((uintptr_t)tag->src))
				tag->cmd |= 0x01000000;
		} else if (!isVeneziaUncachedAddress((uintptr_t)tag->dst)) {
			tag->cmd += 0x01ffe000;
		}
		tag->blockSize = 0;
		tag->pNext = SCE_KERNEL_DMAC_CHAIN_END;
		if (previous)
			previous->pNext = tag;
		tagCount++;
nextTransfer:
		i++;
	} while (i != count);
	if (!tagCount)
		return 0;
	ret = ksceKernelDmaOpSetupChain(g_veneziaDmaOperations[channel], tags,
		&g_veneziaDmaChainParam, packing ? SCE_KERNEL_DMA_OP_VIRTUAL_SRC_ADDR : SCE_KERNEL_DMA_OP_VIRTUAL_DST_ADDR);
	if (ret >= 0) {
		ret = ksceKernelDmaOpEnQueue(g_veneziaDmaOperations[channel]);
		if (ret >= 0)
			ksceKernelDmaOpSync(g_veneziaDmaOperations[channel],
				SCE_KERNEL_DMA_OP_SYNC_WAIT, 0, 0);
	}
	return ret;
}

/* name unknown: complete the thunk's output copies and release its buffers. */
void unpackVeneziaRpcMemory(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

/* name unknown: pack the performance-monitor's four-byte communication word. */
int packVeneziaRpcWord(VeneziaRpcThunk *thunk, const unsigned int *arguments,
		       unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	option.flags = 1;
	option.allocationSize = 4;
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)arguments[0], 4, &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	ret = sceCodecEngineDsp0MemcpyChain(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	return ret;
}

/* name unknown: pack a 64-byte communication buffer and its argument. */
int packVeneziaRpcBuffer64(VeneziaRpcThunk *thunk, const unsigned int *arguments,
			   unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	option.flags = 1;
	option.allocationSize = 64;
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)arguments[0], 64, &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	ret = sceCodecEngineDsp0MemcpyChain(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	packed[1] = arguments[1];
	return ret;
}
