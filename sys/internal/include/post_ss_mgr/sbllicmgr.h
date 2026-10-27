#pragma once

#include <scetypes.h>
#include <stdint.h>

int sceSblLicMgrActivateFromFs(void);
int sceSblLicMgrClearActivationData(void);
int sceSblLicMgrGetLicenseStatusForUser(void);
int sceSblLicMgrGetUsageTimeLimit(SceUInt32 *limit);
