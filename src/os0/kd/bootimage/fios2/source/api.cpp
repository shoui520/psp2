/*
 * The FIOS2 API as the syscalls see it: each call goes to the kernel API,
 * except the directory handle calls, which user mode does not get.
 */

#include "fios2_internal.h"

int sceFiosKernelOverlayAdd(SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	return ksceFiosKernelOverlayAdd(overlay, outId);
}

int sceFiosKernelOverlayRemove(SceFiosOverlayID id)
{
	return ksceFiosKernelOverlayRemove(id);
}

int sceFiosKernelOverlayGetInfo(SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	return ksceFiosKernelOverlayGetInfo(id, outOverlay);
}

int sceFiosKernelOverlayModify(SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	return ksceFiosKernelOverlayModify(id, overlay);
}

int sceFiosKernelOverlayAddForProcess(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	return ksceFiosKernelOverlayAddForProcess(pid, overlay, outId);
}

int sceFiosKernelOverlayRemoveForProcess(SceUID pid, SceFiosOverlayID id)
{
	return ksceFiosKernelOverlayRemoveForProcess(pid, id);
}

int sceFiosKernelOverlayGetInfoForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	return ksceFiosKernelOverlayGetInfoForProcess(pid, id, outOverlay);
}

int sceFiosKernelOverlayModifyForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	return ksceFiosKernelOverlayModifyForProcess(pid, id, overlay);
}

int sceFiosKernelOverlayGetList(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount)
{
	return ksceFiosKernelOverlayGetList(pid, minOrder, maxOrder, outIds, maxIds, outCount);
}

int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				    SceSize maxPath)
{
	return ksceFiosKernelOverlayResolveSync(pid, resolveFlag, inPath, outPath, maxPath);
}

int sceFiosKernelOverlayResolveWithRangeSync(SceUID pid, int resolveFlag, const char *inPath,
					     char *outPath, SceSize maxPath, uint8_t minOrder,
					     uint8_t maxOrder)
{
	return ksceFiosKernelOverlayResolveWithRangeSync(pid, resolveFlag, inPath, outPath, maxPath,
							 minOrder, maxOrder);
}

int sceFiosKernelOverlayDHOpenSync(SceFiosKernelDH **outDH, const char *path, uint8_t minOrder,
				   uint8_t maxOrder)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHReadSync(SceFiosKernelDH *dh, SceIoDirent *dirent)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHCloseSync(SceFiosKernelDH *dh)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHStatSync(SceFiosKernelDH *dh, SceIoStat *stat)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHChstatSync(SceFiosKernelDH *dh, SceIoStat *stat, int bits)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHSyncSync(SceFiosKernelDH *dh, int flag)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayThreadSetDisabled(int disabled)
{
	return ksceFiosKernelOverlayThreadSetDisabled(disabled);
}

int sceFiosKernelOverlayThreadIsDisabled(void)
{
	return ksceFiosKernelOverlayThreadIsDisabled();
}

int sceFiosKernelOverlayGetRecommendedScheduler(int avail, const char *path)
{
	return ksceFiosKernelOverlayGetRecommendedScheduler(avail, path);
}
