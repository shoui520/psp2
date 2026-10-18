/*
 * SceKernelBusError: the bus error interrupt (0x20). On a bus error the kernel
 * log and the error record are handed to the secure world, which reboots
 * into the crash dump (os0:kd/crashdump.skprx) - unless the error is a
 * tolerated one. The error record lives in the sysroot shared memory at
 * +0x6000 (header, then records); SceBusErrorForKernel_EBB1F135 copies it out.
 */

#include <sysmem/cpu.h>
#include <modulemgr/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <buserror/buserror_types.h>
#include "buserror_smc.h"

#define BUSERROR_INTR		0x20
#define BUSERROR_SUSPEND_IDX	0x1c

int RegisterBusErrorHandler_ES3(void);

/* Secure monitor hardware entry points: nine instructions, original order. */
__asm__(
	".syntax unified\n"
	".arch_extension sec\n"
	".thumb\n"
	".global busErrorSmc10C\n"
	".type busErrorSmc10C, %function\n"
	".thumb_func\n"
	"busErrorSmc10C:\n"
	"movw ip, #0x10c\n"
	"smc #0\n"
	"bx lr\n"
	".size busErrorSmc10C, .-busErrorSmc10C\n"
	".global busErrorSmc10D\n"
	".type busErrorSmc10D, %function\n"
	".thumb_func\n"
	"busErrorSmc10D:\n"
	"movw ip, #0x10d\n"
	"smc #0\n"
	"bx lr\n"
	".size busErrorSmc10D, .-busErrorSmc10D\n"
	".global busErrorSmc122\n"
	".type busErrorSmc122, %function\n"
	".thumb_func\n"
	"busErrorSmc122:\n"
	"movw ip, #0x122\n"
	"smc #0\n"
	"bx lr\n"
	".size busErrorSmc122, .-busErrorSmc122\n"
);

/* name unknown: suspend handler */
static int busErrorSuspend(void *args)
{
	return 0;
}

/* the bus error record, up to size bytes */
SceSize SceBusErrorForKernel_EBB1F135(void *buf, SceSize size)
{
	BusErrorRecord *rec = &sysrootSharedMemory()->busError;
	SceSize n = rec->hdrSize + rec->size;

	n = n < size ? n : size;
	memcpy(buf, rec, n);
	return n;
}

/* name unknown: resume handler */
static int busErrorResume(void *args)
{
	if (sceKernelCpuId() == 0)
		sceKernelEnableIntr(BUSERROR_INTR);
	return 0;
}

SceUInt32 RegisterErrorHandler(SceSize args, void *argp)
{
	int ret = RegisterBusErrorHandler_ES3();

	if (ret < 0) {
		SCE_DBG_PRINTF(6, 91, "register default bus error handler failed. 0x%08x\n", ret);
		ret = SCE_KERNEL_START_NO_RESIDENT;
	} else {
		SceSysrootForKernel_82FC6405(SceBusErrorForKernel_EBB1F135);
		SceKernelSuspendForDriver_CE7A2207(BUSERROR_SUSPEND_IDX, busErrorSuspend, NULL);
		SceKernelSuspendForDriver_105C5752(BUSERROR_SUSPEND_IDX, busErrorResume, NULL);
		ret = SCE_KERNEL_START_SUCCESS;
	}
	return ret;
}
