/* SceKernelThreadMgr: kernel timer information (SceKernelTimerInfo: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceKernelGetTimerInfo(SceUID timerId, SceKernelTimerInfo *info);

#ifdef __cplusplus
}
#endif
