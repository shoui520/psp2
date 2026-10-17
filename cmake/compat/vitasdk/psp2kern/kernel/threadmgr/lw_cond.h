/* Retain the SDK declaration while exposing the canonical kernel API. */
#pragma once

#include <threadmgr/lw_mutex.h>

#define ksceKernelGetLwCondInfo ksceKernelGetLwCondInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/lw_cond.h>
#undef ksceKernelGetLwCondInfo

#include <threadmgr/lw_mutex.h>
