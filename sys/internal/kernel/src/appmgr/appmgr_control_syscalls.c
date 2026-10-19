/* Public control system calls; the family filename is inferred. */

#include <intrmgr/syscall_copy.h>
#include <appmgr/appmgr.h>
#include "app_state_internal.h"

int _sceAppMgrSetBackRenderPortOwner(int appId)
{
	return appmgrSetBackRenderPortOwnerSyscall(appId);
}

int _sceAppMgrSetPowerSaveMode(int mode, int suspendBackgroundApps)
{
	return appmgrSetPowerSaveModeSyscall(mode, suspendBackgroundApps);
}

int _sceAppMgrIsCameraActive(int appId, int *active)
{
	int ret;

	BLOCK_OUT(active, sizeof(*active),
		  appmgrIsCameraActiveSyscall(appId, NULL),
		  appmgrIsCameraActiveSyscall(appId, (int *)k));
}

int _sceAppMgrSetShellScreenOrientation(int orientation)
{
	return appmgrSetShellScreenOrientationSyscall(orientation);
}

int _sceAppMgrSetRecommendedScreenOrientationForShell(int orientation)
{
	return appmgrSetRecommendedScreenOrientationForShellSyscall(orientation);
}
