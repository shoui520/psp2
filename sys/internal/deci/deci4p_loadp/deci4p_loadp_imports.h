/*
 * Imports of SceDeci4pLoadp (os0:kd/deci4p_loadp.skprx) that vitasdk does not
 * declare. NID-named ones have no known name; the comment says what they do
 * as seen from the module.
 */

#pragma once

#include <psp2kern/types.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

/* SceAppMgrForDriver, debugger interface */
int sceAppMgrLaunchAppByNameForDebugger(const char *titleId, int flags);
int sceAppMgrGetPidByNameForDebugger(const char *titleId, SceUID *pid);
int sceAppMgrKillAppByNameForDebugger(const char *titleId);
int sceAppMgrLaunchAppForTest2(const char *path, const char *titleId, int flags, const char *args,
			       int option1, int option2, int option0, const void *data);
int ksceAppMgrDestroyOtherAppByPid(SceUID pid);
int SceAppMgrForDriver_3B4F305B(int kind, const char *titleId);	/* tool app control */

/* SceProcessmgrForDriver */
SceUID sceKernelSpawnProcessExt(const char *name, SceUInt32 attr, const char *path, SceSize argSize,
				const void *args, const SceKernelProcessOpt *opt, int gxmDebug);
int sceKernelKillProcess(SceUID pid, int status);

/* SceSblACMgrForDriver */
int SceSblACMgrForDriver_4CBD6156(SceUID pid);		/* the process may be debugged */
int SceSblACMgrForDriver_AD717E7A(int unk);		/* the caller may use test app launch */

/* named imports vitasdk declares only under other names */
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);
