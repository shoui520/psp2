/* Process-mount cleanup protocol. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

#include "mount_holder_internal.h"

struct AppmgrMapFile;

typedef struct AppmgrProcessMountCleanup {
	char driveId[16];
	char *resolvedPath;
	struct AppmgrMapFile *mapFile;
} AppmgrProcessMountCleanup;

_Static_assert(sizeof(AppmgrProcessMountCleanup) == 24,
	       "process mount cleanup size");
_Static_assert(offsetof(AppmgrProcessMountCleanup, driveId) == 0 &&
	       offsetof(AppmgrProcessMountCleanup, resolvedPath) == 16 &&
	       offsetof(AppmgrProcessMountCleanup, mapFile) == 20,
	       "process mount cleanup fields");

int appmgrUnmountDrive(SceUID pid, struct AppmgrMountHolder *holder,
		      const char *driveId, int force);
int appmgrCleanupProcessMount(struct AppmgrMountHolder *holder,
			      AppmgrProcessMountCleanup *cleanup);

extern const SceFiosOverlay g_appmgrOverlayTemplate;
int appmgrAddInvalidOverlay(SceUID pid, const char *driveId,
			    SceFiosOverlayID *overlayId);
int appmgrAddVirtualMountOverlay(SceUID pid, AppmgrVirtualMount *mount);
int appmgrCloneVirtualMount(SceUID pid, const AppmgrVirtualMount *source,
			    AppmgrPfsMount *const *previousPfsMount,
			    const ScePfsRndDriveId *driveId,
			    AppmgrMountHolder *holder,
			    AppmgrVirtualMount **outMount);
int appmgrCloneTemporaryMounts(SceUID pid, const char *titleId,
			       const AppmgrMountHolder *source,
			       AppmgrMountHolder *destination,
			       ScePfsRndDriveId *outDriveId);
