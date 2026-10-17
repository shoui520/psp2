/* SceSysmem's debug LEDs and switches (SceDebugLed, SceDebugLedForDriver): the GPI and GPO words. */

#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

SceUInt32 sceKernelGetGPI(void);
int sceKernelSetGPO(SceUInt32 gpo);
