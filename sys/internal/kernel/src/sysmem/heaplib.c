/*
 * SceSysmem: the heap library's start phases (Sony's heaplib.c; its records name
 * HeaplibStartPhase2). os0 keeps only the record; the boot loader starts the
 * kernel's heaps here, through dlmalloc/heap.c's HeaplibImplementation phases.
 */

#include <fw.h>
#include <sysmem/debug.h>

#if SYSMEM_NSKBL
#include <kernel_boot_loader/kernel_boot_param.h>
#include <sysmem/sysroot.h>
#include "sysmem_internal.h"

/* name unknown: SceUIDKernelHeapClass and SceKernelHeap, made by core 0 */
int HeaplibStart(struct SceKernelBootParam *param, UIDManager *manager)
{
	HeaplibImplementationStart();
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0)
		HeaplibImplementationStartCore0(manager);
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* the uncached heap and the tool's, made by core 0 */
int HeaplibStartPhase2(struct SceKernelBootParam *param, UIDManager *manager)
{
	int ret;

	sceKernelSysrootCorelockLock(0);
	ret = HeaplibImplementationStartPhase2();
	if (sceKernelCpuId() == 0) {
		ret = HeaplibImplementationStartPhase2Core0(manager);
		if (ret < 0)
			SCE_DBG_ASSERT_FAIL(124);
	}
	sceKernelSysrootCorelockUnlock();
	return ret;
}
#elif FW_DEBUG
SCE_DBGINFO_DEFINE(static, heaplibStartPhase2Record, 124, "HeaplibStartPhase2");
#endif
