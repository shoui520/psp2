/* Sony venezia/vnzmem.c: codec memory address translation. */
#undef __FILE_NAME__
#define __FILE_NAME__ "venezia/vnzmem.c"
#include <sce/debug.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/cpu.h>
#include "venezia_internal.h"

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
