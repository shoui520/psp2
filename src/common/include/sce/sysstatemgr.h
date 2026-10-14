/*
 * Imports of os0:kd/sysstatemgr.skprx not declared elsewhere (prototypes recovered from
 * its call sites; NID-named where the name is unknown).
 */

#pragma once

#include <psp2kern/types.h>

int ksceKernelInvokeInitCallback(int stage);
int SceSysmemForKernel_16A54BFE(void);
SceUID SceSysmemForKernel_0164D817(void);
int sceKernelGetPhyMemPartInfoCore(SceUID part, void *info);
int sceKernelSetGPO(int value);
int ksceSblQafMgrIsAllowHost0Access(void);
int ksceSblQafMgrIsAllowDecryptedBootConfigLoad(void);
int ksceSblACMgrIsDevelopmentMode2(void);
int SceSysrootForKernel_47724459(int stage);
int sceKernelSysrootSetStatus(int flags);
int SceSysrootForDriver_7C2C10E2(int kind, int value);
int sceKernelWaitProcessEnd(SceUID pid, int *status, void *timeout);
int SceProcessmgrForKernel_5137B783(SceUID pid, SceSize len, const void *args, void *opt);
int ksceKernelSysrootAppMgrSpawnProcess(const char *name, int type, const char *path, SceSize len,
					const void *args, void *opt, void *out);
