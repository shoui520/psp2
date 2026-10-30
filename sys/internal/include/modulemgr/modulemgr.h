/*
 * SceKernelModulemgr (os0:kd/modulemgr.skprx): the module manager's exports, each declared once
 * with its definition as the prototype, and the records they fill.
 */

#pragma once

#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>
#include <modulemgr/module_library.h>
#include <modulemgr/modulemgr_control.h>
#include <modulemgr/modulemgr_patch.h>
#include <authmgr/sblauthmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProcessImageInfo ProcessImageInfo;	/* processmgr/process.h */

/* sceKernelGetSystemSwVersion's output */
typedef struct SceKernelSystemSwVersion {
	SceSize size;			/* 0x28 */
	char versionString[0x1c];	/* "%d.%02d" */
	SceUInt version;		/* SceSysrootForDriver_67AAB627 */
	SceUInt versionExtra;		/* 0 */
} SceKernelSystemSwVersion;

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelSystemSwVersion) == 0x28, "SceKernelSystemSwVersion size");
#endif

int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);

/* vitasdk: ksceKernelStopUnloadSharedModuleForPid */
int SceModulemgrForDriver_02D3D0C1(SceUID pid, SceUID modid, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelUnloadModuleOpt *opt,
				   int *status);
int sceKernelStopUnloadModule(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				   const SceKernelUnloadModuleOpt *opt, int *status);
int sceKernelStopModule(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				   const SceKernelStopModuleOpt *opt, int *status);
SceUID sceKernelLoadStartModule(const char *path, SceSize args, void *argp, SceUInt32 flags,
				      const SceKernelLoadModuleOpt *opt, int *status);
/* vitasdk: ksceKernelGetModuleInfoByAddr */
int SceModulemgrForDriver_1D9E0F7E(SceUInt32 addr, SceKernelModuleInfo *info);
/* vitasdk: ksceKernelStopUnloadModuleForPid */
int SceModulemgrForDriver_49A3EDC7(SceUID pid, SceUID modid, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelUnloadModuleOpt *opt,
				   int *status);
/* vitasdk: ksceKernelLoadStartModuleForPid */
SceUID SceModulemgrForDriver_9D953C22(SceUID pid, const char *path, SceSize args, void *argp,
				      SceUInt32 flags, const SceKernelLoadModuleOpt *opt,
				      int *status);
/* vitasdk: ksceKernelSearchModuleByName */
SceUID SceModulemgrForDriver_BBE1771C(const char *name);
/* vitasdk: ksceKernelLoadStartSharedModuleForPid */
SceUID SceModulemgrForDriver_E2ADEF8D(SceUID pid, const char *path, SceSize args, void *argp,
				      SceUInt32 flags, const SceKernelLoadModuleOpt *opt,
				      int *status);
/* vitasdk: ksceKernelGetModuleCBByAddr */
int SceModulemgrForKernel_1728612F(SceUID pid, SceUInt32 addr, SceModuleCB **out);
/* vitasdk: ksceKernelRegisterSyscall */
void SceModulemgrForKernel_2E4A10A0(SceUInt32 num, void *func);
int SceModulemgrForKernel_2F82EEBC(void);	/* vitasdk: ksceKernelModuleUnloadMySelf */
/* vitasdk: ksceKernelGetModuleFingerprint */
int SceModulemgrForKernel_337A3908(SceUID modid, SceUInt32 *fingerprint);
int sceKernelGetModuleList(SceUID pid, SceUInt32 flags, SceUInt32 type, SceUID *list,
			   SceSize *num);
/* vitasdk: ksceKernelGetModuleCB */
int SceModulemgrForKernel_37512E29(SceUID modid, SceModuleCB **cb);
int sceKernelStartRemoteModule(SceUID pid, SceUID modid, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelStartModuleOpt *opt,
				   int *status);	/* vitasdk: ksceKernelStartModuleForPid */
/* vitasdk: ksceKernelLoadModuleForPid */
SceUID SceModulemgrForKernel_4E85022D(SceUID pid, const char *path, SceUInt32 flags,
				      const SceKernelLoadModuleOpt *opt);
/* vitasdk: ksceKernelGetModuleInfoMinByAddr */
int SceModulemgrForKernel_5564A860(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint,
				   void **textBase, char *name);
