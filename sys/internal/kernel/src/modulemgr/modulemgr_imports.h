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
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/utils.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>

/* SceIofilemgrForDriver */
SceUID ksceIoOpen(const char *path, int flags, int mode);
SceUID ksceIoOpenForPid(SceUID pid, const char *path, int flags, int mode);
int ksceIoClose(SceUID fd);
int ksceIoRead(SceUID fd, void *buf, SceSize size);
SceOff ksceIoLseek(SceUID fd, SceOff offset, int whence);
int ksceIoGetMediaType(SceUID pid, const char *path, int unk, void *type);

/* SceSysmemForKernel / SceSysmemForDriver */
int ksceKernelFreeMemBlock(SceUID uid);
int ksceKernelMapMemBlock(SceUID uid);
int ksceKernelRemapMemBlock(SceUID uid, SceUInt32 type);
void *ksceKernelAllocHeapMemoryWithOpt(SceUID heap, SceSize size, void *opt);
int ksceKernelFreeHeapMemory(SceUID heap, void *ptr);
void *ksceKernelAlloc(SceSize size);
int ksceKernelFree(void *ptr);
int ksceGUIDReleaseObject(SceUID guid);
int ksceGUIDClose(SceUID guid);
int ksceGUIDSetName(SceUID guid, const char *name);
const char *ksceGUIDName(SceUID guid);
SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid);
int kscePUIDClose(SceUID pid, SceUID puid);
int ksceKernelCopyFromUser(void *dst, const void *src, SceSize len);
int ksceKernelCopyFromUserProc(SceUID pid, void *dst, const void *src, SceSize len);
int ksceKernelCopyToUser(void *dst, const void *src, SceSize len);
int ksceKernelCopyToUserDomain(void *dst, const void *src, SceSize len);
int ksceKernelCopyToUserTextDomain(void *dst, const void *src, SceSize len);
int ksceKernelStrncpyFromUserProc(SceUID pid, char *dst, const char *src, SceSize len);
SceSize ksceKernelStrnlenFromUser(const char *s, SceSize len);
int ksceKernelUserUnmap(SceUID uid);
int SceSysmemForKernel_FCB5745A(void *addr, SceUInt32 time);			/* name unknown */
void ksceMt19937GlobalUninit(void *buf, SceSize len);	/* fills buf with random bytes */
SceUInt32 sceKernelAllocSystemCallTable(SceUInt32 count, void **table);	/* name per NID db */

/* SceCpuForDriver / SceCpuForKernel */
void ksceKernelDcacheCleanRange(void *start, SceSize size);
void ksceKernelDcacheCleanRangeForL1WBWA(void *start, SceSize size);
void ksceKernelL1IcacheInvalidateEntireAllCore(void);
int ksceKernelSpinlockLowLock(SceKernelSpinlock *lock);
int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
int ksceKernelSpinlockLowUnlock(SceKernelSpinlock *lock);
int ksceKernelSpinlockLowLockCpuSuspendIntr(SceKernelSpinlock *lock);
int ksceKernelSpinlockLowUnlockCpuResumeIntr(SceKernelSpinlock *lock, int intr);
int ksceKernelCpuSuspendIntr(void);
int ksceKernelCpuResumeIntr(int intr);
int ksceKernelCpuId(void);
int ksceKernelMMUVAtoPA(void *va, SceUIntPtr *pa);

/* SceThreadmgrForDriver / SceThreadmgrForKernel */
SceUID ksceKernelCreateThread(const char *name, void *entry, int prio, SceSize stack,
			      SceUInt32 attr, int cpu, void *opt);
int ksceKernelStartThread(SceUID thid, SceSize args, void *argp);
int ksceKernelDeleteThread(SceUID thid);
int ksceKernelWaitThreadEnd(SceUID thid, int *stat, SceUInt32 *timeout);
SceUID ksceKernelGetThreadId(void);
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, void *entry, int prio,
				      SceSize stack, SceUInt32 attr, int cpu, void *opt);

