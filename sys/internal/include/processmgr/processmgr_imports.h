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
#include <psp2kern/io/fcntl.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <sysmem/utils.h>
#include <excpmgr/excpmgr.h>

/* SceSblACMgrForKernel */
int SceSblACMgrForKernel_sceSblACMgrIsGameProgram(SceUID pid);
int SceSblACMgrForKernel_49509A83(SceUID pid);
int SceSblACMgrForKernel_BBA13D9C(void);	/* vitasdk: ksceSblACMgrIsDevelopmentMode2 */

/* SceModulemgrForKernel / SceModulemgrForDriver */
int ksceKernelGetLibraryDBFlags(SceUID pid, SceUInt32 *flags);
SceUID ksceKernelGetModuleIdByPid(SceUID pid);
void *ksceKernelGetModuleEntryPoint(SceUID modid);
int ksceKernelStartPreloadingModules(SceUID pid);
SceUID ksceKernelLoadProcessImage(SceUID pid, const char *path, int flags, void *authInfo,
				  void *imageInfo, const void *shims);
int ksceKernelLoadPreloadingModules(SceUID pid, void *param, SceUInt32 flags);
int ksceKernelUnloadProcessModules(SceUID pid);

/* SceThreadmgrForKernel */
int sceKernelThreadMgrProcessDeleteBeforePUIDClose(void *process);
int SceThreadmgrForKernel_59D1D5B0(SceClass *processClass);
int sceKernelThreadMgrProcessDeleteAfterPUIDClose(void *process);
int SceThreadmgrForKernel_7425185E(void *process);
int SceThreadmgrForKernel_89C17825(void *process);
int sceKernelThreadMgrProcessCreate(void *process);
int sceKernelGetDefaultCpuAffinityMask(void *budget);

#endif
