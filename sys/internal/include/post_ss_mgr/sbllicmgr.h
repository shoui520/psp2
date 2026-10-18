#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int sceSblLicMgrActivateFromFs(void);
int sceSblLicMgrClearActivationData(void);
int sceSblLicMgrGetLicenseStatus(void);
int sceSblLicMgrGetUsageTimeLimit(SceUInt32 *limit);
