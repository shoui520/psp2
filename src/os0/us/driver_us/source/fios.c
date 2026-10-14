/*
 * SceDriverUser: SceFios2User, the per-process overlay calls of the Fios2
 * kernel driver.
 */

#include "driver_user.h"

typedef SceInt32 SceFiosOverlayID;

#define FIOS_OVERLAY_LIST_MAX	128	/* IDs the kernel returns at most */

typedef struct FiosResolveRangeArgs {
	char *outPath;
	SceSize maxPath;
	SceUInt8 minOrder;
	SceUInt8 maxOrder;
	SceUInt8 reserved0[6];
	SceSize outSize;	/* bytes the kernel writes to outPath */
	SceUInt32 reserved1[2];
} FiosResolveRangeArgs;

int sceFiosKernelOverlayAddForProcess02(SceUID pid, const void *overlay, SceFiosOverlayID *outId);
int sceFiosKernelOverlayRemoveForProcess02(SceUID pid, SceFiosOverlayID id);
int sceFiosKernelOverlayGetInfoForProcess02(SceUID pid, SceFiosOverlayID id, void *outOverlay);
int sceFiosKernelOverlayModifyForProcess02(SceUID pid, SceFiosOverlayID id, const void *newValue);
int sceFiosKernelOverlayGetList02(SceUID pid, SceUInt8 minOrder, SceUInt8 maxOrder,
				  SceSyscallArgs6 *args);
int sceFiosKernelOverlayResolveSync02(SceUID pid, int resolveFlag, const char *inPath,
				      SceSyscallArgs6 *args);
int sceFiosKernelOverlayResolveWithRangeSync02(SceUID pid, int resolveFlag, const char *inPath,
					       FiosResolveRangeArgs *args);
int sceFiosKernelOverlayThreadSetDisabled02(SceInt32 disabled);
int sceFiosKernelOverlayThreadIsDisabled02(void);
int sceFiosKernelOverlayGetRecommendedScheduler02(int avail, const char *partiallyResolvedPath,
						  SceSyscallArgs2 *args);

int sceFiosOverlayAddForProcess02(SceUID pid, const void *overlay, SceFiosOverlayID *outId)
{
	return sceFiosKernelOverlayAddForProcess02(pid, overlay, outId);
}

int sceFiosOverlayRemoveForProcess02(SceUID pid, SceFiosOverlayID id)
{
	return sceFiosKernelOverlayRemoveForProcess02(pid, id);
}

int sceFiosOverlayGetInfoForProcess02(SceUID pid, SceFiosOverlayID id, void *outOverlay)
{
	return sceFiosKernelOverlayGetInfoForProcess02(pid, id, outOverlay);
}

int sceFiosOverlayModifyForProcess02(SceUID pid, SceFiosOverlayID id, const void *newValue)
{
	return sceFiosKernelOverlayModifyForProcess02(pid, id, newValue);
}

int sceFiosOverlayGetList02(SceUID pid, SceUInt8 minOrder, SceUInt8 maxOrder,
			    SceFiosOverlayID *outIds, SceSize maxIds, SceSize *actualIds)
{
	SceSyscallArgs6 args;

	args.arg[0] = (SceUInt32)outIds;
	args.arg[1] = maxIds;
	args.arg[2] = (SceUInt32)actualIds;
	args.arg[3] = maxIds > FIOS_OVERLAY_LIST_MAX ? FIOS_OVERLAY_LIST_MAX * sizeof(SceFiosOverlayID)
						     : maxIds * sizeof(SceFiosOverlayID);
	return sceFiosKernelOverlayGetList02(pid, minOrder, maxOrder, &args);
}

int sceFiosOverlayResolveSync02(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				SceSize maxPath)
{
	SceSyscallArgs6 args;

	args.arg[0] = (SceUInt32)outPath;
	args.arg[1] = maxPath;
	args.arg[3] = maxPath;
	return sceFiosKernelOverlayResolveSync02(pid, resolveFlag, inPath, &args);
}

int sceFiosOverlayResolveWithRangeSync02(SceUID pid, int resolveFlag, const char *inPath,
					 char *outPath, SceSize maxPath, SceUInt8 minOrder,
					 SceUInt8 maxOrder)
{
	FiosResolveRangeArgs args;

	args.outPath = outPath;
	args.minOrder = minOrder;
	args.maxOrder = maxOrder;
	args.maxPath = maxPath;
	args.outSize = maxPath;
	return sceFiosKernelOverlayResolveWithRangeSync02(pid, resolveFlag, inPath, &args);
}

int sceFiosOverlayThreadSetDisabled02(SceInt32 disabled)
{
	return sceFiosKernelOverlayThreadSetDisabled02(disabled);
}

int sceFiosOverlayThreadIsDisabled02(void)
{
	return sceFiosKernelOverlayThreadIsDisabled02();
}

int sceFiosOverlayGetRecommendedScheduler02(int avail, const char *partiallyResolvedPath)
{
	SceSyscallArgs2 args;

	return sceFiosKernelOverlayGetRecommendedScheduler02(avail, partiallyResolvedPath, &args);
}
