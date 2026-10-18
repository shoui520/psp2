/*
 * vitasdk's lightweight mutex header: SceKernelLwMutexInfo is Sony's (threadmgr/threadmgr.h),
 * so vitasdk's copy and its getter are renamed aside.
 */
#pragma once

#define SceKernelLwMutexInfo SceKernelLwMutexInfo_vitasdk
#define ksceKernelGetLwMutexInfo ksceKernelGetLwMutexInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/lw_mutex.h>
#undef ksceKernelGetLwMutexInfo
#undef SceKernelLwMutexInfo

#include <threadmgr/lw_mutex.h>
