/* Kernel entry adapters called by generated user-copy exports. */

#include "appmgr_device.h"
#include "appmgr_event.h"
#include "mount.h"

extern int sceAppMgrSetDisplayMergeConf(SceUInt32 setting, SceUInt32 alpha);
extern int appmgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);
extern int appmgrReleaseSoundOutExclusive2(void);
extern int appmgrReleaseSoundOutExclusive(void);
extern int sceAppMgrEnablePrioritizingSystemChat(int enabled);
extern int sceAppMgrAcquireBgmPortForMusicPlayer(void);
extern int sceAppMgrEnableDuckingOnSystemChat(int enabled, SceUInt32 volume);
extern int appmgrReleaseSoundOutExclusive3(void);
extern int sceAppMgrSetBgmSubPriority(int enabled, int priority);
extern int sceAppMgrDeclareSystemChatApp(void);
extern int sceAppMgrSetBgmSubPriorityForSystemChat(int enabled);
extern int sceAppMgrAcquireBgmPortWithPriority(int priority);
extern int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);
extern int sceAppMgrReleaseBtrm(void);
extern int sceAppMgrAcquireBgmPort(void);
extern int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest);
extern int sceAppMgrAcquireBtrm(void);
extern int sceAppMgrReleaseBgmPort(void);
extern int sceAppMgrSetBgmProxyApp(const char *appName);
extern int appmgrSystemParamGetInt(int parameter, int *value);
extern int appmgrSystemParamDateTimeGetConf(void *conf);
extern int appmgrSystemParamGetString(int parameter, char *value, SceSize size);
extern int appmgrGetVs0UserModuleDrive(char *mountPoint);
extern int appmgrGetVs0UserDataDrive(char *mountPoint);
extern int appmgrCheckRifGD(const char *path, AppmgrLicenseInfo *info);
extern int appmgrCountPfsMountReferences(unsigned int mountId, int flags);

int appmgrSystemParamDateTimeGetConfSyscall(void *conf)
{
	return appmgrSystemParamDateTimeGetConf(conf);
}

int appmgrSystemParamGetIntSyscall(int parameter, int *value)
{
	return appmgrSystemParamGetInt(parameter, value);
}

int appmgrSystemParamGetStringSyscall(int parameter, char *value, SceSize size)
{
	return appmgrSystemParamGetString(parameter, value, size);
}

int appmgrCountPfsMountReferencesSyscall(unsigned int mountId, int flags)
{
	return appmgrCountPfsMountReferences(mountId, flags);
}

int appmgrGetVs0UserModuleDriveSyscall(char *mountPoint)
{
	return appmgrGetVs0UserModuleDrive(mountPoint);
}

int appmgrGetVs0UserDataDriveSyscall(char *mountPoint)
{
	return appmgrGetVs0UserDataDrive(mountPoint);
}

int appmgrCheckRifGDSyscall(const char *path, AppmgrLicenseInfo *info)
{
	return appmgrCheckRifGD(path, info);
}

int appmgrAcquireBgmPortSyscall(void)
{
	return sceAppMgrAcquireBgmPort();
}

int appmgrAcquireBgmPortForMusicPlayerSyscall(void)
{
	return sceAppMgrAcquireBgmPortForMusicPlayer();
}

int appmgrReleaseBgmPortSyscall(void)
{
	return sceAppMgrReleaseBgmPort();
}

int appmgrAcquireBgmPortWithPrioritySyscall(int priority)
{
	return sceAppMgrAcquireBgmPortWithPriority(priority);
}

int appmgrSetBgmSubPrioritySyscall(int enabled, int priority)
{
	return sceAppMgrSetBgmSubPriority(enabled, priority);
}

int appmgrSetBgmSubPriorityForSystemChatSyscall(int enabled)
{
	return sceAppMgrSetBgmSubPriorityForSystemChat(enabled);
}

int appmgrEnablePrioritizingSystemChatSyscall(int enabled)
{
	return sceAppMgrEnablePrioritizingSystemChat(enabled);
}

int appmgrEnableDuckingOnSystemChatSyscall(int enabled, SceUInt32 volume)
{
	return sceAppMgrEnableDuckingOnSystemChat(enabled, volume);
}

int appmgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait)
{
	return _appmgrQueuePush(request, wait);
}

int sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode)
{
	return appmgrAcquireSoundOutExclusive(priority, displayEnabled, displayMode);
}

int sceAppMgrReleaseSoundOutExclusive(void)
{
	return appmgrReleaseSoundOutExclusive();
}

int sceAppMgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest)
{
	return appmgrAcquireSoundOutExclusive2(priority, displayEnabled, displayMode, displayRequest);
}

int sceAppMgrReleaseSoundOutExclusive2(void)
{
	return appmgrReleaseSoundOutExclusive2();
}

int appmgrAcquireSoundOutExclusive3Syscall(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions)
{
	return appmgrAcquireSoundOutExclusive3(priority, displayEnabled,
		displayMode, displayBlanked, unsupportedOptions);
}

int sceAppMgrReleaseSoundOutExclusive3(void)
{
	return appmgrReleaseSoundOutExclusive3();
}

int appmgrDeclareSystemChatAppSyscall(void)
{
	return sceAppMgrDeclareSystemChatApp();
}

int appmgrSetBgmProxyAppSyscall(const char *appName)
{
	return sceAppMgrSetBgmProxyApp(appName);
}

int appmgrAcquireBtrmSyscall(void)
{
	return sceAppMgrAcquireBtrm();
}

int appmgrReleaseBtrmSyscall(void)
{
	return sceAppMgrReleaseBtrm();
}

int appmgrSetDisplayMergeConfSyscall(SceUInt32 setting, SceUInt32 alpha)
{
	return sceAppMgrSetDisplayMergeConf(setting, alpha);
}
