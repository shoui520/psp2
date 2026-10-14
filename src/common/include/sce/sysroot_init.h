/*
 * SceSysmem sysroot boot init callbacks (SceSysrootForDriver).
 */

#pragma once

#include <stdint.h>

/*
 * Run when sysroot reaches their phase (0..8), with the kind that phase was
 * invoked with; one registered after its phase has passed runs at once,
 * with kind 0.
 */
typedef int (*SysrootInitCallback)(uint32_t kind, void *argument);

int sceKernelRegisterInitCallback(SysrootInitCallback callback, uint32_t phase,
				  void *argument, int *result);
