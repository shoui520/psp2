/* Public control system calls; the family filename is inferred. */

#include <sce/syscall_copy.h>

extern int appmgrSetBackRenderPortOwnerSyscall(int appId);
extern int appmgrIsCameraActiveSyscall(int appId, int *active);
extern int appmgrSetPowerSaveModeSyscall(int mode, int suspendBackgroundApps);
extern int appmgrSetShellScreenOrientationSyscall(int orientation);
extern int appmgrSetRecommendedScreenOrientationForShellSyscall(int orientation);

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
