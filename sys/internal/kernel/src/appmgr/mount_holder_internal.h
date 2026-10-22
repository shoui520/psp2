/* AppMgr virtual mounts and per-process mount lists. */
#pragma once

#include "mount.h"
#include <fios2/fios2.h>

typedef struct AppmgrVirtualMount {
	AppmgrPfsMount *pfsMount;
	SceFiosOverlayID overlayId;
	ScePfsRndDriveId driveId;
	AppmgrPfsMount *previousPfsMount;
	struct AppmgrVirtualMount *prev;
	struct AppmgrVirtualMount *next;
} AppmgrVirtualMount;

typedef struct AppmgrMountHolder {
	unsigned char reserved[4];
	AppmgrVirtualMount *firstMount;
} AppmgrMountHolder;

_Static_assert(sizeof(AppmgrVirtualMount) == 36 &&
	       offsetof(AppmgrVirtualMount, pfsMount) == 0 &&
	       offsetof(AppmgrVirtualMount, overlayId) == 4 &&
	       offsetof(AppmgrVirtualMount, driveId) == 8 &&
	       offsetof(AppmgrVirtualMount, previousPfsMount) == 24 &&
	       offsetof(AppmgrVirtualMount, prev) == 28 &&
	       offsetof(AppmgrVirtualMount, next) == 32,
	       "virtual mount fields");
_Static_assert(sizeof(AppmgrMountHolder) == 8 &&
	       offsetof(AppmgrMountHolder, reserved) == 0 &&
	       offsetof(AppmgrMountHolder, firstMount) == 4,
	       "mount holder fields");

struct AppmgrMapFile;

/* what preparing a process's content leaves to undo once the process is created */
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

int appmgrCleanupProcessMount(AppmgrMountHolder *holder,
			      AppmgrProcessMountCleanup *cleanup);
