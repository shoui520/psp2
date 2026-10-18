/*
 * Imports of SceKernelModulemgr that vitasdk and the shared sce/ headers do
 * not declare (or declare with a conflicting shape). Kept here (one file, named after the module)
 * so the prototypes are shared with the test harness.
 */

#ifndef MODULEMGR_IMPORTS_H
#define MODULEMGR_IMPORTS_H

#include <psp2kern/types.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/dipsw.h>
#include <sysmem/utils.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <processmgr/processmgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <acmgr/acmgr.h>
#include <intrmgr/intrmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <psp2kern/io/fcntl.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/procevent.h>

/* SceIofilemgrForDriver */

/* SceSysmemForKernel / SceSysmemForDriver */

/* SceCpuForDriver / SceCpuForKernel */

/* SceThreadmgrForDriver / SceThreadmgrForKernel */

/* SceSblACMgrForDriver / SceSblAuthMgrForKernel / SceQafMgrForDriver */
int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, void *type);
int ksceSblAuthMgrCompareSwVersion(SceUInt32 version);
int ksceSblAuthMgrOpen(int *ctx);
int ksceSblAuthMgrClose(int ctx);
int ksceSblAuthMgrAuthHeader(int ctx, const void *header, SceSize size, void *info);
int ksceSblAuthMgrSetupAuthSegment(int ctx, int index);
int ksceSblAuthMgrLoadBlock(int ctx, void *buf, SceSize size);

/* SceSysrootForKernel / SceSysrootForDriver */
/* the NID db names these SceSysrootForKernel imports oddly */
#define SceSysrootForDriver_89D19090		SceSysrootForKernel_89D19090
#define SceSysrootForDriver_256B2394 \
/* the instruction a software breakpoint replaced: 9CFF80F9 reads it (0: there is one), 38751FB7 replaces it */

/* SceProcEventForDriver */

/* SceProcessmgrForKernel / SceProcessmgrForDriver */
void sceKernelFreeRemoteToolHeap(SceUID pid, void *ptr);
void sceKernelFreeRemoteProcessHeap(SceUID pid, void *ptr);
int SceProcessmgrForKernel_8B4086AF(SceUID pid);	/* reserve a module slot */

/* SceDebugForKernel / SceExcpmgrForKernel / SceIntrmgr */
void *SceExcpmgrForKernel_5420ED8F(int cpu);		/* top of the exception stack of a CPU */
int SceExcpmgrForKernel_B615A7DA(void);			/* exception nesting level */

int SceIntrmgrForDriver_C568F145(int code);
int SceIntrmgrForKernel_71C5F42F(void *handler);

/* the stack protector */
extern SceUInt32 __stack_chk_guard;

#endif
