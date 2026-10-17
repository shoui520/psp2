/*
 * Imports of SceProcessmgr (os0:kd/processmgr.skprx) that vitasdk and the
 * shared sce/ headers do not declare, or declare with a conflicting shape.
 * Names in comments come from the 4.00 debug build's messages.
 */

#ifndef PROCESSMGR_IMPORTS_H
#define PROCESSMGR_IMPORTS_H

#include <psp2kern/types.h>
#include <psp2kern/kernel/sysmem/uid_class.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/cpu/atomic.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/suspend.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>

/* SceIofilemgrForDriver */
SceUID ksceIoOpen(const char *path, int flags, int mode);
int ksceIoClose(SceUID fd);

/* SceCpuForDriver / SceCpuForKernel */
int ksceKernelCpuSuspendIntr(void);
void ksceKernelCpuResumeIntr(int intr);
int ksceKernelSpinlockLowLockCpuSuspendIntr(SceKernelSpinlock *lock);
void ksceKernelSpinlockLowUnlockCpuResumeIntr(SceKernelSpinlock *lock, int intr);
void ksceKernelSpinlockLowLock(SceKernelSpinlock *lock);
void ksceKernelSpinlockLowUnlock(SceKernelSpinlock *lock);

/* SceSblACMgrForKernel */
int ksceSblACMgrIsGameProgram2(SceUID pid);
int SceSblACMgrForKernel_49509A83(SceUID pid);
int ksceSblACMgrIsDevelopmentMode2(void);

/* SceExcpmgrForKernel */
void SceExcpmgrForKernel_293DFA04(void);
int SceExcpmgrForKernel_B615A7DA(void);			/* in an exception context */
void SceExcpmgrForKernel_E7487AFD(void *phwp);

/* SceIntrmgrForKernel / SceIntrmgrForDriver */
void SceIntrmgrForKernel_7E3D4560(void *phwp);
void SceIntrmgrForKernel_ABEB5A69(void *phbp);
int ksceKernelIsIntrAllowedInCurrentContext(void);
int ksceKernelTriggerSGI(int intrCode, SceUInt32 targetListFilter, SceUInt32 cpuTargetList);
int ksceKernelRegisterIntrHandler(int intrCode, const char *name, int type,
				  int (*handler)(int code, void *arg), void *arg,
				  int priority, int targetCpu, void *opt);

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

/* SceThreadmgrForDriver */
int ksceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
int ksceKernelGetThreadInfo(SceUID thid, void *info);
int SceThreadmgrForDriver_33E85E9E(void *info);
int ksceKernelWaitThreadEnd(SceUID thid, int *stat, SceUInt32 *timeout);
SceUID ksceKernelGetThreadId(void);
int ksceKernelChangeThreadPriority(SceUID thid, int priority);
int ksceKernelDeleteThread(SceUID thid);
SceUID ksceKernelCreateThread(const char *name, int (*entry)(SceSize args, void *argp),
			      int priority, SceSize stackSize, SceUInt32 attr, int cpuAffinity,
			      void *opt);
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, void *entry, int priority,
				      SceSize stackSize, SceUInt32 attr, int cpuAffinity,
				      void *opt);		/* sceKernelCreateRemoteThread */

/* SceSysmemForKernel / SceSysmemForDriver */
int ksceKernelFreeMemBlock(SceUID uid);
int ksceGUIDClose(SceUID guid);
int ksceGUIDGetObject(SceUID guid, void *object);
int ksceGUIDReleaseObject(SceUID guid);
int ksceKernelFreeHeapMemory(SceUID heap, void *ptr);
SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
int ksceGUIDSetName(SceUID guid, const char *name);
int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize len);
int ksceKernelCopyToUser(void *dst, const void *src, SceSize len);
void *ksceKernelAllocHeapMemory(SceUID heap, SceSize size);
int ksceKernelStrncpyToUser(char *dst, const char *src, SceSize len);
int kscePUIDClose(SceUID pid, SceUID puid);
int sceKernelShrinkHeap(SceUID heap);
void *ksceKernelAllocHeapMemoryWithOpt(SceUID heap, SceSize size, void *opt);
int ksceKernelCopyFromUser(void *dst, const void *src, SceSize len);
SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid);
int sceKernelVerifyHeap(SceUID heap, int flags);
int ksceKernelDeleteHeap(SceUID heap);
const char *ksceGUIDName(SceUID guid);
int ksceKernelIsEqualAccessibleRangeProcBySW(SceUID pid, const void *mode, const void *addr,
					      SceSize size);
