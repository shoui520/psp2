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
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/procevent.h>
#include <excpmgr/excpmgr.h>
#include <authmgr/sblauthmgr.h>

/* SceSblACMgrForDriver / SceSblAuthMgrForKernel / SceQafMgrForDriver */
int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, void *type);

/* SceProcessmgrForKernel / SceProcessmgrForDriver */

/* SceIntrmgrForDriver / SceIntrmgrForKernel */
int SceIntrmgrForDriver_C568F145(int code);
int SceIntrmgrForKernel_71C5F42F(void *handler);

/* the stack protector */
extern SceUInt32 __stack_chk_guard;

#endif
