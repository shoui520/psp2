/*
 * Imports of os0:kd/sysstatemgr.skprx not declared elsewhere (prototypes recovered from
 * its call sites; NID-named where the name is unknown).
 */

#pragma once

#include <psp2kern/types.h>
#include <sysmem/debugled.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>

int ksceKernelInvokeInitCallback(int stage);
SceUID SceSysmemForKernel_0164D817(void);
int sceKernelGetPhyMemPartInfoCore(SceUID part, void *info);
int ksceSblACMgrIsDevelopmentMode2(void);
int sceKernelSysrootSetStatus(int flags);
int SceSysrootForDriver_7C2C10E2(int kind, int value);
int sceKernelWaitProcessEnd(SceUID pid, int *status, void *timeout);
int SceProcessmgrForKernel_5137B783(SceUID pid, SceSize len, const void *args, void *opt);
int ksceKernelSysrootAppMgrSpawnProcess(const char *name, int type, const char *path, SceSize len,
					const void *args, void *opt, void *out);
