/* Application-manager callbacks registered with SceSysroot. */

#pragma once

#include <psp2kern/types.h>

typedef int (*SysrootAppMgrSpawnProcess)(const char *name, int type, const char *path,
					 SceSize length, const void *args, void *opt,
					 void *out);

void SceSysrootForDriver_2BE874EF(int (*callback)(int pid, void *out));
void SceSysrootForDriver_3B19B06B(int (*callback)(int appId, void *out));
void SceSysrootForDriver_70AD47A9(int (*callback)(void *out));
void SceSysrootForDriver_9A8871F1(SysrootAppMgrSpawnProcess callback);
void SceSysrootForDriver_E9B4221F(int (*callback)(void));
SceUInt32 SceSysrootForDriver_525B24C5(void);
