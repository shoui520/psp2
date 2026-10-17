/*
 * SceModulemgr (os0:kd/modulemgr.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/types.h>

/* NID of ksceKernelGetSystemSwVersion as our NID db names it; fw is SceKernelFwInfo (size 0x28) */
int sceKernelGetSystemSwVersion(void *fw);
