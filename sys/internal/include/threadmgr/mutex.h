/* SceKernelThreadMgr: kernel mutex information (SceKernelMutexInfo: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceKernelGetMutexInfo(SceUID mutexId, SceKernelMutexInfo *info);

#ifdef __cplusplus
}
#endif
