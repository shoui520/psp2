/*
 * SceSysmem: the system event hooks suspend.c resets, and the init functions.
 */

#pragma once

#include <psp2kern/types.h>

extern void (*g_sysEventPreHook)(int id, int event, void *args, void *handler);
extern void (*g_sysEventPostHook)(int id, int event, int result);

int SuspendInit(void);
int sceKernelSysEventInit(void);
