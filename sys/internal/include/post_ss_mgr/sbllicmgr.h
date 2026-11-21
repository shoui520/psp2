#pragma once

#include <scetypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceSblLicMgrActivateFromFs(void);
int sceSblLicMgrClearActivationData(void);
int sceSblLicMgrGetLicenseStatusForUser(void);
int sceSblLicMgrGetUsageTimeLimit(SceUInt32 *limit);

#ifdef __cplusplus
}
#endif
