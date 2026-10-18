/* Sony venezia/vnzmem.c: RPC communication-memory allocator. */
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include "../venezia_internal.h"

VeneziaCommunicationMemory g_veneziaCommunicationMemory;

extern void *getRpcHeapBase(void);
extern SceSize getRpcHeapSize(void);
#if FW_VERSION >= 0x04000000
extern void _sceVeneziaSimpleHeapInit(void *base, SceSize size,
	VeneziaSimpleHeap *heap, SceSize workSize, SceSize blockSize);
#else
extern void _sceVeneziaSimpleHeapInit(void *base, SceSize size,
	VeneziaSimpleHeap *heap, SceSize workSize, SceSize blockSize);
#endif

/* name unknown: create the RPC communication-memory synchronization and heap. */
int initVeneziaRpcCommunicationMemory(void)
{
	void *base;
	SceSize size;
	int ret;

	g_veneziaCommunicationMemory.event = -1;
	ret = sceKernelInitializeFastMutex(&g_veneziaCommunicationMemory.mutex,
		"SceVeneziaRpcCommunicationMemory", 0, NULL);
	if (ret >= 0) {
		ret = sceKernelCreateEventFlag("SceVeneziaRpcCommunicationMemory",
			0, 0, NULL);
		if (ret >= 0) {
			g_veneziaCommunicationMemory.event = ret;
			base = getRpcHeapBase();
			size = getRpcHeapSize();
			_sceVeneziaSimpleHeapInit(base, size,
				&g_veneziaCommunicationMemory.heap,
				sizeof(VeneziaSimpleHeap), 1024);
		}
	}
	return ret;
}

/* name unknown: release communication-memory synchronization objects. */
int finalizeVeneziaRpcCommunicationMemory(void)
{
	if (g_veneziaCommunicationMemory.event >= 0) {
		sceKernelDeleteEventFlag(g_veneziaCommunicationMemory.event);
		g_veneziaCommunicationMemory.event = -1;
	}
	sceKernelFinalizeFastMutex(&g_veneziaCommunicationMemory.mutex);
	return 0;
}

/* name unknown: serialize access to communication allocations. */
int lockVeneziaCommunicationMemory(void)
{
	return lockFastMutex(&g_veneziaCommunicationMemory.mutex);
}

/* name unknown: release communication-allocation serialization. */
int unlockVeneziaCommunicationMemory(void)
{
	return sceKernelUnlockFastMutex(&g_veneziaCommunicationMemory.mutex);
}

/* name unknown: notify waiters that a channel freed communication memory. */
int signalVeneziaCommunicationMemory(unsigned int channel)
{
	return sceKernelSetEventFlag(g_veneziaCommunicationMemory.event, 1U << channel);
}

#if FW_DEBUG
extern void *_sceVeneziaSimpleHeapAllocMemory(VeneziaSimpleHeap *heap, SceSize size);
extern unsigned int getRpcFreeChannelCount(void);
extern unsigned int getRpcFreeDebugChannelCount(void);
#else
extern void *_sceVeneziaSimpleHeapAllocMemory(VeneziaSimpleHeap *heap, SceSize size);
#endif

void *_sceVeneziaAllocCommunicationMemory(SceSize size)
{
	SceSize alignedSize = (size + 255) & ~255U;
	void *memory;

#if FW_DEBUG
	SceDebugForDriver_35A35322(size != 0,
		SCE_DBGINFO_FUNC(147, "_sceVeneziaAllocCommunicationMemory"),
		__builtin_return_address(0));
#endif
	for (;;) {
		memory = _sceVeneziaSimpleHeapAllocMemory(
			&g_veneziaCommunicationMemory.heap, alignedSize);
		if (memory)
			break;
#if FW_DEBUG
		SceDebugForDriver_35A35322(getRpcFreeChannelCount() <= 7 ||
			getRpcFreeDebugChannelCount() == 0,
			SCE_DBGINFO_FUNC(163, "_sceVeneziaAllocCommunicationMemory"),
			__builtin_return_address(0));
#endif
		sceKernelWaitEventFlag(g_veneziaCommunicationMemory.event, ~0U,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	}
	return memory;
}

#if FW_DEBUG
extern int _sceVeneziaSimpleHeapFreeMemory(VeneziaSimpleHeap *heap, void *memory);
#else
extern int _sceVeneziaSimpleHeapFreeMemory(VeneziaSimpleHeap *heap, void *memory);
#endif

/* name unknown: return an allocation to the RPC communication heap. */
int freeVeneziaCommunicationMemory(void *memory)
{
	return _sceVeneziaSimpleHeapFreeMemory(&g_veneziaCommunicationMemory.heap, memory);
}
