/*
 * SceAppMgrForDriver: the application data mounts SceDeci4pRDrfp makes for
 * the host (names unknown; the module's messages call them
 * sceAppMgrRdrfpMount / sceAppMgrRdrfpUmount).
 */

#pragma once

#include <scetypes.h>

/* a mount point, e.g. "ux0:..." as SceAppMgr returns it */
typedef struct SceAppMgrRdrfpMountPoint {
	char path[16];
} SceAppMgrRdrfpMountPoint;

typedef struct SceAppMgrRdrfpPasscode {
	SceUInt32 data[8];
} SceAppMgrRdrfpPasscode;

int sceAppMgrRdrfpMount(unsigned int type, const char *path, const SceAppMgrRdrfpPasscode *passcode,
				SceAppMgrRdrfpMountPoint *mountPoint);	/* sceAppMgrRdrfpMount */
int sceAppMgrRdrfpUmount(const SceAppMgrRdrfpMountPoint *mountPoint);	/* sceAppMgrRdrfpUmount */
int sceAppMgrGetUserDirPath(int id, char *path, SceSize size);	/* a mount's path */

int SceAppMgrForDriver_5B1BE482(const void *status);	/* vitasdk: ksceAppMgrBgdlSetQueueStatus */
int SceAppMgrForDriver_0B1BEEE0(const char *path);	/* vitasdk: ksceAppMgrCheckPfsMounted */
int SceAppMgrForDriver_7554330F(int setting);	/* vitasdk: ksceAppMgrDebugSettingNotifyUpdate */
/* vitasdk: ksceAppMgrSystemParamDateTimeSetConf */
int SceAppMgrForDriver_A9AA3A68(const void *conf);

int SceAppMgrForDriver_2277F736(void *parameters);	/* vitasdk: ksceAppMgrSaveDataSlotInit */
/* vitasdk: ksceAppMgrSaveDataSlotSetParam */
int SceAppMgrForDriver_7E247E47(void *parameters);
/* vitasdk: ksceAppMgrSaveDataSlotSetStatus */
int SceAppMgrForDriver_9ADB00FA(void *parameters);
int SceAppMgrForDriver_E6F389B8(void *parameters);	/* vitasdk: ksceAppMgrSaveDataSlotCreate */
SceUID sceAppMgrGetShellPidForDriver(void);
int sceAppMgrResumeAppByNameForDebugger(const char *appName);
int sceAppMgrResumeAppForDebugger(SceUID pid);
SceUID sceAppMgrSpawnProcessForDriver(const char *name, int type, const char *path, SceSize args, const void *argp, void *option, void *out);
int sceAppMgrSuspendAppByNameForDebugger(const char *appName);
int sceAppMgrSuspendAppForDebugger(SceUID pid);
