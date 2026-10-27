/*
 * SceSysmem: name copies and the kernel heap's locked entry points. Sony's
 * file name is unknown; it is a file of its own, as SysmemModuleStart calls
 * SysmemHeapModuleStart rather than inlining it.
 */

#include "sysmem_internal.h"
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include "memblock_obj_module.h"

/* name unknown: a 0x20-byte copy of a name, from the sysroot heap while it boots */
char *SysmemAllocName(const char *name)
{
	char *copy;

	if (sceKernelSysrootGetStatus() < 3)
		copy = sceKernelSysrootAlloc(0x20);
	else
		copy = SceSysmemForKernel_85571907(0x20);
	strncpy(copy, name, 0x20);
	return copy;
}

/* name unknown: such a copy given back (the sysroot heap's are kept) */
void SysmemFreeName(char *name)
{
	if (sceKernelSysrootGetStatus() > 2)
		SceSysmemForKernel_4233C16D(name);
}

/* name unknown: an allocation from a heap, under its lock */
void *HeapAllocLocked(void *heap, SceSize size)
{
	int intr = HeapLock(heap);
	void *ptr = HeapAllocPrologue(heap, size, 0);

	HeapUnlock(heap, intr);
	return ptr;
}

/* name unknown: the same for a free */
int HeapFreeLocked(void *heap, void *ptr)
{
	int intr = HeapLock(heap);
	int ret = HeapFree(heap, ptr);

	HeapUnlock(heap, intr);
	return ret;
}

/* name unknown */
void *UIDManagerGetKernelHeap(void)
{
	return g_uidManager->heapDefault;
}

#if !SYSMEM_NSKBL
/* name unknown */
int SysmemHeapModuleStart(const UIDManager *manager)
{
	HeapModuleStart(manager);
	return 0;
}
#endif

#if SYSMEM_NSKBL
static inline __attribute__((always_inline))
void SysmemStartCore0(SceKernelBootParam *pBoot)
{
	UIDManager *manager;
	SceUInt32 i;

	g_uidManager = sceKernelSysrootAlloc(sizeof(UIDManager));
	sceKernelSysrootSetModulePrivate(3, g_uidManager);
	manager = g_uidManager;
	manager->numMemory = pBoot->numMemory;
	for (i = 0; i < pBoot->numMemory; i++) {
		manager->memory[i].pbase = pBoot->memory[i].pbase;
		manager->memory[i].psize = pBoot->memory[i].psize;
	}
	manager->phyPageReset = NULL;
	manager->classes = NULL;
	manager->uidClass = NULL;
	manager->dlinkClass = NULL;
	manager->uidHeapClass = NULL;
	manager->fixedHeapClass = NULL;
	manager->fixedHeap16B = NULL;
	manager->fixedHeap32B = NULL;
	manager->fixedHeap48B = NULL;
	manager->fixedHeap64B = NULL;
	manager->fixedHeap128B = NULL;
	manager->fixedHeap256B = NULL;
	manager->fixedHeap512B = NULL;
	manager->fixedHeapL2Object = NULL;
	manager->entryHeapClass = NULL;
	manager->registerHeap = NULL;
	manager->guidEntryHeap = NULL;
	manager->memBlockClass = NULL;
	manager->partitionClass = NULL;
	manager->kernelPartition = NULL;
	manager->heapClass = NULL;
	manager->heapDefault = NULL;
	manager->addressSpaceGlobals = NULL;
	manager->kernelObjectHeap = NULL;
}

/*
 * name unknown: the boot loader's sysmem, brought up from the boot param in
 * the order the module's own start does it
 */
int SysmemStart(SceKernelBootParam *pBoot)
{
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0)
		SysmemStartCore0(pBoot);
	sceKernelSysrootCorelockUnlock();
	L1PageTableStartFirst();
	PhyMemLowStart(g_uidManager);
	PhyMemStart(pBoot, g_uidManager);
	UIDClassStart(g_uidManager);
	VirPageDestroy();
	UIDDLinkStart(g_uidManager);
	UIDHeapClassInitManager(g_uidManager);
	FixedHeapStart(pBoot, g_uidManager);
	ObjectHeapStart(g_uidManager);
	UIDStart(pBoot, g_uidManager);
	L1PageTableStart(pBoot, g_uidManager);
	L2PageTableStart(pBoot, g_uidManager);
	VirPageStart();
	MemBlockObjStart(g_uidManager);
	BlockStart();
	PartitionStart(pBoot, g_uidManager);
	SimpleMemBlockStartPhase2();
	PartitionStartPhase2();
	NameHeapStart(g_uidManager);
	UIDStartPhase2();
	sceKernelSysrootCorelockLock(0);
	sceKernelCpuId();
	sceKernelSysrootCorelockUnlock();
	FixedHeapStartPhase2();
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		ObjectHeapSetKernelSizeClass(16, g_uidManager->fixedHeap16B, -1);
		ObjectHeapSetKernelSizeClass(128, g_uidManager->fixedHeap128B, -1);
		ObjectHeapSetKernelSizeClass(256, g_uidManager->fixedHeap256B, -1);
		ObjectHeapSetKernelSizeClass(512, g_uidManager->fixedHeap512B, -1);
	}
	sceKernelSysrootCorelockUnlock();
	HeaplibStart(pBoot, g_uidManager);
	AddressSpaceStart(pBoot, g_uidManager);
	HeaplibStartPhase2(pBoot, g_uidManager);
	return 0;
}
#endif
