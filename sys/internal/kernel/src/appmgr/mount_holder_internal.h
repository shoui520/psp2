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
