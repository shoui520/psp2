/*
 * SceAppMgrForDriver: the application data mounts SceDeci4pRDrfp makes for
 * the host (names unknown; the module's messages call them
 * sceAppMgrRdrfpMount / sceAppMgrRdrfpUmount).
 */

#pragma once

#include <psp2kern/types.h>

/* a mount point, e.g. "ux0:..." as SceAppMgr returns it */
typedef struct SceAppMgrRdrfpMountPoint {
	char path[16];
} SceAppMgrRdrfpMountPoint;

typedef struct SceAppMgrRdrfpPasscode {
	SceUInt32 data[8];
} SceAppMgrRdrfpPasscode;

int SceAppMgrForDriver_1574E1A4(int type, const char *path, const SceAppMgrRdrfpPasscode *passcode,
				SceAppMgrRdrfpMountPoint *mountPoint);	/* sceAppMgrRdrfpMount */
int SceAppMgrForDriver_E8DB7942(const SceAppMgrRdrfpMountPoint *mountPoint);	/* sceAppMgrRdrfpUmount */
int SceAppMgrForDriver_0813AFCA(int id, char *path, SceSize size);	/* a mount's path */
