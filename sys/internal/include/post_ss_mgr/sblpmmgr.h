#pragma once

#include <scetypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceSblPmMgrGetCurrentMode(SceUInt8 *mode);
int sceSblPmMgrGetProductModeForUser(SceUInt8 *mode);
int sceSblPmMgrGetProductModeFromNVSForUser(SceUInt8 *mode);
int sceSblPmMgrSetProductModeOffForUser(void);

#ifdef __cplusplus
}
#endif
