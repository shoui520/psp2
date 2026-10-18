/*
 * SceKernelModulemgr (os0:kd/modulemgr.skprx): the module manager's exports, each declared once
 * with its definition as the prototype, and the records they fill.
 */

#pragma once

#include <psp2kern/types.h>
#include <kernel/modulemgr.h>
#include <modulemgr/modulemgr_control.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProcessImageInfo ProcessImageInfo;	/* processmgr/process.h */

/* a loaded segment of a module (SceKernelModuleInfo.segments) */
typedef struct SceKernelSegmentInfo {
	SceSize size;			/* 0x18 */
	SceUInt perms;
	void *vaddr;
	SceSize memsz;
	SceSize filesz;
	SceUInt reserved;
} SceKernelSegmentInfo;

/* a module's information (SceModulemgrForKernel_DAA90093, the sceKernelGetModuleInfo syscall) */
typedef struct SceKernelModuleInfo {
	SceSize size;			/* 0x1b8 */
	SceUID modid;
	SceUInt16 modattr;
	SceUInt8 modver[2];
	char module_name[28];
	SceUInt reserved;
	void *start_entry;
	void *stop_entry;
	void *exit_entry;
	void *exidx_top;
	void *exidx_btm;
	void *extab_top;
	void *extab_btm;
	void *tlsInit;
	SceSize tlsInitSize;
	SceSize tlsAreaSize;
	char path[256];
	SceKernelSegmentInfo segments[4];
	SceUInt state;			/* 2 loaded, 6 started, 9 stopped */
} SceKernelModuleInfo;

/* sceKernelGetSystemSwVersion's output */
typedef struct SceKernelSystemSwVersion {
	SceSize size;			/* 0x28 */
	char versionString[0x1c];	/* "%d.%02d" */
	SceUInt version;		/* SceSysrootForDriver_67AAB627 */
	SceUInt versionExtra;		/* 0 */
} SceKernelSystemSwVersion;

_Static_assert(sizeof(SceKernelSegmentInfo) == 0x18, "SceKernelSegmentInfo size");
_Static_assert(sizeof(SceKernelModuleInfo) == 0x1b8, "SceKernelModuleInfo size");
_Static_assert(sizeof(SceKernelSystemSwVersion) == 0x28, "SceKernelSystemSwVersion size");

int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);

/* vitasdk: ksceKernelStopUnloadSharedModuleForPid */
int SceModulemgrForDriver_02D3D0C1(SceUID pid, SceUID modid, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelUnloadModuleOpt *opt,
				   int *status);
/* vitasdk: ksceKernelStopUnloadModule */
int SceModulemgrForDriver_03B30B7E(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				   const SceKernelUnloadModuleOpt *opt, int *status);
/* vitasdk: ksceKernelStopModule */
int SceModulemgrForDriver_100DAEB9(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				   const SceKernelStopModuleOpt *opt, int *status);
/* vitasdk: ksceKernelLoadStartModule */
SceUID SceModulemgrForDriver_189BFBBB(const char *path, SceSize args, void *argp, SceUInt32 flags,
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
/* vitasdk: ksceKernelGetModuleCB */
int SceModulemgrForKernel_37512E29(SceUID modid, SceModuleCB **cb);
int SceModulemgrForKernel_3FE47DDF(SceUID pid, SceUID modid, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelStartModuleOpt *opt,
				   int *status);	/* vitasdk: ksceKernelStartModuleForPid */
/* vitasdk: ksceKernelLoadModuleForPid */
SceUID SceModulemgrForKernel_4E85022D(SceUID pid, const char *path, SceUInt32 flags,
				      const SceKernelLoadModuleOpt *opt);
/* vitasdk: ksceKernelGetModuleInfoMinByAddr */
int SceModulemgrForKernel_5564A860(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint,
				   void **textBase, char *name);
int SceModulemgrForKernel_74A4E0EE(void (*callback)(SceModuleCB *cb));
/* vitasdk: ksceKernelGetModulePath */
int SceModulemgrForKernel_79E761B5(SceUID modid, char *path, SceSize size);
/* vitasdk: ksceKernelGetModuleEntryPoint */
SceModuleEntry SceModulemgrForKernel_7B302C5D(SceUID modid);
int SceModulemgrForKernel_ACBC97C9(void (*callback)(SceModuleCB *cb));
/* vitasdk: ksceKernelGetModuleInfo */
int SceModulemgrForKernel_DAA90093(SceUID pid, SceUID modid, SceKernelModuleInfo *info);
int SceModulemgrForKernel_E71530D7(SceUID pid);	/* vitasdk: ksceKernelUnloadProcessModules */
/* vitasdk: ksceKernelGetModuleIsSharedByAddr */
int SceModulemgrForKernel_F1DE6949(SceUID pid, SceUInt32 addr);
/* vitasdk: ksceKernelUnloadModuleForPid */
int SceModulemgrForKernel_FCA9FDB1(SceUID pid, SceUID modid, SceUInt32 flags,
				   const SceKernelUnloadModuleOpt *opt);
int sceKernelFinalizeKbl(void);
int sceKernelGetExportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num);
int sceKernelGetImportedLibraryListInModule(SceUID pid, SceUID modid, SceUID *list, SceSize *num);
int sceKernelGetLibEntCBListForSyslibtrace(struct LibEntCB **list, SceSize *num);
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
SceUID sceKernelGetModuleIdByAddr(const void *addr);
int sceKernelGetModuleInfo(SceUID modid, SceKernelModuleInfo *uinfo);
int sceKernelInhibitLoadingModule(SceUInt32 flags);
int sceKernelIsCalledFromSysModule(const void *addr);

#ifdef __cplusplus
}
#endif
