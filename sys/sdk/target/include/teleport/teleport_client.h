/*
 * teleport/teleport_client.h: SceTeleportClient public C API.
 */
#ifndef _SCE_TELEPORT_CLIENT_H_
#define _SCE_TELEPORT_CLIENT_H_

#include <scetypes.h>
#include <sceerror.h>
#include "teleport/teleport_error.h"
#include "teleport/teleport_client_types.h"

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 sceTeleportClientInitialize(SceTeleportClientInitializeInfo *info);
SceInt32 sceTeleportClientRegisterGetDeviceInfoCallback(SceTeleportClientGetDeviceInfoCallback func);
SceInt32 sceTeleportClientStartSearchDevice(void);
SceInt32 sceTeleportClientGetDeviceDescription(const SceTeleportDeviceInfo *device,
					       SceTeleportDeviceDescription *desc,
					       SceUInt32 timeout);
SceInt32 sceTeleportClientGetRemoteAppInfoNum(const SceTeleportDeviceInfo *device,
					      SceTeleportRemoteAppInfoNum *infoNum,
					      SceUInt32 timeout);
SceInt32 sceTeleportClientGetRemoteAppInfo(const SceTeleportDeviceInfo *device,
					   SceUInt16 size,
					   SceTeleportRemoteAppInfo **info,
					   SceUInt32 timeout);
SceInt32 sceTeleportClientLaunchRemoteApp(const SceTeleportDeviceInfo *device,
					  const SceTeleportRemoteAppInfo *appInfo,
					  const SceTeleportRemoteAppArg *argInfo,
					  SceTeleportLaunchResponse *res,
					  SceUInt32 timeout);
SceInt32 sceTeleportClientDeleteRemoteApp(const SceTeleportDeviceInfo *device,
					  const SceTeleportRemoteAppInfo *appInfo,
					  SceUInt32 timeout);
SceInt32 sceTeleportClientGetCacheInfo(SceTeleportCacheInfo *cacheInfo);
SceInt32 sceTeleportClientClearCacheInfo(void);
SceInt32 sceTeleportClientWakeupLatestDevice(const SceTeleportWakeupInfo *info);
SceInt32 sceTeleportClientEndSearchDevice(void);
SceInt32 sceTeleportClientFinalize(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_TELEPORT_CLIENT_H_ */
