/* Sony venezia/vapa.c: codec memory address translation. */
#include <sysmem/debug.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/cpu.h>
#include "../venezia_internal.h"
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>

extern void *getVeneziaImageBase(void);
extern unsigned int getVeneziaImagePhysicalBase(void);
extern SceSize getVeneziaImageSize(void);
extern VeneziaRpcMemory *getRpcMemoryBase(void);
extern unsigned int getRpcMemoryPhysicalBase(void);
extern SceSize getRpcMemorySize(void);

unsigned int SceCodecEngineWrapperForDriver_F9495589(unsigned int physical)
{
	return veneziaPhysicalToVirtual(physical);
}

static inline __attribute__((always_inline)) unsigned int veneziaVirtualToPhysical(unsigned int address)
{
	uintptr_t imageBase = (uintptr_t)getVeneziaImageBase();
	unsigned int imagePhysical = getVeneziaImagePhysicalBase();
	SceSize imageSize = getVeneziaImageSize();
	uintptr_t rpcBase = (uintptr_t)getRpcMemoryBase();
	unsigned int rpcPhysical = getRpcMemoryPhysicalBase();
	SceSize rpcSize = getRpcMemorySize();

	if (imagePhysical) {
		if (imageBase <= address) {
			if (address < imageBase + imageSize)
				return address - imageBase + imagePhysical;
		}
	}
	if (rpcPhysical) {
		if (rpcBase <= address) {
			if (address < rpcBase + rpcSize)
				return address - rpcBase + rpcPhysical;
		}
	}
	return 0;
}

unsigned int SceCodecEngineWrapperForDriver_C7C150AE(unsigned int address)
{
	return veneziaVirtualToPhysical(address);
}

#if FW_DEBUG
extern int isVeneziaPhysicalRangeContiguous(const void *base, SceSize size);
#else
extern int isVeneziaPhysicalRangeContiguous(const void *base, SceSize size);
#endif

