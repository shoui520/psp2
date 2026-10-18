#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int sceSblPmMgrGetCurrentMode(SceUInt8 *mode);
int sceSblPmMgrGetProductModeForUser(SceUInt8 *mode);
int sceSblPmMgrGetProductModeFromNVS(SceUInt8 *mode);
int sceSblPmMgrSetProductModeOffForUser(void);