/* SceSblACMgrForDriver / SceSblAuthMgrForKernel / SceQafMgrForDriver */
int ksceSblACMgrIsGameProgram2(SceUID pid);
int ksceSblACMgrIsNonGameProgram2(SceUID pid);
int ksceSblACMgrIsDevelopmentMode2(void);
int ksceSblACMgrGetMediaType2(const char *path, void *type);
int ksceSblAuthMgrCompareSwVersion(SceUInt32 version);
int ksceSblAuthMgrOpen(int *ctx);
int ksceSblAuthMgrClose(int ctx);
int ksceSblAuthMgrAuthHeader(int ctx, const void *header, SceSize size, void *info);
int ksceSblAuthMgrSetupAuthSegment(int ctx, int index);
int ksceSblAuthMgrLoadBlock(int ctx, void *buf, SceSize size);

/* SceSysrootForKernel / SceSysrootForDriver */
int ksceKernelSysrootGetProcessTitleId(SceUID pid, char *titleid, SceSize len);
SceUInt32 ksceKernelSysrootGetSystemSwVersion(void);
int sceKernelApiDeclareTypeUInt32(void);		/* name per NID db */
/* the NID db names these SceSysrootForKernel imports oddly */
#define ksceSysrootIsExternalBootMode		SceSysrootForKernel_ksceSysrootIsExternalBootMode
#define ksceKernelSysrootDbgpSuspendProcessAndWaitResume \
	SceSysrootForKernel_ksceKernelSysrootDbgpSuspendProcessAndWaitResume
int ksceSysrootIsManufacturingMode(void);
int ksceSysrootIsExternalBootMode(void);
int ksceKernelSysrootDbgpSuspendProcessAndWaitResume(SceUID pid, SceUID modid, int event);
/* the instruction a software breakpoint replaced: 9CFF80F9 reads it (0: there is one), 38751FB7 replaces it */
int SceSysrootForKernel_9CFF80F9(SceUID pid, SceUInt32 addr, void *orig);
int SceSysrootForKernel_B171CC2D(const void *callbacks);

/* SceProcEventForDriver */
SceUID ksceKernelRegisterProcEventHandler(const char *name, const void *handler, int a2);

/* SceProcessmgrForKernel / SceProcessmgrForDriver */
void *sceKernelAllocRemoteToolHeap(SceUID pid, SceSize size);
void sceKernelFreeRemoteToolHeap(SceUID pid, void *ptr);
void *sceKernelAllocRemoteProcessHeap(SceUID pid, SceSize size, void *opt);
void sceKernelFreeRemoteProcessHeap(SceUID pid, void *ptr);
int ksceKernelGetProcessSelfAuthInfo(SceUID pid, void *info);
void *ksceKernelGetProcessModuleInfo(SceUID pid);
int sceKernelSetProcessLibdb(SceUID pid, void *libdb);
void *ksceKernelGetUIDProcessClass(void);
int sceKernelGetProcessType(SceUID pid);
int SceProcessmgrForKernel_8B4086AF(SceUID pid);	/* reserve a module slot */
int SceProcessmgrForKernel_BC110987(SceUID pid, int a1);
int SceProcessmgrForKernel_F160B4D8(SceUID pid, int isLibrary);	/* release the slot */
int SceProcessmgrForDriver_2CEB1C7A(SceUID pid, void *buf, SceSize len);	/* random bytes */
int SceProcessmgrForDriver_D141C076(SceUID pid, SceUInt32 *sdkVersion);

/* SceDebugForKernel / SceExcpmgrForKernel / SceIntrmgr */
int SceDebugForKernel_38F1A352(void *func);
int SceDebugForKernel_91E7D24F(void *func);
void *SceExcpmgrForKernel_5420ED8F(int cpu);		/* top of the exception stack of a CPU */
int SceExcpmgrForKernel_B615A7DA(void);			/* exception nesting level */

int ksceKernelIsIntrAllowedInCurrentContext(void);
int SceIntrmgrForDriver_C568F145(int code);
int SceIntrmgrForKernel_71C5F42F(void *handler);

/* the stack protector */
extern SceUInt32 __stack_chk_guard;

#endif