int SceSysmemForKernel_0164D817(void);
int sceKernelObjectHeapSetResourceLimitAll(void *objectHeap, int count, const void *limits);
int SceSysmemForKernel_0B1B4ACA(void);
int SceSysmemForKernel_133024A5(void *entryHeapObject);		/* sceUIDEntryHeapCloseAllPUID */
int SceSysmemForKernel_2F526AB8(void *objectHeap);		/* sceKernelDeleteObjectHeap */
int ksceKernelAddressSpaceFreeAllMemBlock(SceUID guid);
int sceKernelGrowPhyMemPartByPbase(void *partition, SceUIntPtr base, SceSize size);
int SceSysmemForKernel_3753C584(void *partition);
int sceKernelDeletePhyMemPart(void *partition);
int ksceKernelFree(void *ptr);
int ksceKernelDeleteAddressSpace(SceUID guid);
int SceSysmemForKernel_4E923101(void *partition);
int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				SceSize *count);
int SceSysmemForKernel_5B434019(void *partition, void *info);
int CreatePhyMemPartByPbase(const char *name, SceUInt32 type, SceUIntPtr base,
			  SceSize size, void **partition);
int SceSysmemForKernel_5EDDEEB5(SceUID budgetId, SceClass *cls, const char *name, void *budget,
				SceSize size);
int SceSysmemForKernel_62800A6B(void);
int SceSysmemForKernel_660DC18A(void *objectHeap, SceUID pid, SceUID heap);	/* sceKernelObjectHeapStart */
int SceSysmemForKernel_6D563733(void *info);
int sceKernelGetPhyMemPartInfoForDebugger(void *partition, void *info);
void *SceSysmemForKernel_791D6205(void);				/* the shared object heap */
void *ksceKernelAlloc(SceSize size);
SceUID ksceKernelCreateAddressSpace(SceUID pid, const char *name, SceUInt32 type, void *opt);
int sceKernelResetPhyMemPart(void *partition);
int sceKernelCreateObjectHeap(SceUID pid, int kind, void *opt, void *objectHeap);
int SceSysmemForKernel_A2DFA383(const char *name, SceUInt32 type, SceSize size, void *partition);
int sceKernelGetPhyMemPartInfoCore(void *partition, void *info);
int SceSysmemForKernel_BF275C5A(SceUID guid, SceClass *cls, int unk, void *object);	/* sceGUIDSet */
int SceSysmemForKernel_CAB15F74(SceUID pid);
int SceSysmemForKernel_E8CC2166(SceUID pid, int level);
int SceSysmemForKernel_FA1A73DD(SceUID addressSpace, void *info);

/* SceSysrootForDriver / SceSysrootForKernel */
int SceSysrootForDriver_1D61C837(SceUID pid, SceUID spawnPid, SceUInt64 time);
void sceKernelApiDeclareTypeUInt64(void);
int SceSysrootForDriver_3F76D531(SceUID pid);
int SceSysrootForDriver_4D98B15B(void);
int SceSysrootForDriver_51F9C118(SceUID pid, SceUID spawnPid, SceUID prevPid,
				 SceUInt64 time);
int SceSysrootForDriver_571E5B79(SceUID parentPid, SceUID pid, int unk, void *process,
				 SceUInt64 time);
int SceSysrootForDriver_582616EC(SceUID pid, SceUID spawnPid, SceUID prevPid,
				 SceUInt64 time);
int SceSysrootForDriver_5C86E49B(SceUID pid, SceUInt32 status, SceUInt64 time);
int SceSysrootForDriver_6050A467(SceUID pid);
int SceSysrootForDriver_6D44F190(SceUID pid, SceUInt64 time);
int sceKernelApiDeclareTypeUInt32(void);
int SceSysrootForDriver_73522F65(SceUID pid, void *info);
int SceSysrootForDriver_77CF3FBD(SceUID pid, SceUID spawnPid, SceUInt64 time);
int SceSysrootForDriver_8498E23F(SceUID pid, int exitStatus, SceUInt64 time);
int ksceKernelInvokeInitCallback(int kind);
int SceSysrootForDriver_9D058A32(SceUID pid, int exitStatus, SceUInt64 time);
int SceSysrootForDriver_B501E861(SceUID pid, SceUInt64 time);
int SceSysrootForDriver_F2BC9E1D(SceUID pid);
int SceSysrootForDriver_F4340469(SceUID parentPid, SceUID pid, SceUInt64 time);
int sceKernelSysrootPrintBacktrace(SceUID pid, void *buf, SceSize len, int unk);
int SceSysrootForKernel_733C243E(const void *ops);		/* processmgr ops for sysroot */
int sceKernelSysrootSetModulePrivate(int kind, void *value);
int sceKernelSysrootSetStatus(SceUInt32 flags);
int sceKernelSysrootBacktrace(SceUID pid, void *buf, SceSize len, void *out, int unk);
int SceSysrootForKernel_D29BCA77(const void *ops);		/* processmgr ops for sysroot */

/* SceDebugForKernel (beyond sysmem/debug.h) */
int k_sceEventLogPut(SceUInt32 id, SceUInt32 kind, SceUInt32 unk, const void *data,
		     SceSize size);

/* SceKernelUtilsForDriver */
int SceKernelUtilsForDriver_335AF34D(void *random);
void ksceMt19937GlobalUninit(void *buf, SceSize len);	/* fills buf with random bytes */
int SceKernelUtilsForDriver_B55C69B7(void *random);

#endif
