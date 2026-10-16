/* Retain the SDK declaration while exposing the complete kernel API. */
#pragma once

#define ksceKernelGetMutexInfo ksceKernelGetMutexInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/mutex.h>
#undef ksceKernelGetMutexInfo

#include <sce/mutex.h>