int SceModulemgrForKernel_74A4E0EE(void (*callback)(SceModuleCB *cb, int status));
/* vitasdk: ksceKernelGetModulePath */
int SceModulemgrForKernel_79E761B5(SceUID modid, char *path, SceSize size);
/* vitasdk: ksceKernelGetModuleEntryPoint */
SceModuleEntry SceModulemgrForKernel_7B302C5D(SceUID modid);
int SceModulemgrForKernel_ACBC97C9(void (*callback)(SceModuleCB *cb, int status));
int sceKernelGetModuleInfo(SceUID pid, SceUID modid, SceKernelModuleInfo *info);
int sceKernelUnloadProcessModules(SceUID pid);
/* vitasdk: ksceKernelGetModuleIsSharedByAddr */
int SceModulemgrForKernel_F1DE6949(SceUID pid, SceUInt32 addr);
/* vitasdk: ksceKernelUnloadModuleForPid */
int SceModulemgrForKernel_FCA9FDB1(SceUID pid, SceUID modid, SceUInt32 flags,
				   const SceKernelUnloadModuleOpt *opt);
int sceKernelFinalizeKbl(void);
int sceKernelGetExportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num);
int sceKernelGetImportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num);
int sceKernelGetLibEntCBListForSyslibtrace(struct LibEntCB **list, SceSize *num);
/* name unknown: install the syscall tracer (tool mode) */
void SceModulemgrForKernel_B23286B8(SyscallTraceInfo *info, void *entry);
int sceKernelGetLibraryClientList(SceUID pid, SceUID libid, SceUID *list, SceSize *num,
				  SceUInt32 start);
int sceKernelGetLibraryDBFlags(SceUID pid, SceUInt32 *flags);
int sceKernelGetLibraryList(SceUID pid, SceUID *list, SceSize *num);
SceUID sceKernelGetModuleIdByAddrForDebugger(SceUID pid, SceUInt32 addr);
SceUID sceKernelGetModuleIdByPid(SceUID pid);
int sceKernelGetProgramIdentificationInfo(const char *path, SceUInt64 *authId, SceUInt64 *version);
int sceKernelGetStubList(SceUID pid, SceUID *list, SceUInt32 *count);
SceUID sceKernelLoadModule(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *opt);
int sceKernelLoadPreloadingModules(SceUID pid, const ProcessImageInfo *info, SceUInt32 flags);
SceUID sceKernelLoadProcessImage(SceUID pid, const char *path, SceUInt32 flags,
				 SceSelfAuthInfo *auth, ProcessImageInfo *info, const ModulemgrTitlePatch *patch);
int sceKernelLoadPtLoadSegForFwloader(const char *path, int index, void *dst, SceSize size,
				      int flags, SceSize *out);
int sceKernelMountBootimageFS(const char *path);
int sceKernelRegisterLibrary(const void *libent);
int sceKernelRegisterModulesAfterBoot(void);
int sceKernelStartModule(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			 const SceKernelStartModuleOpt *opt, int *status);
int sceKernelStartPreloadingModules(SceUID pid);
int sceKernelStopRemoteModule(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			      const SceKernelStopModuleOpt *opt, int *status);
int sceKernelUmountBootimageFS(void);
int sceKernelUnlinkNormalSyscall(SceUID pid);
int sceKernelUnloadModule(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *opt);
int sceKernelUnregisterLibrary(const void *libent);

/* SceModulemgr: the user syscalls */
SceUID _sceKernelLoadModule(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *uopt);
int _sceKernelUnloadModule(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *uopt);
SceUInt32 sceKernelGetAllowedSdkVersionOnSystem(void);
SceUID sceKernelGetModuleIdByAddrForUser(const void *addr);
int sceKernelGetModuleInfoForUser(SceUID modid, SceKernelModuleInfo *uinfo);
int sceKernelInhibitLoadingModule(SceUInt32 flags);
int sceKernelIsCalledFromSysModule(const void *addr);

/* backtrace.c: SceBacktraceForDriver */
struct SceKernelBacktraceFrame;
struct SceKernelBacktraceFrameForKernel;	/* sysmem/sysroot_modulemgr.h */
int sceKernelBacktrace(SceUID thid, struct SceKernelBacktraceFrame *frames, SceSize size,
			SceSize *count, SceUInt32 flags);	/* vitasdk: ksceKernelBacktrace */
int SceBacktraceForDriver_888E99B8(SceUID thid, struct SceKernelBacktraceFrameForKernel *frames,
				   SceSize size, SceSize *count, SceUInt32 flags);
int SceBacktraceForDriver_7C878F90(SceUID pid, const struct SceKernelBacktraceFrameForKernel *frames,
				   SceSize count, SceUInt32 start);

#ifdef __cplusplus
}
#endif
