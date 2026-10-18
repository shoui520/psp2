/*
 * Imports of SceProcessmgr (os0:kd/processmgr.skprx) that vitasdk and the
 * shared sce/ headers do not declare, or declare with a conflicting shape.
 * Names in comments come from the 4.00 debug build's messages.
 */

#ifndef PROCESSMGR_IMPORTS_H
#define PROCESSMGR_IMPORTS_H

#include <psp2kern/types.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>
#include <intrmgr/intrmgr.h>
#include <acmgr/acmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <sysmem/utils.h>
#include <excpmgr/excpmgr.h>
#include <modulemgr/modulemgr.h>

/* SceSblACMgrForKernel */

/* SceModulemgrForKernel / SceModulemgrForDriver */
SceUID sceKernelLoadProcessImage(SceUID pid, const char *path, int flags, void *authInfo,
				  void *imageInfo, const void *shims);

/* SceThreadmgrForKernel */
int sceKernelThreadMgrProcessDeleteBeforePUIDClose(void *process);
int SceThreadmgrForKernel_59D1D5B0(SceClass *processClass);
int sceKernelThreadMgrProcessDeleteAfterPUIDClose(void *process);
int SceThreadmgrForKernel_7425185E(void *process);
int SceThreadmgrForKernel_89C17825(void *process);
int sceKernelThreadMgrProcessCreate(void *process);
int sceKernelGetDefaultCpuAffinityMask(void *budget);

#endif
