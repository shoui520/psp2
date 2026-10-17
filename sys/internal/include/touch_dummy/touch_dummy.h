/* SceTouchDummy imports absent from the public kernel headers. */
#pragma once
#include <psp2kern/types.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <threadmgr/threadmgr.h>

int ksceKernelPrintf(const char *format, ...);
