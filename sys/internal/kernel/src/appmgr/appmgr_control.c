/* Scalar control syscall adapters; the family filename is inferred. */

#include "appmgr_device.h"
#include "app_state_internal.h"

extern int appmgrSetShellScreenOrientation(int orientation);
extern int sceAppMgrSetRecommendedScreenOrientationForShell(int orientation);

int appmgrSetBackRenderPortOwnerSyscall(int appId)
{
	return sceAppMgrSetBackRenderPortOwner(appId);
}

int appmgrIsCameraActiveSyscall(int appId, int *active)
{
	return sceAppMgrIsCameraActive(appId, active);
}

int appmgrSetPowerSaveModeSyscall(int mode, int suspendBackgroundApps)
{
	return appmgrSetPowerSaveMode(mode, suspendBackgroundApps);
}

int appmgrSetShellScreenOrientationSyscall(int orientation)
{
	return appmgrSetShellScreenOrientation(orientation);
}

int appmgrSetRecommendedScreenOrientationForShellSyscall(int orientation)
{
	return sceAppMgrSetRecommendedScreenOrientationForShell(orientation);
}
