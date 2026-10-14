/* SceTouchDummy imports absent from the public kernel headers. */
#pragma once
#include <psp2kern/types.h>
#include <psp2kern/kernel/suspend.h>

int sceKernelCancelAlarm(SceUID alarm);
int ksceKernelEnqueueWorkQueue(SceUID queue, const char *name,
			     int (*handler)(void *), void *argp);
int ksceKernelPrintf(const char *format, ...);
