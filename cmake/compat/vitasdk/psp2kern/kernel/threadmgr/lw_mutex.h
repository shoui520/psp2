/* Retain the SDK declaration while exposing the canonical kernel API. */
#pragma once

#define ksceKernelGetLwMutexInfo ksceKernelGetLwMutexInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/lw_mutex.h>
#undef ksceKernelGetLwMutexInfo

#include <threadmgr/lw_mutex.h>
