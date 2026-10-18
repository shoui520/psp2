/*
 * vitasdk's lightweight condition variable header: SceKernelLwCondInfo is Sony's
 * (threadmgr/threadmgr.h), so vitasdk's copy and its getter are renamed aside.
 */
#pragma once

#include <psp2kern/kernel/threadmgr/lw_mutex.h>

#define SceKernelLwCondInfo SceKernelLwCondInfo_vitasdk
#define ksceKernelGetLwCondInfo ksceKernelGetLwCondInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/lw_cond.h>
#undef ksceKernelGetLwCondInfo
#undef SceKernelLwCondInfo

#include <threadmgr/lw_mutex.h>
