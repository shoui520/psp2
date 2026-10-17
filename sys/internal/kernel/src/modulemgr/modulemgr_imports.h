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

/* SceSysclibForDriver */
void *memset(void *dst, int c, SceSize len);
void *memcpy(void *dst, const void *src, SceSize len);
void *memchr(const void *s, int c, SceSize len);
int strncmp(const char *a, const char *b, SceSize len);
char *strncpy(char *dst, const char *src, SceSize len);
SceSize strnlen(const char *s, SceSize len);
int snprintf(char *dst, SceSize len, const char *fmt, ...);

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

/* SceZlibForDriver / SceDeflateForDriver */
int ksceDeflateDecompressPartial(void *dst, SceSize dstSize, const void *src,
				 const void **next, void *param);
int ksceZlibGetInfo(const void *src, SceUInt8 *cmf, SceUInt8 *flg, SceUInt32 *dictid,
		    const void **data);

/* SceThreadmgrForDriver / SceThreadmgrForKernel */
SceUID ksceKernelCreateThread(const char *name, void *entry, int prio, SceSize stack,
			      SceUInt32 attr, int cpu, void *opt);
int ksceKernelStartThread(SceUID thid, SceSize args, void *argp);
int ksceKernelDeleteThread(SceUID thid);
int ksceKernelWaitThreadEnd(SceUID thid, int *stat, SceUInt32 *timeout);
SceUID ksceKernelGetThreadId(void);
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, void *entry, int prio,
				      SceSize stack, SceUInt32 attr, int cpu, void *opt);

/* SceKernelSuspendForDriver */
int SceKernelSuspendForDriver_4DF40893(int a0);	/* lock power state */
int SceKernelSuspendForDriver_2BB92967(int a0);	/* unlock power state */

/* SceSblACMgrForDriver / SceSblAuthMgrForKernel / SceQafMgrForDriver */
int ksceSblACMgrIsGameProgram2(SceUID pid);
int ksceSblACMgrIsNonGameProgram2(SceUID pid);
int ksceSblACMgrIsDevelopmentMode2(void);
int ksceSblACMgrGetMediaType2(const char *path, void *type);
int ksceSblQafMgrIsAllowHost0Access(void);
int SceQafMgrForDriver_382C71E8(void);
int SceQafMgrForDriver_B9770A13(void);
int ksceSblAuthMgrCompareSwVersion(SceUInt32 version);
int ksceSblAuthMgrOpen(int *ctx);
int ksceSblAuthMgrClose(int ctx);
int ksceSblAuthMgrAuthHeader(int ctx, const void *header, SceSize size, void *info);
int ksceSblAuthMgrSetupAuthSegment(int ctx, int index);
int ksceSblAuthMgrLoadBlock(int ctx, void *buf, SceSize size);

/* SceSysrootForKernel / SceSysrootForDriver */
void *ksceKernelSysrootGetModulePrivate(int index);
int ksceKernelSysrootGetStatus(void);
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
int SceSysrootForKernel_73522F65(SceUID pid, SceUInt32 *flags);	/* debug flags; bit 0: debugger */
int SceSysrootForKernel_6050A467(SceUID pid);			/* the debugger attached */
int SceSysrootForKernel_CA497324(SceUID pid, SceUID modid, SceUInt64 time);	/* module loaded */
int SceSysrootForKernel_C81B7E2B(SceUID pid, SceUID modid, SceUInt64 time);	/* module loaded, debugger */
int SceSysrootForKernel_20D2A0DF(SceUID pid, SceUID modid, SceUInt64 time);	/* module unloaded */
int SceSysrootForKernel_AA8D34EE(SceUID pid, SceUID modid, SceUInt64 time);	/* module unloaded */
/* the instruction a software breakpoint replaced: 9CFF80F9 reads it (0: there is one), 38751FB7 replaces it */
int SceSysrootForKernel_9CFF80F9(SceUID pid, SceUInt32 addr, void *orig);
int SceSysrootForKernel_38751FB7(SceUID pid, SceUInt32 addr, SceUInt32 insn);
int SceSysrootForKernel_B171CC2D(const void *callbacks);
int SceSysrootForKernel_C67A1FA9(SceUID pid, SceUID modid, int percent, const char *path);
int SceSysrootForDriver_1B3FA457(SceUID pid);		/* end of an entry call */
int SceSysrootForDriver_9AF075D4(SceUID pid);		/* start of an entry call */
int SceSysrootForDriver_CAE820B2(SceUID pid, SceUInt32 *sdkVersion);

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

/* SceDipswForDriver */
int ksceKernelCheckDipsw(SceUInt32 bit);

/* the stack protector */
extern SceUInt32 __stack_chk_guard;
void __stack_chk_fail(void);

#endif