int sceCodecEngineDsp0ConvertVirtualToPhysical(VeneziaRpcThunk *thunk, void *base,
					     SceSize size, const VeneziaMemoryOption *option)
{
	unsigned int physical = 0;
	unsigned int flags = 0;
	SceSize allocationSize = 0;
	int copyRequired, nonContiguous;
	int communicationMemory, ret;
	VeneziaRpcMemoryTransfer *transfer;
	uintptr_t cacheBase;
	SceSize cacheSize;

	if (option) {
		flags = option->flags;
		allocationSize = option->allocationSize;
	}
	copyRequired = flags & 1;
	nonContiguous = flags & 2;

#if FW_DEBUG
	ksceKernelAssert(thunk != NULL,
		SCE_DBGINFO_FUNC(135, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	ksceKernelAssert(thunk->channel <= 8,
		SCE_DBGINFO_FUNC(136, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	ksceKernelAssert(thunk->allocationCount <= 15,
		SCE_DBGINFO_FUNC(137, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	ksceKernelAssert(thunk->packing,
		SCE_DBGINFO_FUNC(138, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	if (base)
		ksceKernelAssert(size != 0,
			SCE_DBGINFO_FUNC(140, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	else
		ksceKernelAssert(size == 0 && flags == 1,
			SCE_DBGINFO_FUNC(142, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
	ksceKernelAssert((flags & 0x30) != 0x30,
		SCE_DBGINFO_FUNC(145, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
#endif
	if (copyRequired) {
#if FW_DEBUG
		ksceKernelAssert(size <= allocationSize,
			SCE_DBGINFO_FUNC(148, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
#endif
		if (!nonContiguous)
			nonContiguous = !isVeneziaPhysicalRangeContiguous(base, size);
		goto communication;
	}
#if FW_DEBUG
	ksceKernelAssert(allocationSize == 0,
		SCE_DBGINFO_FUNC(150, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
#endif
	if (!isVeneziaPhysicalRangeContiguous(base, size)) {
		allocationSize = size;
		nonContiguous = 1;
		goto communication;
	}
	ret = ksceKernelVAtoPA(base, &physical);
#if FW_DEBUG
	ksceKernelAssert(ret >= 0,
		SCE_DBGINFO_FUNC(167, "sceCodecEngineDsp0ConvertVirtualToPhysical"), __builtin_return_address(0));
#endif
	if (physical >= 0x40000000 && size + physical <= 0x80000000) {
		cacheBase = (uintptr_t)base & ~255U;
		cacheSize = (size + 255 + (uintptr_t)base - cacheBase) & ~255U;
		if (flags & 0x10)
			ksceKernelDcacheCleanRange((void *)cacheBase, cacheSize);
		else if (flags & 0x20)
			ksceKernelDcacheInvalidateRange((void *)cacheBase, cacheSize);
		else
			ksceKernelDcacheCleanInvalidateRange((void *)cacheBase, cacheSize);
	}
	copyRequired = 0;
	communicationMemory = 0;
	goto store;

communication:
	communicationMemory = 1;
	copyRequired = (flags & 0x20) ? 0 : size != 0;
	ret = 0;
store:
	transfer = &thunk->memories[thunk->allocationCount];
	transfer->copyRequired = copyRequired;
	transfer->nonContiguous = nonContiguous;
	transfer->physicalBase = physical;
	transfer->size = size;
	transfer->allocationSize = allocationSize;
	transfer->flags = flags;
	transfer->communicationMemory = communicationMemory;
	transfer->virtualBase = base;
	thunk->allocationCount++;
	return ret;
}

int sceCodecEngineDsp0ConvertPhysicalToVirtual(VeneziaRpcThunk *thunk)
{
	unsigned int count, i;
	VeneziaRpcMemoryTransfer *transfer;
	int copyRequired;

#if FW_DEBUG
	ksceKernelAssert(thunk != NULL,
		SCE_DBGINFO_FUNC(216, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
	ksceKernelAssert(thunk->channel <= 8,
		SCE_DBGINFO_FUNC(217, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
	ksceKernelAssert(thunk->allocationCount <= 16,
		SCE_DBGINFO_FUNC(218, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
	ksceKernelAssert(thunk->packing == 0,
		SCE_DBGINFO_FUNC(219, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
#endif
	count = thunk->allocationCount;
	for (i = 0; i != count; i++) {
		transfer = &thunk->memories[i];
#if FW_DEBUG
		__builtin_prefetch(&thunk->memories[i + 3].virtualBase);
		if (transfer->virtualBase)
			ksceKernelAssert(transfer->size != 0,
				SCE_DBGINFO_FUNC(236, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
		else
			ksceKernelAssert(transfer->size == 0 && transfer->flags == 1,
				SCE_DBGINFO_FUNC(238, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
		ksceKernelAssert(transfer->physicalBase != 0,
			SCE_DBGINFO_FUNC(240, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
		ksceKernelAssert((transfer->flags & 0x30) != 0x30,
			SCE_DBGINFO_FUNC(242, "sceCodecEngineDsp0ConvertPhysicalToVirtual"), __builtin_return_address(0));
#endif
		copyRequired = transfer->communicationMemory &&
			!(transfer->flags & 0x10) && transfer->size != 0;
		transfer->copyRequired = copyRequired;
	}
	return 0;
}

extern int lockVeneziaCommunicationMemory(void);
extern int unlockVeneziaCommunicationMemory(void);
extern void *_sceVeneziaAllocCommunicationMemory(SceSize size);
extern int freeVeneziaCommunicationMemory(void *memory);
extern int signalVeneziaCommunicationMemory(unsigned int channel);

int sceCodecEngineDsp0AllocCommunicationMemories(VeneziaRpcThunk *thunk)
{
	unsigned int count, i;
	VeneziaRpcMemoryTransfer *transfer;
	void *memory;

#if FW_DEBUG
	ksceKernelAssert(thunk != NULL,
		SCE_DBGINFO_FUNC(278, "sceCodecEngineDsp0AllocCommunicationMemories"), __builtin_return_address(0));
#endif
	count = thunk->allocationCount;
	if (!count)
		return 0;
	for (i = 0; i != count; i++) {
		if (thunk->memories[i].communicationMemory)
			break;
	}
	if (i == count)
		return 0;
	lockVeneziaCommunicationMemory();
	for (i = 0; i != count; i++) {
		transfer = &thunk->memories[i];
		__builtin_prefetch(&thunk->memories[i + 2].communicationMemory);
		if (transfer->communicationMemory) {
			memory = _sceVeneziaAllocCommunicationMemory(transfer->allocationSize);
			transfer->physicalBase = veneziaVirtualToPhysical((uintptr_t)memory);
		}
#if FW_DEBUG
		ksceKernelAssert(transfer->physicalBase != 0,
			SCE_DBGINFO_FUNC(295, "sceCodecEngineDsp0AllocCommunicationMemories"), __builtin_return_address(0));
#endif
	}
	unlockVeneziaCommunicationMemory();
	return 0;
}

int sceCodecEngineDsp0FreeCommunicationMemories(VeneziaRpcThunk *thunk)
{
	unsigned int count, i, freed = 0;
	VeneziaRpcMemoryTransfer *transfer;
	void *memory;

#if FW_DEBUG
	ksceKernelAssert(thunk != NULL,
		SCE_DBGINFO_FUNC(310, "sceCodecEngineDsp0FreeCommunicationMemories"), __builtin_return_address(0));
#endif
	count = thunk->allocationCount;
	if (!count)
		return 0;
	for (i = 0; i != count; i++) {
		transfer = &thunk->memories[i];
		__builtin_prefetch(&thunk->memories[i + 2].communicationMemory);
		if (transfer->communicationMemory) {
			memory = (void *)(uintptr_t)veneziaPhysicalToVirtual(transfer->physicalBase);
			freed++;
			freeVeneziaCommunicationMemory(memory);
		}
	}
	if (!freed)
		return 0;
	signalVeneziaCommunicationMemory(thunk->channel);
	return 0;
}

/* Venezia DSP DMA copy operations. */
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
