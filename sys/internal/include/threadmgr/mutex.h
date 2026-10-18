/* SceKernelThreadMgr: kernel mutex information (SceKernelMutexInfo: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

int ksceKernelGetMutexInfo(SceUID mutexId, SceKernelMutexInfo *info);
